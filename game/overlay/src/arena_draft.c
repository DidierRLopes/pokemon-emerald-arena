#include "global.h"
#include "arena_draft.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "main.h"
#include "main_menu.h"
#include "malloc.h"
#include "menu.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "random.h"
#include "save.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "title_screen.h"
#include "trainer_pokemon_sprites.h"
#include "window.h"
#include "constants/battle.h"
#include "constants/global.h"
#include "constants/moves.h"
#include "constants/opponents.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/species.h"

// Arena mode (see arena_draft.h). Screens: the ARENA menu that replaces the
// title menu, the team draft, the versus screen, and the result screen.
// Everything here is built at runtime from the game's own graphics.

#include "data/arena_draft.h"
// 24x24 grid portraits of the party icons, generated at build time.
#include "data/arena_draft_minis.h"
#include "arena_moves.h"

#define CREDITS 100
#define TEAM_MAX 6
// The grid: 24x24 portraits, 5 per row, 5 rows on screen, scrolling.
#define GRID_COLS 5
// Cell 0 is the READY button; cell n shows roster entry n - 1.
#define GRID_CELLS (ARENA_DRAFT_COUNT + 1)
#define GRID_ROWS ((GRID_CELLS + GRID_COLS - 1) / GRID_COLS)
#define CELL_READY 0
#define CELL_MON(cell) ((cell) - 1)
#define GRID_VISIBLE 5
#define CELL 24
#define STRIP_DONE 6

EWRAM_DATA u8 gArenaDraftMode = ARENA_DRAFT_NONE;
EWRAM_DATA bool8 gArenaDraftBattle = FALSE;
EWRAM_DATA u16 gArenaP2Keys = 0;
EWRAM_DATA u8 gArenaDraftVoice[2] = {};
EWRAM_DATA u8 gArenaVoiceEnroll = 0;
static EWRAM_DATA bool8 sComHard = FALSE;
EWRAM_DATA u8 gArenaDraftScreen = 0;
static EWRAM_DATA bool8 sAdventureOnce = FALSE;
static EWRAM_DATA u8 sTeams[2][TEAM_MAX] = {};
static EWRAM_DATA u8 sTeamCount[2] = {};

struct DraftScreen
{
    u8 player, cursor, focus, strip, state, menuSel, top;
    u8 picSprite, cursorSprite, stripSprite;
    u8 iconSprites[2][TEAM_MAX];
    u8 canvas[15 * 16 * 32];
    u32 ready[9 * 8];
    u16 timer;
    u8 message;
};
static EWRAM_DATA struct DraftScreen *sDraft = NULL;
enum { WIN_MAIN, WIN_CREDITS };
enum { MSG_NONE, MSG_FULL, MSG_COST, MSG_TWICE };

static const u8 sText_Arena[] = _("ARENA");
static const u8 sText_VsCom[] = _("1P VS COM");
static const u8 sText_VsP2[] = _("1P VS 1P");
static const u8 sText_Adventure[] = _("ADVENTURE");
static const u8 sText_MenuHint[] = _("{A_BUTTON} SELECT   {B_BUTTON} BACK");
static const u8 sText_Cursor[] = _("▶");
static const u8 sText_P1[] = _("PLAYER 1");
static const u8 sText_P2[] = _("PLAYER 2");
static const u8 sText_Com[] = _("COM");
static const u8 sText_Cr[] = _(" CR");
static const u8 sText_Left[] = _("LEFT");
static const u8 sText_Done[] = _("{START_BUTTON} DONE");
static const u8 sText_Full[] = _("TEAM IS FULL");
static const u8 sText_TooCostly[] = _("NOT ENOUGH CREDITS");
static const u8 sText_Twice[] = _("ALREADY IN TEAM");
static const u8 sText_Hp[] = _("HP");
static const u8 sText_Atk[] = _("ATK");
static const u8 sText_Def[] = _("DEF");
static const u8 sText_SpA[] = _("SPA");
static const u8 sText_SpD[] = _("SPD");
static const u8 sText_Spe[] = _("SPE");
static const u8 sText_Slash[] = _("/");
static const u8 sText_Vs[] = _("VS");
static const u8 sText_VsHint[] = _("{A_BUTTON} BATTLE   {B_BUTTON} REDRAFT");
static const u8 sText_Wins[] = _(" WINS!");
static const u8 sText_Draw[] = _("NO WINNER");
static const u8 sText_ResultHint[] = _("{A_BUTTON} ARENA MENU");
static const u8 sText_DraftHint[] = _("{A_BUTTON}PICK {B_BUTTON}UNDO");
static const u8 sText_Lose[] = _("GOOD GAME!");
static const u8 sText_Empty[] = _("");
static const u8 sText_Close[] = _("CLOSE");
static const u8 sText_Dash[] = _("DASH");
static const u8 sText_Ranged[] = _("RANGED");
static const u8 sText_Beam[] = _("BEAM");
static const u8 sText_Self[] = _("SELF");

// UI palette (BG 15 and OBJ cursor): transparent, panel, white, shadow,
// yellow, red, light blue, green.
static const u16 sUiPalette[16] =
{
    RGB(0, 0, 0), RGB(4, 5, 9), RGB(31, 31, 31), RGB(9, 10, 14),
    RGB(31, 27, 8), RGB(31, 10, 8), RGB(16, 24, 31), RGB(12, 28, 12),
    RGB(2, 3, 6), RGB(10, 12, 18), RGB(0, 0, 0), RGB(0, 0, 0),
    RGB(0, 0, 0), RGB(0, 0, 0), RGB(0, 0, 0), RGB(0, 0, 0),
};
static const u8 sColorWhite[] = {0, 2, 3};
static const u8 sColorYellow[] = {0, 4, 3};
static const u8 sColorRed[] = {0, 5, 3};
static const u8 sColorBlue[] = {0, 6, 3};
static const u8 sColorGray[] = {0, 9, 3};

#define TAG_CURSOR 0x5A10
#define TAG_STRIP 0x5A11
#define TILE_PANEL 1
#define TILE_GRIDBG 2
#define TILE_VLINE 3
#define TILE_HLINE 4
#define PANEL_PAL 6

static const struct BgTemplate sBgTemplates[] =
{
    {.bg = 0, .charBaseIndex = 2, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
    {.bg = 1, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0},
    {.bg = 2, .charBaseIndex = 1, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0},
};
static const struct WindowTemplate sDraftWindows[] =
{
    [WIN_MAIN] = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 15, .height = 16, .paletteNum = 15, .baseBlock = 1},
    [WIN_CREDITS] = {.bg = 0, .tilemapLeft = 24, .tilemapTop = 16, .width = 6, .height = 4, .paletteNum = 15, .baseBlock = 1 + 15 * 16},
    DUMMY_WIN_TEMPLATE
};
static const struct WindowTemplate sFullWindows[] =
{
    [WIN_MAIN] = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = 20, .paletteNum = 15, .baseBlock = 1},
    DUMMY_WIN_TEMPLATE
};

