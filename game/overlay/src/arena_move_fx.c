#include "global.h"
#include "arena_move_fx.h"
#include "arena_render.h"
#include "arena_feedback.h"
#include "arena_terrain.h"
#include "arena_psychic.h"
#include "battle.h"
#include "palette.h"
#include "constants/moves.h"
#include "sprite.h"
#include "constants/rgb.h"
#define TAG 0xA760
static const u32 sActions[] = INCBIN_U32(".arena-dev/art/actions.4bpp");
static const u32 sPsychicAura[] = INCBIN_U32("graphics/arena/psychic/aura.4bpp");
extern const u16 gPsychicPaletteTag;
static const u32 sBolts[] = INCBIN_U32(".arena-dev/art/bolts.4bpp");
static const u16 sPalettes[]=INCBIN_U16(".arena-dev/art/prop-palettes.gbapal");
static const u16 sCoastPalettes[]=INCBIN_U16(".arena-dev/art/coast-prop-palettes.gbapal");
static const u16 sCavePalettes[]=INCBIN_U16(".arena-dev/art/cave-prop-palettes.gbapal");
static const u16 sDesertPalettes[]=INCBIN_U16(".arena-dev/art/desert-prop-palettes.gbapal");
static const u16 sGymPalettes[]=INCBIN_U16(".arena-dev/art/gym-prop-palettes.gbapal");
static const u16 *const sBiomePalettes[]={sPalettes,sCoastPalettes,sCavePalettes,sDesertPalettes,sGymPalettes};
static const s16 sDirections[8][2] = {{0,256},{181,181},{256,0},{181,-181},
    {0,-256},{-181,-181},{-256,0},{-181,181}};
static EWRAM_DATA u8 sActorFx[2] = {};
static EWRAM_DATA u16 sDrawn[2] = {};
static EWRAM_DATA u16 sBoltFrames[ARENA_BOLT_SLOTS] = {};
static const u32 sFlameTiles[]=INCBIN_U32("graphics/arena/flame/plume.4bpp");
static const u16 sFlamePalette[]=INCBIN_U16("graphics/arena/flame/palette.gbapal");
static EWRAM_DATA u8 sFlameSprites[2]={};
EWRAM_DATA u32 gArenaFlameFxFailures=0;
static const struct OamData sFlameOam={.affineMode=ST_OAM_AFFINE_DOUBLE,
    .shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=0};
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
    // Only six live projectile frames in VRAM, independent of move catalogue
    // size. Water trails share their head's slot and never own extra hitboxes.
    struct SpriteSheet bolts={sBolts,ARENA_BOLT_SLOTS*128,TAG+2};
    LoadSpriteSheet(&bolts);
    for(i=0;i<ARENA_BOLT_SLOTS;i++)sBoltFrames[i]=0xFFFF;
    for(i=0;i<ARENA_MOVE_PALETTES;i++)
    {
        struct SpritePalette pal={sBiomePalettes[gArenaBiome]+i*16,TAG+i};LoadSpritePalette(&pal);
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
    // Reuse the two existing action tile banks and spare fire-palette entries.
    // One bounded double-size affine object per side. No extra VRAM or heap.
    gArenaFlameFxFailures=0;
    LoadPalette(sFlamePalette,OBJ_PLTT_ID(ArenaFeedback_FirePalette())+5,14);
    for(i=0;i<2;i++)
    {
        u32 j;
        bool8 needed=FALSE;
        for(j=0;j<MAX_MON_MOVES;j++)if(gBattleMons[i].moves[j]==MOVE_FLAMETHROWER)needed=TRUE;
        sFlameSprites[i]=MAX_SPRITES;
        if(needed)
        {
            struct SpriteTemplate t=sActionTemplate;
            t.tileTag=TAG+i;t.paletteTag=0xA740;t.oam=&sFlameOam;
            sFlameSprites[i]=CreateSprite(&t,0,0,1);
            if(sFlameSprites[i]<MAX_SPRITES)gSprites[sFlameSprites[i]].invisible=TRUE;
            else gArenaFlameFxFailures++;
        }
    }
}

