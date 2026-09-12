#include "global.h"
#include "arena_move_fx.h"
#include "arena_render.h"
#include "sprite.h"
#include "constants/rgb.h"
#define TAG 0xA760
static const u32 sActions[] = INCBIN_U32(".arena-dev/art/actions.4bpp");
static const u32 sBolts[] = INCBIN_U32(".arena-dev/art/bolts.4bpp");
static const u16 sPalettes[]=INCBIN_U16(".arena-dev/art/prop-palettes.gbapal");
static const s16 sDirections[8][2] = {{0,256},{181,181},{256,0},{181,-181},
    {0,-256},{-181,-181},{-256,0},{-181,181}};
static EWRAM_DATA u8 sActorFx[2] = {};
static EWRAM_DATA u16 sDrawn[2] = {};
static const struct OamData sActionOam = {.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
static const struct OamData sBoltOam = {.shape=SPRITE_SHAPE(16x16),.size=SPRITE_SIZE(16x16),.priority=0};
static const struct SpriteTemplate sActionTemplate = {
    .tileTag=TAG,.paletteTag=TAG,.oam=&sActionOam,.anims=gDummySpriteAnimTable,
    .images=NULL,.affineAnims=gDummySpriteAffineAnimTable,.callback=SpriteCallbackDummy
};
static const struct SpriteTemplate sBoltTemplate = {
    .tileTag=TAG+2,.paletteTag=TAG,.oam=&sBoltOam,.anims=gDummySpriteAnimTable,
    .images=NULL,.affineAnims=gDummySpriteAffineAnimTable,.callback=SpriteCallbackDummy
};
void ArenaMoveFx_Init(void)
{
    u32 i;
    struct SpriteSheet bolts={sBolts,sizeof(sBolts),TAG+2};
    LoadSpriteSheet(&bolts);
    for(i=0;i<ARENA_MOVE_PALETTES;i++)
    {
        struct SpritePalette pal={sPalettes+i*16,TAG+i};LoadSpritePalette(&pal);
    }
    for(i=0;i<2;i++)
    {
        struct SpriteSheet sheet={sActions,2048,TAG+i};
        struct SpriteTemplate template=sActionTemplate;
        LoadSpriteSheet(&sheet);template.tileTag+=i;
        sActorFx[i]=CreateSprite(&template,120,80,0);
        if(sActorFx[i]!=MAX_SPRITES)gSprites[sActorFx[i]].invisible=TRUE;
        sDrawn[i]=0xFFFF;
    }
}
u8 ArenaMoveFx_Palette(u8 material){return IndexOfSpritePaletteTag(TAG+material);}
void ArenaMoveFx_Action(u8 side,const struct ArenaMoveProfile *p,
                       s16 x,s16 y,u8 dir,u8 age,bool8 active,bool8 paused)
{
    struct Sprite *sprite;
    u16 frame;
    if(sActorFx[side]==MAX_SPRITES)return;
    sprite=&gSprites[sActorFx[side]];
    sprite->invisible=!active||paused||!p||p->kind==ARENA_MOVE_PROJECTILE;
    if(sprite->invisible)return;
    frame=min(3,age*4/p->active);
    frame=(p->visual*8+dir)*4+frame;
    if(sDrawn[side]!=frame)
    {
        ArenaRender_Copy((const u8*)sActions+frame*2048,
            (u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+side)*32,2048);
        sDrawn[side]=frame;
    }
    if(p->kind==ARENA_MOVE_CONE){x+=sDirections[dir][0]/8;y+=sDirections[dir][1]/8;}
    sprite->x=x;sprite->y=y;
    sprite->oam.paletteNum=IndexOfSpritePaletteTag(TAG+p->palette);
}
u8 ArenaMoveFx_CreateBolt(const struct ArenaMoveProfile *p,s16 x,s16 y,u8 dir)
{
    u8 sprite=CreateSprite(&sBoltTemplate,x,y,0);
    if(sprite!=MAX_SPRITES)ArenaMoveFx_Bolt(sprite,p,x,y,dir,0);
    return sprite;
}
void ArenaMoveFx_Bolt(u8 sprite,const struct ArenaMoveProfile *p,s16 x,s16 y,u8 dir,u8 age)
{
    u16 frame=((p->visual-ARENA_VIS_ABSORB)*8+dir)*4+((age/3)&3);
    gSprites[sprite].x=x;gSprites[sprite].y=y;
    gSprites[sprite].oam.tileNum=GetSpriteTileStartByTag(TAG+2)+frame*4;
    gSprites[sprite].oam.paletteNum=IndexOfSpritePaletteTag(TAG+p->palette);
}
