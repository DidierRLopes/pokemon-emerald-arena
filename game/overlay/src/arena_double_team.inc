// A decoy is not a battler: it owns no party slot, HP transaction or reward.
// It shares the owner's animated tiles, palette and affine matrix, never frees them.
struct ArenaDecoy
{
    s16 x,y,startX,startY;
    u16 life,age;
    u8 sprites[3],shadow,burst,fx,frame;
};
static EWRAM_DATA struct ArenaDecoy sDecoys[2]={};
// casts, broken, expired, allocation failures, AI fooled, live mask.
EWRAM_DATA u32 gArenaDecoyTelemetry[6]={};
static const struct OamData sDecoyOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
#define DECOY_TAG 0xA800
static const u32 sDecoyFrames[]=INCBIN_U32("graphics/arena/double-team/effect.4bpp");
// The illusion shares Psychic's existing violet ramp. Never overwrite a
// simultaneous psychic attack, send-out, terrain, capture or fire palette.
static const u16 sDecoyPalette[]=INCBIN_U16("graphics/arena/psychic/palette.gbapal");

static s32 DecoyAim(u8 side,bool8 y)
{
    if(sDecoys[side].life&&sDecoys[side].age>=24)
        return (y?sDecoys[side].y:sDecoys[side].x)*Q;
    return y?sArena.bodies[side].y:sArena.bodies[side].x;
}

static void DecoyReset(void)
{
    u32 side,j;
    for(side=0;side<2;side++)
    {
        struct ArenaDecoy *d=&sDecoys[side];
        d->life=d->age=d->burst=0;
        for(j=0;j<3;j++)d->sprites[j]=MAX_SPRITES;
        d->shadow=d->fx=MAX_SPRITES;d->frame=255;
    }
    gArenaDecoyTelemetry[5]=0;
}

static void DecoyHide(void)
{
    u32 side,j;
    for(side=0;side<2;side++)
    {
        struct ArenaDecoy *d=&sDecoys[side];
        for(j=0;j<3;j++)if(d->sprites[j]<MAX_SPRITES)gSprites[d->sprites[j]].invisible=TRUE;
        if(d->shadow<MAX_SPRITES)gSprites[d->shadow].invisible=TRUE;
        if(d->fx<MAX_SPRITES)gSprites[d->fx].invisible=TRUE;
        d->life=d->burst=0;
    }
    gArenaDecoyTelemetry[5]=0;
}

static bool8 DecoyStart(u8 side)
{
    struct ArenaDecoy *d=&sDecoys[side];
    struct ArenaBody *body=&sArena.bodies[side];
    static const s8 offsets[12][2]={{40,-20},{-40,-20},{36,20},{-36,20},{0,-40},{0,40},
        {28,-28},{-28,-28},{28,28},{-28,28},{24,0},{-24,0}};
    u32 i,j;
    s16 x=body->x/Q,y=body->y/Q;
    for(i=0;i<ARRAY_COUNT(offsets);i++)
    {
        s16 nx=x+offsets[i][0],ny=y+offsets[i][1]*(side?-1:1);
        if(ArenaNav_CanStand(nx,ny)&&ArenaNav_LineClear(x,y,nx,ny,ARENA_BODY_RADIUS)
            &&ArenaNav_Distance(nx,ny,sArena.bodies[side^1].x/Q,sArena.bodies[side^1].y/Q)>20)
        {d->x=nx;d->y=ny;break;}
    }
    if(i==ARRAY_COUNT(offsets))return FALSE;
    for(j=0;j<3;j++)
    {
        if(d->sprites[j]==MAX_SPRITES)
        {
            struct SpriteTemplate t=sMonTemplate;
            t.tileTag=MON_TAG+side*2;t.oam=&sDecoyOam;
            d->sprites[j]=CreateSprite(&t,x,y,1);
            if(d->sprites[j]<MAX_SPRITES)
            {gSprites[d->sprites[j]].animPaused=TRUE;gSprites[d->sprites[j]].invisible=TRUE;}
        }
    }
    if(d->sprites[0]==MAX_SPRITES){gArenaDecoyTelemetry[3]++;return FALSE;}
    if(d->shadow==MAX_SPRITES)d->shadow=CreateSprite(&sShadowTemplate,x,y+8,2);
    if(d->fx==MAX_SPRITES)
    {
        struct SpriteSheet sheet={sDecoyFrames,2048,DECOY_TAG+side};
        struct SpriteTemplate t=sMonTemplate;
        t.tileTag=DECOY_TAG+side;t.paletteTag=SHOT_TAG;t.oam=&sDecoyOam;
        if(LoadSpriteSheet(&sheet)!=0xFFFF)
        {
            d->fx=CreateSprite(&t,x,y-8,0);
            if(d->fx<MAX_SPRITES)gSprites[d->fx].animPaused=TRUE;
        }
        if(d->fx==MAX_SPRITES)gArenaDecoyTelemetry[3]++;
    }
    LoadPalette(sDecoyPalette+1,OBJ_PLTT_ID(IndexOfSpritePaletteTag(SHOT_TAG))+5,16);
    d->frame=255;
    d->startX=x;d->startY=y;d->age=0;d->life=360;d->burst=0;
    gArenaDecoyTelemetry[0]++;gArenaDecoyTelemetry[5]|=1<<side;
    PlaySE(SE_M_DOUBLE_TEAM);
    if(!side){sArena.thinkTimer=1;sArena.goalTimer=0;}
    return TRUE;
}

