#include "global.h"
#include "realtime_arena.h"
#include "arena_lab.h"
#include "arena_navigation.h"
#include "arena_sprites.h"
#include "arena_feedback.h"
#include "arena_moves.h"
#include "arena_move_fx.h"
#include "arena_physics.h"
#include "arena_terrain.h"
#include "arena_render.h"
#include "field_weather.h"
#include "fonts.h"
#include "constants/weather.h"
#include "battle.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "field_screen_effect.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "reshow_battle_screen.h"
#include "save.h"
#include "scanline_effect.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_move_effects.h"
#include "constants/heal_locations.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/rgb.h"
#include "constants/songs.h"

#define Q 256
#define SHOTS_COUNT 6
#define SHOT_TAG 0xA710
#define MON_TAG 0xA720
#define AI_REPOSITION 0
#define AI_AIM 1
#define AI_RECOVER 2
#define AI_EVADE 3
// PMDCollab rows: down, down-right, right, up-right, up, up-left, left, down-left.
#define FACE_UP 4
#define FACE_DOWN 0
#define FACE_LEFT 6
#define FACE_RIGHT 2

struct ArenaBody
{
    s32 x, y;
    s32 aimX, aimY;
    u16 cooldown, dashCooldown;
    u8 sprite, moveSlot, dash, flash, facing, moving, shadow;
    const struct ArenaSpriteSet *art;
    u32 animClock;
    u16 shotTimer, shotElapsed;
    u8 animation, drawnFrame, drawnDirection, shotFacing, shotSlot;
    s8 dashX, dashY;
    s16 attackX, attackY;
    u8 actionLife, actionAge, connected, terrainMask,manualAim;
    s16 knockX,knockY;
};

struct ArenaShot
{
    s32 x, y, vx, vy;
    u16 move;
    u8 side, sprite, life, direction, age;
};

struct ArenaState
{
    struct ArenaBody bodies[2];
    struct ArenaShot shots[SHOTS_COUNT];
    MainCallback savedCB1;
    u16 frame;
    u8 active, paused, classic, resultTimer;
    u8 lastAttacker, lastTarget, hudDirty;
    u32 aiRandom;
    struct ArenaPoint goal, waypoint, observed, previous;
    u16 thinkTimer, aimTimer, goalTimer;
    u8 aiState, style, reaction, aimError, cueSprite, aimSprite;
    u8 hitstop, attackBuffer, dashBuffer;
    s8 dashRequestX, dashRequestY;
    u32 lastBlast;
};

static EWRAM_DATA struct ArenaState sArena = {};
static EWRAM_DATA bool8 sDemoRequested = FALSE;
EWRAM_DATA struct RealtimeArenaTelemetry gRealtimeArenaTelemetry = {};
EWRAM_DATA struct ArenaAiTelemetry gArenaAiTelemetry = {};
EWRAM_DATA struct ArenaSpriteTelemetry gArenaSpriteTelemetry = {};
EWRAM_DATA struct ArenaCombatTelemetry gArenaCombatTelemetry = {};
EWRAM_DATA struct ArenaMoveTelemetry gArenaMoveTelemetry = {};
EWRAM_DATA struct ArenaFrameTelemetry gArenaFrameTelemetry = {};
EWRAM_DATA bool8 gRealtimeArenaRestoringFaint = FALSE;
EWRAM_DATA bool8 gRealtimeArenaQuietResult = FALSE;
EWRAM_DATA struct ArenaResultTelemetry gArenaResultTelemetry = {};
EWRAM_DATA struct ArenaIntroTelemetry gArenaIntroTelemetry = {};
EWRAM_DATA u16 gArenaRenderTelemetry[8] = {};
static EWRAM_DATA u16 sHudHp[2]={},sHudMaxHp[2]={};
static EWRAM_DATA u8 sHudReady=0,sHudSlot=0,sHudPP=0;

static const struct BgTemplate sArenaBgs[] =
{
    { .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31,
      .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30,
      .screenSize = 0, .paletteMode = 1, .priority = 2, .baseTile = 0 }
};
static const u32 sForestTiles[] = INCBIN_U32(".arena-dev/art/forest.8bpp");
static const u16 sForestPalette[] = INCBIN_U16(".arena-dev/art/forest.gbapal");
static const u16 sForestMap[] = INCBIN_U16(".arena-dev/art/forest.bin");
static const struct WindowTemplate sArenaWindows[] =
{
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0,
      .width = 30, .height = 2, .paletteNum = 0, .baseBlock = 1 },
    { .bg = 0, .tilemapLeft = 1, .tilemapTop = 6,
      .width = 28, .height = 9, .paletteNum = 0, .baseBlock = 61 },
    DUMMY_WIN_TEMPLATE
};
static const u16 sArenaPalette[16] =
{
    RGB(3, 6, 9), RGB(4, 9, 12), RGB(6, 13, 15), RGB(10, 20, 19),
    RGB(29, 31, 29), RGB(8, 29, 23), RGB(31, 14, 8), RGB(29, 24, 9),
    RGB(12, 15, 17), RGB(2, 3, 6), RGB(31, 31, 31)
};
static const u32 sShotTiles[8] =
{
    0x00022000, 0x00211200, 0x02111120, 0x21111112,
    0x21111112, 0x02111120, 0x00211200, 0x00022000
};
static const u16 sPlayerShotPalette[16] = {RGB_BLACK, RGB(10,31,25), RGB_WHITE};
static const u16 sEnemyShotPalette[16] = {RGB_BLACK, RGB(31,10,5), RGB(31,25,10)};
static const struct SpriteSheet sShotSheet = {sShotTiles, sizeof(sShotTiles), SHOT_TAG};
static const struct SpritePalette sShotPalettes[] =
{
    {sPlayerShotPalette, SHOT_TAG}, {sEnemyShotPalette, SHOT_TAG + 1}
};
static const struct OamData sShotOam =
{
    .shape = SPRITE_SHAPE(8x8), .size = SPRITE_SIZE(8x8), .priority = 0
};
static const struct SpriteTemplate sShotTemplate =
{
    .tileTag = SHOT_TAG, .paletteTag = SHOT_TAG, .oam = &sShotOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const struct OamData sMonOam =
{
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .shape = SPRITE_SHAPE(64x64), .size = SPRITE_SIZE(64x64), .priority = 0
};
static const struct SpriteTemplate sMonTemplate =
{
    .tileTag = MON_TAG, .paletteTag = TAG_NONE, .oam = &sMonOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const u32 sShadowTiles[] = {0x00000000, 0x11100000, 0x22211100, 0x22222110, 0x22211100, 0x11100000, 0x00000000, 0x00000000, 0x00000000, 0x00000111, 0x00111222, 0x01122222, 0x00111222, 0x00000111, 0x00000000, 0x00000000};
static const u16 sShadowPalette[16] = {RGB_BLACK, RGB(9,14,7), RGB(7,11,6)};
static const struct SpriteSheet sShadowSheet = {sShadowTiles, sizeof(sShadowTiles), SHOT_TAG + 2};
static const struct SpritePalette sShadowPal = {sShadowPalette, SHOT_TAG + 2};
static const struct OamData sShadowOam =
{
    .shape = SPRITE_SHAPE(16x8), .size = SPRITE_SIZE(16x8), .priority = 1
};
static const struct SpriteTemplate sShadowTemplate =
{
    .tileTag = SHOT_TAG + 2, .paletteTag = SHOT_TAG + 2, .oam = &sShadowOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const struct OamData sAimOam =
{
    .shape = SPRITE_SHAPE(32x32), .size = SPRITE_SIZE(32x32), .priority = 1
};
static const struct SpriteTemplate sAimTemplate =
{
    .tileTag = SHOT_TAG + 3, .paletteTag = SHOT_TAG, .oam = &sAimOam,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const u8 sTextControls[] = _("A ATTACK  B DODGE  L/R MOVE");
static const u8 sTextMenu[] = _("SELECT: CLASSIC   START: PAUSE");
static const u8 sTextPaused[] = _("PAUSED - START TO RESUME");
static const u8 sTextNoPP[] = _("NO PP - SELECT: CLASSIC BATTLE");
static const u8 sTextPP[] = _(" PP ");
static const u8 sTextDirections[] = _("D-PAD: MOVE / RED AIM: BLOCKED");
static const u8 sTextShortPP[] = _("P");
#if ARENA_LAB
static const u8 sDemoName[] = _("GERMAN");
#else
static const u8 sDemoName[] = _("ARENA");
#endif

// Native encounter shortcut for the opt-in practice SAVE, not a ROM mailbox.
// Curated levels retain genuine compatible moves from each native learnset.
static const struct {u16 species;u8 level;} sPracticeRivals[] =
{
    {SPECIES_EEVEE,36}, {SPECIES_BLASTOISE,36}, {SPECIES_SCIZOR,15},
    {SPECIES_BLAZIKEN,20}, {SPECIES_DRAGONITE,30}, {SPECIES_CHARIZARD,36},
    {SPECIES_SCEPTILE,43}
};
static EWRAM_DATA u8 sPracticeRival = 0;

void RealtimeArena_PracticeTick(void)
{
    extern const u8 EventScript_ArenaPracticeBattle[];
    u32 i;
    if (!FlagGet(FLAG_ARENA_PRACTICE) || gPaletteFade.active
        || ArePlayerFieldControlsLocked() || ScriptContext_IsEnabled()
        || (JOY_HELD(L_BUTTON | R_BUTTON) != (L_BUTTON | R_BUTTON))
        || !JOY_NEW(L_BUTTON | R_BUTTON)) return;
    for (i = 0; i < gPlayerPartyCount; i++)
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) && !GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG)) break;
    if (i == gPlayerPartyCount) return;
    CreateScriptedWildMon(sPracticeRivals[sPracticeRival].species,
                         sPracticeRivals[sPracticeRival].level, ITEM_NONE);
    sPracticeRival = (sPracticeRival + 1) % ARRAY_COUNT(sPracticeRivals);
    ScriptContext_SetupScript(EventScript_ArenaPracticeBattle);
}

static void CB2_ArenaInit(void);
static void CB2_Arena(void);
static void ArenaExit(bool8 fainted);
static void AiChooseGoal(void);

static s32 Abs(s32 n) { return n < 0 ? -n : n; }
static s32 Clamp(s32 n, s32 lo, s32 hi) { return n < lo ? lo : n > hi ? hi : n; }

static bool8 SupportedMove(u16 move)
{
    return ArenaMoves_Get(move) != NULL;
}

static u8 FirstMove(u8 side, bool8 needsPP)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (SupportedMove(gBattleMons[side].moves[i]) && (!needsPP || gBattleMons[side].pp[i]))
            return i;
    return MAX_MON_MOVES;
}

