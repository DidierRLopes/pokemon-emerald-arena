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
// Player 2's selected move slot in a 1P vs 1P battle, for the voice agent.
extern u8 gArenaP2MoveSlot;

// The voice agent's requests on the draft screens: {op, roster index}.
#define ARENA_VOICE_NONE 0
#define ARENA_VOICE_PICK 1
#define ARENA_VOICE_REMOVE 2
#define ARENA_VOICE_DONE 3
#define ARENA_VOICE_SHOW 4
extern u8 gArenaDraftVoice[2];
// 0 elsewhere, 1 or 2 while Player 1 or 2 drafts, 3 on the versus screen.
extern u8 gArenaDraftScreen;
// A line a trainer calls out in the arena, in the game's charset: set the
// text, then bump the sequence number to show it.
extern u8 gArenaToastText[2][28];
extern u8 gArenaToastSeq[2];

// Player 2 (human) switching in a 1P vs 1P battle, from the voice agent:
// {op, party index}: op 1 opens Player 2's chooser, op 2 switches to the index.
#define ARENA_P2_OPEN 1
#define ARENA_P2_SWITCH 2
extern u8 gArenaP2Voice[2];
// Player 2's pick for the next Pokémon (index + 1), for the engine.
extern u8 gArenaP2NextMon;
// A voluntary switch for Player 2 on the way out of the arena (index + 1).
extern u8 gArenaP2SwitchTo;
// Voice setup on the versus screen (a modal): 1/2 waiting for Player 1/2,
// 3 both set, 5 GO BACK pressed (the agent starts over), 0 closed. What the
// agent heard from each player, in the game's charset; bump the sequence.
extern u8 gArenaVoiceEnroll;
extern u8 gArenaVoiceHeard[2][40];
extern u8 gArenaVoiceHeardSeq;
// Why the current player is asked again: 1 not the sentence, 2 sounded like
// Player 1 (Player 2's turn).
extern u8 gArenaVoiceRetry;

// Esc (read by the voice agent): opens RESUME / START OVER on the draft,
// versus and battle screens. START OVER in a battle ends it and returns to
// the ARENA menu without a result screen.
extern u8 gArenaEscRequest;
extern u8 gArenaStartOver;
// In battle_main.c: ends the battle at once (START OVER).
void RealtimeArena_Forfeit(void);

void CB2_ArenaTitleMenu(void);
bool8 ArenaDraft_TakeOverMainMenu(void);
const u8 *ArenaDraft_OpponentName(void);
const u8 *ArenaDraft_OpponentClass(void);
// In battle_setup.c: the opponent's line when it loses a drafted battle.
void ArenaDraft_SetOpponentDefeatText(const u8 *text);

#endif // GUARD_ARENA_DRAFT_H
