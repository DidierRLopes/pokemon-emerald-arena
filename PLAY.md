# Play Emerald Arena

[Download the ZIP](https://github.com/GBurgardt/pokemon-emerald-arena/releases/download/v0.3.2/Emerald-Arena-0.3.2.zip) · [Watch gameplay](https://github.com/GBurgardt/pokemon-emerald-arena#watch-gameplay)

[**How to play · 1 minute video (MP4)**](https://github.com/GBurgardt/pokemon-emerald-arena/releases/download/v0.3.2/emerald-arena-walkthrough.mp4)
From download to your first fight, saving and continuing. GBA button names are
shown in the video; your emulator's keyboard bindings may differ.

## 1. Prepare the game on a computer

You need your own **unmodified Pokémon Emerald (USA/Europe) `.gba`** and Internet
access. Other languages, other Pokémon games and already-patched ROMs will not work.

1. Download **Emerald-Arena-0.3.2.zip** above. Extract/unzip it first.
2. Open **Prepare-Emerald-Arena.html** in a current Chrome, Firefox or Safari.
   It is a local web page, not an app to install. Do not open `install.mjs`.
3. Click **Choose Emerald ROM** and select your original `.gba` file, not a ZIP.
4. Wait while it checks the ROM and prepares the animations. Keep the page open.
5. Click **Download game**. You will get **Emerald-Arena-0.3.1.gba** in your downloads.

Package 0.3.2 improves setup only. It produces the exact same verified game as
0.3.1, so the game filename stays unchanged. Existing players do not need to update.

Your ROM is read locally, never uploaded or overwritten. The setup downloads
animation files from pinned public sources. The finished game works offline.

## 2. Open it in an emulator

Open **Emerald-Arena-0.3.1.gba** using your GBA emulator's **Open / Load game**
command. [mGBA](https://mgba.io/downloads.html) is the desktop emulator used for
testing. If you already have a GBA emulator, you do not need another one.

On a phone or handheld, transfer this finished `.gba`, then import it using that
emulator's game browser. Preparing the ROM directly inside a phone's file preview
is not the supported setup path. Physical GBA hardware has not been verified.

## 3. Start your first fight

1. Start with **no save attached to this game**. Keep your existing saves backed up
   and separate; do not delete them to try this.
2. At the main menu, highlight **NEW GAME** and press the emulator button mapped
   to **SELECT**, not A. This skips the normal introduction and creates a practice team.
3. You arrive in the field with Charizard and five teammates. Press **L and R
   together** to start a practice encounter.
4. Move with the direction buttons, **A** to attack, **B + a direction** to dodge.
   **L / R** changes your selected move. **START** pauses and shows help.

These are **GBA button names**, not literal keyboard keys. In your emulator's
input settings, check which keys correspond to A, B, L, R, START and SELECT.
The movement keys are usually the keyboard arrows. Bind L and R to separate keys
that you can press together. Touch controls show the GBA labels directly.

Save using the game's field menu. Practice encounters consume real HP and PP;
visit a Pokémon Center between fights. Reorder your party to try another lead.
Six additional Pokémon are in Box 1 at the PC.

## Controls

| GBA control | Action |
|---|---|
| Direction buttons | Move in eight directions |
| A | Attack; hold a direction to aim |
| B + a direction | Dodge |
| L / R | Previous / next move |
| START | Pause and help |
| SELECT | Switch to classic battle |
| L + R in the field | Next practice opponent |

Regular saves keep their encounters and do not receive the practice party.

## Something went wrong?

| What you see | What to do |
|---|---|
| The ZIP opens as a list of files | Extract it, then open `Prepare-Emerald-Arena.html`. |
| ROM rejected | Use the original unmodified USA/Europe `.gba`, not the download ZIP or a previous patched game. |
| Setup cannot fetch animations | Check your connection and retry. If it still fails, send the error text below. |
| The button does nothing in a file preview | Open the HTML in an actual desktop browser, with JavaScript enabled. |
| The normal Professor Birch introduction starts | You pressed A on NEW GAME. Restart with a separate fresh save and press SELECT instead. |
| A classic turn-based battle starts | This is the fallback for unsupported moves or encounters. The practice team has supported moves. |
| The game opens but controls do nothing | Check the emulator's input bindings and that the game is not paused. |
| Your save does not appear | Check the emulator's save location and matching ROM/save basenames. Back up before moving anything. |

[Report a problem](https://github.com/GBurgardt/pokemon-emerald-arena/issues/new?template=bug_report.yml).
Include the emulator/version, device, error message and steps. **Do not attach your ROM or save.**

## Current scope

Twelve animated Pokémon and ten adapted move profiles. Wild 1v1 arena encounters;
trainer, double and link battles remain classic. Unsupported effects also fall
back to classic battles. This is not a fully rebalanced adventure.

### Pokémon

Charizard, Blastoise, Eevee, Dragonite, Scizor, Blaziken, Treecko, Poochyena,
Bulbasaur, Squirtle, Grovyle and Sceptile.

### Moves and objects

Ten move profiles: Pound, Tackle, Quick Attack, Leer, Absorb, Water Gun,
Wing Attack, Slam, Peck and Scratch. Unsupported moves use classic battles;
for example, Charizard uses Wing Attack in the arena, not Flamethrower.

Rocks, wood, foliage, crystals and explosive pods can break. Enemies navigate
obstacles, aim and dodge, with level-based reactions.

[Release verification](RELEASE.md) · [Source architecture](HOW-IT-WORKS.md)