// ---------------------------------------------------------------- setup

static void VBlankCB_Arena(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_ArenaRun(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static EWRAM_DATA void *sTilemaps[3] = {};
static EWRAM_DATA u32 sPanelTiles[5][8] = {};

// A plain screen: three backgrounds, a text layer, panels from four tiles.
static void SetupScreen(const struct WindowTemplate *windows)
{
    u32 i;
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    for (i = 0; i < 4; i++)
    {
        // The title screen leaves its backgrounds scrolled.
        ChangeBgX(i, 0, BG_COORD_SET);
        ChangeBgY(i, 0, BG_COORD_SET);
    }
    for (i = 0; i < 3; i++)
    {
        sTilemaps[i] = AllocZeroed(BG_SCREEN_SIZE);
        SetBgTilemapBuffer(i, sTilemaps[i]);
    }
    ResetPaletteFade();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetAllPicSprites();
    DeactivateAllTextPrinters();
    InitWindows(windows);
    // Panel tiles: blank, solid panel, darker grid ground, and 1 px lines.
    for (i = 0; i < 8; i++)
    {
        sPanelTiles[0][i] = 0;
        sPanelTiles[1][i] = 0x11111111;
        sPanelTiles[2][i] = 0x22222222;
        sPanelTiles[3][i] = 0x11111113;
        sPanelTiles[4][i] = i ? 0x11111111 : 0x33333333;
    }
    LoadBgTiles(2, sPanelTiles, sizeof(sPanelTiles), 0);
    LoadPalette(sUiPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    LoadPalette(sUiPalette, BG_PLTT_ID(PANEL_PAL), PLTT_SIZE_4BPP);
    gPlttBufferUnfaded[BG_PLTT_ID(PANEL_PAL) + 1] = RGB(4, 5, 9);
    gPlttBufferUnfaded[BG_PLTT_ID(PANEL_PAL) + 2] = RGB(2, 3, 6);
    gPlttBufferUnfaded[BG_PLTT_ID(PANEL_PAL) + 3] = RGB(10, 12, 18);
    gPlttBufferUnfaded[0] = RGB(2, 3, 6);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
}

static void TeardownScreen(void)
{
    u32 i;
    FreeAllWindowBuffers();
    for (i = 0; i < 3; i++)
    {
        UnsetBgTilemapBuffer(i);
        TRY_FREE_AND_SET_NULL(sTilemaps[i]);
    }
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetAllPicSprites();
}

static void FillPanel(u8 left, u8 top, u8 width, u8 height, u16 tile)
{
    FillBgTilemapBufferRect_Palette0(2, tile | (PANEL_PAL << 12), left, top, width, height);
}

static void StartScreen(void)
{
    CopyBgTilemapBufferToVram(0);
    CopyBgTilemapBufferToVram(1);
    CopyBgTilemapBufferToVram(2);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_Arena);
    SetMainCallback2(CB2_ArenaRun);
}

static void Print(u8 window, u8 font, u8 x, u8 y, const u8 *color, const u8 *text)
{
    AddTextPrinterParameterized3(window, font, x, y, color, TEXT_SKIP_DRAW, text);
}

// ---------------------------------------------------------------- menu

static void Task_ArenaMenu(u8 taskId);

static const u8 sText_Easy[] = _("{LEFT_ARROW}EASY{RIGHT_ARROW}");
static const u8 sText_Hard[] = _("{LEFT_ARROW}HARD{RIGHT_ARROW}");

static void DrawMenu(u8 sel)
{
    static const u8 *const options[] = {sText_VsCom, sText_VsP2, sText_Adventure};
    u32 i;
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    Print(WIN_MAIN, FONT_NORMAL, 96, 24, sColorYellow, sText_Arena);
    for (i = 0; i < ARRAY_COUNT(options); i++)
    {
        if (i == sel)
            Print(WIN_MAIN, FONT_NORMAL, 64, 56 + i * 20, sColorYellow, sText_Cursor);
        Print(WIN_MAIN, FONT_NORMAL, 80, 56 + i * 20, i == sel ? sColorWhite : sColorGray, options[i]);
    }
    // COM's draft: EASY picks at random within its budget, HARD for
    // strength and type coverage. LEFT / RIGHT on 1P VS COM.
    Print(WIN_MAIN, FONT_SMALL, 164, 59, sel == 0 ? sColorYellow : sColorGray, sComHard ? sText_Hard : sText_Easy);
    Print(WIN_MAIN, FONT_SMALL, 64, 136, sColorGray, sText_MenuHint);
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
}

void CB2_ArenaTitleMenu(void)
{
    SetupScreen(sFullWindows);
    FillPanel(0, 0, 30, 20, TILE_PANEL);
    gArenaDraftMode = ARENA_DRAFT_NONE;
    gArenaDraftBattle = FALSE;
    DrawMenu(0);
    CreateTask(Task_ArenaMenu, 0);
    PlayBGM(MUS_B_FRONTIER);
    StartScreen();
}

bool8 ArenaDraft_TakeOverMainMenu(void)
{
    if (sAdventureOnce)
    {
        sAdventureOnce = FALSE;
        return FALSE;
    }
    SetMainCallback2(CB2_ArenaTitleMenu);
    return TRUE;
}

static void CB2_StartDraft(void);

static void Task_ArenaMenu(u8 taskId)
{
    s16 *sel = &gTasks[taskId].data[0];
    s16 *leaving = &gTasks[taskId].data[1];
    if (gPaletteFade.active)
        return;
    if (*leaving)
    {
        DestroyTask(taskId);
        TeardownScreen();
        if (*leaving == 1)
            SetMainCallback2(CB2_StartDraft);
        else if (*leaving == 2)
        {
            sAdventureOnce = TRUE;
            FadeOutBGM(4);
            SetMainCallback2(CB2_InitMainMenu);
        }
        else
            SetMainCallback2(CB2_InitTitleScreen);
        return;
    }
    if (JOY_NEW(DPAD_UP) && *sel > 0)
        (*sel)--, PlaySE(SE_SELECT), DrawMenu(*sel);
    else if (JOY_NEW(DPAD_DOWN) && *sel < 2)
        (*sel)++, PlaySE(SE_SELECT), DrawMenu(*sel);
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT) && *sel == 0)
        sComHard ^= TRUE, PlaySE(SE_SELECT), DrawMenu(*sel);
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (*sel == 2)
            *leaving = 2;
        else
        {
            gArenaDraftMode = *sel == 0 ? ARENA_DRAFT_VS_COM : ARENA_DRAFT_VS_P2;
            sTeamCount[0] = sTeamCount[1] = 0;
            *leaving = 1;
        }
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        *leaving = 3;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
}

// ---------------------------------------------------------------- draft data

static u8 TeamCost(u8 player)
{
    u32 i, sum = 0;
    for (i = 0; i < sTeamCount[player]; i++)
        sum += sArenaDraftRoster[sTeams[player][i]].cost;
    return sum;
}

