# ARENA mode announcement video

A 33 s showreel-style video announcing Emerald Arena's ARENA mode, made with
the [launch-video](https://github.com/dzhng/skills) skill. The concept is
**draft night → game day**: a salary cap of 100 gold credits, a draft board
of all 150 Pokémon with their prices, picks stamped onto a bench (and Mewtwo
stamped OVER THE CAP), a night stadium whose jumbotron shows real gameplay,
two coaches whose shouts become the moves, and a final whistle.

```bash
npm install
npm run assets     # icons, sprites, roster from ../../workspace (Pillow)
npm run capture    # gameplay clips from ../../Emerald-Arena-draft.gba (headless mGBA)
npm run audio      # synthesize + master public/soundtrack.wav (-12 LUFS, -1 dBTP)
npm run studio     # scrub it in the browser
npm run draft      # half-scale review render
npm run render     # 1080p60 H.264, 320k AAC -> out/emerald-arena-mode.mp4
npm run still -- KeyArt out/key-art.png
python3 scripts/sheets.py out/draft.mp4   # contact sheets per shot
```

Nothing derived from the game is committed: `public/gen/` (icons, sprites,
roster) is built from the local pokeemerald workspace, and `public/clips/`
is recorded from your own ROM build by driving it headlessly through the
voice agent's own brain (typed commands), so every move and speech bubble on
the jumbotron is real gameplay. Renders and generated audio are ignored.

## One timing source

`src/cues.ts` holds every event in song seconds at 120 BPM. The scenes and
`audio/synth.ts` both read it, so moving a cue moves its picture and its
sound together; the lens (flash, shake, colour split) fires on the same
impact list the score hits. Add a visual event as a cue first, then give it
a sound. Everything is a pure function of song time.

The first 0.3 s is a pre-roll holding the thumbnail (the lockup beside a
real Flamethrower on the jumbotron); `PREROLL` shifts picture and music.

## Claims to preserve

- 150 Pokémon, level 100, 100 credits, up to 6 picks: `game/overlay/src/arena_draft.c`
  (`CREDITS`, `TEAM_MAX`) and the generated roster. Prices shown are the
  roster's (Magikarp 2, Mewtwo 32; the five picks cost 73, leaving 27).
- "Whisper on-device: median 120 ms per command (261 clips) · Jev: median
  215 ms (101 calls)" was measured from the voice agent's log
  (`~/.config/emerald-voice/agent.log`) on the author's Mac. Re-measure
  before changing it; keep the sample sizes and "measured on the author's Mac".
- The COM team on the versus card is illustrative.

The audio was checked by measurement (ffmpeg `ebur128` and a spectrogram),
not by ear.
