# Emerald Arena landing

A small English landing page for the playable GBA demo.

- `/` and `/arena`: title, one sentence, real gameplay, download and GitHub repo.
- `/prepare.html`: local-only ROM preparation. No files are uploaded.
- `public/emerald-arena-17s.mp4`: two-second Emerald intro + the original fight, with native audio.
- `public/emerald-arena-15s.mp4`: preserved original battle-only clip.
- `public/og.png`: generated social card, not gameplay.
- `public/fonts/tiny5-regular.ttf`: locally hosted Tiny5; SIL Open Font License beside the font.

```sh
npm ci
npm run dev
npm run build
node --test tests/rendered-html.test.mjs
```

Built with vinext and hosted with Sites. No database, user accounts or analytics.
Autoplay starts muted. Sound is one click away; native video controls remain
available. Reduced-motion preferences pause automatic playback.

[Game source and release](https://github.com/GBurgardt/pokemon-emerald-arena).

Design reference: [Downwell](https://downwellgame.com/) — pixel typography,
strong contrast and flat geometry. This page uses its own ivory/ink/cobalt palette,
not Downwell's artwork or code. No feature cards, counters or rounded containers.
The original gameplay clip and local-only ROM preparer are unchanged.
