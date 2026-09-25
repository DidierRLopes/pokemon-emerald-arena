#include "global.h"
#include "arena_terrain.h"
#include "arena_physics.h"
#include "arena_move_fx.h"
#include "arena_render.h"
#include "arena_psychic.h"
#include "sprite.h"
#include "sound.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#define TAG 0xA790
static const u32 sProps[]=INCBIN_U32(".arena-dev/art/props.4bpp");
static const u32 sPieces[]=INCBIN_U32(".arena-dev/art/pieces.4bpp");
static const u32 sCoastPieces[]=INCBIN_U32(".arena-dev/art/coast-pieces.4bpp");
static const u32 sCavePieces[]=INCBIN_U32(".arena-dev/art/cave-pieces.4bpp");
static const u32 sDesertPieces[]=INCBIN_U32(".arena-dev/art/desert-pieces.4bpp");
static const u32 sGymPieces[]=INCBIN_U32(".arena-dev/art/gym-pieces.4bpp");
static const u32 *const sBiomePieces[]={sPieces,sCoastPieces,sCavePieces,sDesertPieces,sGymPieces};
static const u32 sCoastProps[]=INCBIN_U32(".arena-dev/art/coast-props.4bpp");
static const u32 sCaveProps[]=INCBIN_U32(".arena-dev/art/cave-props.4bpp");
static const u32 sDesertProps[]=INCBIN_U32(".arena-dev/art/desert-props.4bpp");
static const u32 sGymProps[]=INCBIN_U32(".arena-dev/art/gym-props.4bpp");
static const u32 *const sBiomeProps[]={sProps,sCoastProps,sCaveProps,sDesertProps,sGymProps};
static const u32 sBlast[]=INCBIN_U32(".arena-dev/art/blast.4bpp");
static const u32 sCrater[]=INCBIN_U32(".arena-dev/art/crater.4bpp");
static const struct OamData sCraterOam={.shape=SPRITE_SHAPE(64x32),.size=SPRITE_SIZE(64x32),.priority=1};
static EWRAM_DATA u8 sCraterSprite=0,sCraterFrame=0;
static EWRAM_DATA u16 sCraterAge=0;
static const u16 sPalette[16]={0,RGB(6,9,7),RGB(11,14,10),RGB(15,18,15),RGB(21,24,18),
    RGB(28,30,24),RGB(13,8,5),RGB(21,13,7),RGB(13,20,7),RGB(30,23,11),
    RGB(22,27,10),RGB(5,20,27),RGB(22,31,31),RGB(17,10,25),RGB(31,10,4),RGB(31,30,19)};
static const struct OamData sPropOam={.shape=SPRITE_SHAPE(32x32),.size=SPRITE_SIZE(32x32),.priority=1};
static const struct OamData sPieceOam={.shape=SPRITE_SHAPE(8x8),.size=SPRITE_SIZE(8x8),.priority=1};
static const struct OamData sBlastOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=1};
static const struct SpriteTemplate sTemplate={.tileTag=TAG,.paletteTag=TAG,.oam=&sPropOam,
    .anims=gDummySpriteAnimTable,.images=NULL,.affineAnims=gDummySpriteAffineAnimTable,.callback=SpriteCallbackDummy};
static EWRAM_DATA u8 sPropSprite[ARENA_OBSTACLES]={};
static EWRAM_DATA u8 sPieceSprite[ARENA_FRAGMENTS]={};
static EWRAM_DATA u8 sPropFrame[ARENA_OBSTACLES]={};
static EWRAM_DATA u8 sBlastSprite=0,sBlastLife=0,sBlastFrame=0;
static EWRAM_DATA u32 sSeenBlast=0,sSeenBroken=0;
EWRAM_DATA u32 gArenaOamTelemetry[2]={}; // cosmetic evictions, unavailable projectile slots