static bool8 SupportedBattler(u8 side)
{
    // The prototype deliberately stays inside a tested damage-only contract.
    // Contact/status/turn/item mechanics retain the complete classic battle.
    if (gBattleMons[side].item || gBattleMons[side].status1 || gBattleMons[side].status2
        || gStatuses3[side] || gBattleMons[side].hp == 0 || FirstMove(side, TRUE) == MAX_MON_MOVES)
        return FALSE;
    switch (gBattleMons[side].ability)
    {
    case ABILITY_NONE:
    case ABILITY_OVERGROW:
    case ABILITY_BLAZE:
    case ABILITY_TORRENT:
    case ABILITY_SWARM:
    case ABILITY_RUN_AWAY:
    case ABILITY_PICKUP:
    case ABILITY_KEEN_EYE:
    case ABILITY_SHIELD_DUST:
    case ABILITY_COMPOUND_EYES:
    case ABILITY_HUSTLE:
    case ABILITY_HUGE_POWER:
    case ABILITY_PURE_POWER:
    case ABILITY_BATTLE_ARMOR:
    case ABILITY_SHELL_ARMOR:
    case ABILITY_LEVITATE:
    case ABILITY_WONDER_GUARD:
    case ABILITY_INNER_FOCUS:
    case ABILITY_INTIMIDATE:
        return TRUE;
    }
    return FALSE;
}

void RealtimeArena_ResetBattle(void)
{
    memset(&sArena, 0, sizeof(sArena));
    gRealtimeArenaTelemetry.active = FALSE;
    gRealtimeArenaRestoringFaint = FALSE;
    gRealtimeArenaQuietResult = FALSE;
    memset(&gArenaResultTelemetry,0,sizeof(gArenaResultTelemetry));
    gArenaIntroTelemetry.started=gMain.vblankCounter1;
    gArenaIntroTelemetry.elapsed=gArenaIntroTelemetry.skipped=0;
}

static bool8 Eligible(void)
{
#if ARENA_LAB
    if (gArenaLabMailbox.classic) return FALSE;
#endif
    // FIRST_BATTLE is still a 1v1 with a real starter and native return script.
    if (sArena.classic || gBattlersCount != 2 || (gBattleTypeFlags & ~(BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_IS_MASTER))
        || gBattleWeather || gAbsentBattlerFlags
        || !SupportedBattler(0) || !SupportedBattler(1))
        return FALSE;
    return TRUE;
}

bool8 RealtimeArena_CanSkipIntro(void)
{
    u8 weather=GetCurrentWeather();
    // Active switch-in abilities/weather may execute native animation scripts.
    // Keep their complete intro until that specific path is supported and tested.
    return Eligible()&&gBattleMons[0].ability!=ABILITY_INTIMIDATE
        &&gBattleMons[1].ability!=ABILITY_INTIMIDATE
        &&(weather==WEATHER_NONE||weather==WEATHER_SUNNY);
}

bool8 RealtimeArena_TryStart(void)
{
    if(!Eligible())return FALSE;
    // Wait inside selection rather than issuing a classic ChooseAction command
    // while the intro/fade is still completing. That menu would own the controller.
    if (gBattleControllerExecFlags || gPaletteFade.active) return TRUE;
    sArena.savedCB1 = gMain.callback1;
    gMain.callback1 = NULL;
    SetMainCallback2(CB2_ArenaInit);
    return TRUE;
}

