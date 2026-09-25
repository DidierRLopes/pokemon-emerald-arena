// Spatial cover, not a third native battler. No extra XP/party slots.
#define COVER_TAG 0xA840
struct ArenaCover {s32 x,y; s16 vx,vy; u16 hp,age; u8 sprite,frame,burst;};
struct ArenaSmoke {s16 x,y;u16 life,age;u8 sprites[3];};
static EWRAM_DATA struct ArenaCover sCover[2]={};
static EWRAM_DATA struct ArenaSmoke sSmoke[2]={};
static EWRAM_DATA u8 sSmokeFrame=255;
// casts, hits, breaks, HP paid, smoke casts, sight blocks, reserved, reserved, allocation failures
EWRAM_DATA u32 gArenaCoverTelemetry[9]={};
static const u32 sCoverFrames[]=INCBIN_U32("graphics/arena/cover-smoke/effect.4bpp");
static const u32 sCoverDoll[]=INCBIN_U32("graphics/arena/cover-smoke/doll-effect.4bpp");
static const u32 sCoverCoast[]=INCBIN_U32("graphics/arena/cover-smoke/doll-coast.4bpp");
static const u32 sCoverCave[]=INCBIN_U32("graphics/arena/cover-smoke/doll-cave.4bpp");
static const u32 sCoverDesert[]=INCBIN_U32("graphics/arena/cover-smoke/doll-desert.4bpp");
static const u32 sCoverGym[]=INCBIN_U32("graphics/arena/cover-smoke/doll-gym.4bpp");
static const u32 *const sDollArt[]={sCoverDoll,sCoverCoast,sCoverCave,sCoverDesert,sCoverGym};
static const u16 sCoverPalette[]=INCBIN_U16("graphics/arena/cover-smoke/palette.gbapal");
static const struct OamData sCoverOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
static const struct OamData sDollOam={.shape=SPRITE_SHAPE(32x32),.size=SPRITE_SIZE(32x32),.priority=0};