static bool8 InTeam(u8 player, u8 index)
{
    u32 i;
    for (i = 0; i < sTeamCount[player]; i++)
        if (sTeams[player][i] == index)
            return TRUE;
    return FALSE;
}

static bool8 CanPick(u8 player, u8 index)
{
    return sTeamCount[player] < TEAM_MAX && !InTeam(player, index)
        && TeamCost(player) + sArenaDraftRoster[index].cost <= CREDITS;
}

static bool8 AnyPick(u8 player)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sArenaDraftRoster); i++)
        if (CanPick(player, i))
            return TRUE;
    return FALSE;
}

static u16 BaseTotal(u16 species)
{
    const struct SpeciesInfo *i = &gSpeciesInfo[species];
    return i->baseHP + i->baseAttack + i->baseDefense + i->baseSpAttack + i->baseSpDefense + i->baseSpeed;
}

// HARD: every pick weighs strength (base stats), fit to what is left per
// open slot, and coverage (a new type for the team, attacks of types the
// team can't hit with yet), with a little randomness.
static void DraftComHard(u8 player)
{
    u32 i, k, attackTypes, teamTypes;
    s32 left, target, score, best, pick;
    sTeamCount[player] = 0;
    while (sTeamCount[player] < TEAM_MAX)
    {
        left = CREDITS - TeamCost(player);
        target = left / (TEAM_MAX - sTeamCount[player]);
        attackTypes = teamTypes = 0;
        for (i = 0; i < sTeamCount[player]; i++)
        {
            const struct ArenaDraftEntry *e = &sArenaDraftRoster[sTeams[player][i]];
            teamTypes |= (1 << gSpeciesInfo[e->species].types[0]) | (1 << gSpeciesInfo[e->species].types[1]);
            for (k = 0; k < MAX_MON_MOVES; k++)
                if (gBattleMoves[e->moves[k]].power)
                    attackTypes |= 1 << gBattleMoves[e->moves[k]].type;
        }
        best = -0x7FFFFFFF;
        pick = -1;
        for (i = 0; i < ARRAY_COUNT(sArenaDraftRoster); i++)
        {
            const struct ArenaDraftEntry *e = &sArenaDraftRoster[i];
            s32 cost = e->cost, miss = cost > target ? cost - target : target - cost;
            if (!CanPick(player, i))
                continue;
            // Leave at least 2 credits for every slot still open after this one.
            if (left - cost < 2 * (TEAM_MAX - sTeamCount[player] - 1))
                continue;
            score = BaseTotal(e->species) - miss * 13 + Random() % 40;
            if (!(teamTypes & (1 << gSpeciesInfo[e->species].types[0])))
                score += 45;
            for (k = 0; k < MAX_MON_MOVES; k++)
                if (gBattleMoves[e->moves[k]].power && !(attackTypes & (1 << gBattleMoves[e->moves[k]].type)))
                    score += 12;
            if (score > best)
                best = score, pick = i;
        }
        if (pick < 0)
            break;
        sTeams[player][sTeamCount[player]++] = pick;
    }
}

// COM spends its credits evenly: each pick is near what is left per open
// slot, at random, and never a species twice.
static void DraftCom(u8 player)
{
    u8 pool[ARRAY_COUNT(sArenaDraftRoster)];
    u32 i, n;
    s32 left, target;
    sTeamCount[player] = 0;
    while (sTeamCount[player] < TEAM_MAX)
    {
        left = CREDITS - TeamCost(player);
        target = left / (TEAM_MAX - sTeamCount[player]);
        for (n = 0, i = 0; i < ARRAY_COUNT(sArenaDraftRoster); i++)
            if (CanPick(player, i) && sArenaDraftRoster[i].cost >= target - 4 && sArenaDraftRoster[i].cost <= target + 3)
                pool[n++] = i;
        if (!n)
            for (i = 0; i < ARRAY_COUNT(sArenaDraftRoster); i++)
                if (CanPick(player, i))
                    pool[n++] = i;
        if (!n)
            break;
        sTeams[player][sTeamCount[player]++] = pool[Random() % n];
    }
}

// ---------------------------------------------------------------- grid minis

// The READY button: a 24x24 cell with a check mark and READY in a 3x5
// font, green once the team has a Pokémon, grey before.
static const u16 sReadyGlyphs[] = {0x6BAD, 0x79A7, 0x2BED, 0x6B6E, 0x5A92};   // R E A D Y

static void ReadyPixel(u32 *tiles, s32 x, s32 y, u8 color)
{
    u8 *b = (u8 *)tiles + ((y >> 3) * 3 + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1);
    *b = (x & 1) ? (*b & 0x0F) | (color << 4) : (*b & 0xF0) | color;
}

static void ReadyTiles(u32 *tiles, bool8 on)
{
    s32 x, y, i;
    u8 fill = on ? 7 : 9, ink = 8, edge = on ? 4 : 3;
    for (y = 0; y < 24; y++)
        for (x = 0; x < 24; x++)
            ReadyPixel(tiles, x, y, (x == 0 || y == 0 || x == 23 || y == 23) ? edge : fill);
    // Check mark: a short stroke down-right, then a long one up-right, 2 px thick.
    for (i = 0; i < 4; i++)
    {
        ReadyPixel(tiles, 6 + i, 7 + i, ink);
        ReadyPixel(tiles, 6 + i, 8 + i, ink);
    }
    for (i = 0; i < 8; i++)
    {
        ReadyPixel(tiles, 10 + i, 10 - i, ink);
        ReadyPixel(tiles, 10 + i, 11 - i, ink);
    }
    for (i = 0; i < 5; i++)
        for (y = 0; y < 5; y++)
            for (x = 0; x < 3; x++)
                if (sReadyGlyphs[i] & (1 << (14 - y * 3 - x)))
                    ReadyPixel(tiles, 3 + i * 4 + x, 15 + y, ink);
}

// Visible rows into the canvas (15x16 tiles on BG1, right half), each
// portrait its own 3x3 tiles with its icon palette, greyed when out of reach.
static void DrawGrid(void)
{
    u16 *map = sTilemaps[1];
    u32 r, c, t, i, tile;
    u8 pal;
    memset(sDraft->canvas, 0, sizeof(sDraft->canvas));
    for (r = 0; r < 16; r++)
        for (c = 0; c < 15; c++)
            map[r * 32 + 15 + c] = 0;
    for (r = 0; r < GRID_VISIBLE; r++)
        for (c = 0; c < GRID_COLS; c++)
        {
            i = (sDraft->top + r) * GRID_COLS + c;
            if (i >= GRID_CELLS)
                continue;
            if (i == CELL_READY)
            {
                ReadyTiles(sDraft->ready, sTeamCount[sDraft->player] != 0);
                pal = PANEL_PAL;
            }
            else
                pal = sArenaDraftMiniPal[CELL_MON(i)] + (CanPick(sDraft->player, CELL_MON(i)) ? 0 : 3);
            for (t = 0; t < 9; t++)
            {
                tile = (r * 3 + t / 3) * 15 + c * 3 + t % 3;
                memcpy(&sDraft->canvas[tile * 32], i == CELL_READY ? (const u8 *)sDraft->ready + t * 32 : &sArenaDraftMinis[CELL_MON(i)][t * 32], 32);
                map[(r * 3 + t / 3) * 32 + 15 + c * 3 + t % 3] = (1 + tile) | (pal << 12);
            }
        }
    LoadBgTiles(1, sDraft->canvas, sizeof(sDraft->canvas), 1);
    CopyBgTilemapBufferToVram(1);
}

