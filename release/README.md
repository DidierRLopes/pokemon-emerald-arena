# Emerald Arena 0.3.1

Pokémon Emerald with real-time battles. Move, dodge, attack and break the arena.
Requires your own unmodified Emerald ROM (USA/Europe).

1. Open **Prepare-Emerald-Arena.html** in a current Chrome, Safari or Firefox.
2. Choose your `.gba` and download the result. Your ROM stays on your device.
3. Open it in a GBA emulator. With no existing save, press SELECT on NEW GAME.
4. Press L+R in the field to fight. Charizard leads; reorder the party to try others.

Setup needs Internet for the animations. The game then works offline.
Terminal option, Node 22+: `node install.mjs original.gba arena.gba`.

GBA controls: D-pad to move, A to attack, B to dodge, L/R to change move,
START to pause, SELECT for classic battles. Save from the field menu.

Party: Charizard, Blastoise, Eevee, Dragonite, Scizor and Blaziken.
Box 1: Treecko, Poochyena, Bulbasaur, Squirtle, Grovyle and Sceptile.
Ten move profiles are adapted. Other effects still use classic battles.
This is a playable demo, not a fully rebalanced adventure.

This package contains a compiled code delta and a local installer, not a ROM,
save or sprite sheets. The installer verifies pinned SpriteCollab assets and
checks that the result matches the tested release, byte for byte.

Source and credits: https://github.com/GBurgardt/pokemon-emerald-arena
