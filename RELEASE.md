# Emerald Arena 0.3.3 — web setup fixes

[Download setup 0.3.3](https://github.com/GBurgardt/pokemon-emerald-arena/releases/download/v0.3.3/Emerald-Arena-0.3.3.zip).

The hosted and downloaded preparers now use the same source. Setup shows a
visible loading message, browser guidance and a ZIP alternative even if scripts
cannot start. Animation downloads time out with a retry message instead of
waiting indefinitely. The game and save filename are unchanged from 0.3.1.

## Previous setup 0.3.2

[**Download Emerald Arena — ZIP**](https://github.com/GBurgardt/pokemon-emerald-arena/releases/download/v0.3.2/Emerald-Arena-0.3.2.zip)
· [Installation guide](https://github.com/GBurgardt/pokemon-emerald-arena/blob/main/PLAY.md)
· [Watch gameplay](https://github.com/GBurgardt/pokemon-emerald-arena#watch-gameplay)

**Setup update only. The game is identical to 0.3.1. Existing players do not need to update.**

- Extract the ZIP, open `Prepare-Emerald-Arena.html`, choose your original Emerald
  ROM, then click **Download game**. Open the resulting `.gba` in your emulator.
- Clear next steps for starting the practice team and first fight.
- Browser compatibility checks, visible preparation status and recoverable errors.
- A short player guide, troubleshooting and a structured bug report form.
- `SHA256SUMS.txt` checks the downloaded ZIP. The installer also checks your source
  ROM, every downloaded animation and the final game automatically.

The output remains `Emerald-Arena-0.3.1.gba` to preserve the existing save filename.
Keep backups of your saves. No original ROM, save or sprite sheets are included.

## Game release 0.3.1

### Setup 0.3.2 checks — September 14, 2026

- Ten public installer and player-journey tests pass.
- Real browser preparation with the original ROM: final SHA-256 matches below.
- Fresh release boot: SELECT creates the six-member practice party with Charizard.
- L+R starts a native practice encounter; movement and an attack verified in mGBA.
- No game binary changes. The 211-check acceptance result below belongs to the
  original 0.3.1 game release; it is not a newly claimed full-suite run.

### Original game release notes

Pokémon Emerald with real-time battles. Twelve animated Pokémon, ten move
profiles and breakable objects. Game UI, installer and documentation in English.

**Download the ZIP, open Prepare-Emerald-Arena.html, and choose your Emerald ROM.**
Requires your own unmodified Pokémon Emerald (USA/Europe).

Open the result in a GBA emulator. With no existing save, press SELECT on
NEW GAME for the practice team. Press L+R in the field to fight.

Charizard, Blastoise, Eevee, Dragonite, Scizor and Blaziken start in the party.
Six more Pokémon are in the PC. Save from the field menu to keep your progress.
Moves and effects not yet adapted use classic battles.

## Verification

- 211 real-game checks pass on the matching English lab build.
- Fresh release boot, native practice encounters and classic Run verified.
- Public installer tested with all 47 pinned source downloads.
- Reconstructed output matches the release byte for byte.
- BPS checksums, PNG conversion and wrong-ROM rejection tested separately.

Release SHA-256: `d357b8648b955529cc60e127164dfa75491508be61f87a7e747a14c53b024890`.
Source SHA-1: `f3ae088181bf583e55daf962a92bb46f4f1d07b7`.
Lab SHA-256: `5495d6e7772c1e5ea61f02dc3fc02d5f9ac2e2838e1bfcc32c3dcc03652147ec`.

This ZIP contains no ROM, save or sprite sheets.
[Source and controls](https://github.com/GBurgardt/pokemon-emerald-arena).