static void VBlank_Arena(void)
{
    LoadOam();
    ArenaRender_Flush();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

#define H(n) ((((n)&192)==64?15:0)|(((n)&48)==16?240:0)|(((n)&12)==4?3840:0)|(((n)&3)==1?61440:0))
static const u16 sHudRowMask[256]={
    H(0),H(1),H(2),H(3),H(4),H(5),H(6),H(7),H(8),H(9),H(10),H(11),H(12),H(13),H(14),H(15),
    H(16),H(17),H(18),H(19),H(20),H(21),H(22),H(23),H(24),H(25),H(26),H(27),H(28),H(29),H(30),H(31),
    H(32),H(33),H(34),H(35),H(36),H(37),H(38),H(39),H(40),H(41),H(42),H(43),H(44),H(45),H(46),H(47),
    H(48),H(49),H(50),H(51),H(52),H(53),H(54),H(55),H(56),H(57),H(58),H(59),H(60),H(61),H(62),H(63),
    H(64),H(65),H(66),H(67),H(68),H(69),H(70),H(71),H(72),H(73),H(74),H(75),H(76),H(77),H(78),H(79),
    H(80),H(81),H(82),H(83),H(84),H(85),H(86),H(87),H(88),H(89),H(90),H(91),H(92),H(93),H(94),H(95),
    H(96),H(97),H(98),H(99),H(100),H(101),H(102),H(103),H(104),H(105),H(106),H(107),H(108),H(109),H(110),H(111),
    H(112),H(113),H(114),H(115),H(116),H(117),H(118),H(119),H(120),H(121),H(122),H(123),H(124),H(125),H(126),H(127),
    H(128),H(129),H(130),H(131),H(132),H(133),H(134),H(135),H(136),H(137),H(138),H(139),H(140),H(141),H(142),H(143),
    H(144),H(145),H(146),H(147),H(148),H(149),H(150),H(151),H(152),H(153),H(154),H(155),H(156),H(157),H(158),H(159),
    H(160),H(161),H(162),H(163),H(164),H(165),H(166),H(167),H(168),H(169),H(170),H(171),H(172),H(173),H(174),H(175),
    H(176),H(177),H(178),H(179),H(180),H(181),H(182),H(183),H(184),H(185),H(186),H(187),H(188),H(189),H(190),H(191),
    H(192),H(193),H(194),H(195),H(196),H(197),H(198),H(199),H(200),H(201),H(202),H(203),H(204),H(205),H(206),H(207),
    H(208),H(209),H(210),H(211),H(212),H(213),H(214),H(215),H(216),H(217),H(218),H(219),H(220),H(221),H(222),H(223),
    H(224),H(225),H(226),H(227),H(228),H(229),H(230),H(231),H(232),H(233),H(234),H(235),H(236),H(237),H(238),H(239),
    H(240),H(241),H(242),H(243),H(244),H(245),H(246),H(247),H(248),H(249),H(250),H(251),H(252),H(253),H(254),H(255)
};
#undef H

static void Print(u32 x, u32 y, const u8 *text, u8 color)
{
    // Same original Emerald glyphs, tightly bounded HUD blit. Invoking the full
    // dialogue state machine per HP/PP change stalled several hardware frames.
    // Foreground only: this strip is cleared once, shadow/background are both 9.
    u32 n=0;
    while(*text!=EOS&&n++<32)
    {
        u32 glyph=*text++,width=gFontSmallLatinGlyphWidths[glyph],row;
        const u16 *src=gFontSmallLatinGlyphs+glyph*32;
        if(x+width>240)break;
        for(row=0;row<13&&y+row<16;row++)
        {
            u32 bits=src[row<8?row:row+8],py=y+row,shift=(x&7)*4;
            u32 mask=sHudRowMask[bits>>8]|(sHudRowMask[bits&255]<<16);
            u32 ink,colorWord=0x11111111*color;
            u32 *dest=(u32*)gWindows[0].tileData+((py/8)*30+x/8)*8+(py&7);
            if(width<8)mask&=0xffffffff>>((8-width)*4);
            ink=mask&colorWord;
            *dest=(*dest&~(mask<<shift))|(ink<<shift);
            if(shift&&x/8<29)dest[8]=(dest[8]&~(mask>>(32-shift)))|(ink>>(32-shift));
        }
        x+=width;
    }
}

static void HudRect(u32 x,u32 top,u32 width,u32 height,u32 color)
{
    u32 y;
    for(y=top;y<top+height;y++)
    {
        u32 at=x,left=width;
        while(left)
        {
            u32 count=min(left,8-(at&7)),shift=(at&7)*4;
            u32 mask=(0xffffffff>>((8-count)*4))<<shift;
            u32 *dest=(u32*)gWindows[0].tileData+((y/8)*30+at/8)*8+(y&7);
            *dest=(*dest&~mask)|(0x11111111*color&mask);
            left-=count;at+=count;
        }
    }
}

static u32 HudTextWidth(const u8 *text)
{
    u32 width=0,n=0;
    while(*text!=EOS&&n++<32)width+=gFontSmallLatinGlyphWidths[*text++];
    return width;
}

static void PrintPopup(u32 x, u32 y, const u8 *text, u8 color)
{
    u8 colors[3] = {9, color, 9};
    AddTextPrinterParameterized3(1, FONT_SMALL, x, y, colors, TEXT_SKIP_DRAW, text);
}

static void DrawStage(void)
{
    // Art is on BG1. Clearing an overlay never redraws terrain or erases rocks.
    // Keep the cached HUD pixels too: pause must not erase unchanged HP/names.
    ClearWindowTilemap(1);
    CopyWindowToVram(1, COPYWIN_MAP);
}

static void DrawHud(void)
{
    u32 side;
    u8 text[64];
    u8 *end;
    // Tile-aligned clears avoid thousands of slow per-pixel writes every tick.
    if(!sHudReady)CpuFastFill(0x99999999, gWindows[0].tileData, 30 * 2 * 32);
    for (side = 0; side < 2; side++)
    {
        u32 x = side ? 126 : 6;
        const struct BattlePokemon *mon = &gBattleMons[side];
        if(!sHudReady||(!side&&sHudSlot!=sArena.bodies[0].moveSlot))
        {
            StringCopy(text,side?gSpeciesNames[mon->species]:gMoveNames[mon->moves[sArena.bodies[0].moveSlot]]);
            if(!side)
            {
                u32 length=StringLength(text);
                while(length&&HudTextWidth(text)>49)text[--length]=EOS;
            }
            HudRect(x,0,side?75:49,12,9);
            Print(x,0,text,side?6:5);
        }
        if (!side)
        {
            u8 pp = mon->pp[sArena.bodies[0].moveSlot];
            if(!sHudReady||pp!=sHudPP)
            {
                end=StringCopy(text,sTextShortPP);
                ConvertIntToDecimalStringN(end,pp,STR_CONV_MODE_LEFT_ALIGN,2);
                HudRect(60,0,20,12,9);Print(60,0,text,pp>5?7:6);
            }
            sHudPP=pp;sHudSlot=sArena.bodies[0].moveSlot;
        }
        if(!sHudReady||sHudHp[side]!=mon->hp||sHudMaxHp[side]!=mon->maxHP)
        {
            HudRect(x,12,108,3,8);HudRect(x,12,mon->hp*108/mon->maxHP,3,side?6:5);
            end=ConvertIntToDecimalStringN(text,mon->hp,STR_CONV_MODE_LEFT_ALIGN,3);*end++=CHAR_SLASH;
            ConvertIntToDecimalStringN(end,mon->maxHP,STR_CONV_MODE_LEFT_ALIGN,3);
            HudRect(x+76,0,32,12,9);Print(x+76,0,text,4);
            sHudHp[side]=mon->hp;sHudMaxHp[side]=mon->maxHP;
        }
    }
    sHudReady=TRUE;
    if (sArena.paused)
    {
        FillWindowPixelBuffer(1, PIXEL_FILL(9));
        PrintPopup(5, 3, sTextPaused, 7);
        if (sArena.paused)
        {
            PrintPopup(5, 17, sTextDirections, 4);
            PrintPopup(5, 29, sTextControls, 4);
            PrintPopup(5, 41, sTextMenu, 4);
            end = StringCopy(text, gMoveNames[gBattleMons[0].moves[sArena.bodies[0].moveSlot]]);
            end = StringCopy(end, sTextPP);
            ConvertIntToDecimalStringN(end, gBattleMons[0].pp[sArena.bodies[0].moveSlot], STR_CONV_MODE_LEFT_ALIGN, 2);
            PrintPopup(5, 55, FirstMove(0, TRUE) == MAX_MON_MOVES ? sTextNoPP : text, 7);
        }
        PutWindowTilemap(1);
        CopyWindowToVram(1, COPYWIN_FULL);
    }
    CopyWindowToVram(0, COPYWIN_GFX);
    sArena.hudDirty = FALSE;
}

static void LoadAimMarker(void)
{
    u32 tiles[128] = {0};
    struct SpriteSheet sheet = {tiles, sizeof(tiles), SHOT_TAG + 3};
    u32 x, y;
    // Code-native pixel art: four small corners around the target. No opaque
    // center, so the actor's animation stays completely visible.
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++)
            if (((y == 4 || y == 27) && ((x >= 4 && x <= 10) || (x >= 21 && x <= 27)))
                || ((x == 4 || x == 27) && ((y >= 4 && y <= 10) || (y >= 21 && y <= 27))))
            {
                u32 tile = (y / 8) * 4 + x / 8;
                tiles[tile * 8 + y % 8] |= 1 << ((x % 8) * 4);
            }
    LoadSpriteSheet(&sheet);
}

static void CB2_ArenaInit(void)
{
    u32 i;
    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    ScanlineEffect_Stop();
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_FORCED_BLANK);
    FreeAllWindowBuffers();
    ResetTasks();
    ResetSpriteData();
    ArenaRender_Reset();
    FreeAllSpritePalettes();
    gReservedSpritePaletteCount = 2;
    ResetPaletteFade();
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sArenaBgs, ARRAY_COUNT(sArenaBgs));
    InitWindows(sArenaWindows);
    sHudReady=FALSE;
    DeactivateAllTextPrinters();
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    LoadPalette(sForestPalette, 0, sizeof(sForestPalette));
    LoadPalette(sArenaPalette, 0, sizeof(sArenaPalette));
    LoadBgTiles(1, sForestTiles, sizeof(sForestTiles), 0);
    LoadBgTilemap(1, sForestMap, sizeof(sForestMap), 0);
    DrawStage();
    ArenaNav_Init();
    ArenaPhysics_Init();
    LoadSpriteSheet(&sShotSheet);
    LoadSpritePalette(&sShotPalettes[0]);
    LoadSpritePalette(&sShotPalettes[1]);
    LoadSpriteSheet(&sShadowSheet);
    LoadSpritePalette(&sShadowPal);
    LoadAimMarker();
    ArenaFeedback_Init();
    ArenaMoveFx_Init();
    ArenaTerrain_Init();
    memset(sArena.bodies, 0, sizeof(sArena.bodies));
    memset(sArena.shots, 0, sizeof(sArena.shots));
    memset(&gArenaSpriteTelemetry, 0, sizeof(gArenaSpriteTelemetry));
    memset(&gArenaCombatTelemetry, 0, sizeof(gArenaCombatTelemetry));
    memset(&gArenaMoveTelemetry, 0, sizeof(gArenaMoveTelemetry));
    memset(&gArenaFrameTelemetry,0,sizeof(gArenaFrameTelemetry));
    memset(gArenaRenderTelemetry,0,sizeof(gArenaRenderTelemetry));
    gArenaFrameTelemetry.lastVBlank=gMain.vblankCounter1;
    sArena.lastBlast=0;
    for (i = 0; i < 2; i++)
    {
        struct ArenaBody *body = &sArena.bodies[i];
        struct SpriteTemplate template = sMonTemplate;
        struct SpriteSheet sheet;
        u32 front;
        body->art = ArenaSprites_Get(gBattleMons[i].species);
        gArenaSpriteTelemetry.pmd[i] = body->art != NULL;
        if (body->art)
        {
            sheet.data = body->art->animations[ARENA_ANIM_IDLE].tiles;
            sheet.size = 2048; sheet.tag = MON_TAG + i * 2;
            LoadSpriteSheet(&sheet);
            LoadPalette(body->art->palette, OBJ_PLTT_ID(i), PLTT_SIZE_4BPP);
            template.tileTag = MON_TAG + i * 2;
        }
        else
        {
            // Other species retain the native fallback until their art is imported.
            for (front = 0; front < 2; front++)
            {
                LoadSpecialPokePic(front ? &gMonFrontPicTable[gBattleMons[i].species] : &gMonBackPicTable[gBattleMons[i].species],
                    gDecompressionBuffer, gBattleMons[i].species, gBattleMons[i].personality, front);
                sheet.data = gDecompressionBuffer; sheet.size = 2048; sheet.tag = MON_TAG + i * 2 + front;
                LoadSpriteSheet(&sheet);
            }
            LoadCompressedPalette(GetMonSpritePalFromSpeciesAndPersonality(gBattleMons[i].species,
                gBattleMons[i].otId, gBattleMons[i].personality), OBJ_PLTT_ID(i), PLTT_SIZE_4BPP);
            template.tileTag = MON_TAG + i * 2 + (i ? 1 : 0);
        }
        body->x = 120 * Q;
        body->y = (i ? ARENA_MIN_Y : ARENA_MAX_Y) * Q;
        body->facing = i ? FACE_DOWN : FACE_UP;
        body->moveSlot = FirstMove(i, TRUE);
        body->cooldown = i ? 30 : 15;
        body->sprite = CreateSprite(&template, body->x / Q, body->y / Q, 0);
        body->shadow = CreateSprite(&sShadowTemplate, body->x / Q, body->y / Q + 8, 10);
        gSprites[body->sprite].oam.paletteNum = i;
        gSprites[body->sprite].affineAnimPaused = TRUE;
        gSprites[body->sprite].affineAnimBeginning = FALSE;
        SetOamMatrix(gSprites[body->sprite].oam.matrixNum, body->art ? 0x100 : 0x200, 0, 0, body->art ? 0x100 : 0x200);
        body->drawnFrame = body->drawnDirection = 255;
    }
    sArena.cueSprite = CreateSprite(&sShotTemplate, 120, 32, 0);
    gSprites[sArena.cueSprite].oam.paletteNum = IndexOfSpritePaletteTag(SHOT_TAG + 1);
    gSprites[sArena.cueSprite].invisible = TRUE;
    sArena.aimSprite = CreateSprite(&sAimTemplate, 120, 32, 1);
    gSprites[sArena.aimSprite].invisible = TRUE;
    sArena.aiRandom = gBattleMons[1].personality ^ 0xA12E7A11;
    sArena.style = gBattleMons[1].personality % 3;
    sArena.reaction = Clamp(21 - gBattleMons[1].level / 2, 8, 21);
    sArena.aimError = Clamp(12 - gBattleMons[1].level / 4, 3, 12);
    sArena.thinkTimer = 1; sArena.aimTimer = 0; sArena.goalTimer = 0;
    sArena.aiState = AI_REPOSITION;
    sArena.observed.x = sArena.previous.x = 120;
    sArena.observed.y = sArena.previous.y = ARENA_MAX_Y;
    sArena.goal.x = sArena.waypoint.x = 120;
    sArena.goal.y = sArena.waypoint.y = ARENA_MIN_Y;
    memset(&gArenaAiTelemetry, 0, sizeof(gArenaAiTelemetry));
    gArenaAiTelemetry.level = gBattleMons[1].level;
    gArenaAiTelemetry.reaction = sArena.reaction;
    gArenaAiTelemetry.aimError = sArena.aimError;
    gArenaAiTelemetry.style = sArena.style;
    // Prime the initial route while the screen is forced blank. The first
    // playable frame should not pay for graph search plus sprite initialization.
    AiChooseGoal();sArena.thinkTimer=sArena.reaction;
    gArenaAiTelemetry.decisions++;
    sArena.frame = 0;
    sArena.active = TRUE;
    gRealtimeArenaQuietResult = FALSE;
    gArenaIntroTelemetry.elapsed=gMain.vblankCounter1-gArenaIntroTelemetry.started;
    sArena.paused = FALSE;
    sArena.resultTimer = 0;
    sArena.hitstop = sArena.attackBuffer = sArena.dashBuffer = 0;
    gRealtimeArenaTelemetry.active = TRUE;
    gRealtimeArenaTelemetry.paused = FALSE;
    gRealtimeArenaTelemetry.entries++;
    gRealtimeArenaTelemetry.hpBefore = gBattleMons[0].hp;
    gRealtimeArenaTelemetry.expBefore = GetMonData(&gPlayerParty[gBattlerPartyIndexes[0]], MON_DATA_EXP);
    DrawHud();
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
    AnimateSprites();BuildOamBuffer();
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
    ShowBg(0);
    ShowBg(1);
    SetVBlankCallback(VBlank_Arena);
    SetMainCallback2(CB2_Arena);
}

