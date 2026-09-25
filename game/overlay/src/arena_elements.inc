// Elemental surfaces are battle-local. No heap, save fields or per-frame RNG.
// A conducted projectile resolves its native hit once, never once per puddle.
#define ELEMENT_COUNT 6
#define ELEMENT_TAG 0xA7E0
#define ELEMENT_PAL 0xA742
enum { SURFACE_NONE, SURFACE_WATER, SURFACE_ICE, SURFACE_CHARGED };
struct ArenaSurface {s16 x,y;u16 life,age;u8 kind,sprite;};
static EWRAM_DATA struct ArenaSurface sSurfaces[ELEMENT_COUNT]={};
static EWRAM_DATA s16 sSlide[2][2]={};
static EWRAM_DATA s16 sElementInput[2][2]={};
static EWRAM_DATA bool8 sElementApplying=FALSE;
static EWRAM_DATA u8 sElementCrash[2]={};
static EWRAM_DATA u8 sElementFrames[4]={};
static EWRAM_DATA u8 sElementBurst=MAX_SPRITES,sElementBurstAge=0;
// deposits, freezes, charge chains, conducted hits, slide frames, wall crashes,
// melts, expired, peak surfaces, allocation failures, grounded immune, ticks.
EWRAM_DATA u32 gArenaElementTelemetry[12]={};
static const u32 sElementTiles[]=INCBIN_U32("graphics/arena/elements/pools.4bpp");
static const u32 sElementBreak[]=INCBIN_U32("graphics/arena/elements/shatter.4bpp");
static const u16 sElementPalette[]=INCBIN_U16("graphics/arena/elements/palette.gbapal");
static const struct OamData sElementOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=2};
static const struct OamData sElementBurstOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
static const struct SpriteTemplate sElementTemplate={.tileTag=ELEMENT_TAG,.paletteTag=ELEMENT_PAL,
    .oam=&sElementOam,.anims=gDummySpriteAnimTable,.images=NULL,
    .affineAnims=gDummySpriteAffineAnimTable,.callback=SpriteCallbackDummy};

