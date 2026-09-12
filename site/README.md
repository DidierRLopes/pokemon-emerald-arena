# Emerald Arena landing

A small English landing page for the playable GBA demo.

- `/` and `/arena`: download, real gameplay and controls.
- `/prepare.html`: local-only ROM preparation. No files are uploaded.
- `public/emerald-arena-15s.mp4`: real emulator capture with native audio.
- `public/og.png`: generated social card, not gameplay.

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
