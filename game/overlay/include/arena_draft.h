#ifndef GUARD_ARENA_DRAFT_H
#define GUARD_ARENA_DRAFT_H

// Arena mode: a title-screen menu (1P vs COM, 1P vs 1P), a credit-based team
// draft over the arena's 150 animated Pokémon, and a 6-vs-6 real-time battle
// between the two drafted teams at level 100.

#define ARENA_DRAFT_NONE 0
#define ARENA_DRAFT_VS_COM 1
#define ARENA_DRAFT_VS_P2 2

struct ArenaDraftEntry
{
    u16 species;
    u8 cost;
    u8 abilityNum;
    u16 moves[4];
};

// Set while a drafted battle runs: the battle skips building a trainer
// party, names the opponent from the draft, and the arena accepts the
// draft roster's passive abilities.
extern u8 gArenaDraftMode;
extern bool8 gArenaDraftBattle;
// Player 2's GBA-style keys (A=1, B=2, ..., L=512), written each frame by the
// voice agent's bridge in a 1P vs 1P battle.
extern u16 gArenaP2Keys;

void CB2_ArenaTitleMenu(void);
bool8 ArenaDraft_TakeOverMainMenu(void);
const u8 *ArenaDraft_OpponentName(void);
const u8 *ArenaDraft_OpponentClass(void);
// In battle_setup.c: the opponent's line when it loses a drafted battle.
void ArenaDraft_SetOpponentDefeatText(const u8 *text);

#endif // GUARD_ARENA_DRAFT_H