static bool8 ElementGrounded(u8 side)
{return !IS_BATTLER_OF_TYPE(side,TYPE_FLYING)&&gBattleMons[side].ability!=ABILITY_LEVITATE;}
static bool8 ElementContains(const struct ArenaSurface *p,s16 x,s16 y)
{s32 dx=x-p->x,dy=y-p->y;return p->kind && dx*dx+dy*dy*4<28*28;}
static s16 ElementIce(u8 side)
{
    u32 i;
    if(!ElementGrounded(side)||IS_BATTLER_OF_TYPE(side,TYPE_ICE))return -1;
    for(i=0;i<ELEMENT_COUNT;i++)if(sSurfaces[i].kind==SURFACE_ICE&&ElementContains(&sSurfaces[i],sArena.bodies[side].x/Q,sArena.bodies[side].y/Q))return i;
    return -1;
}
static bool8 ElementMotion(u8 side,s32 dx,s32 dy)
{
    if(sElementApplying)return FALSE;
    if(ElementIce(side)<0){sSlide[side][0]=sSlide[side][1]=0;return FALSE;}
    sElementInput[side][0]=Clamp(sElementInput[side][0]+dx,-1800,1800);
    sElementInput[side][1]=Clamp(sElementInput[side][1]+dy,-1800,1800);
    return TRUE;
}
static void ElementsResetBattle(void)
{
    memset(sSurfaces,0,sizeof(sSurfaces));
    memset(gArenaElementTelemetry,0,sizeof(gArenaElementTelemetry));
}
static void ElementsInit(void)
{
    u32 i;
    struct SpriteTemplate t=sElementTemplate;
    memset(sSlide,0,sizeof(sSlide));
    memset(sElementInput,0,sizeof(sElementInput));memset(sElementCrash,0,sizeof(sElementCrash));sElementApplying=FALSE;
    memset(sElementFrames,255,sizeof(sElementFrames));sElementBurstAge=0;
    // Feedback dust/number bank only uses 1..2, so 5..11 are ours.
    LoadPalette(sElementPalette,OBJ_PLTT_ID(IndexOfSpritePaletteTag(ELEMENT_PAL))+5,14);
    for(i=0;i<3;i++)
    {
        struct SpriteSheet sheet={sElementTiles+i*8*512,2048,ELEMENT_TAG+i};
        if(LoadSpriteSheet(&sheet)==0xFFFF)gArenaElementTelemetry[9]++;
    }
    for(i=0;i<ELEMENT_COUNT;i++)
    {
        sSurfaces[i].sprite=MAX_SPRITES;
        if(sSurfaces[i].kind)
        {
            sSurfaces[i].sprite=CreateSprite(&sElementTemplate,sSurfaces[i].x,sSurfaces[i].y-10,20);
            if(sSurfaces[i].sprite==MAX_SPRITES)gArenaElementTelemetry[9]++;
        }
    }
    {
        struct SpriteSheet sheet={sElementBreak,2048,ELEMENT_TAG+3};
        sElementBurst=MAX_SPRITES;
        if(LoadSpriteSheet(&sheet)!=0xFFFF)
        {t.tileTag=ELEMENT_TAG+3;t.oam=&sElementBurstOam;sElementBurst=CreateSprite(&t,0,0,0);}
        if(sElementBurst<MAX_SPRITES)gSprites[sElementBurst].invisible=TRUE;
        else gArenaElementTelemetry[9]++;
    }
}
static void ElementBurst(s16 x,s16 y)
{
    sElementBurstAge=1;sElementFrames[3]=255;
    if(sElementBurst<MAX_SPRITES){gSprites[sElementBurst].x=x;gSprites[sElementBurst].y=y-8;}
}
static void ElementDeposit(s16 x,s16 y)
{
    u32 i,best=0,count=0;
    struct ArenaSurface *p;
    if(x<30||x>210||y<45||y>132||!ArenaNav_LineClear(x,y,x,y,4))return;
    for(i=0;i<ELEMENT_COUNT;i++)
    {
        p=&sSurfaces[i];
        if(p->kind && Abs(p->x-x)<22&&Abs(p->y-y)<15)
        {if(p->kind==SURFACE_WATER)p->life=720;return;}
        if(p->kind)count++;
        if(!p->kind||p->life<sSurfaces[best].life)best=i;
    }
    p=&sSurfaces[best];
    if(p->sprite==MAX_SPRITES)
    {
        p->sprite=CreateSprite(&sElementTemplate,x,y,20);
        if(p->sprite==MAX_SPRITES){gArenaElementTelemetry[9]++;return;}
    }
    p->x=x;p->y=y;p->kind=SURFACE_WATER;p->life=720;p->age=0;
    gArenaElementTelemetry[0]++;
    gArenaElementTelemetry[8]=max(gArenaElementTelemetry[8],min(ELEMENT_COUNT,count+1));
}
static bool8 ElementReact(struct ArenaShot *shot)
{
    u32 i,j,pass,mask=0,changed=0;
    u8 type=gBattleMoves[shot->move].type;
    s16 x=shot->x/Q,y=shot->y/Q;
    if(type==TYPE_WATER)
    {
        if(!(shot->age%6))
        {
            if(shot->move==MOVE_SURF)
            {
                s16 ox=shot->vy*20/1024,oy=-shot->vx*20/1024;
                ElementDeposit(x+ox,y+oy);ElementDeposit(x-ox,y-oy);
            }
            else ElementDeposit(x,y);
        }
        return FALSE;
    }
    if(type!=TYPE_ICE&&type!=TYPE_ELECTRIC&&type!=TYPE_FIRE)return FALSE;
    for(i=0;i<ELEMENT_COUNT;i++)if(ElementContains(&sSurfaces[i],x,y))mask|=1<<i;
    if(!mask)return FALSE;
    // Only connected wet/icy patches conduct. Six bounded passes, no recursion.
    for(pass=0;pass<ELEMENT_COUNT;pass++)for(i=0;i<ELEMENT_COUNT;i++)if(mask&(1<<i))
        for(j=0;j<ELEMENT_COUNT;j++)if(sSurfaces[j].kind&&!(mask&(1<<j)))
        {
            s32 dx=sSurfaces[i].x-sSurfaces[j].x,dy=sSurfaces[i].y-sSurfaces[j].y;
            if(dx*dx+dy*dy*4<52*52 && ArenaNav_LineClear(sSurfaces[i].x,sSurfaces[i].y,sSurfaces[j].x,sSurfaces[j].y,0))mask|=1<<j;
        }
    mask&=~shot->elementMask;
    if(!mask)return FALSE;
    shot->elementMask|=mask;
    for(i=0;i<ELEMENT_COUNT;i++)if(mask&(1<<i))
    {
        struct ArenaSurface *p=&sSurfaces[i];
        if(type==TYPE_ICE && p->kind!=SURFACE_ICE)
        {p->kind=SURFACE_ICE;p->life=600;p->age=0;changed++;gArenaElementTelemetry[1]++;ElementBurst(p->x,p->y);}
        else if(type==TYPE_ELECTRIC && p->kind!=SURFACE_CHARGED)
        {if(p->kind==SURFACE_ICE)ElementBurst(p->x,p->y);p->kind=SURFACE_CHARGED;p->life=150;p->age=0;}
        else if(type==TYPE_FIRE)
        {
            bool8 wasIce=p->kind==SURFACE_ICE;
            p->kind=wasIce?SURFACE_WATER:SURFACE_NONE;p->life=600;
            gArenaElementTelemetry[6]++;
            if(wasIce)ElementBurst(p->x,p->y);
            else ArenaFeedback_Wall(p->x,p->y);
        }
    }
    if(type==TYPE_ICE){if(changed)PlaySE(SE_M_ICY_WIND);return FALSE;}
    if(type==TYPE_ELECTRIC)
    {
        u8 target=shot->side^1;
        gArenaElementTelemetry[2]++;PlaySE(SE_M_THUNDERBOLT);
        for(i=0;i<ELEMENT_COUNT;i++)if((mask&(1<<i))&&ElementContains(&sSurfaces[i],sArena.bodies[target].x/Q,sArena.bodies[target].y/Q))
        {
            if(ElementGrounded(target)&&!sArena.bodies[target].dash)
            {ApplyMoveHit(shot->side,shot->move);gArenaElementTelemetry[3]++;return TRUE;}
            gArenaElementTelemetry[10]++;break;
        }
        // Discharge is the projectile's one committed hit opportunity.
        return TRUE;
    }
    return FALSE;
}
static void ElementsTick(void)
{
    u32 i;
    gArenaElementTelemetry[11]++;
    if(sElementBurstAge&&++sElementBurstAge>32)sElementBurstAge=0;
    for(i=0;i<2;i++)
    {
        s16 index=ElementIce(i);
        if(SpecialBusy(i)){sSlide[i][0]=sSlide[i][1]=sElementInput[i][0]=sElementInput[i][1]=0;continue;}
        if(sElementCrash[i])sElementCrash[i]--;
        if(index>=0)
        {
            struct ArenaBody *b=&sArena.bodies[i];
            s16 obstacle;
            sSlide[i][0]=Clamp(sSlide[i][0]*244/256+sElementInput[i][0]/8,-900,900);
            sSlide[i][1]=Clamp(sSlide[i][1]*244/256+sElementInput[i][1]/8,-900,900);
            obstacle=ArenaNav_FirstObstacle(b->x/Q,b->y/Q,(b->x+sSlide[i][0])/Q,(b->y+sSlide[i][1])/Q,ARENA_BODY_RADIUS);
            sElementApplying=TRUE;MoveDelta(i,sSlide[i][0],sSlide[i][1]);sElementApplying=FALSE;
            if(Abs(sSlide[i][0])+Abs(sSlide[i][1])>40)gArenaElementTelemetry[4]++;
            if(obstacle>=0&&!sElementCrash[i]&&Abs(sSlide[i][0])+Abs(sSlide[i][1])>380)
            {
                ArenaPhysics_Hit(obstacle,sSlide[i][0],sSlide[i][1],2);
                ElementBurst(b->x/Q,b->y/Q);ArenaFeedback_Wall(b->x/Q,b->y/Q);
                sSlide[i][0]=-sSlide[i][0]/2;sSlide[i][1]=-sSlide[i][1]/2;
                sElementCrash[i]=24;gArenaElementTelemetry[5]++;PlaySE(SE_M_ROCK_THROW);
            }
        }
        else sSlide[i][0]=sSlide[i][1]=0;
        sElementInput[i][0]=sElementInput[i][1]=0;
    }
    for(i=0;i<ELEMENT_COUNT;i++)
    {
        struct ArenaSurface *p=&sSurfaces[i];
        if(!p->kind)continue;
        p->age++;
        if(p->life&&!--p->life)
        {
            if(p->kind!=SURFACE_WATER){p->kind=SURFACE_WATER;p->life=240;p->age=0;}
            else {p->kind=SURFACE_NONE;gArenaElementTelemetry[7]++;}
        }
    }
}
static void ElementsDraw(void)
{
    u32 i,used=0;
    for(i=0;i<ELEMENT_COUNT;i++)
    {
        struct ArenaSurface *p=&sSurfaces[i];
        if(p->sprite>=MAX_SPRITES)continue;
        gSprites[p->sprite].invisible=!p->kind;
        if(!p->kind)continue;
        used|=1<<(p->kind-1);
        gSprites[p->sprite].x=p->x;gSprites[p->sprite].y=p->y-10;
        gSprites[p->sprite].hFlip=i&1;
        gSprites[p->sprite].oam.tileNum=GetSpriteTileStartByTag(ELEMENT_TAG+p->kind-1);
    }
    for(i=0;i<3;i++)if(used&(1<<i))
    {
        u8 frame=(gArenaElementTelemetry[11]/5)%8;
        if(sElementFrames[i]!=frame)
        {ArenaRender_Copy(sElementTiles+(i*8+frame)*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(ELEMENT_TAG+i)*32,2048);sElementFrames[i]=frame;}
    }
    if(sElementBurst<MAX_SPRITES)
    {
        u8 frame=(sElementBurstAge-1)/4;
        gSprites[sElementBurst].invisible=!sElementBurstAge;
        if(sElementBurstAge&&sElementFrames[3]!=frame)
        {ArenaRender_Copy(sElementBreak+frame*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(ELEMENT_TAG+3)*32,2048);sElementFrames[3]=frame;}
    }
}
