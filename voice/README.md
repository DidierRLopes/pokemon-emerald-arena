# Talk to your Pokémon (local experiment)

Hold **Right Option**, say a command, release. Your Pokémon does it in the
real-time arena of desktop mGBA.

    get closer · go away · stop · go up / left / down right …
    dodge left · dash up · flamethrower · use seismic toss · attack
    switch to Blastoise · come back Charizard, go Dragonite · resume

## How it works

1. **Voice.** Push-to-talk on Right Option, or hands-free with
   `--hands-free`: WebRTC's voice detector cuts speech into utterances (a
   pause of about a third of a second ends one). The audio is transcribed on
   this Mac by Whisper (`mlx-whisper`, small.en by default, about 80 ms),
   primed with your Pokémon's move names and your party's names.
2. **Intent (Jev).** The transcript and the options that are possible right
   now (your moves, the Pokémon you can switch to, movement, dodges, stop) go to
   TypeSafe's Jev as one Choice. Low-confidence answers are ignored and shown.
3. **Paths.** Every walk, approach, retreat or attack follows a path around
   the rocks (`nav.py`, the same visibility-graph A* as the game's own AI,
   re-planned every few frames as the opponent moves). A direction ("go
   right") ends past a rock rather than against it. A move is fired from its
   reach: a close-range blow first runs up to the opponent, a shot or beam
   needs a clear line.
4. **Execution.** `voice/bridge.lua`, loaded in mGBA's scripting window, streams
   the relevant memory every frame and holds the keys the agent asks for, on
   top of your own keyboard. Macros run frame by frame: cycling L/R to a move,
   waiting out the dash cooldown, walking the party menu for a switch.
5. **Switching.** The ROM has a small addition: a switch request the bridge
   can set. It opens the game's own party menu and returns to the real-time
   arena afterwards, in wild battles as in trainer ones (normally SELECT in a
   wild battle makes the rest of it turn-based). After a faint, "Use next
   POKéMON?" is answered for you.

Without an API key (or if TypeSafe is unreachable) built-in rules make the
same decisions, so the game never waits on the network.

## Run it

    ./play-voice-fight.command            # from the repository root

Then in mGBA: **Tools > Scripting…**, and in that window's **File** menu pick
the recent script `emerald-voice-bridge.lua` (once per mGBA launch; mGBA 0.10.5
cannot load scripts by itself). The launcher keeps that copy, in
`~/.config/mgba/scripts`, in sync with `voice/bridge.lua`; it lives outside
`~/Documents` because macOS only lets mGBA read files there that you picked.
The Terminal says **Game connected.** once it is running. The first time, macOS asks
Terminal for the microphone. Options: `--text` to type commands,
`--verbose` to see every control decision, `--no-jev` for rules only,
`--stt-model mlx-community/whisper-small.en-mlx` for faster transcription.

The TypeSafe key is read from `TYPESAFE_API_KEY`, or from the macOS Keychain:

    security add-generic-password -a "$USER" -s emerald-voice-typesafe -w

(`-w` last prompts for the key, so it never lands in your shell history.)

## Setup (already done on this Mac)

    python3 -m venv voice/.venv && voice/.venv/bin/pip install -r voice/requirements.txt
    # ROM: build workspace/pokeemerald.gba from this branch, then
    cp workspace/pokeemerald.gba Emerald-Arena-voice.gba
    arm-none-eabi-nm workspace/pokeemerald.elf > Emerald-Arena-voice.sym

The agent reads names and move profiles from `workspace/` and addresses from
`Emerald-Arena-voice.sym`, so rebuild both together.

## The practice fight

`play-voice-fight.command` loads `~/.config/mgba/states/voice-charizard.ss1`,
and "reset the fight" returns to it (the agent also takes typed commands on
port 7357: `echo "reset the fight" | nc localhost 7357`). Save states are not
kept in Git; rebuild one with `voice/tools/practice_state.py`, with `--dummy`
for a level 70, 420 HP Eevee that hits softly, good for showing moves off.
This branch's practice team: Charizard (Seismic Toss, Flamethrower, Wing
Attack), Mewtwo (Psychic, Shadow Ball, Swift, Teleport), Golem (Rollout,
Defense Curl, Rock Throw, Rock Tomb), Blastoise, Dragonite, Blaziken.

## Arena mode (branch arena-draft)

The title menu becomes **ARENA**: 1P VS COM, 1P VS 1P, or ADVENTURE (the
original game). Each player drafts up to six level-100 Pokémon with 100
credits from the arena's 150 animated Pokémon (costs and movesets from
`tools/arena/draft_data.py`), then the two teams fight a 6v6 real-time
battle. Launch with `./play-arena.command`.

In 1P VS 1P, Player 2 plays on the same keyboard: I J K L move, U attacks,
O dodges, Y / P change move (the agent reads these keys and sends them in
through the bridge; if Player 2 does not move, allow Terminal under System
Settings > Privacy & Security > Input Monitoring). Voice: Right Option is
Player 1 and Left Option is Player 2; run once with `--enroll` (each player
says two sentences) and Right Option works for both, the voice deciding
whose Pokémon obeys. Hands-free (the launcher's default), no enrollment is
needed: the first voice heard in the battle is Player 1 and the first clearly
different one Player 2 (Resemblyzer speaker embeddings, adapted as you play).
Player 1's moves and PP sit top left under its HP bar, and every command
understood appears as a speech bubble bottom left. Player 2's are top right
and bottom right, only when Player 2 is human (nothing is shown for COM).
Switching Pokémon (by voice or SELECT) shows only the party list; the
recall and send-out happen in the arena.

Chains: "retreat and then shadow ball", "get closer, then dragon claw" queue
up to two commands; the second starts when the first is done (a new order
replaces the queue).

The draft works by voice too: "pick Charizard and Gengar", "remove Pikachu",
"show me Dragonite", "done"; on the versus screen, "fight". Draft battles go
straight into the arena: no classic intro, messages or party menu (after a
KO the next Pokémon in draft order is sent out).

Build note: `python3 tools/arena/draft_data.py` in the workspace writes the
draft table and, from the party icons, the grid miniatures
(`src/data/arena_draft_minis.h`, not committed) before `make`.