static void BuildGrid(void)
{
    u32 i, y;
    u16 *pal;
    // Icon palettes 0-2 as they are; 3-5 greyed out for Pokémon out of reach.
    for (i = 0; i < 3; i++)
    {
        LoadPalette(gMonIconPaletteTable[i].data, BG_PLTT_ID(i), PLTT_SIZE_4BPP);
        LoadPalette(gMonIconPaletteTable[i].data, BG_PLTT_ID(i + 3), PLTT_SIZE_4BPP);
        pal = &gPlttBufferUnfaded[BG_PLTT_ID(i + 3)];
        for (y = 1; y < 16; y++)
        {
            u16 c = pal[y];
            u32 l = ((c & 31) * 2 + ((c >> 5) & 31) * 3 + ((c >> 10) & 31)) / 6;
            l = l / 3 + 3;
            pal[y] = RGB(l, l, l + 1);
        }
    }
    FillPanel(15, 0, 15, 16, TILE_GRIDBG);
}

// ---------------------------------------------------------------- draft view

static const struct OamData sOamCursor = {.shape = SPRITE_SHAPE(32x32), .size = SPRITE_SIZE(32x32), .priority = 0};
static const struct OamData sOamStrip = {.shape = SPRITE_SHAPE(32x32), .size = SPRITE_SIZE(32x32), .priority = 0};
static EWRAM_DATA u32 sCursorTiles[8 * 16] = {};
static EWRAM_DATA u32 sStripTiles[8 * 16] = {};

static void SetPixel(u32 *tiles, u8 widthTiles, s32 x, s32 y, u8 color)
{
    u8 *b = (u8 *)tiles + ((y >> 3) * widthTiles + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1);
    *b = (x & 1) ? (*b & 0x0F) | (color << 4) : (*b & 0xF0) | color;
}

static void DrawBox(u32 *tiles, u8 widthTiles, s32 size)
{
    s32 i;
    for (i = 0; i < size; i++)
    {
        SetPixel(tiles, widthTiles, i, 0, 4);
        SetPixel(tiles, widthTiles, i, size - 1, 4);
        SetPixel(tiles, widthTiles, 0, i, 4);
        SetPixel(tiles, widthTiles, size - 1, i, 4);
    }
}

static void CreateCursors(void)
{
    struct SpriteSheet sheet;
    struct SpritePalette pal = {sUiPalette, TAG_CURSOR};
    struct SpriteTemplate t = {TAG_CURSOR, TAG_CURSOR, &sOamCursor, gDummySpriteAnimTable, NULL, gDummySpriteAffineAnimTable, SpriteCallbackDummy};
    memset(sCursorTiles, 0, sizeof(sCursorTiles));
    memset(sStripTiles, 0, sizeof(sStripTiles));
    DrawBox(sCursorTiles, 4, 26);
    DrawBox(sStripTiles, 4, 32);
    sheet.data = sCursorTiles; sheet.size = sizeof(sCursorTiles); sheet.tag = TAG_CURSOR;
    LoadSpriteSheet(&sheet);
    sheet.data = sStripTiles; sheet.size = sizeof(sStripTiles); sheet.tag = TAG_STRIP;
    LoadSpriteSheet(&sheet);
    LoadSpritePalette(&pal);
    sDraft->cursorSprite = CreateSprite(&t, 0, 0, 0);
    t.tileTag = TAG_STRIP;
    t.oam = &sOamStrip;
    sDraft->stripSprite = CreateSprite(&t, 0, 0, 0);
}

static void PlaceCursors(void)
{
    struct Sprite *c = &gSprites[sDraft->cursorSprite], *s = &gSprites[sDraft->stripSprite];
    c->invisible = sDraft->focus != 0;
    s->invisible = sDraft->focus == 0 || sDraft->strip == STRIP_DONE;
    c->x = 120 + (sDraft->cursor % GRID_COLS) * CELL - 1 + 16;
    c->y = (sDraft->cursor / GRID_COLS - sDraft->top) * CELL + 4 - 1 + 16;
    s->x = sDraft->strip * 32 + 16;
    s->y = 144;
}

static const u8 *KindText(u16 move)
{
    const struct ArenaMoveProfile *p = ArenaMoves_Get(move);
    if (!p)
        return sText_Empty;
    switch (p->kind)
    {
    case ARENA_MOVE_MELEE: return sText_Close;
    case ARENA_MOVE_RUSH: return sText_Dash;
    case ARENA_MOVE_PROJECTILE: return sText_Ranged;
    case ARENA_MOVE_CONE: return sText_Beam;
    default: return sText_Self;
    }
}

static const u8 sText_Total[] = _("TOTAL");

static const u8 sText_ReadyTitle[] = _("READY");
static const u8 sText_ReadyTeam[] = _("POKéMON IN TEAM");
static const u8 sText_ReadySpent[] = _("CREDITS SPENT");
static const u8 sText_ReadyGo[] = _("{A_BUTTON} TO FINISH THE DRAFT");
static const u8 sText_ReadyEmpty[] = _("PICK A POKéMON FIRST");

static void DrawReadyInfo(void)
{
    u8 buf[8];
    u32 i;
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    Print(WIN_MAIN, FONT_NORMAL, 2, 0, sColorYellow, sText_ReadyTitle);
    Print(WIN_MAIN, FONT_SMALL, 2, 20, sColorGray, sText_ReadyTeam);
    ConvertIntToDecimalStringN(buf, sTeamCount[sDraft->player], STR_CONV_MODE_LEFT_ALIGN, 1);
    Print(WIN_MAIN, FONT_SMALL, 90, 20, sColorWhite, buf);
    Print(WIN_MAIN, FONT_SMALL, 2, 31, sColorGray, sText_ReadySpent);
    ConvertIntToDecimalStringN(buf, TeamCost(sDraft->player), STR_CONV_MODE_LEFT_ALIGN, 3);
    Print(WIN_MAIN, FONT_SMALL, 90, 31, sColorWhite, buf);
    for (i = 0; i < sTeamCount[sDraft->player]; i++)
        Print(WIN_MAIN, FONT_SMALL, 2 + (i % 2) * 58, 46 + (i / 2) * 11, sColorWhite,
              gSpeciesNames[sArenaDraftRoster[sTeams[sDraft->player][i]].species]);
    Print(WIN_MAIN, FONT_SMALL, 2, 112, sTeamCount[sDraft->player] ? sColorYellow : sColorRed,
          sTeamCount[sDraft->player] ? sText_ReadyGo : sText_ReadyEmpty);
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
}

