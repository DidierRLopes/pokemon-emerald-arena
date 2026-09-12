#include "global.h"
#include "arena_lab.h"
#if ARENA_LAB
#include "battle_setup.h"
#include "event_data.h"
#include "fieldmap.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "constants/items.h"

// Only included in the separate arena_lab.gba development build.
// Commands are consumed in the real overworld, never halfway through a menu.
EWRAM_DATA struct ArenaLabMailbox gArenaLabMailbox = {};
extern const u8 EventScript_ArenaLabBattle[];

void ArenaLab_Tick(void)
{
    u32 command;
    gArenaLabMailbox.magic = 0x414C4142;
    if (gPaletteFade.active || ArePlayerFieldControlsLocked() || ScriptContext_IsEnabled())
        return;
    command = gArenaLabMailbox.command;
    if (!command) return;
    gArenaLabMailbox.command = 0;
    gArenaLabMailbox.result = 0;
    switch (command)
    {
    case 1:
        if (gArenaLabMailbox.species == 0 || gArenaLabMailbox.species >= NUM_SPECIES
            || gArenaLabMailbox.level == 0 || gArenaLabMailbox.level > MAX_LEVEL)
        {
            gArenaLabMailbox.result = 2;
            break;
        }
        CreateScriptedWildMon(gArenaLabMailbox.species, gArenaLabMailbox.level, ITEM_NONE);
        ScriptContext_SetupScript(EventScript_ArenaLabBattle);
        break;
    case 2:
        // The normal save menu snapshots visible metatiles before writing flash.
        // Omitting this preserves party values but reloads an empty map view.
        SaveMapView();
        gArenaLabMailbox.result = TrySavingData(SAVE_NORMAL);
        break;
    case 3:
        HealPlayerParty();
        break;
    case 4:
        // Explicit, disposable test fixture. Never used to claim earned XP,
        // never available in release, and never while a battle is running.
        if (gArenaLabMailbox.species == 0 || gArenaLabMailbox.species >= NUM_SPECIES
            || gArenaLabMailbox.level == 0 || gArenaLabMailbox.level > MAX_LEVEL)
        {
            gArenaLabMailbox.result = 2;
            break;
        }
        CreateMon(&gPlayerParty[0], gArenaLabMailbox.species, gArenaLabMailbox.level,
            20, TRUE, 0, OT_ID_PLAYER_ID, 0);
        if (!gPlayerPartyCount) gPlayerPartyCount = 1;
        break;
    case 5:
    case 6:
    {
        u32 i;
        if (!gArenaLabMailbox.teamCount || gArenaLabMailbox.teamCount > PARTY_SIZE)
        {gArenaLabMailbox.result = 2; break;}
        for (i = 0; i < gArenaLabMailbox.teamCount; i++)
            if (!gArenaLabMailbox.teamSpecies[i] || gArenaLabMailbox.teamSpecies[i] >= NUM_SPECIES
                || !gArenaLabMailbox.teamLevels[i] || gArenaLabMailbox.teamLevels[i] > MAX_LEVEL)
                break;
        if (i != gArenaLabMailbox.teamCount)
        {gArenaLabMailbox.result = 2; break;}
        if (command == 6)
        {
            // Populate empty slots only; never overwrite existing stored mons.
            for (i = 0; i < gArenaLabMailbox.teamCount; i++)
                if (GetBoxMonDataAt(0, i, MON_DATA_SPECIES)) break;
            if (i != gArenaLabMailbox.teamCount)
            {gArenaLabMailbox.result = 3; break;}
            for (i = 0; i < gArenaLabMailbox.teamCount; i++)
                CreateBoxMonAt(0, i, gArenaLabMailbox.teamSpecies[i], gArenaLabMailbox.teamLevels[i],
                    20, TRUE, i * 2, OT_ID_PLAYER_ID, 0);
            break;
        }
        // Explicitly replaces the DISPOSABLE fixture team. Native CreateMon
        // owns moves, PP, stats, encryption and checksums; never a victory hack.
        ZeroPlayerPartyMons();
        FlagSet(FLAG_ARENA_PRACTICE);
        gPlayerPartyCount = gArenaLabMailbox.teamCount;
        for (i = 0; i < gPlayerPartyCount; i++)
            CreateMon(&gPlayerParty[i], gArenaLabMailbox.teamSpecies[i], gArenaLabMailbox.teamLevels[i],
                20, TRUE, i * 2, OT_ID_PLAYER_ID, 0);
        break;
    }
    default:
        gArenaLabMailbox.result = 2;
    }
    gArenaLabMailbox.completed++;
}
#endif