static u16 Speed(u8 side)
{
    // Q8 pixels per hardware frame; no render-rate-dependent clock.
    // 50% faster traversal for BOTH sides; native Speed still orders them.
    return 288 + Clamp(gBattleMons[side].speed, 1, 180) * 3;
}

static u8 Facing(s32 dx, s32 dy)
{
    if (Abs(dx) * 2 < Abs(dy)) return dy < 0 ? FACE_UP : FACE_DOWN;
    if (Abs(dy) * 2 < Abs(dx)) return dx < 0 ? FACE_LEFT : FACE_RIGHT;
    return dy < 0 ? (dx < 0 ? 5 : 3) : (dx < 0 ? 7 : 1);
}

static void MoveDelta(u8 side, s32 dx, s32 dy)
{
    struct ArenaBody *body = &sArena.bodies[side];
    struct ArenaBody *other = &sArena.bodies[side ^ 1];
    s32 x, y;
    body->moving = dx || dy;
    if (dx || dy) body->facing = Facing(dx, dy);
    x = Clamp(body->x + dx, ARENA_MIN_X * Q, ARENA_MAX_X * Q);
    y = Clamp(body->y + dy, ARENA_MIN_Y * Q, ARENA_MAX_Y * Q);
    // Axis-separated sliding. Even the fastest dash step is smaller than any
    // solid obstacle; the swept test also prevents corner cutting.
    if (ArenaNav_LineClear(body->x / Q, body->y / Q, x / Q, body->y / Q, ARENA_BODY_RADIUS)
        && (Abs(x - other->x) >= 22 * Q || Abs(body->y - other->y) >= 22 * Q)) body->x = x;
    else if (dx) gArenaAiTelemetry.wallBlocks[side]++;
    if (ArenaNav_LineClear(body->x / Q, body->y / Q, body->x / Q, y / Q, ARENA_BODY_RADIUS)
        && (Abs(body->x - other->x) >= 22 * Q || Abs(y - other->y) >= 22 * Q)) body->y = y;
    else if (dy) gArenaAiTelemetry.wallBlocks[side]++;
}

static void Move(u8 side, s32 dx, s32 dy, s32 speed)
{
    if (dx && dy) speed = speed * 181 / 256;
    MoveDelta(side, dx * speed, dy * speed);
}

static void Fire(u8 side, u8 slot, s32 targetX, s32 targetY)
{
    struct ArenaBody *body = &sArena.bodies[side];
    const struct ArenaMoveProfile *profile;
    u32 i;
    s32 dx, dy, len;
    u8 pp;
    // The selected move is captured at BeginShot. Changing L/R during its
    // animation changes the NEXT attack, never this attack's cost or power.
    if (slot >= MAX_MON_MOVES || !gBattleMons[side].pp[slot]) return;
    profile = ArenaMoves_Get(gBattleMons[side].moves[slot]);
    if (!profile) return;
    dx = body->attackX; dy = body->attackY;
    len = max(Abs(dx), Abs(dy)) + min(Abs(dx), Abs(dy)) / 2;
    if (!len) { dx = Q; len = Q; }
    body->attackX = dx * Q / len; body->attackY = dy * Q / len;
    if (profile->kind == ARENA_MOVE_PROJECTILE)
    {
        for (i = 0; i < SHOTS_COUNT; i++) if (!sArena.shots[i].life) break;
        if (i == SHOTS_COUNT) return;
        sArena.shots[i].sprite = ArenaMoveFx_CreateBolt(profile,body->x/Q,body->y/Q,body->shotFacing);
        if (sArena.shots[i].sprite == MAX_SPRITES) return;
        sArena.shots[i].x = body->x; sArena.shots[i].y = body->y;
        sArena.shots[i].vx = dx * profile->speed / len;
        sArena.shots[i].vy = dy * profile->speed / len;
        sArena.shots[i].side = side; sArena.shots[i].move = profile->move;
        sArena.shots[i].life = profile->range * Q / profile->speed;
        sArena.shots[i].direction = body->shotFacing; sArena.shots[i].age = 0;
    }
    body->actionLife = profile->active;
    body->actionAge = 0; body->connected = FALSE; body->terrainMask=0;
    pp = --gBattleMons[side].pp[slot];
    SetMonData(side ? &gEnemyParty[gBattlerPartyIndexes[side]] : &gPlayerParty[gBattlerPartyIndexes[side]],
        MON_DATA_PP1 + slot, &pp);
    body->cooldown = profile->recovery + (side ? Clamp(8-gBattleMons[1].level/8,2,8) : 0);
    gRealtimeArenaTelemetry.shots[side]++;
    gArenaCombatTelemetry.lastMove[side] = profile->move;
    sArena.hudDirty = TRUE;
    PlaySE(profile->kind == ARENA_MOVE_RUSH ? SE_M_SWIFT
        : profile->kind == ARENA_MOVE_CONE ? SE_M_LEER
        : profile->move == MOVE_ABSORB ? SE_M_ABSORB : SE_BALL);
}

static void BeginShot(u8 side, s32 targetX, s32 targetY)
{
    struct ArenaBody *body = &sArena.bodies[side];
    const struct ArenaMoveProfile *profile = ArenaMoves_Get(gBattleMons[side].moves[body->moveSlot]);
    s32 dx,dy,len;
    if (!profile || body->shotTimer || !gBattleMons[side].pp[body->moveSlot]) return;
    body->shotSlot = body->moveSlot;
    body->aimX = targetX; body->aimY = targetY;
    body->shotFacing = Facing(targetX - body->x, targetY - body->y);
    dx=targetX-body->x;dy=targetY-body->y;
    len=max(Abs(dx),Abs(dy))+min(Abs(dx),Abs(dy))/2;
    if(!len){dy=-Q;len=Q;}
    body->attackX=dx*Q/len;body->attackY=dy*Q/len;
    body->shotElapsed = 0;
    body->shotTimer = profile->windup + profile->active + 8;
    body->animation = profile->animation;
    body->animClock = 0;
    body->drawnFrame = 255;
}

static void TickPendingShots(void)
{
    u32 side;
    for (side = 0; side < 2; side++)
    {
        struct ArenaBody *body = &sArena.bodies[side];
        const struct ArenaMoveProfile *profile;
        if (!body->shotTimer) continue;
        profile = ArenaMoves_Get(gBattleMons[side].moves[body->shotSlot]);
        // Presentation maps the profile's release to the source HitFrame.
        if (body->shotElapsed == profile->windup)
            Fire(side, body->shotSlot, body->aimX, body->aimY);
        body->shotElapsed++;
        body->shotTimer--;
    }
}

