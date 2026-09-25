#include "global.h"
#include "arena_feedback.h"
#include "arena_render.h"
#include "palette.h"
#include "sprite.h"
#include "constants/rgb.h"

#define FX_TAG 0xA740
#define FX_COUNT 8
#define Q 256

struct Particle
{
    s32 x, y;
    s16 vx, vy;
    u8 sprite, life;
};
struct Number
{
    u32 tiles[32];
    s32 y;
    u8 sprite, life;
};
static EWRAM_DATA struct Particle sParticles[FX_COUNT] = {};
static EWRAM_DATA struct Number sNumbers[2] = {};
EWRAM_DATA struct ArenaFeedbackTelemetry gArenaFeedbackTelemetry = {};

static const u32 sDotTiles[8] = {0,0,0,0x00011000,0x00011000,0,0,0};
static const struct SpriteSheet sDotSheet = {sDotTiles, sizeof(sDotTiles), FX_TAG + 2};
static const u16 sPalettes[4][16] =
{
    {RGB_BLACK, RGB(31,14,10), RGB(6,5,8), RGB(31,29,9), RGB(31,8,2)},
    {RGB_BLACK, RGB(31,30,19), RGB(5,8,7)},
    {RGB_BLACK, RGB(22,29,14), RGB(7,11,6)},
    {RGB_BLACK, RGB(31,20,31), RGB(8,5,11)}
};
static const struct OamData sNumberOam =
{
    .shape = SPRITE_SHAPE(32x8), .size = SPRITE_SIZE(32x8), .priority = 0
};
static const struct OamData sDotOam =
{
    .shape = SPRITE_SHAPE(8x8), .size = SPRITE_SIZE(8x8), .priority = 1
};
static const struct SpriteTemplate sNumberTemplate =
{
    .tileTag = FX_TAG, .paletteTag = FX_TAG, .oam = &sNumberOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const struct SpriteTemplate sDotTemplate =
{
    .tileTag = FX_TAG + 2, .paletteTag = FX_TAG + 2, .oam = &sDotOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};

void ArenaFeedback_Init(void)
{
    u32 i;
    memset(sParticles, 0, sizeof(sParticles));
    memset(sNumbers, 0, sizeof(sNumbers));
    memset(&gArenaFeedbackTelemetry, 0, sizeof(gArenaFeedbackTelemetry));
    LoadSpriteSheet(&sDotSheet);
    for (i = 0; i < 4; i++)
    {
        struct SpritePalette pal = {sPalettes[i], FX_TAG + i};
        LoadSpritePalette(&pal);
    }
    for (i = 0; i < 2; i++)
    {
        struct SpriteSheet sheet = {sNumbers[i].tiles, sizeof(sNumbers[i].tiles), FX_TAG + i};
        struct SpriteTemplate template = sNumberTemplate;
        LoadSpriteSheet(&sheet);
        template.tileTag += i; template.paletteTag += i;
        sNumbers[i].sprite = CreateSprite(&template, 120, 24, 0);
        gSprites[sNumbers[i].sprite].invisible = TRUE;
    }
}

u8 ArenaFeedback_FirePalette(void){return IndexOfSpritePaletteTag(FX_TAG);}

static void Particle(s16 x, s16 y, s16 vx, s16 vy, u8 life, u8 palette)
{
    u32 i;
    for (i = 0; i < FX_COUNT; i++)
        if (!sParticles[i].life)
        {
            struct Particle *p = &sParticles[i];
            p->sprite = CreateSprite(&sDotTemplate, x, y, 3);
            if (p->sprite == MAX_SPRITES) return;
            p->x = x * Q; p->y = y * Q;
            p->vx = vx; p->vy = vy; p->life = life;
            gSprites[p->sprite].oam.paletteNum = IndexOfSpritePaletteTag(FX_TAG + palette);
            gArenaFeedbackTelemetry.particlesSpawned++;
            return;
        }
}

static void Number(u8 side, s16 x, s16 y, u16 damage, u8 kind)
{
    struct Number *number = &sNumbers[side];
    ArenaNumber_Render(number->tiles, damage, kind);
    ArenaRender_Copy((const u8 *)number->tiles, (u8 *)OBJ_VRAM0 + GetSpriteTileStartByTag(FX_TAG + side)*32, sizeof(number->tiles));
    gSprites[number->sprite].x = max(16, min(x,224));
    gSprites[number->sprite].oam.paletteNum=IndexOfSpritePaletteTag(FX_TAG+
        (kind==ARENA_FEEDBACK_HEAL?2:kind==ARENA_FEEDBACK_BURN?0:kind>=ARENA_FEEDBACK_DEFENSE?3:side));
    number->y = max(24, y-17) * Q;
    number->life = 36;
}

void ArenaFeedback_Capture(s16 x,s16 y)
{
    Particle(x-8,y-4,-150,-300,22,1);
    Particle(x,y-8,0,-380,24,1);
    Particle(x+8,y-4,150,-300,22,1);
}

void ArenaFeedback_CaptureGlow(s16 x,s16 y,bool8 inward)
{
    s16 d=inward?18:2,v=inward?-256:256;
    Particle(x-d,y,-v,0,16,3);
    Particle(x+d,y,v,0,16,3);
    Particle(x,y-d,0,-v,16,3);
    Particle(x,y+d,0,v,16,3);
}

void ArenaFeedback_Impact(u8 target, s16 x, s16 y, u16 damage, u8 kind)
{
    Number(target, x, y, damage, kind);
    if (kind != ARENA_FEEDBACK_DAMAGE || !damage) return;
    gArenaFeedbackTelemetry.impacts++;
    Particle(x-3,y-2,-210,-240,14,target);
    Particle(x+3,y-2,210,-240,14,target);
    Particle(x-2,y+2,-170,100,11,target);
    Particle(x+2,y+2,170,100,11,target);
}

void ArenaFeedback_Wall(s16 x, s16 y)
{
    gArenaFeedbackTelemetry.wallImpacts++;
    Particle(x,y,-170,-150,10,2);
    Particle(x,y,170,-150,10,2);
}

void ArenaFeedback_Embers(s16 x,s16 y,s16 dx,s16 dy)
{
    Particle(x-2,y-4,dx/2-100,dy/2-140,24,0);
    Particle(x+3,y-2,dx/2+100,dy/2-200,20,1);
}

void ArenaFeedback_Dust(s16 x, s16 y, bool8 dash)
{
    Particle(x-3,y+7,-60,-25,dash ? 14 : 9,2);
    if (dash) Particle(x+3,y+7,60,-25,12,2);
}

void ArenaFeedback_Drain(u8 side,s16 x,s16 y,s16 sourceX,s16 sourceY,u16 healing)
{
    if(healing)Number(side,x,y,healing,ARENA_FEEDBACK_HEAL);
    Particle(sourceX-2,sourceY,(x-sourceX)*Q/20,(y-sourceY)*Q/20-120,20,2);
    Particle(sourceX+2,sourceY+3,(x-sourceX)*Q/20,(y-sourceY)*Q/20-100,20,2);
}

void ArenaFeedback_Update(bool8 paused, bool8 frozen)
{
    u32 i;
    gArenaFeedbackTelemetry.liveParticles = 0;
    for (i = 0; i < FX_COUNT; i++)
    {
        struct Particle *p = &sParticles[i];
        if (!p->life) continue;
        if (!paused && !frozen)
        {
            p->x += p->vx; p->y += p->vy; p->vy += 12;
            if (--p->life == 0) { DestroySprite(&gSprites[p->sprite]); continue; }
        }
        gSprites[p->sprite].x = p->x / Q;
        gSprites[p->sprite].y = p->y / Q;
        gSprites[p->sprite].invisible = paused || (p->life < 4 && (p->life & 1));
        gArenaFeedbackTelemetry.liveParticles++;
    }
    for (i = 0; i < 2; i++)
    {
        struct Number *n = &sNumbers[i];
        if (n->life && !paused && !frozen) { n->life--; n->y = max(20*Q,n->y-90); }
        gSprites[n->sprite].y = n->y/Q;
        gSprites[n->sprite].invisible = paused || !n->life;
        gArenaFeedbackTelemetry.numberLife[i] = n->life;
    }
}

void ArenaFeedback_Destroy(void)
{
    u32 i;
    for (i = 0; i < FX_COUNT; i++)
        if (sParticles[i].life) DestroySprite(&gSprites[sParticles[i].sprite]);
    for (i = 0; i < 2; i++) DestroySprite(&gSprites[sNumbers[i].sprite]);
    for (i = 0; i < 4; i++)
    {
        FreeSpriteTilesByTag(FX_TAG+i);
        FreeSpritePaletteByTag(FX_TAG+i);
    }
}