static void DrawInfo(void)
{
    const struct ArenaDraftEntry *e = &sArenaDraftRoster[CELL_MON(sDraft->cursor)];
    const struct SpeciesInfo *info = &gSpeciesInfo[e->species];
    static const u8 *const statNames[] = {sText_Hp, sText_Atk, sText_Def, sText_SpA, sText_SpD, sText_Spe};
    u8 stats[6];
    u8 buf[32], *p;
    u32 i, total = 0, width;
    if (sDraft->cursor == CELL_READY)
    {
        DrawReadyInfo();
        return;
    }
    stats[0] = info->baseHP; stats[1] = info->baseAttack; stats[2] = info->baseDefense;
    stats[3] = info->baseSpAttack; stats[4] = info->baseSpDefense; stats[5] = info->baseSpeed;
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    // The font has no pound sign: draw a 5x7 one.
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(9), 3, 3, 1, 7);
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(9), 5, 3, 1, 7);
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(9), 2, 5, 5, 1);
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(9), 2, 8, 5, 1);
    ConvertIntToDecimalStringN(buf, SpeciesToNationalPokedexNum(e->species), STR_CONV_MODE_LEADING_ZEROS, 3);
    Print(WIN_MAIN, FONT_SMALL, 8, 0, sColorGray, buf);
    Print(WIN_MAIN, FONT_SMALL, 28, 0, sColorWhite, gSpeciesNames[e->species]);
    p = ConvertIntToDecimalStringN(buf, e->cost, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringCopy(p, sText_Cr);
    Print(WIN_MAIN, FONT_SMALL, 94, 0, CanPick(sDraft->player, CELL_MON(sDraft->cursor)) ? sColorYellow : sColorRed, buf);
    p = StringCopy(buf, gTypeNames[info->types[0]]);
    if (info->types[1] != info->types[0])
    {
        p = StringCopy(p, sText_Slash);
        StringCopy(p, gTypeNames[info->types[1]]);
    }
    Print(WIN_MAIN, FONT_SMALL, 2, 11, sColorBlue, buf);
    for (i = 0; i < 6; i++)
    {
        total += stats[i];
        Print(WIN_MAIN, FONT_SMALL, 2, 23 + i * 9, sColorGray, statNames[i]);
        ConvertIntToDecimalStringN(buf, stats[i], STR_CONV_MODE_RIGHT_ALIGN, 3);
        Print(WIN_MAIN, FONT_SMALL, 22, 23 + i * 9, sColorWhite, buf);
        width = stats[i] * 64 / 180;
        if (width > 72) width = 72;
        FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(9), 42, 27 + i * 9, 72, 3);
        FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(stats[i] >= 100 ? 7 : stats[i] >= 70 ? 4 : 5), 42, 27 + i * 9, width, 3);
    }
    if (sDraft->message)
    {
        static const u8 *const msgs[] = {NULL, sText_Full, sText_TooCostly, sText_Twice};
        Print(WIN_MAIN, FONT_SMALL, 2, 78, sColorRed, msgs[sDraft->message]);
    }
    else
    {
        Print(WIN_MAIN, FONT_SMALL, 2, 78, sColorGray, sText_Total);
        ConvertIntToDecimalStringN(buf, total, STR_CONV_MODE_LEFT_ALIGN, 3);
        Print(WIN_MAIN, FONT_SMALL, 34, 78, sColorWhite, buf);
    }
    for (i = 0; i < 4; i++)
    {
        if (e->moves[i] == MOVE_NONE)
            continue;
        Print(WIN_MAIN, FONT_SMALL, 2, 90 + i * 9, sColorWhite, gMoveNames[e->moves[i]]);
        Print(WIN_MAIN, FONT_SMALL, 84, 90 + i * 9, sColorGray, KindText(e->moves[i]));
    }
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
}

static void DrawCredits(void)
{
    u8 buf[16], *p;
    FillWindowPixelBuffer(WIN_CREDITS, PIXEL_FILL(0));
    Print(WIN_CREDITS, FONT_SMALL, 2, 0, sColorYellow,
          sDraft->player ? sText_P2 : sText_P1);
    p = ConvertIntToDecimalStringN(buf, CREDITS - TeamCost(sDraft->player), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(p, sText_Cr);
    Print(WIN_CREDITS, FONT_SMALL, 2, 10, sColorWhite, buf);
    Print(WIN_CREDITS, FONT_SMALL, 2, 20, sDraft->focus && sDraft->strip == STRIP_DONE ? sColorYellow : sColorGray, sText_Done);
    PutWindowTilemap(WIN_CREDITS);
    CopyWindowToVram(WIN_CREDITS, COPYWIN_FULL);
}

static void DrawTeamIcons(u8 player, u8 row, s16 y)
{
    u32 i;
    for (i = 0; i < TEAM_MAX; i++)
    {
        if (sDraft->iconSprites[row][i] != 0xFF)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sDraft->iconSprites[row][i]]);
            sDraft->iconSprites[row][i] = 0xFF;
        }
        if (i < sTeamCount[player])
            sDraft->iconSprites[row][i] = CreateMonIcon(sArenaDraftRoster[sTeams[player][i]].species,
                                                         SpriteCB_MonIcon, i * 32 + 16, y, 1, 0, TRUE);
    }
}

static void RefreshDraft(void)
{
    DrawGrid();
    DrawInfo();
    DrawCredits();
    DrawTeamIcons(sDraft->player, 0, 144);
    PlaceCursors();
}

static void Task_Draft(u8 taskId);

static void CB2_StartDraftFor(u8 player)
{
    u32 i;
    SetupScreen(sDraftWindows);
    if (!sDraft)
        sDraft = AllocZeroed(sizeof(*sDraft));
    sDraft->player = player;
    sDraft->cursor = 0;
    sDraft->top = 0;
    sDraft->focus = 0;
    sDraft->strip = 0;
    sDraft->message = MSG_NONE;
    for (i = 0; i < TEAM_MAX; i++)
        sDraft->iconSprites[0][i] = sDraft->iconSprites[1][i] = 0xFF;
    FillPanel(0, 0, 15, 16, TILE_PANEL);
    FillPanel(0, 16, 30, 4, TILE_PANEL);
    FillPanel(0, 16, 30, 1, TILE_HLINE);
    for (i = 1; i < 7; i++)
        FillPanel(i * 4, 16, 1, 4, TILE_VLINE);
    BuildGrid();
    // The portraits sit 4 px below the top edge.
    ChangeBgY(1, -(4 << 8), BG_COORD_SET);
    LoadMonIconPalettes();
    CreateCursors();
    RefreshDraft();
    CreateTask(Task_Draft, 0);
    StartScreen();
}

static void CB2_StartDraft(void)
{
    CB2_StartDraftFor(0);
}

static void CB2_StartDraftP2(void)
{
    CB2_StartDraftFor(1);
}

static void CB2_ShowVersus(void);

