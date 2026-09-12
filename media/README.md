# Real gameplay

[Watch / download the 17-second video](emerald-arena-17s.mp4) ·
[Download the looping GIF](emerald-arena-17s.gif)

Two seconds of Route 103's tall grass and Emerald's native encounter transition,
then the complete original fight. The field intro is a separate recording from
the same public release, using the demo's L+R practice encounter. A short section
of its transition is omitted to keep the opening to two seconds; this is an edit,
not one continuous field-to-battle take. No generated gameplay or extra effects.

- MP4: 17.000 seconds, 960 × 640, 60 fps, H.264 / AAC, 1.98 MB.
- GIF: 17.000 seconds, 480 × 320, 20 fps, looping, 4.23 MB.
- All 900 battle video frames and all 300 battle GIF frames match the originals
  exactly after decoding. Video packets are copied; GIF image data and source
  palettes are preserved. Native audio is joined and encoded to AAC.
- Intro controls: [intro-inputs.txt](capture-source/intro-inputs.txt).
- Lossless GIF joiner: [concat_gif.py](capture-source/concat_gif.py).
- Verification: [verify_media.py](capture-source/verify_media.py).

MP4 SHA-256: `45d81fca963066195a14018ace9e94687154f231ae9f7286f868f761cf0cbfd4`.
GIF SHA-256: `e22e9027aa2bb6089a47bb2800fba1c1ca0c774a2c2b1dbc3a4eece1840cc2de`.

## Original battle-only clip

[Preserved 15-second video](emerald-arena-15s.mp4) ·
[Preserved 15-second GIF](emerald-arena-15s.gif)

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
