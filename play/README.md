# Play in the browser (prototype)

A static page that builds Emerald Arena from the player's own Emerald ROM and
runs it in [mGBA for the web](https://github.com/thenick775/mgba/tree/feature/wasm),
entirely on their device. No server, no account, no upload.

- **Build:** the release installer (`release/installer.mjs` and `payload.json`)
  runs in the page, exactly as `Prepare-Emerald-Arena.html` does: it checks the
  ROM, applies the patch, fetches the SpriteCollab sheets and verifies the result
  byte for byte. The finished game goes into the browser's storage (IndexedDB),
  so the next visit goes straight to **Play**.
- **Run:** mGBA 2.5.1 compiled to WebAssembly, pinned by hash in
  `tools/build-play.mjs`. It uses threads, so the page must be cross-origin
  isolated: the host sends the headers, or `sw.js` adds them (GitHub Pages).
- **Saves:** in-game saves and a resume point stay in the browser. Export and
  import `.sav` files from the toolbar.
- **Controls:** keyboard (arrows, X, Z, A, S, Enter, Backspace, as in desktop
  mGBA), game controllers, and on-screen buttons on touch screens.

## Try it locally

```bash
node tools/build-play.mjs          # writes play/dist (downloads mGBA once)
node tools/serve-play.mjs          # http://localhost:8123
```

`--no-isolation` serves without the headers, to try the service-worker path.
`--rom original.gba` offers that ROM to the page, so it builds the game without
the file picker (local only; a hosted copy never has it).

## Host it

`play/dist` is plain static files, about 3.6 MB. The workflow
`.github/workflows/play-pages.yml` builds and deploys it to GitHub Pages when run
by hand, once Pages is set to deploy from GitHub Actions in the repository settings.