static void DecoyBreak(u8 side)
{
    struct ArenaDecoy*d=&sDecoys[side];
    if(!d->life)return;
    d->life=0;d->burst=24;
    gArenaDecoyTelemetry[1]++;gArenaDecoyTelemetry[5]&=~(1<<side);
    ArenaFeedback_Dust(d->x,d->y,TRUE);
    ArenaFeedback_CaptureGlow(d->x,d->y,FALSE);
    PlaySE(SE_M_DOUBLE_TEAM);
    if(!side){sArena.thinkTimer=1;sArena.goalTimer=0;}
}

static void DecoyTick(void)
{
    u32 i;
    for(i=0;i<2;i++)
    {
        struct ArenaDecoy*d=&sDecoys[i];
        if(d->burst)d->burst--;
        if(!d->life)continue;
        d->age++;
        if(!--d->life)
        {d->burst=24;gArenaDecoyTelemetry[2]++;gArenaDecoyTelemetry[5]&=~(1<<i);}
    }
}

static bool8 DecoySegment(u8 target,s16 x1,s16 y1,s16 x2,s16 y2,u8 radius)
{
    struct ArenaDecoy*d=&sDecoys[target];
    struct ArenaBody*b=&sArena.bodies[target];
    if(!d->life||d->age<24)return FALSE;
    if(!ArenaMoves_SegmentHit(x1,y1,x2,y2,d->x,d->y,radius))return FALSE;
    // A decoy behind the real Pokemon must not magically intercept its hit.
    if(!b->dash&&ArenaMoves_SegmentHit(x1,y1,x2,y2,b->x/Q,b->y/Q,radius)
        &&ArenaNav_Distance(x1,y1,b->x/Q,b->y/Q)<=ArenaNav_Distance(x1,y1,d->x,d->y))return FALSE;
    DecoyBreak(target);return TRUE;
}

static void DecoyDraw(void)
{
    u32 side,j;
    for(side=0;side<2;side++)
    {
        struct ArenaDecoy*d=&sDecoys[side];
        struct Sprite*owner=&gSprites[sArena.bodies[side].sprite];
        for(j=0;j<3;j++)
        {
            struct Sprite*s;
            s16 progress;
            if(d->sprites[j]==MAX_SPRITES)continue;
            s=&gSprites[d->sprites[j]];
            s->invisible=sArena.resultTimer||(!d->life&&!d->burst)||(j&&d->age>=24&&!d->burst);
            if(s->invisible)continue;
            s->oam=owner->oam;s->x2=s->y2=0;
            progress=min(24,d->age);
            s->x=d->startX+(d->x-d->startX)*progress/24;
            s->y=d->startY+(d->y-d->startY)*progress/24+owner->y-sArena.bodies[side].y/Q;
            if(j)
            {s->x-=j*(d->x-d->startX)/4;s->y-=j*(d->y-d->startY)/4;s->invisible=(d->age+j)%3==0;}
            if(d->burst)
            {s->x+=(j?j==1?-1:1:0)*(24-d->burst)*2;s->invisible=d->burst<17||(d->burst+j)%3!=0;}
        }
        if(d->shadow<MAX_SPRITES)
        {
            struct Sprite*s=&gSprites[d->shadow];
            s->x=d->x;s->y=d->y+8;
            s->invisible=!d->life||d->age<24||sArena.resultTimer;
        }
        if(d->fx<MAX_SPRITES)
        {
            struct Sprite*s=&gSprites[d->fx];
            u8 f=d->burst?8+(24-d->burst)/3:min(7,d->age/3);
            s->invisible=sArena.resultTimer||(!d->burst&&(!d->life||d->age>=24));
            s->x=d->life?d->startX+(d->x-d->startX)*min(24,d->age)/24:d->x;
            s->y=(d->life?d->startY+(d->y-d->startY)*min(24,d->age)/24:d->y)-8;
            if(!s->invisible&&d->frame!=f)
            {
                ArenaRender_Copy(sDecoyFrames+f*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(DECOY_TAG+side)*32,2048);
                d->frame=f;
            }
        }
    }
}
