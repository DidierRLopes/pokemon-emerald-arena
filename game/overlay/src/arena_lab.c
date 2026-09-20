#include "global.h"
#include "arena_lab.h"
#if ARENA_LAB
#include "battle_setup.h"
#include "event_data.h"
#include "fieldmap.h"
#include "field_screen_effect.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon.h"
#include "item.h"
#include "pokedex.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/heal_locations.h"

// Only included in the separate arena_lab.gba development build.
// Commands are consumed in the real overworld, never halfway through a menu.
EWRAM_DATA struct ArenaLabMailbox gArenaLabMailbox = {};
EWRAM_DATA u32 gArenaLabCaptureAudit[10] = {};
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
    case 7:
    {
        // Explicit preparation of a PRIVATE advanced-save COPY, never a
        // release feature or earned-progress assertion. Keep all story/PC
        // progress. The original imported save is retained byte-for-byte.
        static const u16 species[6] = {SPECIES_GROVYLE,SPECIES_SWELLOW,SPECIES_MANECTRIC,
            SPECIES_BRELOOM,SPECIES_PELIPPER,SPECIES_MIGHTYENA};
        static const u8 levels[6] = {33,32,32,31,32,30};
        static const u16 moves[6][4] = {
            {MOVE_LEAF_BLADE,MOVE_ABSORB,MOVE_QUICK_ATTACK,MOVE_PURSUIT},
            {MOVE_WING_ATTACK,MOVE_QUICK_ATTACK,MOVE_PECK,MOVE_DOUBLE_TEAM},
            {MOVE_QUICK_ATTACK,MOVE_SPARK,MOVE_THUNDER_WAVE,MOVE_HOWL},
            {MOVE_MEGA_DRAIN,MOVE_MACH_PUNCH,MOVE_HEADBUTT,MOVE_LEECH_SEED},
            {MOVE_WATER_GUN,MOVE_WING_ATTACK,MOVE_PROTECT,MOVE_MIST},
            {MOVE_BITE,MOVE_TACKLE,MOVE_ODOR_SLEUTH,MOVE_ROAR}
        };
        u32 i,j;
        u8 ability = 1; // Manectric's native Lightning Rod, not disabled Static.
        ZeroPlayerPartyMons();
        gPlayerPartyCount = PARTY_SIZE;
        for(i=0;i<PARTY_SIZE;i++)
        {
            CreateMon(&gPlayerParty[i],species[i],levels[i],20,TRUE,i*2,OT_ID_PLAYER_ID,0);
            for(j=0;j<MAX_MON_MOVES;j++)SetMonMoveSlot(&gPlayerParty[i],moves[i][j],j);
        }
        SetMonData(&gPlayerParty[2],MON_DATA_ABILITY_NUM,&ability);
        FlagClear(FLAG_ARENA_PRACTICE);
        gSaveBlock2Ptr->optionsTextSpeed=OPTIONS_TEXT_SPEED_FAST;
        SetLastHealLocationWarp(HEAL_LOCATION_LILYCOVE_CITY);
        SetWarpDestination(MAP_GROUP(MAP_LILYCOVE_CITY),MAP_NUM(MAP_LILYCOVE_CITY),-1,24,15);
        DoWarp();
        break;
    }
    case 8:
        // Disposable fixture inventory, native bag encryption/stack handling.
        if(gArenaLabMailbox.species>999){gArenaLabMailbox.result=2;break;}
        RemoveBagItem(ITEM_POKE_BALL,CountTotalItemQuantityInBag(ITEM_POKE_BALL));
        if(gArenaLabMailbox.species && !AddBagItem(ITEM_POKE_BALL,gArenaLabMailbox.species))
            gArenaLabMailbox.result=2;
        break;
    case 9:
    {
        // Explicit ALL-FULL private fixture. Never compiled into release.
        u32 box,slot;
        for(box=0;box<TOTAL_BOXES_COUNT;box++)
            for(slot=0;slot<IN_BOX_COUNT;slot++)
                CreateBoxMonAt(box,slot,SPECIES_MAGIKARP,5,20,TRUE,box*30+slot,OT_ID_PLAYER_ID,0);
        break;
    }
    case 10:
    {
        u32 box,slot;
        u16 species=gArenaLabMailbox.species;
        if(!species || species>=NUM_SPECIES){gArenaLabMailbox.result=2;break;}
        memset(gArenaLabCaptureAudit,0,sizeof(gArenaLabCaptureAudit));
        gArenaLabCaptureAudit[0]=CountTotalItemQuantityInBag(ITEM_POKE_BALL);
        gArenaLabCaptureAudit[2]=CalculatePlayerPartyCount();
        gArenaLabCaptureAudit[3]=GetSetPokedexFlag(SpeciesToNationalPokedexNum(species),FLAG_GET_SEEN);
        gArenaLabCaptureAudit[4]=GetGameStat(GAME_STAT_POKEMON_CAPTURES);
        gArenaLabCaptureAudit[5]=GetSetPokedexFlag(SpeciesToNationalPokedexNum(species),FLAG_GET_CAUGHT);
        for(box=0;box<TOTAL_BOXES_COUNT;box++)
            for(slot=0;slot<IN_BOX_COUNT;slot++)
            {
                u16 stored=GetBoxMonDataAt(box,slot,MON_DATA_SPECIES);
                if(stored)gArenaLabCaptureAudit[1]++;
                if(stored==species)
                {
                    gArenaLabCaptureAudit[6]=box*IN_BOX_COUNT+slot+1;
                    gArenaLabCaptureAudit[7]=GetLevelFromBoxMonExp(GetBoxedMonPtr(box,slot));
                    gArenaLabCaptureAudit[8]=GetBoxMonDataAt(box,slot,MON_DATA_POKEBALL);
                    gArenaLabCaptureAudit[9]=GetBoxMonDataAt(box,slot,MON_DATA_SANITY_IS_BAD_EGG);
                }
            }
        break;
    }
    default:
        gArenaLabMailbox.result = 2;
    }
    gArenaLabMailbox.completed++;
}
#endif