static void BufferPlayerActions(void)
{
    struct ArenaBody *body = &sArena.bodies[0];
    s32 dx = (JOY_HELD(DPAD_RIGHT) != 0) - (JOY_HELD(DPAD_LEFT) != 0);
    s32 dy = (JOY_HELD(DPAD_DOWN) != 0) - (JOY_HELD(DPAD_UP) != 0);
    // Brief taps made just before recovery ends survive for six active frames.
    // This also catches input during the two-frame impact stop.
    if (JOY_NEW(A_BUTTON)) sArena.attackBuffer = 6;
    else if (sArena.attackBuffer) sArena.attackBuffer--;
    if (JOY_NEW(B_BUTTON) && (dx || dy))
    {
        sArena.dashBuffer = 6;
        sArena.dashRequestX = dx; sArena.dashRequestY = dy;
    }
    else if (sArena.dashBuffer) sArena.dashBuffer--;
    if (JOY_NEW(L_BUTTON | R_BUTTON))
    {
        u32 i;
        for (i = 1; i <= MAX_MON_MOVES; i++)
        {
            u8 slot = (body->moveSlot + (JOY_NEW(L_BUTTON) ? MAX_MON_MOVES - i : i)) % MAX_MON_MOVES;
            if (SupportedMove(gBattleMons[0].moves[slot])) { body->moveSlot = slot; break; }
        }
    }
}

static void TickPlayer(void)
{
    struct ArenaBody *body = &sArena.bodies[0];
    s32 dx = (JOY_HELD(DPAD_RIGHT) != 0) - (JOY_HELD(DPAD_LEFT) != 0);
    s32 dy = (JOY_HELD(DPAD_DOWN) != 0) - (JOY_HELD(DPAD_UP) != 0);
    if (body->dashCooldown) body->dashCooldown--;
    if (sArena.dashBuffer && !body->dashCooldown && !body->actionLife)
    {
        body->dash = 10; body->dashCooldown = 75;
        body->dashX = sArena.dashRequestX; body->dashY = sArena.dashRequestY;
        sArena.dashBuffer = 0;
        if (body->shotTimer && body->shotElapsed <= ArenaMoves_Get(gBattleMons[0].moves[body->shotSlot])->windup)
            gArenaCombatTelemetry.cancelled[0]++;
        body->shotTimer = 0;
        gRealtimeArenaTelemetry.dodges++;
        PlaySE(SE_M_DOUBLE_TEAM);
    }
    if (body->dash) { dx = body->dashX; dy = body->dashY; }
    if (!body->actionLife)
        Move(0, dx, dy, Speed(0) * (body->dash ? 3 : body->shotTimer ? 1 : 2) / (body->dash ? 1 : 2));
    if (body->moving && !(sArena.frame % (body->dash ? 3 : 14)))
        ArenaFeedback_Dust(body->x/Q, body->y/Q, body->dash != 0);
    if (body->dash) body->dash--;
    if (body->cooldown) body->cooldown--;
    if ((JOY_HELD(A_BUTTON) || sArena.attackBuffer) && !body->cooldown && !body->dash && !body->shotTimer)
    {
        // A alone aims at the rival. D-pad + A gives explicit directional
        // aim, including destructible cover. This is still the same four moves.
        body->manualAim=dx||dy;
        BeginShot(0,dx||dy?body->x+dx*200*Q:sArena.bodies[1].x,
                    dx||dy?body->y+dy*200*Q:sArena.bodies[1].y);
        sArena.attackBuffer = 0;
    }
}

static u16 AiRandom(void)
{
    // Isolated deterministic stream: AI thinking never consumes the native
    // accuracy/critical-hit/damage RNG, nor reads future player input.
    sArena.aiRandom = sArena.aiRandom * 1664525 + 1013904223;
    return sArena.aiRandom >> 16;
}

static void AiRoute(void)
{
    struct ArenaBody *body = &sArena.bodies[1];
    if (ArenaNav_NextWaypoint(body->x / Q, body->y / Q, sArena.goal.x, sArena.goal.y, &sArena.waypoint))
        gArenaAiTelemetry.paths++;
}

static void AiChooseGoal(void)
{
    static const s16 offsets[8][2] =
    {
        {256,0}, {181,181}, {0,256}, {-181,181},
        {-256,0}, {-181,-181}, {0,-256}, {181,-181}
    };
    struct ArenaBody *body = &sArena.bodies[1];
    const struct ArenaMoveProfile *p = ArenaMoves_Get(gBattleMons[1].moves[body->moveSlot]);
    s32 preferred = p->kind == ARENA_MOVE_MELEE ? 24 : p->kind == ARENA_MOVE_RUSH ? p->range-22
        : p->kind == ARENA_MOVE_CONE ? 40 : 70+sArena.style*8;
    s32 best = 0x7FFFFFFF;
    u32 i;
    if (p->kind == ARENA_MOVE_PROJECTILE && gBattleMons[1].hp * 3 < gBattleMons[1].maxHP) preferred += 12;
    for (i = 0; i < 8 + ARENA_CORNERS; i++)
    {
        struct ArenaPoint p;
        s32 score;
        // Prefer the eight tactical positions around the observed opponent.
        // Only scan all obstacle corners when those positions are occluded.
        // This bounds routine thinking cost without removing the fallback.
        if(i==8&&best!=0x7FFFFFFF)break;
        if (i < 8)
        {
            p.x = Clamp(sArena.observed.x + offsets[i][0] * preferred / Q, ARENA_MIN_X, ARENA_MAX_X);
            p.y = Clamp(sArena.observed.y + offsets[i][1] * preferred / Q, ARENA_MIN_Y, ARENA_MAX_Y);
        }
        else p = ArenaNav_Corner(i - 8);
        if (!ArenaNav_CanStand(p.x, p.y)
            || !ArenaNav_LineClear(p.x, p.y, sArena.observed.x, sArena.observed.y, 2)) continue;
        score = Abs(ArenaNav_Distance(p.x,p.y,sArena.observed.x,sArena.observed.y) - preferred) * 3
            + ArenaNav_Distance(body->x/Q,body->y/Q,p.x,p.y) + AiRandom() % 24;
        if (score < best) { best = score; sArena.goal = p; }
    }
    sArena.goalTimer = 55;
    AiRoute();
}