bool8 ArenaTerrain_ReserveProjectile(void)
{
    u32 i,oldest=ARENA_FRAGMENTS;
    for(i=0;i<MAX_SPRITES;i++)if(!gSprites[i].inUse)return TRUE;
    // Physical fragments remain simulated. Recycle only a cosmetic sprite,
    // never an actor, a hitbox, a capture object or another projectile.
    for(i=0;i<ARENA_FRAGMENTS;i++)
        if(sPieceSprite[i]!=MAX_SPRITES
            && (oldest==ARENA_FRAGMENTS || gArenaFragments[i].life<gArenaFragments[oldest].life))oldest=i;
    if(oldest!=ARENA_FRAGMENTS)
    {
        DestroySprite(&gSprites[sPieceSprite[oldest]]);
        sPieceSprite[oldest]=MAX_SPRITES;gArenaOamTelemetry[0]++;
        return TRUE;
    }
    gArenaOamTelemetry[1]++;
    return FALSE;
}
void ArenaTerrain_Init(void)
{
    u32 i;
    struct SpritePalette pal={sPalette,TAG};
    struct SpriteSheet pieces={sBiomePieces[gArenaBiome],sizeof(sPieces),TAG+ARENA_OBSTACLES};
    struct SpriteSheet blast={sBlast,2048,TAG+ARENA_OBSTACLES+1};
    struct SpriteSheet crater={sCrater,1024,TAG+ARENA_OBSTACLES+2};
    struct SpriteTemplate template=sTemplate;
    LoadSpritePalette(&pal);LoadSpriteSheet(&pieces);LoadSpriteSheet(&blast);
    LoadSpriteSheet(&crater);
    for(i=0;i<ARENA_OBSTACLES;i++)
    {
        const struct ArenaRect*r=&gArenaObstacles[i];
        struct SpriteSheet sheet={(const u8*)sBiomeProps[gArenaBiome]+i*3*512,512,TAG+i};
        LoadSpriteSheet(&sheet);template.tileTag=TAG+i;
        sPropSprite[i]=CreateSprite(&template,(r->left+r->right)/2,(r->top+r->bottom)/2,8);
        if(sPropSprite[i]!=MAX_SPRITES)gSprites[sPropSprite[i]].oam.paletteNum=ArenaMoveFx_Palette(gArenaProps[i].kind);
        sPropFrame[i]=0;
    }
    // Do not reserve 28 invisible OAM objects for fragments which do not
    // exist yet. That could starve attacks when impact particles are active.
    for(i=0;i<ARENA_FRAGMENTS;i++)
        sPieceSprite[i]=MAX_SPRITES;
    memset(gArenaOamTelemetry,0,sizeof(gArenaOamTelemetry));
    template.tileTag=TAG+ARENA_OBSTACLES+1;template.oam=&sBlastOam;
    sBlastSprite=CreateSprite(&template,0,0,7);
    if(sBlastSprite!=MAX_SPRITES)gSprites[sBlastSprite].invisible=TRUE;
    sBlastLife=0;sBlastFrame=255;sSeenBlast=sSeenBroken=0;
    template.tileTag=TAG+ARENA_OBSTACLES+2;template.oam=&sCraterOam;
    sCraterSprite=CreateSprite(&template,0,0,11);
    if(sCraterSprite!=MAX_SPRITES)gSprites[sCraterSprite].invisible=TRUE;
    sCraterAge=0;sCraterFrame=255;
}
// DidierRLopes, PR #1: last impact remains visible throughout the battle.
void ArenaTerrain_Crater(s16 x,s16 y)
{
    if(sCraterSprite==MAX_SPRITES)return;
    gSprites[sCraterSprite].x=x;gSprites[sCraterSprite].y=y;
    gSprites[sCraterSprite].invisible=FALSE;sCraterAge=1;
}
void ArenaTerrain_Draw(bool8 paused,bool8 frozen)
{
    u32 i;
    if(sCraterSprite!=MAX_SPRITES&&sCraterAge)
    {
        u8 frame=sCraterAge<30?0:1;
        if(frame!=sCraterFrame)
        {
            ArenaRender_Copy((const u8*)sCrater+frame*1024,
                (u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+ARENA_OBSTACLES+2)*32,1024);
            sCraterFrame=frame;
        }
        if(sCraterAge<60&&!paused&&!frozen)sCraterAge++;
    }
    for(i=0;i<ARENA_OBSTACLES;i++)
    {
        const struct ArenaProp*p=&gArenaProps[i];
        u8 frame=p->broken?2:p->hp<p->maxHp?1:0;
        if(sPropFrame[i]!=frame)
        {
            ArenaRender_Copy((const u8*)sBiomeProps[gArenaBiome]+(i*3+frame)*512,
                (u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+i)*32,512);
            sPropFrame[i]=frame;
        }
        if(sPropSprite[i]!=MAX_SPRITES)
        {
            struct Sprite*s=&gSprites[sPropSprite[i]];
            const struct ArenaPsychicRock *r=&gArenaPsychicRocks[i];
            const struct ArenaRect *rect=&gArenaObstacles[i];
            s->x=r->state?r->x/256:(rect->left+rect->right)/2;
            s->y=r->state?r->y/256-r->height:(rect->top+rect->bottom)/2;
            s->oam.priority=r->state?0:1;
            s->x2=paused||!p->flash?0:(p->flash&1)?1:-1;
            s->invisible=p->fuse&&(p->fuse&2)&&!paused;
        }
    }
    for(i=0;i<ARENA_FRAGMENTS;i++)
    {
        const struct ArenaFragment*f=&gArenaFragments[i];
        struct Sprite*s;
        if(!f->life)
        {
            if(sPieceSprite[i]!=MAX_SPRITES)DestroySprite(&gSprites[sPieceSprite[i]]);
            sPieceSprite[i]=MAX_SPRITES;
            continue;
        }
        if(sPieceSprite[i]==MAX_SPRITES)
        {
            struct SpriteTemplate template=sTemplate;
            template.tileTag=TAG+ARENA_OBSTACLES;template.oam=&sPieceOam;
            sPieceSprite[i]=CreateSprite(&template,0,0,6);
        }
        if(sPieceSprite[i]==MAX_SPRITES)continue;
        s=&gSprites[sPieceSprite[i]];
        s->x=f->x/256;s->y=(f->y-f->z)/256;
        s->invisible=paused||!f->life||s->y<20||(f->life<12&&(f->life&2));
        s->oam.tileNum=GetSpriteTileStartByTag(TAG+ARENA_OBSTACLES)+f->kind*4+((f->age/4)&3);
        s->oam.paletteNum=ArenaMoveFx_Palette(f->kind);
    }
    if(sSeenBroken!=gArenaPhysicsTelemetry.broken)
    {sSeenBroken=gArenaPhysicsTelemetry.broken;PlaySE(SE_M_ROCK_THROW);}
    if(sSeenBlast!=gArenaPhysicsTelemetry.blastSerial)
    {
        sSeenBlast=gArenaPhysicsTelemetry.blastSerial;sBlastLife=24;
        if(sBlastSprite!=MAX_SPRITES){gSprites[sBlastSprite].x=gArenaPhysicsTelemetry.blastX;gSprites[sBlastSprite].y=gArenaPhysicsTelemetry.blastY;}
        PlaySE(SE_M_SELF_DESTRUCT);
    }
    if(sBlastSprite!=MAX_SPRITES)
    {
        u8 frame=(24-sBlastLife)/3;
        gSprites[sBlastSprite].invisible=paused||!sBlastLife;
        if(sBlastLife&&frame!=sBlastFrame)
        {
            ArenaRender_Copy((const u8*)sBlast+frame*2048,
                (u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+ARENA_OBSTACLES+1)*32,2048);
            sBlastFrame=frame;
        }
        if(sBlastLife&&!paused&&!frozen)sBlastLife--;
    }
}
