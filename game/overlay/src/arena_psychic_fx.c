#include "global.h"
#include "arena_psychic.h"
#include "arena_render.h"
#include "sprite.h"
#include "palette.h"
#define TAG 0xA7C0
// Player shot/shadow use 1..4. Share 5..12; capture owns the enemy palette.
#define PALETTE_TAG 0xA710
const u16 gPsychicPaletteTag=PALETTE_TAG;
static const u32 sHalo[]=INCBIN_U32("graphics/arena/psychic/halo.4bpp");
static const u32 sImpact[]=INCBIN_U32("graphics/arena/psychic/impact.4bpp");
static const u16 sPalette[]=INCBIN_U16("graphics/arena/psychic/palette.gbapal");
static const struct OamData sHaloOam={.shape=SPRITE_SHAPE(32x32),.size=SPRITE_SIZE(32x32),.priority=0};
static const struct OamData sImpactOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
static const struct SpriteTemplate sTemplate={.tileTag=TAG,.paletteTag=PALETTE_TAG,.oam=&sHaloOam,
    .anims=gDummySpriteAnimTable,.images=NULL,.affineAnims=gDummySpriteAffineAnimTable,.callback=SpriteCallbackDummy};
static EWRAM_DATA u8 sHalos[3]={},sBurst=0,sAge=0,sFrame=255,sHaloFrame=255;
void ArenaPsychicFx_Init(bool8 enabled)
{
    u32 i;
    struct SpriteSheet halo={sHalo,512,TAG},impact={sImpact,2048,TAG+1};
    struct SpriteTemplate t=sTemplate;
    sBurst=MAX_SPRITES;sAge=0;sFrame=sHaloFrame=255;
    for(i=0;i<3;i++)sHalos[i]=MAX_SPRITES;
    if(!enabled)return;
    LoadPalette(sPalette+1,256+IndexOfSpritePaletteTag(PALETTE_TAG)*16+5,16);
    LoadSpriteSheet(&halo);LoadSpriteSheet(&impact);
    for(i=0;i<3;i++) {sHalos[i]=CreateSprite(&t,0,0,1);if(sHalos[i]!=MAX_SPRITES)gSprites[sHalos[i]].invisible=TRUE;}
    t.tileTag=TAG+1;t.oam=&sImpactOam;sBurst=CreateSprite(&t,0,0,0);
    if(sBurst!=MAX_SPRITES)gSprites[sBurst].invisible=TRUE;
    sAge=0;sFrame=sHaloFrame=255;
}
void ArenaPsychicFx_Impact(s16 x,s16 y)
{
    sAge=1;sFrame=255;
    if(sBurst!=MAX_SPRITES){gSprites[sBurst].x=x;gSprites[sBurst].y=y;}
}
void ArenaPsychicFx_Draw(bool8 paused,bool8 frozen)
{
    u32 i,n=0;
    u8 frame=0;
    for(i=0;i<3;i++)if(sHalos[i]!=MAX_SPRITES)gSprites[sHalos[i]].invisible=TRUE;
    for(i=0;i<ARENA_OBSTACLES&&n<3;i++)
    {
        const struct ArenaPsychicRock *r=&gArenaPsychicRocks[i];
        if(!r->state)continue;
        frame=(r->age/4)%12;
        if(sHalos[n]!=MAX_SPRITES)
        {
            struct Sprite *s=&gSprites[sHalos[n]];
            s->x=r->x/256;s->y=r->y/256-r->height;s->invisible=paused;
        }
        n++;
    }
    if(n&&frame!=sHaloFrame)
    {ArenaRender_Copy((const u8*)sHalo+frame*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG)*32,512);sHaloFrame=frame;}
    if(sBurst!=MAX_SPRITES)
    {
        gSprites[sBurst].invisible=paused||!sAge;
        if(sAge)
        {
            frame=(sAge-1)/3;
            if(frame!=sFrame){ArenaRender_Copy((const u8*)sImpact+frame*2048,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+1)*32,2048);sFrame=frame;}
            if(!paused&&!frozen&&++sAge>36)sAge=0;
        }
    }
}