static bool8 AiTryEvade(void)
{
    struct ArenaBody *body = &sArena.bodies[1];
    u32 i;
    // Only visible incoming projectiles at a perception tick. Novices often
    // miss the opportunity; high levels still have a reaction delay.
    for (i = 0; i < SHOTS_COUNT; i++)
    {
        struct ArenaShot *shot = &sArena.shots[i];
        s32 dx, dy, t, px, py, len, sign;
        struct ArenaPoint goal;
        if (!shot->life || shot->side != 0
            || !ArenaNav_LineClear(body->x/Q,body->y/Q,shot->x/Q,shot->y/Q,2)) continue;
        dx = shot->x - body->x; dy = shot->y - body->y;
        t = -(dx * shot->vx + dy * shot->vy) / (shot->vx * shot->vx + shot->vy * shot->vy);
        if (t < 1 || t > 22) continue;
        px = (dx + shot->vx * t) / Q; py = (dy + shot->vy * t) / Q;
        if (px * px + py * py > 256 || AiRandom() % 100 >= min(75, 15 + gBattleMons[1].level)) continue;
        len = max(Abs(shot->vx), Abs(shot->vy));
        for (sign = -1; sign <= 1; sign += 2)
        {
            goal.x = body->x/Q + sign * -shot->vy * 26 / len;
            goal.y = body->y/Q + sign * shot->vx * 26 / len;
            if (ArenaNav_CanStand(goal.x,goal.y)
                && ArenaNav_LineClear(body->x/Q,body->y/Q,goal.x,goal.y,ARENA_BODY_RADIUS))
            {
                sArena.goal = sArena.waypoint = goal;
                sArena.aiState = AI_EVADE;
                sArena.goalTimer = sArena.reaction;
                gArenaAiTelemetry.dodges++;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void AiBeginAim(void)
{
    struct ArenaBody *body = &sArena.bodies[1];
    s32 travel = ArenaNav_Distance(body->x/Q,body->y/Q,sArena.observed.x,sArena.observed.y) * 2 / 5;
    s32 prediction = min(30+gBattleMons[1].level*2, 80);
    s32 errorX = AiRandom() % (sArena.aimError * 2 + 1) - sArena.aimError;
    s32 errorY = AiRandom() % (sArena.aimError * 2 + 1) - sArena.aimError;
    body->aimX = (sArena.observed.x + errorX
        + (sArena.observed.x - sArena.previous.x) * travel * prediction / (sArena.reaction * 100)) * Q;
    body->aimY = (sArena.observed.y + errorY
        + (sArena.observed.y - sArena.previous.y) * travel * prediction / (sArena.reaction * 100)) * Q;
    sArena.aimTimer = Clamp(12 - gBattleMons[1].level / 4, 6, 12);
    sArena.aiState = AI_AIM;
    // This target is locked for the entire visible wind-up.
}

static void AiChooseMove(s32 distance)
{
    u32 i;
    s32 best=-100000;
    for(i=0;i<MAX_MON_MOVES;i++)
    {
        u16 move=gBattleMons[1].moves[i];
        const struct ArenaMoveProfile *p=ArenaMoves_Get(move);
        s32 score;
        if(!p||!gBattleMons[1].pp[i])continue;
        score=gBattleMoves[move].power*2+AiRandom()%35;
        if(distance>p->range)score-=distance-p->range;
        if(move==MOVE_ABSORB&&gBattleMons[1].hp*2<gBattleMons[1].maxHP)score+=90;
        if(move==MOVE_LEER)score=gBattleMons[0].statStages[STAT_DEF]>4?60+AiRandom()%50:-1000;
        if(score>best){best=score;sArena.bodies[1].moveSlot=i;}
    }
}

static void TickEnemy(void)
{
    struct ArenaBody *body = &sArena.bodies[1];
    s32 dx, dy, len;
    body->moving = FALSE;
    if (body->cooldown) body->cooldown--;
    if (sArena.goalTimer) sArena.goalTimer--;
    if (body->shotTimer) return;
    if (sArena.aiState == AI_AIM)
    {
        if (--sArena.aimTimer == 0)
        {
            if (ArenaNav_LineClear(body->x/Q,body->y/Q,body->aimX/Q,body->aimY/Q,2))
                BeginShot(1, body->aimX, body->aimY);
            sArena.aiState = AI_RECOVER;
            sArena.thinkTimer = 1;
        }
        return;
    }
    if (--sArena.thinkTimer == 0)
    {
        s32 distance;
        sArena.thinkTimer = sArena.reaction;
        sArena.previous = sArena.observed;
        sArena.observed.x = sArena.bodies[0].x / Q;
        sArena.observed.y = sArena.bodies[0].y / Q;
        gArenaAiTelemetry.decisions++;
        distance = ArenaNav_Distance(body->x/Q,body->y/Q,sArena.observed.x,sArena.observed.y);
        if(!body->cooldown)AiChooseMove(distance);
        if (!AiTryEvade())
        {
            const struct ArenaMoveProfile *p=ArenaMoves_Get(gBattleMons[1].moves[body->moveSlot]);
            if (!body->cooldown && gBattleMons[1].pp[body->moveSlot]
                && distance < p->range && distance > 8
                && ArenaNav_LineClear(body->x/Q,body->y/Q,sArena.observed.x,sArena.observed.y,2))
            {
                AiBeginAim(); return;
            }
            sArena.aiState = body->cooldown ? AI_RECOVER : AI_REPOSITION;
            if (!sArena.goalTimer || distance < 38
                || ArenaNav_Distance(body->x/Q,body->y/Q,sArena.goal.x,sArena.goal.y) < 5
                || ArenaNav_Distance(sArena.previous.x,sArena.previous.y,sArena.observed.x,sArena.observed.y) > 18)
                AiChooseGoal();
            else AiRoute();
        }
    }
    if (ArenaNav_Distance(body->x/Q,body->y/Q,sArena.waypoint.x,sArena.waypoint.y) < 3
        && ArenaNav_Distance(body->x/Q,body->y/Q,sArena.goal.x,sArena.goal.y) >= 4) AiRoute();
    dx = sArena.waypoint.x * Q - body->x;
    dy = sArena.waypoint.y * Q - body->y;
    len = max(Abs(dx), Abs(dy)) + min(Abs(dx), Abs(dy)) / 2;
    if (len > Q) MoveDelta(1, dx * Speed(1) / len, dy * Speed(1) / len);
}

static void ApplyMoveHit(u8 side,u16 move)
{
    u8 targetSide=side^1;
    struct ArenaBody *target=&sArena.bodies[targetSide];
    s32 damage;
    if(move==MOVE_LEER)
    {
        bool8 worked=RealtimeArena_ResolveLeer(side,targetSide);
        if(worked)gArenaMoveTelemetry.statChanges[side]++;
        ArenaFeedback_Impact(targetSide,target->x/Q,target->y/Q,0,
            worked?ARENA_FEEDBACK_DEFENSE:ARENA_FEEDBACK_MISS);
        return;
    }
    damage=RealtimeArena_ResolveDamage(side,targetSide,move);
    if(damage>0)
    {
        u16 hp=gBattleMons[targetSide].hp;
        u16 dealt=min(damage,hp);
        ArenaFeedback_Impact(targetSide,target->x/Q,target->y/Q,dealt,ARENA_FEEDBACK_DAMAGE);
        sArena.hitstop=damage>=8?3:2;
        hp-=dealt;
        gBattleMons[targetSide].hp=hp;
        SetMonData(targetSide?&gEnemyParty[gBattlerPartyIndexes[targetSide]]:
                   &gPlayerParty[gBattlerPartyIndexes[targetSide]],MON_DATA_HP,&hp);
        if(move==MOVE_ABSORB)
        {
            u16 healing=min(RealtimeArena_DrainAmount(dealt),gBattleMons[side].maxHP-gBattleMons[side].hp);
            u16 healedHp=gBattleMons[side].hp+healing;
            gBattleMons[side].hp=healedHp;
            SetMonData(side?&gEnemyParty[gBattlerPartyIndexes[side]]:
                       &gPlayerParty[gBattlerPartyIndexes[side]],MON_DATA_HP,&healedHp);
            gArenaMoveTelemetry.healed[side]+=healing;
            ArenaFeedback_Drain(side,sArena.bodies[side].x/Q,sArena.bodies[side].y/Q,
                                target->x/Q,target->y/Q,healing);
        }
        target->flash=8;
        target->knockX=sArena.bodies[side].attackX*(move==MOVE_QUICK_ATTACK?4:2);
        target->knockY=sArena.bodies[side].attackY*(move==MOVE_QUICK_ATTACK?4:2);
        gRealtimeArenaTelemetry.hits[side]++;
        gRealtimeArenaTelemetry.lastDamage=damage;
        sArena.hudDirty=TRUE;
        PlaySE(move==MOVE_ABSORB?SE_M_ABSORB_2:SE_M_COMET_PUNCH);
        if(!hp)
        {
            sArena.resultTimer=12;sArena.lastAttacker=side;sArena.lastTarget=targetSide;
            gArenaResultTelemetry.started=gMain.vblankCounter1;
        }
    }
    else
    {
        gRealtimeArenaTelemetry.misses++;
        ArenaFeedback_Impact(targetSide,target->x/Q,target->y/Q,0,
            (gMoveResultFlags&MOVE_RESULT_DOESNT_AFFECT_FOE)?ARENA_FEEDBACK_IMMUNE:ARENA_FEEDBACK_MISS);
    }
}

static void HitTerrain(u8 index,const struct ArenaMoveProfile*p,s16 ix,s16 iy)
{
    u32 before=gArenaPhysicsTelemetry.broken;
    ArenaPhysics_Hit(index,ix,iy,p->kind==ARENA_MOVE_PROJECTILE?1:2);
    if(before!=gArenaPhysicsTelemetry.broken)sArena.hitstop=3;
}

static void TickActions(void)
{
    u32 side;
    for(side=0;side<2&&!sArena.resultTimer;side++)
    {
        struct ArenaBody *body=&sArena.bodies[side],*target=&sArena.bodies[side^1];
        const struct ArenaMoveProfile *p;
        s16 oldX=body->x/Q,oldY=body->y/Q;
        bool8 hit=FALSE;
        if(!body->actionLife)continue;
        p=ArenaMoves_Get(gBattleMons[side].moves[body->shotSlot]);
        if(p->kind==ARENA_MOVE_RUSH)
        {
            s32 dx=body->attackX*p->speed/Q,dy=body->attackY*p->speed/Q;
            s16 obstacle=ArenaNav_FirstObstacle(oldX,oldY,(body->x+dx)/Q,(body->y+dy)/Q,ARENA_BODY_RADIUS);
            if(obstacle>=0)
            {
                body->actionLife=1;gArenaMoveTelemetry.rushWalls[side]++;
                HitTerrain(obstacle,p,body->attackX*3,body->attackY*3);
                ArenaFeedback_Wall(oldX,oldY);PlaySE(SE_WALL_HIT);
            }
            MoveDelta(side,dx,dy);
            if(!(body->actionAge%3))ArenaFeedback_Dust(body->x/Q,body->y/Q,TRUE);
            hit=ArenaMoves_SegmentHit(oldX,oldY,body->x/Q,body->y/Q,target->x/Q,target->y/Q,p->radius);
        }
        else if(p->kind==ARENA_MOVE_MELEE||p->kind==ARENA_MOVE_CONE)
        {
            u8 range=p->kind==ARENA_MOVE_CONE?min(p->range,24+body->actionAge*4):p->range;
            u32 i;
            if(p->kind==ARENA_MOVE_MELEE)
                for(i=0;i<ARENA_OBSTACLES;i++)
                {
                    const struct ArenaRect*r=&gArenaObstacles[i];
                    s16 x=Clamp(oldX,r->left,r->right),y=Clamp(oldY,r->top,r->bottom);
                    if(!(body->terrainMask&(1<<i))&&ArenaNav_IsSolid(i)
                        &&ArenaMoves_InCone(x-oldX,y-oldY,body->attackX,body->attackY,range,p->cone)
                        &&ArenaNav_FirstObstacle(oldX,oldY,x,y,0)==i)
                    {
                        body->terrainMask|=1<<i;
                        HitTerrain(i,p,body->attackX*3,body->attackY*3);
                    }
                }
            hit=ArenaMoves_InCone((target->x-body->x)/Q,(target->y-body->y)/Q,
                                  body->attackX,body->attackY,range,p->cone);
        }
        if(hit&&!body->connected&&!target->dash
            &&ArenaNav_LineClear(body->x/Q,body->y/Q,target->x/Q,target->y/Q,2))
        {
            body->connected=TRUE;
            ApplyMoveHit(side,p->move);
        }
        body->actionAge++;body->actionLife--;
    }
}

static void TickShots(void)
{
    u32 i;
    for (i = 0; i < SHOTS_COUNT && !sArena.resultTimer; i++)
    {
        struct ArenaShot *shot = &sArena.shots[i];
        struct ArenaBody *target;
        const struct ArenaMoveProfile *p;
        s16 oldX,oldY;
        s16 obstacle;
        if (!shot->life) continue;
        p=ArenaMoves_Get(shot->move);
        oldX=shot->x/Q;oldY=shot->y/Q;
        target = &sArena.bodies[shot->side ^ 1];
        obstacle=ArenaNav_FirstObstacle(shot->x/Q,shot->y/Q,(shot->x+shot->vx)/Q,(shot->y+shot->vy)/Q,2);
        if (obstacle>=0)
        {
            gArenaAiTelemetry.blockedShots[shot->side]++;
            ArenaFeedback_Wall(shot->x/Q,shot->y/Q);
            PlaySE(SE_WALL_HIT);
            HitTerrain(obstacle,p,shot->vx,shot->vy);
            DestroySprite(&gSprites[shot->sprite]);
            shot->life = 0;
            continue;
        }
        shot->x += shot->vx; shot->y += shot->vy;
        if (ArenaMoves_SegmentHit(oldX,oldY,shot->x/Q,shot->y/Q,target->x/Q,target->y/Q,p->radius) && !target->dash)
        {
            ApplyMoveHit(shot->side,shot->move);
            shot->life = 1;
        }
        if (shot->x < 3 * Q || shot->x > 237 * Q || shot->y < 19 * Q || shot->y > 157 * Q) shot->life = 1;
        if (--shot->life == 0) DestroySprite(&gSprites[shot->sprite]);
        else ArenaMoveFx_Bolt(shot->sprite,p,shot->x/Q,shot->y/Q,shot->direction,++shot->age);
    }
}

static void TickPhysics(void)
{
    u32 side;
    ArenaPhysics_Update();
    for(side=0;side<2;side++)
    {
        struct ArenaBody *body=&sArena.bodies[side];
        if(sArena.lastBlast!=gArenaPhysicsTelemetry.blastSerial)
        {
            s32 dx=body->x/Q-gArenaPhysicsTelemetry.blastX;
            s32 dy=body->y/Q-gArenaPhysicsTelemetry.blastY;
            s32 len=max(1,max(Abs(dx),Abs(dy))+min(Abs(dx),Abs(dy))/2);
            if(dx*dx+dy*dy<46*46){body->knockX=dx*1200/len;body->knockY=dy*1200/len;}
        }
        if(body->knockX||body->knockY)
        {
            MoveDelta(side,body->knockX,body->knockY);
            body->knockX=body->knockX*180/256;body->knockY=body->knockY*180/256;
            if(Abs(body->knockX)<8)body->knockX=0;
            if(Abs(body->knockY)<8)body->knockY=0;
        }
    }
    sArena.lastBlast=gArenaPhysicsTelemetry.blastSerial;
}

static void CB2_Arena(void)
{
    u32 i;
    bool8 frozen = FALSE;
    u32 stamp=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228,phaseStamp=stamp,now;
    u32 gap=gMain.vblankCounter1-gArenaFrameTelemetry.lastVBlank;
    gArenaFrameTelemetry.lastVBlank=gMain.vblankCounter1;
    gArenaFrameTelemetry.updates++;
    if(gap>gArenaFrameTelemetry.maxGap)gArenaFrameTelemetry.maxGap=gap;
    if(gap>1)gArenaFrameTelemetry.missedVBlanks+=gap-1;
    if (JOY_NEW(SELECT_BUTTON) && !sArena.resultTimer) { ArenaExit(FALSE); return; }
    if (JOY_NEW(START_BUTTON) && !sArena.resultTimer)
    {
        sArena.paused ^= TRUE;
        DrawStage();
        sArena.hudDirty = TRUE;
    }
    if (!sArena.paused)
    {
        sArena.frame++;
        gRealtimeArenaTelemetry.frames++;
        if (sArena.resultTimer)
        {
            if (--sArena.resultTimer == 0) { ArenaExit(TRUE); return; }
        }
        else
        {
            BufferPlayerActions();
            if (sArena.hitstop) { sArena.hitstop--; frozen = TRUE; }
            else
            {
                TickPhysics();
                now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
                gArenaFrameTelemetry.scanlines[0]=now-phaseStamp;phaseStamp=now;
                TickPlayer(); TickEnemy();
                now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
                gArenaFrameTelemetry.scanlines[1]=now-phaseStamp;phaseStamp=now;
                TickPendingShots(); TickActions(); TickShots();
            }
        }
    }
    frozen |= sArena.hitstop != 0;
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    gArenaFrameTelemetry.scanlines[2]=now-phaseStamp;phaseStamp=now;
    for (i = 0; i < 2; i++)
    {
        struct ArenaBody *body = &sArena.bodies[i];
        const struct ArenaMoveProfile *profile=ArenaMoves_Get(gBattleMons[i].moves[body->shotTimer?body->shotSlot:body->moveSlot]);
        struct Sprite *sprite = &gSprites[body->sprite];
        sprite->x = body->x / Q; sprite->y = body->y / Q;
        gSprites[body->shadow].x = sprite->x;
        gSprites[body->shadow].y = sprite->y + 8;
        gSprites[body->shadow].invisible = gBattleMons[i].hp == 0;
        if (!sArena.paused && !frozen)
            sprite->x2 = i && sArena.aiState == AI_AIM ? ((sArena.frame & 2) ? 1 : -1) : 0;
        if (body->art)
        {
            u8 animation = body->shotTimer ? profile->animation : body->moving ? ARENA_ANIM_WALK : ARENA_ANIM_IDLE;
            u8 direction = body->shotTimer ? body->shotFacing : body->facing;
            const struct ArenaSpriteAnimation *anim = &body->art->animations[animation];
            u8 frame;
            u16 tick=body->animClock/Q;
            if (body->animation != animation)
            {
                body->animation = animation; body->animClock = 0; body->drawnFrame = 255;
            }
            if(body->shotTimer)
                tick=body->shotElapsed<profile->windup?body->shotElapsed*anim->hitTick/profile->windup:
                    anim->hitTick+(body->shotElapsed-profile->windup)*(anim->totalTicks-anim->hitTick-1)/(profile->active+8);
            frame = ArenaSprites_Frame(anim,tick);
            if (body->drawnFrame != frame || body->drawnDirection != direction)
            {
                ArenaRender_Copy(anim->tiles + (direction * anim->frames + frame) * 2048,
                    (u8 *)OBJ_VRAM0 + GetSpriteTileStartByTag(MON_TAG + i * 2) * 32, 2048);
                body->drawnFrame = frame; body->drawnDirection = direction;
                gArenaSpriteTelemetry.uploads[i]++;
            }
            if (!sArena.paused && !sArena.resultTimer && !frozen)
                body->animClock += animation == ARENA_ANIM_WALK ? Clamp(Speed(i) * Q / 220, 128, 512) : Q;
            gArenaSpriteTelemetry.animation[i] = animation;
            gArenaSpriteTelemetry.frame[i] = frame;
            gArenaSpriteTelemetry.direction[i] = direction;
        }
        else
        {
            if (!sArena.paused && !frozen)
                sprite->y2 = body->moving && (sArena.frame & 8) ? -1 : 0;
            sprite->oam.tileNum = GetSpriteTileStartByTag(MON_TAG + i * 2 + (body->facing < 3 || body->facing > 5));
            SetOamMatrix(sprite->oam.matrixNum, body->facing > 0 && body->facing < 4 ? -0x200 : 0x200, 0, 0, 0x200);
        }
        // A short palette flash keeps the original pose visible, unlike
        // alternating invisible frames. Dust now communicates the dash.
        sprite->invisible = sArena.resultTimer && !gBattleMons[i].hp;
        gSprites[body->shadow].invisible = sprite->invisible;
        BlendPalette(OBJ_PLTT_ID(i), 16, body->flash ? body->flash + 3 : 0, RGB_WHITE);
        if (body->flash && !sArena.paused && !frozen) body->flash--;
        gRealtimeArenaTelemetry.x[i] = sprite->x;
        gRealtimeArenaTelemetry.y[i] = sprite->y;
        gRealtimeArenaTelemetry.speed[i] = Speed(i);
        gArenaAiTelemetry.facing[i] = body->facing;
        gArenaCombatTelemetry.cooldown[i] = body->cooldown;
        gArenaCombatTelemetry.windup[i] = body->shotTimer;
        gArenaCombatTelemetry.dashCooldown[i] = body->dashCooldown;
        gArenaCombatTelemetry.pendingSlot[i] = body->shotTimer ? body->shotSlot : 255;
        gArenaMoveTelemetry.kind[i]=profile->kind;
        gArenaMoveTelemetry.phase[i]=body->actionLife?2:body->shotTimer&&body->shotElapsed<=profile->windup?1:body->cooldown?3:0;
        gArenaMoveTelemetry.range[i]=profile->range;
        gArenaMoveTelemetry.active[i]=body->actionLife;
        gArenaMoveTelemetry.actionMove[i]=profile->move;
        ArenaMoveFx_Action(i,profile,body->x/Q,body->y/Q,body->shotFacing,body->actionAge,body->actionLife!=0,sArena.paused);
    }
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    gArenaFrameTelemetry.scanlines[3]=now-phaseStamp;phaseStamp=now;
    gArenaCombatTelemetry.aimBlocked = !ArenaNav_LineClear(sArena.bodies[0].x/Q,
        sArena.bodies[0].y/Q, sArena.bodies[1].x/Q, sArena.bodies[1].y/Q, 2);
    gArenaCombatTelemetry.aimVisible = !sArena.paused && !sArena.resultTimer
        && !(sArena.bodies[0].shotTimer?sArena.bodies[0].manualAim:JOY_HELD(DPAD_ANY))
        && (JOY_HELD(A_BUTTON) || sArena.bodies[0].shotTimer);
    gSprites[sArena.aimSprite].x = sArena.bodies[1].x / Q;
    gSprites[sArena.aimSprite].y = sArena.bodies[1].y / Q;
    gSprites[sArena.aimSprite].invisible = !gArenaCombatTelemetry.aimVisible;
    gSprites[sArena.aimSprite].oam.paletteNum = IndexOfSpritePaletteTag(SHOT_TAG
        + (gArenaCombatTelemetry.aimBlocked ? 1 : sArena.bodies[0].cooldown ? 2 : 0));
    gSprites[sArena.cueSprite].x = sArena.bodies[1].x / Q;
    gSprites[sArena.cueSprite].y = sArena.bodies[1].y / Q + 14;
    gSprites[sArena.cueSprite].invisible = sArena.aiState != AI_AIM || sArena.resultTimer;
    gArenaAiTelemetry.state = sArena.aiState;
    gArenaAiTelemetry.goalX = sArena.goal.x; gArenaAiTelemetry.goalY = sArena.goal.y;
    gArenaAiTelemetry.waypointX = sArena.waypoint.x; gArenaAiTelemetry.waypointY = sArena.waypoint.y;
    gRealtimeArenaTelemetry.paused = sArena.paused;
    gRealtimeArenaTelemetry.selectedMove = sArena.bodies[0].moveSlot;
    ArenaFeedback_Update(sArena.paused, frozen);
    ArenaTerrain_Draw(sArena.paused,frozen);
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    gArenaRenderTelemetry[0]=now-phaseStamp;
    gArenaFeedbackTelemetry.hitstop = sArena.hitstop;
    gArenaFeedbackTelemetry.attackBuffer = sArena.attackBuffer;
    gArenaFeedbackTelemetry.dashBuffer = sArena.dashBuffer;
    if (sArena.hudDirty || JOY_NEW(START_BUTTON | L_BUTTON | R_BUTTON)) DrawHud();
    gArenaRenderTelemetry[1]=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228-now;
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    AnimateSprites();
    BuildOamBuffer();
    ArenaRender_Ready();
    gArenaRenderTelemetry[2]=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228-now;
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    UpdatePaletteFade();
    gArenaRenderTelemetry[3]=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228-now;
    for(i=0;i<4;i++)if(gArenaRenderTelemetry[i]>gArenaRenderTelemetry[4+i])gArenaRenderTelemetry[4+i]=gArenaRenderTelemetry[i];
    now=gMain.vblankCounter1*228+(REG_VCOUNT+68)%228;
    gArenaFrameTelemetry.scanlines[4]=now-phaseStamp;
    gArenaFrameTelemetry.scanlines[5]=now-stamp;
    for(i=0;i<6;i++)if(gArenaFrameTelemetry.scanlines[i]>gArenaFrameTelemetry.peak[i])
        gArenaFrameTelemetry.peak[i]=gArenaFrameTelemetry.scanlines[i];
}

static void ArenaExit(bool8 fainted)
{
    u32 i;
    // Finish the ORIGINAL battle scripts behind the arena image. No classic
    // scene reconstruction just to say "fainted" and animate an EXP bar.
    // Interactive choices explicitly restore the native UI if needed.
    if(fainted)
    {
        sArena.active=FALSE;sArena.classic=FALSE;
        gRealtimeArenaTelemetry.active=FALSE;
        gRealtimeArenaTelemetry.exits++;
        gRealtimeArenaTelemetry.lastExitFainted=TRUE;
        gRealtimeArenaRestoringFaint=TRUE;
        gRealtimeArenaQuietResult=TRUE;
        gBattlerAttacker=sArena.lastAttacker;gBattlerTarget=sArena.lastTarget;
        ArenaFeedback_Destroy();
        for(i=0;i<2;i++)ArenaMoveFx_Action(i,NULL,0,0,0,0,FALSE,TRUE);
        gSprites[sArena.aimSprite].invisible=TRUE;
        gSprites[sArena.cueSprite].invisible=TRUE;
        BuildOamBuffer();ArenaRender_Ready();
        gMain.callback1=sArena.savedCB1;
        RealtimeArena_ResumeBattle(TRUE);
        SetMainCallback2(BattleMainCB2);
        return;
    }
    SetVBlankCallback(NULL);
    ArenaFeedback_Destroy();
    for (i = 0; i < 2; i++)
    {
        struct Sprite *sprite = &gSprites[sArena.bodies[i].sprite];
        FreeOamMatrix(sprite->oam.matrixNum);
        DestroySprite(sprite);
        FreeSpriteTilesByTag(MON_TAG + i * 2);
        FreeSpriteTilesByTag(MON_TAG + i * 2 + 1);
    }
    FreeAllWindowBuffers();
    sArena.active = FALSE;
    sArena.classic = TRUE;
    gRealtimeArenaTelemetry.active = FALSE;
    gRealtimeArenaTelemetry.exits++;
    gRealtimeArenaTelemetry.lastExitFainted = FALSE;
    gRealtimeArenaRestoringFaint = FALSE;
    gMain.callback1 = sArena.savedCB1;
    RealtimeArena_ResumeBattle(FALSE);
    ReshowBattleScreenAfterMenu();
}

bool8 RealtimeArena_RequestDemo(void)
{
    // A new, disposable save only. Never reset an existing player's progress.
    if (gSaveFileStatus != SAVE_STATUS_EMPTY) return FALSE;
    sDemoRequested = TRUE;
    return TRUE;
}

bool8 RealtimeArena_SetupDemo(void)
{
#if !ARENA_LAB
    static const struct {u16 species;u8 level;} team[] = {
        {SPECIES_CHARIZARD,36}, {SPECIES_BLASTOISE,36}, {SPECIES_EEVEE,25},
        {SPECIES_DRAGONITE,30}, {SPECIES_SCIZOR,15}, {SPECIES_BLAZIKEN,20}
    };
    static const struct {u16 species;u8 level;} box[] = {
        {SPECIES_TREECKO,11}, {SPECIES_POOCHYENA,12}, {SPECIES_BULBASAUR,12},
        {SPECIES_SQUIRTLE,15}, {SPECIES_GROVYLE,16}, {SPECIES_SCEPTILE,43}
    };
    u32 i;
#endif
    if (!sDemoRequested) return FALSE;
    sDemoRequested = FALSE;
    StringCopy(gSaveBlock2Ptr->playerName, sDemoName);
    gSaveBlock2Ptr->playerGender = MALE;
    gSaveBlock2Ptr->optionsTextSpeed = OPTIONS_TEXT_SPEED_FAST;
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_OLDALE_TOWN);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_OLDALE_TOWN);
    gSaveBlock1Ptr->location.warpId = -1;
    gSaveBlock1Ptr->location.x = 10;
    gSaveBlock1Ptr->location.y = 17;
    gSaveBlock1Ptr->pos.x = 10;
    gSaveBlock1Ptr->pos.y = 17;
    SetLastHealLocationWarp(HEAL_LOCATION_OLDALE_TOWN);
    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_SYS_POKEDEX_GET);
    FlagSet(FLAG_ADVENTURE_STARTED);
    FlagSet(FLAG_SYS_B_DASH);
    FlagSet(FLAG_HIDE_ROUTE_101_BIRCH_STARTERS_BAG);
    FlagSet(FLAG_HIDE_ROUTE_101_BIRCH_ZIGZAGOON_BATTLE);
    FlagSet(FLAG_HIDE_ROUTE_101_ZIGZAGOON);
    FlagSet(FLAG_HIDE_ROUTE_101_BIRCH);
    VarSet(VAR_ROUTE101_STATE, 3);
#if ARENA_LAB
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 6, 20, TRUE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_MUDKIP, 5, 20, TRUE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 2;
#else
    // A native NEW GAME preset, never an imported save or a fixture mailbox.
    // RequestDemo already refuses when any existing save is present.
    FlagSet(FLAG_ARENA_PRACTICE);
    gPlayerPartyCount = ARRAY_COUNT(team);
    for (i = 0; i < ARRAY_COUNT(team); i++)
        CreateMon(&gPlayerParty[i], team[i].species, team[i].level,
                  20, TRUE, i * 2, OT_ID_PLAYER_ID, 0);
    for (i = 0; i < ARRAY_COUNT(box); i++)
        CreateBoxMonAt(0, i, box[i].species, box[i].level,
                       20, TRUE, i * 2, OT_ID_PLAYER_ID, 0);
#endif
    return TRUE;
}

void RealtimeArena_DemoFieldCallback(void)
{
    FieldCB_WarpExitFadeFromBlack();
}
