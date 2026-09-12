# Emerald Arena

Pokémon Emerald with real-time battles.

Move freely. Dodge attacks. Break rocks. Your team keeps its experience.
The arena runs inside the GBA game, with Emerald's own damage and save system.

## Play

[**Download the demo**](https://github.com/GBurgardt/pokemon-emerald-arena/releases/latest)

Open `Prepare-Emerald-Arena.html`, choose your unmodified Emerald ROM
(USA/Europe), and download the result. Your file stays on your device.
Setup needs Internet; the game then works offline.

Open the result in a GBA emulator. With no existing save, press **SELECT** on
**NEW GAME** for the practice team. Press **L+R** in the field to fight.

| GBA control | Action |
|---|---|
| D-pad | Move in eight directions |
| A | Attack; D-pad + A aims |
| B + D-pad | Dodge |
| L / R | Previous / next move |
| START | Pause and help |
| SELECT | Switch to classic battle |
| L + R in the field | Next practice opponent |

Save from the field menu. Practice encounters do not heal or reset your team.
Regular saves keep their encounters and do not receive the demo party.

## In this demo

- 12 animated Pokémon: Charizard, Blastoise, Eevee, Dragonite, Scizor, Blaziken,
  Treecko, Poochyena, Bulbasaur, Squirtle, Grovyle and Sceptile.
- 10 move profiles: Pound, Tackle, Quick Attack, Leer, Absorb, Water Gun,
  Wing Attack, Slam, Peck and Scratch.
- Breakable rocks, wood, foliage, crystals and explosive pods.
- Enemies that navigate obstacles, aim and dodge, with level-based reactions.
- Native HP, PP, experience, leveling and saves. Classic battles remain available.

This is a playable experiment, not a fully rebalanced adventure.
Moves and situations not yet adapted use classic battles. Flamethrower is one
example: Charizard's arena move is Wing Attack.

## Build and adapt

[Build instructions](BUILD.md) · [Credits](CREDITS.md) · [Release notes](RELEASE.md)

The source is an overlay and patch for
[pret/pokeemerald](https://github.com/pret/pokeemerald), pinned to
`5eff78649e7170a877b961ef0b3da13b81a16038`.

The download includes a compiled delta and a local installer, not a ROM, save
or sprite sheets. The installer fetches pinned animation sources, prepares them
locally and verifies the final ROM against the tested release.

New project code is [MIT](LICENSE). This is an independent, unofficial project.