void ArenaMoveFx_Flame(u8 side,s16 x,s16 y,u8 dir,u8 age,u8 reach,bool8 active)
{
    u8 frame=(age/4)&7;
    if(active&&reach&&sDrawn[side]!=(0xC000|frame))
    {
        ArenaRender_Copy(sFlameTiles+frame*512,(u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+side)*32,2048);
        sDrawn[side]=0xC000|frame;
    }
    if(sFlameSprites[side]<MAX_SPRITES)
    {
        struct Sprite *s=&gSprites[sFlameSprites[side]];
        s32 inverseX=56*256/max(8,reach),inverseY=180*120/max(30,reach);
        s16 dx=sDirections[dir][0],dy=sDirections[dir][1],d=reach/2+5;
        s->invisible=!active||!reach;
        if(s->invisible)return;
        s->x=x+dx*d/256;s->y=y+dy*d/256;
        SetOamMatrix(s->oam.matrixNum,dx*inverseX/256,dy*inverseX/256,-dy*inverseY/256,dx*inverseY/256);
    }
}
u8 ArenaMoveFx_Palette(u8 material){return IndexOfSpritePaletteTag(TAG+material);}
void ArenaMoveFx_Psychic(u8 side,s16 x,s16 y,u8 frame,bool8 visible)
{
    struct Sprite *s;
    if(sActorFx[side]==MAX_SPRITES)return;
    s=&gSprites[sActorFx[side]];s->invisible=!visible;
    s->x=x;s->y=y;s->oam.paletteNum=IndexOfSpritePaletteTag(gPsychicPaletteTag);
    if(sDrawn[side]!=(0x8000|frame))
    {
        ArenaRender_Copy((const u8*)sPsychicAura+frame*2048,
            (u8*)OBJ_VRAM0+GetSpriteTileStartByTag(TAG+side)*32,2048);
        sDrawn[side]=0x8000|frame;
    }
}
void ArenaMoveFx_Action(u8 side,const struct ArenaMoveProfile *p,
                       s16 x,s16 y,u8 dir,u8 age,bool8 active,bool8 paused)
{
    struct Sprite *sprite;
    u16 frame;
    if(sActorFx[side]==MAX_SPRITES)return;
    sprite=&gSprites[sActorFx[side]];
    sprite->invisible=!active||paused||!p||p->kind==ARENA_MOVE_PROJECTILE||p->move==MOVE_FLAMETHROWER;
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
    sprite->oam.paletteNum=(p->move==MOVE_FIRE_PUNCH||p->move==MOVE_BLAZE_KICK)
        ?ArenaFeedback_FirePalette():IndexOfSpritePaletteTag(TAG+p->palette);
}
u8 ArenaMoveFx_CreateBolt(const struct ArenaMoveProfile *p,s16 x,s16 y,u8 dir,u8 slot)
{
    u8 sprite;
    if(slot>=ARENA_BOLT_SLOTS)return MAX_SPRITES;
    sprite=CreateSprite(&sBoltTemplate,x,y,0);
    if(sprite!=MAX_SPRITES)
    {
        gSprites[sprite].data[7]=slot;
        sBoltFrames[slot]=0xFFFF;
        ArenaMoveFx_Bolt(sprite,p,x,y,dir,0);
    }
    return sprite;
}
void ArenaMoveFx_Bolt(u8 sprite,const struct ArenaMoveProfile *p,s16 x,s16 y,u8 dir,u8 age)
{
    u16 frame=((p->visual-ARENA_VIS_ABSORB)*8+dir)*4+((age/3)&3);
    u8 slot=gSprites[sprite].data[7];
    gSprites[sprite].x=x;gSprites[sprite].y=y;
    gSprites[sprite].oam.tileNum=GetSpriteTileStartByTag(TAG+2)+slot*4;
    if(sBoltFrames[slot]!=frame)
    {
        ArenaRender_Copy((const u8*)sBolts+frame*128,
            (u8*)OBJ_VRAM0+gSprites[sprite].oam.tileNum*32,128);
        sBoltFrames[slot]=frame;
    }
    gSprites[sprite].oam.paletteNum=p->visual==ARENA_VIS_EMBER
        ?ArenaFeedback_FirePalette():IndexOfSpritePaletteTag(TAG+p->palette);
}