static void MoveGrid(s32 dx, s32 dy)
{
    s32 col = sDraft->cursor % GRID_COLS, row = sDraft->cursor / GRID_COLS;
    col = (col + dx + GRID_COLS) % GRID_COLS;
    row += dy;
    if (row >= GRID_ROWS || (row == GRID_ROWS - 1 && row * GRID_COLS + col >= GRID_CELLS))
    {
        sDraft->focus = 1;
        sDraft->strip = col * 7 / GRID_COLS;
        return;
    }
    if (row < 0)
        row = 0;
    sDraft->cursor = row * GRID_COLS + col;
    if (row < sDraft->top)
        sDraft->top = row;
    else if (row >= sDraft->top + GRID_VISIBLE)
        sDraft->top = row - GRID_VISIBLE + 1;
}

static void TryPick(u8 player, u8 mon)
{
    if (sTeamCount[player] >= TEAM_MAX) sDraft->message = MSG_FULL;
    else if (InTeam(player, mon)) sDraft->message = MSG_TWICE;
    else if (!CanPick(player, mon)) sDraft->message = MSG_COST;
    else
    {
        sTeams[player][sTeamCount[player]++] = mon;
        PlayCry_Normal(sArenaDraftRoster[mon].species, 0);
        // Nothing left to pick (a full team, or no credits for anyone
        // else): straight to DONE.
        if (!AnyPick(player))
        {
            sDraft->focus = 1;
            sDraft->strip = STRIP_DONE;
        }
    }
    if (sDraft->message)
        PlaySE(SE_FAILURE);
}

static void ShowCell(u8 cell)
{
    u8 row = cell / GRID_COLS;
    sDraft->focus = 0;
    sDraft->cursor = cell;
    if (row < sDraft->top)
        sDraft->top = row;
    else if (row >= sDraft->top + GRID_VISIBLE)
        sDraft->top = row - GRID_VISIBLE + 1;
}

// A request from the voice agent: pick, remove or show a Pokémon, or finish.
static bool8 DraftVoice(u8 player, s16 *leaving)
{
    u8 op = gArenaDraftVoice[0], mon = gArenaDraftVoice[1];
    u32 i;
    gArenaDraftVoice[0] = ARENA_VOICE_NONE;
    sDraft->message = MSG_NONE;
    if (op == ARENA_VOICE_DONE)
    {
        if (sTeamCount[player])
        {
            PlaySE(SE_SELECT);
            *leaving = 1;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            return FALSE;
        }
        PlaySE(SE_FAILURE);
        return TRUE;
    }
    if (mon >= ARRAY_COUNT(sArenaDraftRoster))
        return FALSE;
    if (op == ARENA_VOICE_PICK || op == ARENA_VOICE_SHOW)
        ShowCell(mon + 1);
    if (op == ARENA_VOICE_PICK)
        TryPick(player, mon);
    else if (op == ARENA_VOICE_REMOVE)
    {
        for (i = 0; i < sTeamCount[player]; i++)
            if (sTeams[player][i] == mon)
                break;
        if (i == sTeamCount[player])
        {
            PlaySE(SE_FAILURE);
            return TRUE;
        }
        for (; i + 1 < sTeamCount[player]; i++)
            sTeams[player][i] = sTeams[player][i + 1];
        sTeamCount[player]--;
        PlaySE(SE_BALL_OPEN);
    }
    return TRUE;
}

static void Task_Draft(u8 taskId)
{
    s16 *leaving = &gTasks[taskId].data[0];
    u8 player = sDraft ? sDraft->player : 0;
    bool8 changed = FALSE;
    if (gPaletteFade.active)
        return;
    if (*leaving)
    {
        DestroyTask(taskId);
        TeardownScreen();
        gArenaDraftScreen = 0;
        if (*leaving == 2)
            SetMainCallback2(CB2_ArenaTitleMenu);
        else if (*leaving == 3)
            SetMainCallback2(CB2_StartDraft);     // Player 2 backs out to Player 1's picks
        else if (gArenaDraftMode == ARENA_DRAFT_VS_P2 && player == 0)
            SetMainCallback2(CB2_StartDraftP2);
        else
        {
            if (gArenaDraftMode == ARENA_DRAFT_VS_COM)
            {
                if (sComHard)
                    DraftComHard(1);
                else
                    DraftCom(1);
            }
            FREE_AND_SET_NULL(sDraft);
            SetMainCallback2(CB2_ShowVersus);
        }
        return;
    }
    gArenaDraftScreen = 1 + player;
    if (gArenaDraftVoice[0] && DraftVoice(player, leaving))
        changed = TRUE;
    else if (*leaving)
        return;
    else if (JOY_NEW(DPAD_ANY))
    {
        sDraft->message = MSG_NONE;
        if (!sDraft->focus)
        {
            if (JOY_NEW(DPAD_LEFT)) MoveGrid(-1, 0);
            else if (JOY_NEW(DPAD_RIGHT)) MoveGrid(1, 0);
            else if (JOY_NEW(DPAD_UP)) MoveGrid(0, -1);
            else if (JOY_NEW(DPAD_DOWN)) MoveGrid(0, 1);
        }
        else
        {
            if (JOY_NEW(DPAD_LEFT) && sDraft->strip > 0) sDraft->strip--;
            else if (JOY_NEW(DPAD_RIGHT) && sDraft->strip < STRIP_DONE) sDraft->strip++;
            else if (JOY_NEW(DPAD_UP))
            {
                sDraft->focus = 0;
                sDraft->cursor = min(GRID_CELLS - 1, (sDraft->top + GRID_VISIBLE - 1) * GRID_COLS + (sDraft->strip * GRID_COLS / 7));
            }
        }
        PlaySE(SE_SELECT);
        changed = TRUE;
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (!sDraft->focus && sDraft->cursor != CELL_READY)
            TryPick(player, CELL_MON(sDraft->cursor));
        else if (!sDraft->focus)
        {
            // READY takes the cursor to DONE; DONE (A or START) finishes.
            sDraft->focus = 1;
            sDraft->strip = STRIP_DONE;
            PlaySE(SE_SELECT);
        }
        else if (sDraft->strip == STRIP_DONE)
        {
            if (sTeamCount[player])
            {
                PlaySE(SE_SELECT);
                *leaving = 1;
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            }
            else
                PlaySE(SE_FAILURE);
        }
        else if (sDraft->strip < sTeamCount[player])
        {
            u32 i;
            for (i = sDraft->strip; i + 1 < sTeamCount[player]; i++)
                sTeams[player][i] = sTeams[player][i + 1];
            sTeamCount[player]--;
            PlaySE(SE_BALL_OPEN);
        }
        changed = TRUE;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sTeamCount[player])
        {
            sTeamCount[player]--;
            PlaySE(SE_BALL_OPEN);
            changed = TRUE;
        }
        else
        {
            PlaySE(SE_SELECT);
            *leaving = player ? 3 : 2;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        }
    }
    else if (JOY_NEW(START_BUTTON) && sTeamCount[player])
    {
        PlaySE(SE_SELECT);
        *leaving = 1;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
    if (changed && !*leaving)
        RefreshDraft();
}