static void CoverReset(void)
{
    u32 i,j;
    memset(sCover,0,sizeof(sCover));memset(sSmoke,0,sizeof(sSmoke));
    for(i=0;i<2;i++){sCover[i].sprite=MAX_SPRITES;sCover[i].frame=255;for(j=0;j<3;j++)sSmoke[i].sprites[j]=MAX_SPRITES;}
    sSmokeFrame=255;
    memset(gArenaCoverTelemetry,0,sizeof(gArenaCoverTelemetry));
}
static u8 CoverSprite(u16 tag,const u32 *tiles,s16 x,s16 y,const struct OamData *oam,u16 size)
{
    struct SpriteSheet sheet={tiles,size,tag};
    struct SpriteTemplate t=sMonTemplate;
    u8 sprite;
    t.tileTag=tag;t.paletteTag=SHOT_TAG+1;t.oam=oam;
    if(GetSpriteTileStartByTag(tag)==0xFFFF)LoadSpriteSheet(&sheet);
    if(GetSpriteTileStartByTag(tag)==0xFFFF){gArenaCoverTelemetry[8]++;return MAX_SPRITES;}
    if(!ArenaTerrain_ReserveProjectile())return MAX_SPRITES;
    sprite=CreateSprite(&t,x,y,0);
    if(sprite<MAX_SPRITES){gSprites[sprite].animPaused=TRUE;gSprites[sprite].invisible=TRUE;}
    else gArenaCoverTelemetry[8]++;
    // Keep enemy cue (1/2) and capture ball (3..9) intact. No extra OBJ bank.
    if(tag==COVER_TAG+2)LoadPalette(sCoverPalette,OBJ_PLTT_ID(IndexOfSpritePaletteTag(SHOT_TAG+1))+10,12);
    if(sprite<MAX_SPRITES&&tag<COVER_TAG+2)gSprites[sprite].oam.paletteNum=ArenaMoveFx_Palette(ARENA_PROP_BUSH);
    return sprite;
}
static bool8 CoverStart(u8 side)
{
    struct ArenaCover *c=&sCover[side];struct ArenaBody*b=&sArena.bodies[side];
    u16 cost=max(1,gBattleMons[side].maxHP/4),hp;
    s16 distance,x=0,y=0;
    if(c->hp||gBattleMons[side].hp<=cost)return FALSE;
    for(distance=32;distance>=24;distance-=2)
    {
        x=b->x/Q+b->attackX*distance/Q;y=b->y/Q+b->attackY*distance/Q;
        if(ArenaNav_CanStand(x,y)&&ArenaNav_LineClear(b->x/Q,b->y/Q,x,y,9)
           &&ArenaNav_Distance(x,y,sArena.bodies[side^1].x/Q,sArena.bodies[side^1].y/Q)>24)break;
    }
    if(distance<24)return FALSE;
    if(c->sprite==MAX_SPRITES)c->sprite=CoverSprite(COVER_TAG+side,sDollArt[gArenaBiome],x,y-12,&sDollOam,512);
    if(c->sprite==MAX_SPRITES)return FALSE;
    hp=gBattleMons[side].hp-cost;gBattleMons[side].hp=hp;
    SetMonData(side?&gEnemyParty[gBattlerPartyIndexes[side]]:&gPlayerParty[gBattlerPartyIndexes[side]],MON_DATA_HP,&hp);
    c->hp=cost;c->x=x*Q;c->y=y*Q;c->vx=c->vy=0;c->age=c->burst=0;c->frame=255;
    sArena.hudDirty=TRUE;gArenaCoverTelemetry[0]++;gArenaCoverTelemetry[3]+=cost;
    ArenaFeedback_Dust(x,y,TRUE);PlaySE(SE_M_MINIMIZE);return TRUE;
}
static void SmokeStart(u8 side)
{
    struct ArenaSmoke*s=&sSmoke[side];struct ArenaBody*b=&sArena.bodies[side];u32 j;
    for(j=0;j<3;j++)if(s->sprites[j]==MAX_SPRITES)
        s->sprites[j]=CoverSprite(COVER_TAG+2,sCoverFrames+16*512,0,0,&sCoverOam,2048);
    if(s->sprites[0]==MAX_SPRITES)return;
    s->x=Clamp(b->x/Q+b->attackX*22/Q,30,210);s->y=Clamp(b->y/Q+b->attackY*22/Q,40,130);
    s->life=270;s->age=0;sSmokeFrame=255;gArenaCoverTelemetry[4]++;
    PlaySE(SE_M_POISON_POWDER);
}
static bool8 SmokeBlocks(s16 x,s16 y,s16 tx,s16 ty)
{
    u32 i;
    for(i=0;i<2;i++)if(sSmoke[i].life>24&&sSmoke[i].age>12
        &&ArenaMoves_SegmentHit(x,y,tx,ty,sSmoke[i].x,sSmoke[i].y,36))return TRUE;
    return FALSE;
}
static bool8 CoverCanStep(s16 x,s16 y)
{
    u32 i;for(i=0;i<2;i++)if(sCover[i].hp&&ArenaNav_Distance(x,y,sCover[i].x/Q,sCover[i].y/Q)<22)return FALSE;
    return TRUE;
}
static void CoverHit(u8 owner,u8 attacker,u16 move,s16 dx,s16 dy)
{
    struct ArenaCover*c=&sCover[owner];s32 damage,len;
    if(!c->hp)return;
    damage=RealtimeArena_ResolveDamage(attacker,attacker^1,move);
    if(damage<=0){ArenaFeedback_Wall(c->x/Q,c->y/Q);return;}
    c->hp-=min(c->hp,damage);c->burst=c->hp?12:24;
    len=max(1,max(Abs(dx),Abs(dy)));c->vx=dx*700/len;c->vy=dy*700/len;
    gArenaCoverTelemetry[1]++;if(!c->hp)gArenaCoverTelemetry[2]++;
    ArenaFeedback_Dust(c->x/Q,c->y/Q,TRUE);PlaySE(SE_M_COMET_PUNCH);
    if(!c->hp)ArenaFeedback_CaptureGlow(c->x/Q,c->y/Q,FALSE);
}
static bool8 CoverSegment(u8 attacker,u16 move,s16 x,s16 y,s16 tx,s16 ty,u8 radius)
{
    struct ArenaCover*c=&sCover[attacker^1];struct ArenaBody*t=&sArena.bodies[attacker^1];
    if(!c->hp||!ArenaMoves_SegmentHit(x,y,tx,ty,c->x/Q,c->y/Q,radius+7))return FALSE;
    if(!t->dash&&ArenaMoves_SegmentHit(x,y,tx,ty,t->x/Q,t->y/Q,radius)
       &&ArenaNav_Distance(x,y,t->x/Q,t->y/Q)<ArenaNav_Distance(x,y,c->x/Q,c->y/Q))return FALSE;
    CoverHit(attacker^1,attacker,move,tx-x,ty-y);return TRUE;
}
static bool8 CoverCone(u8 side,const struct ArenaMoveProfile*p,u8 range,bool8 bodyHit)
{
    struct ArenaCover*c=&sCover[side^1];struct ArenaBody*b=&sArena.bodies[side],*t=&sArena.bodies[side^1];
    s16 x=b->x/Q,y=b->y/Q;
    if(!c->hp||!ArenaMoves_InCone(c->x/Q-x,c->y/Q-y,b->attackX,b->attackY,range,p->cone)
       ||!ArenaNav_LineClear(x,y,c->x/Q,c->y/Q,2))return FALSE;
    if(bodyHit&&!t->dash&&ArenaNav_Distance(x,y,t->x/Q,t->y/Q)<ArenaNav_Distance(x,y,c->x/Q,c->y/Q))return FALSE;
    CoverHit(side^1,side,p->move,b->attackX,b->attackY);return TRUE;
}
static void CoverTick(void)
{
    u32 i;for(i=0;i<2;i++)
    {
        struct ArenaCover*c=&sCover[i];
        if(c->burst)c->burst--;
        if(c->hp)
        {
            s32 nx=c->x+c->vx,ny=c->y+c->vy;c->age++;
            if(ArenaNav_CanStand(nx/Q,ny/Q)
               &&ArenaNav_Distance(nx/Q,ny/Q,sArena.bodies[0].x/Q,sArena.bodies[0].y/Q)>=24
               &&ArenaNav_Distance(nx/Q,ny/Q,sArena.bodies[1].x/Q,sArena.bodies[1].y/Q)>=24)
            {c->x=nx;c->y=ny;}
            else{c->vx=-c->vx/3;c->vy=-c->vy/3;}
            c->vx=c->vx*210/256;c->vy=c->vy*210/256;
        }
        if(sSmoke[i].life){sSmoke[i].life--;sSmoke[i].age++;}
    }

}
static void CoverDraw(void)
{
    u32 i,j;u8 smokeFrame=20+(sArena.frame/6)%8;
    for(i=0;i<2;i++)
    {
        struct ArenaCover*c=&sCover[i];struct ArenaSmoke*m=&sSmoke[i];
        if(c->sprite<MAX_SPRITES)
        {
            struct Sprite*s=&gSprites[c->sprite];u8 f;
            s->invisible=sArena.resultTimer||(!c->hp&&!c->burst)||TossActive();s->x=c->x/Q;s->y=c->y/Q-12;
            f=!c->hp?12+min(3,(24-c->burst)/6):c->burst?8+(12-c->burst)/3:c->age<24?c->age/6:4+(c->age/12)%4;
            if(!s->invisible&&f!=c->frame){ArenaRender_Copy(sDollArt[gArenaBiome]+f*128,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(COVER_TAG+i)*32,512);c->frame=f;}
        }
        if(m->life&&m->age<24)smokeFrame=16+m->age/6;
        else if(m->life&&m->life<24)smokeFrame=28+(24-m->life)/6;
        for(j=0;j<3;j++)if(m->sprites[j]<MAX_SPRITES)
        {
            struct Sprite*s=&gSprites[m->sprites[j]];
            s->invisible=!m->life||sArena.resultTimer||TossActive();
            s->x=m->x+(j==1?-14:j==2?14:0)+Sin((m->age*2+j*70)&255,3);
            s->y=m->y+(j?4:-10)+Cos((m->age+j*60)&255,3);
        }
    }
    if((sSmoke[0].life||sSmoke[1].life)&&sSmokeFrame!=smokeFrame&&GetSpriteTileStartByTag(COVER_TAG+2)!=0xFFFF)
    {ArenaRender_Copy(sCoverFrames+smokeFrame*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(COVER_TAG+2)*32,2048);sSmokeFrame=smokeFrame;}
    if(SmokeBlocks(sArena.bodies[0].x/Q,sArena.bodies[0].y/Q,sArena.bodies[1].x/Q,sArena.bodies[1].y/Q))
    {gSprites[sArena.aimSprite].invisible=TRUE;gSprites[sArena.bodies[1].sprite].invisible=TRUE;gSprites[sArena.bodies[1].shadow].invisible=TRUE;}
}
static void CoverHide(void)
{
    u32 i,j;for(i=0;i<2;i++)
    {if(sCover[i].sprite<MAX_SPRITES)gSprites[sCover[i].sprite].invisible=TRUE;
     for(j=0;j<3;j++)if(sSmoke[i].sprites[j]<MAX_SPRITES)gSprites[sSmoke[i].sprites[j]].invisible=TRUE;}
}
