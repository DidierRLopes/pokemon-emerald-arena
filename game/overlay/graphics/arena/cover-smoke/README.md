# Substitute and Smokescreen artwork

Project-created pixel art. `substitute.html` and `substitute-metadata.json` describe the redesigned doll; `source.html` preserves the original smoke animation. Both were developed with Claude using real arena screenshots as references.

The runtime doll is 32×32 (512 OBJ bytes), with a 23×26 body, arrival, idle, impact and break frames. It uses existing biome palettes through the five `doll-*.4bpp` files. Peripheral effect flecks outside the runtime canvas are cropped; the body is not downscaled. Native particles accompany its appearance and break.

Smoke uses frames 16–31 of `effect.4bpp`. Both effects use existing palette banks. The Makefile tracks these binary dependencies. The binaries are project-created assets listed by SHA-256 in `game/effect-assets.json`, not extracted Pokémon sprite sheets.