// ---------------------------------------------------------------- versus

static void Task_Versus(u8 taskId);
static void StartBattle(void);

// The versus screen. In 1P VS 1P with the voice agent running, a modal first
// asks each player to say one sentence (the agent tells the voices apart from
// these), shows what it heard, and offers READY or GO BACK (record again).
static EWRAM_DATA u8 sVsIcons[2 * TEAM_MAX] = {};
static EWRAM_DATA u8 sVsIconCount = 0;
EWRAM_DATA u8 gArenaVoiceHeard[2][24] = {};
EWRAM_DATA u8 gArenaVoiceHeardSeq = 0;

EWRAM_DATA u8 gArenaVoiceRetry = 0;

static const u8 sText_VoiceTitle[] = _("SET EACH PLAYER'S VOICE");
static const u8 sText_VoiceHelp[] = _("Read this aloud, PLAYER 1 then PLAYER 2:");
// Kept in sync with ENROLL_SENTENCE in voice/emerald_voice/voice.py. Long
// (3-4 s) and full of different sounds, with words said in battle.
static const u8 sText_Sentence1[] = _("“Quick, Pikachu! Dodge left, jump up, then");
static const u8 sText_Sentence2[] = _("fire a Thunderbolt at the huge Gyarados!”");
static const u8 sText_RetrySentence[] = _("Not the sentence, read it again. Heard:");
static const u8 sText_RetrySame[] = _("Sounded like PLAYER 1. PLAYER 2, read it:");
static const u8 sText_VoiceP1[] = _("PLAYER 1");
static const u8 sText_VoiceP2[] = _("PLAYER 2");
static const u8 sText_SpeakNow[] = _("SPEAK NOW…");
static const u8 sText_Waiting[] = _("NEXT");
static const u8 sText_VoiceSet[] = _("SET");
static const u8 sText_Restarting[] = _("STARTING OVER…");
static const u8 sText_OpenQuote[] = _("“");
static const u8 sText_CloseQuote[] = _("”");
static const u8 sText_VoiceKeys[] = _("{A_BUTTON} READY   {B_BUTTON} GO BACK (READ IT AGAIN)");

static bool8 VoiceModalActive(void)
{
    return gArenaDraftMode == ARENA_DRAFT_VS_P2 && gArenaVoiceEnroll && gArenaVoiceEnroll != 4;
}

static void DrawVersus(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    Print(WIN_MAIN, FONT_NORMAL, 8, 4, sColorYellow, sText_P1);
    Print(WIN_MAIN, FONT_NORMAL, 112, 68, sColorWhite, sText_Vs);
    Print(WIN_MAIN, FONT_NORMAL, 8, 84, sColorYellow, gArenaDraftMode == ARENA_DRAFT_VS_COM ? sText_Com : sText_P2);
    Print(WIN_MAIN, FONT_SMALL, 56, 146, sColorGray, sText_VsHint);
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
    for (i = 0; i < sVsIconCount; i++)
        gSprites[sVsIcons[i]].invisible = FALSE;
}

static void DrawVoiceRow(u8 player, u8 y)
{
    u8 buf[32], *p;
    u8 state = gArenaVoiceEnroll;
    bool8 done = state == 3 || (player == 0 && state == 2);
    bool8 current = state == player + 1;
    Print(WIN_MAIN, FONT_SMALL, 20, y, sColorYellow, player ? sText_VoiceP2 : sText_VoiceP1);
    if (state == 5)
        Print(WIN_MAIN, FONT_SMALL, 70, y, sColorGray, sText_Restarting);
    else if (done)
        Print(WIN_MAIN, FONT_SMALL, 70, y, sColorBlue, sText_VoiceSet);
    else if (current)
        Print(WIN_MAIN, FONT_SMALL, 70, y, sColorWhite, sText_SpeakNow);
    else
        Print(WIN_MAIN, FONT_SMALL, 70, y, sColorGray, sText_Waiting);
    // What was heard, once accepted (a refused reading shows below).
    if (done && gArenaVoiceHeard[player][0] != EOS)
    {
        p = StringCopy(buf, sText_OpenQuote);
        p = StringCopy(p, gArenaVoiceHeard[player]);
        StringCopy(p, sText_CloseQuote);
        Print(WIN_MAIN, FONT_SMALL, 96, y, sColorWhite, buf);
    }
}

static void DrawVoiceModal(void)
{
    u32 i;
    for (i = 0; i < sVsIconCount; i++)
        gSprites[sVsIcons[i]].invisible = TRUE;
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(4), 8, 12, 224, 136);
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(1), 9, 13, 222, 134);
    Print(WIN_MAIN, FONT_NORMAL, 20, 16, sColorYellow, sText_VoiceTitle);
    Print(WIN_MAIN, FONT_SMALL, 20, 31, sColorWhite, sText_VoiceHelp);
    FillWindowPixelRect(WIN_MAIN, PIXEL_FILL(3), 16, 44, 208, 26);
    Print(WIN_MAIN, FONT_SMALL, 20, 45, sColorWhite, sText_Sentence1);
    Print(WIN_MAIN, FONT_SMALL, 26, 57, sColorWhite, sText_Sentence2);
    DrawVoiceRow(0, 78);
    DrawVoiceRow(1, 94);
    if (gArenaVoiceRetry && (gArenaVoiceEnroll == 1 || gArenaVoiceEnroll == 2))
    {
        u8 heard[32], *q;
        u8 who = gArenaVoiceEnroll - 1;
        Print(WIN_MAIN, FONT_SMALL, 20, 108, sColorRed, gArenaVoiceRetry == 2 ? sText_RetrySame : sText_RetrySentence);
        if (gArenaVoiceRetry == 1 && gArenaVoiceHeard[who][0] != EOS)
        {
            q = StringCopy(heard, sText_OpenQuote);
            q = StringCopy(q, gArenaVoiceHeard[who]);
            StringCopy(q, sText_CloseQuote);
            Print(WIN_MAIN, FONT_SMALL, 26, 118, sColorRed, heard);
        }
    }
    Print(WIN_MAIN, FONT_SMALL, 20, 132, gArenaVoiceEnroll == 3 ? sColorYellow : sColorGray, sText_VoiceKeys);
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
}

static void CB2_ShowVersus(void)
{
    u32 i, p;
    SetupScreen(sFullWindows);
    FillPanel(0, 0, 30, 20, TILE_PANEL);
    LoadMonIconPalettes();
    sVsIconCount = 0;
    for (p = 0; p < 2; p++)
        for (i = 0; i < sTeamCount[p]; i++)
            sVsIcons[sVsIconCount++] = CreateMonIcon(sArenaDraftRoster[sTeams[p][i]].species, SpriteCB_MonIcon,
                                                     24 + i * 36, p ? 124 : 44, 1, 0, TRUE);
    DrawVersus();
    CreateTask(Task_Versus, 0);
    StartScreen();
}

