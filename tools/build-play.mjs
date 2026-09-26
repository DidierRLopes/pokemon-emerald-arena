// Build the browser player into play/dist: the page, the release installer
// and payload, and mGBA for the web from npm, pinned and checked. No ROM is
// read or packaged here. Usage: node tools/build-play.mjs
import {readFile, writeFile, mkdir, copyFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';

const root = new URL('../', import.meta.url);
const out = new URL('play/dist/', root), cache = new URL('play/.cache/', root);
const MGBA = {
  version: '2.5.1',
  tarball: 'https://registry.npmjs.org/@thenick775/mgba-wasm/-/mgba-wasm-2.5.1.tgz',
  integrity: 'sha512-FXgorZQXF2kiMtI4yxFj2Vw6d00T98uOcZ7dteA5kf4rk19IUhNtoNHdasXOnblFNF0i1w9Ci8akzPQUCDUeaw==',
};

async function emulatorPackage() {
  await mkdir(cache, {recursive: true});
  const file = new URL(`mgba-wasm-${MGBA.version}.tgz`, cache);
  let bytes = await readFile(file).catch(() => null);
  if (!bytes) {
    const response = await fetch(MGBA.tarball);
    if (!response.ok) throw new Error(`Could not download mGBA for the web: ${response.status}`);
    bytes = Buffer.from(await response.arrayBuffer());
  }
  const integrity = 'sha512-' + createHash('sha512').update(bytes).digest('base64');
  if (integrity !== MGBA.integrity) throw new Error('mGBA for the web does not match the pinned release.');
  await writeFile(file, bytes);
  return gunzipSync(bytes);
}

// The few files needed from a plain ustar archive.
function untar(tar, wanted) {
  const files = new Map();
  for (let at = 0; at + 512 <= tar.length;) {
    const name = tar.subarray(at, at + 100).toString('utf8').replace(/\0.*$/s, '');
    if (!name) break;
    const size = parseInt(tar.subarray(at + 124, at + 136).toString('utf8').replace(/\0.*$/s, '').trim() || '0', 8);
    if (wanted.includes(name)) files.set(name, tar.subarray(at + 512, at + 512 + size));
    at += 512 + Math.ceil(size / 512) * 512;
  }
  for (const name of wanted) if (!files.has(name)) throw new Error('Missing from the mGBA package: ' + name);
  return files;
}

await mkdir(out, {recursive: true});
for (const name of ['index.html', 'app.mjs', 'style.css', 'sw.js']) await copyFile(new URL(`play/${name}`, root), new URL(name, out));
await copyFile(new URL('release/installer.mjs', root), new URL('installer.mjs', out));
const payload = await readFile(new URL('release/payload.json', root), 'utf8');
JSON.parse(payload);
await writeFile(new URL('payload.json', out), payload);
const files = untar(await emulatorPackage(), ['package/dist/mgba.js', 'package/dist/mgba.wasm']);
await writeFile(new URL('mgba.js', out), files.get('package/dist/mgba.js'));
await writeFile(new URL('mgba.wasm', out), files.get('package/dist/mgba.wasm'));
await writeFile(new URL('NOTICE.txt', out), `mgba.js and mgba.wasm: mGBA for the web ${MGBA.version}, @thenick775/mgba-wasm,
built from https://github.com/thenick775/mgba/tree/feature/wasm, a fork of mGBA by Vicki Pfau and contributors.
Mozilla Public License 2.0: https://mozilla.org/MPL/2.0/ . Source: the repository above.
installer.mjs and payload.json: Emerald Arena, https://github.com/GBurgardt/pokemon-emerald-arena (MIT).
No ROM, save or sprite sheet is included. The game is built on the player's device from their own ROM.
`);
console.log('Browser player built in play/dist. Serve it with: node tools/serve-play.mjs');
