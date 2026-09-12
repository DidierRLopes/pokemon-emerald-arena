# Real gameplay

[Watch / download the 15-second video](emerald-arena-15s.mp4) ·
[Download the looping GIF](emerald-arena-15s.gif)

Charizard versus Blastoise, both level 36. A continuous take from the public
0.3.1 release: seven dodges, six objects destroyed, a chain explosion and hits
on both Pokémon. The video includes the game's own music and sound effects.
The GIF has no audio.

This is real ARM code running in mGBA. The route was rehearsed with timed GBA
buttons. No HP, PP, damage, enemy behavior or outcome was changed for the take.
The recorded core's final party, battlers and physics matched the rehearsal.

- MP4: 15.000 seconds, 960 × 640, H.264 / AAC, 1.72 MB.
- GIF: 15.000 seconds, 480 × 320, looping, 2.66 MB.
- Source: 896 GBA frames at the native clock, stereo PCM at 65,536 Hz.
  Pixel-nearest scaling; no interpolated animation or generated gameplay.
- Recorder: [capture.c](capture-source/capture.c).
- Exact input sequence: [inputs.txt](capture-source/inputs.txt).

Replaying the exact take also needs its matching private emulator snapshot.
That snapshot and its ROM are not published. Start the demo normally to play
these Pokémon yourself.

Release ROM SHA-256: `d357b8648b955529cc60e127164dfa75491508be61f87a7e747a14c53b024890`.
MP4 SHA-256: `c26fa29e1f65f437649167493231d2cbe9accfdcb940fc8eb1ef3952611422db`.
GIF SHA-256: `1ec74faa603fada8802cb62ff0bc7aa83b98be2f8357cd98766bed6f590ca4e4`.

Original game, music and character credits: [CREDITS.md](../CREDITS.md).