static void Task_Versus(u8 taskId)
{
    s16 *leaving = &gTasks[taskId].data[0];
    s16 *shownState = &gTasks[taskId].data[1];
    s16 *shownSeq = &gTasks[taskId].data[2];
    bool8 modal;
    if (gPaletteFade.active)
        return;
    modal = VoiceModalActive();
    if (!*leaving && (*shownState != gArenaVoiceEnroll || *shownSeq != gArenaVoiceHeardSeq))
    {
        *shownState = gArenaVoiceEnroll;
        *shownSeq = gArenaVoiceHeardSeq;
        if (modal)
            DrawVoiceModal();
        else
            DrawVersus();
    }
    if (*leaving)
    {
        DestroyTask(taskId);
        TeardownScreen();
        gArenaDraftScreen = 0;
        gArenaVoiceEnroll = 0;
        gArenaP2Voice[0] = gArenaP2NextMon = gArenaP2SwitchTo = 0;
        if (*leaving == 1)
            StartBattle();
        else
        {
            // REDRAFT goes back to the last draft screen with its picks kept
            // (Player 2's in 1P vs 1P); COM drafts again after DONE.
            SetMainCallback2(gArenaDraftMode == ARENA_DRAFT_VS_P2 ? CB2_StartDraftP2 : CB2_StartDraft);
        }
        return;
    }
    gArenaDraftScreen = 3;
    if (JOY_NEW(A_BUTTON | START_BUTTON) || gArenaDraftVoice[0] == ARENA_VOICE_DONE)
    {
        // READY (or "fight"): to the battle, voices set or not.
        gArenaDraftVoice[0] = ARENA_VOICE_NONE;
        PlaySE(SE_SELECT);
        *leaving = 1;
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
    else if (JOY_NEW(B_BUTTON) && modal)
    {
        // GO BACK: both players say their sentence again.
        PlaySE(SE_SELECT);
        gArenaVoiceHeard[0][0] = gArenaVoiceHeard[1][0] = EOS;
        gArenaVoiceRetry = 0;
        gArenaVoiceEnroll = 5;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        *leaving = 2;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
}

// ---------------------------------------------------------------- battle

static const u8 sText_OpponentDefeat[] = _("That was a great battle!");
static const u8 sText_PlayerName1[] = _("P1");

// Battle text reads "<class> <name>": ARENA COM, or PLAYER 2.
static const u8 sText_ClassArena[] = _("ARENA");
static const u8 sText_ClassPlayer[] = _("PLAYER");
static const u8 sText_Two[] = _("2");

const u8 *ArenaDraft_OpponentName(void)
{
    return gArenaDraftMode == ARENA_DRAFT_VS_P2 ? sText_Two : sText_Com;
}

const u8 *ArenaDraft_OpponentClass(void)
{
    return gArenaDraftMode == ARENA_DRAFT_VS_P2 ? sText_ClassPlayer : sText_ClassArena;
}

static void BuildParty(struct Pokemon *party, u8 player)
{
    u32 i, j;
    for (i = 0; i < sTeamCount[player]; i++)
    {
        const struct ArenaDraftEntry *e = &sArenaDraftRoster[sTeams[player][i]];
        u8 ability = e->abilityNum;
        CreateMon(&party[i], e->species, 100, 31, FALSE, 0, player ? OT_ID_RANDOM_NO_SHINY : OT_ID_PLAYER_ID, 0);
        for (j = 0; j < MAX_MON_MOVES; j++)
            SetMonMoveSlot(&party[i], e->moves[j], j);
        SetMonData(&party[i], MON_DATA_ABILITY_NUM, &ability);
    }
}

static void CB2_ArenaResult(void);

static void StartBattle(void)
{
    static const u8 environments[] = {BATTLE_ENVIRONMENT_GRASS, BATTLE_ENVIRONMENT_WATER, BATTLE_ENVIRONMENT_CAVE,
                                      BATTLE_ENVIRONMENT_SAND, BATTLE_ENVIRONMENT_BUILDING};
    TRY_FREE_AND_SET_NULL(sDraft);
    // The name first: a Pokémon's original trainer is recorded when it is
    // created, and level 100 Pokémon from another trainer disobey.
    StringCopy(gSaveBlock2Ptr->playerName, sText_PlayerName1);
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    BuildParty(gPlayerParty, 0);
    BuildParty(gEnemyParty, 1);
    CalculatePlayerPartyCount();
    CalculateEnemyPartyCount();
    gSaveBlock2Ptr->optionsBattleStyle = OPTIONS_BATTLE_STYLE_SET;
    gArenaDraftBattle = TRUE;
    gArenaP2Keys = 0;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gTrainerBattleOpponent_A = TRAINER_MAY_ROUTE_103_MUDKIP;
    gBattleEnvironment = environments[Random() % ARRAY_COUNT(environments)];
    ArenaDraft_SetOpponentDefeatText(sText_OpponentDefeat);
    gMain.savedCallback = CB2_ArenaResult;
    PlayBGM(MUS_VS_RIVAL);
    SetMainCallback2(CB2_InitBattle);
}

// ---------------------------------------------------------------- result

static void Task_Result(u8 taskId);

static void CB2_ArenaResult(void)
{
    u8 buf[24], *p;
    u8 outcome = gBattleOutcome;
    gArenaDraftBattle = FALSE;
    gArenaP2Keys = 0;
    // Battles write to the save blocks in RAM (names, money, dex flags);
    // reload them so the adventure save is exactly as it was.
    LoadGameSave(SAVE_NORMAL);
    SetupScreen(sFullWindows);
    FillPanel(0, 0, 30, 20, TILE_PANEL);
    FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));
    if (outcome == B_OUTCOME_WON)
        p = StringCopy(buf, sText_P1);
    else if (outcome == B_OUTCOME_LOST)
        p = StringCopy(buf, gArenaDraftMode == ARENA_DRAFT_VS_P2 ? sText_P2 : sText_Com);
    else
        p = NULL;
    if (p)
    {
        StringCopy(p, sText_Wins);
        Print(WIN_MAIN, FONT_NORMAL, 60, 56, sColorYellow, buf);
    }
    else
        Print(WIN_MAIN, FONT_NORMAL, 76, 56, sColorWhite, sText_Draw);
    Print(WIN_MAIN, FONT_SMALL, 88, 84, sColorGray, sText_Lose);
    Print(WIN_MAIN, FONT_SMALL, 80, 140, sColorGray, sText_ResultHint);
    PutWindowTilemap(WIN_MAIN);
    CopyWindowToVram(WIN_MAIN, COPYWIN_FULL);
    CreateTask(Task_Result, 0);
    PlayBGM(MUS_B_FRONTIER);
    StartScreen();
}

static void Task_Result(u8 taskId)
{
    s16 *leaving = &gTasks[taskId].data[0];
    if (gPaletteFade.active)
        return;
    if (*leaving)
    {
        DestroyTask(taskId);
        TeardownScreen();
        SetMainCallback2(CB2_ArenaTitleMenu);
        return;
    }
    if (JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON))
    {
        PlaySE(SE_SELECT);
        *leaving = 1;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }
}
