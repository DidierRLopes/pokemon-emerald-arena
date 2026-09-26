// Emerald Arena in the browser: build the game from the player's own ROM with
// the release installer, keep it in the browser's storage, and run it in
// mGBA compiled to WebAssembly. Nothing is uploaded; there is no server.
import {prepareRom} from './installer.mjs';
import mGBA from './mgba.js';

const $ = id => document.getElementById(id);
const status = (text, error = false) => { $('status').textContent = text; $('status').dataset.error = error; };
const fail = error => { console.error(error); status(error.message || String(error), true); };

let emulator, paths, payload, romPath, savePath, syncTimer, muted = false;
const romName = () => `Emerald-Arena-${payload.manifest.version}.gba`;

// Keyboard: the same layout as desktop mGBA. SDL key names.
const KEYS = {Up: 'Up', Down: 'Down', Left: 'Left', Right: 'Right',
  X: 'A', Z: 'B', A: 'L', S: 'R', Return: 'Start', Backspace: 'Select'};

async function start() {
  if (!window.crossOriginIsolated) {
    if (window.isSecureContext && 'serviceWorker' in navigator && !sessionStorage.getItem('arena-isolation-reload')) {
      status('Preparing the page…');   // the isolation reload is on its way
      return;
    }
    throw new Error('This browser cannot run the game here: the page is not cross-origin isolated. Try a current Chrome, Firefox or Safari over https.');
  }
  status('Loading the emulator…');
  const [loaded] = await Promise.all([
    fetch('payload.json').then(r => { if (!r.ok) throw new Error('Could not load the game package.'); return r.json(); }),
    (async () => {
      emulator = await mGBA({canvas: $('screen')});
      await emulator.FSInit();
      paths = emulator.filePaths();
    })(),
  ]);
  payload = loaded;
  $('version').textContent = `EMERALD ARENA · ${payload.manifest.version}`;
  romPath = `${paths.gamePath}/${romName()}`;
  savePath = `${paths.savePath}/${romName().replace(/\.gba$/, '.sav')}`;
  for (const [key, input] of Object.entries(KEYS)) {
    try { emulator.bindKey(key, input); } catch (error) { console.warn('bindKey', key, error); }
  }
  emulator.toggleInput(false);
  emulator.addCoreCallbacks({
    saveDataUpdatedCallback: scheduleSync,
    autoSaveStateCapturedCallback: scheduleSync,
    coreCrashedCallback: () => fail(new Error('The emulator stopped. Reload the page to continue from your last save.')),
  });
  showMenu();
}

function haveGame() {
  try { return emulator.listRoms().includes(romName()); } catch { return false; }
}

function showMenu() {
  $('setup').hidden = haveGame();
  $('ready').hidden = !haveGame();
  $('choose').disabled = false;
  status(haveGame() ? 'Ready.' : 'Choose your ROM to build the game. This takes about a minute the first time.');
}

// Build the game from the player's ROM with the release's own installer, and
// keep only the result, in this browser.
async function build(file) {
  $('choose').disabled = true; $('progress').hidden = false;
  try {
    if (file.size !== payload.manifest.source_size)
      throw new Error('Choose an unmodified Pokémon Emerald (USA/Europe) .gba file, not a ZIP.');
    const source = new Uint8Array(await file.arrayBuffer());
    const patch = Uint8Array.from(atob(payload.patch), c => c.charCodeAt(0));
    const rom = await prepareRom(source, payload.manifest, patch, text => status(text));
    // Older versions' games and resume points do not carry over; in-game saves do.
    for (const name of emulator.listRoms()) if (/^Emerald-Arena-.*\.gba$/.test(name)) emulator.FS.unlink(`${paths.gamePath}/${name}`);
    emulator.FS.writeFile(romPath, rom);
    await emulator.FSSync();
    status('Your game is ready.');
  } catch (error) {
    fail(error);
  } finally {
    $('progress').hidden = true; $('choose').disabled = false; $('file').value = '';
  }
  if (haveGame()) showMenu();
}

function scheduleSync() {
  clearTimeout(syncTimer);
  syncTimer = setTimeout(() => emulator.FSSync().catch(console.error), 1000);
}

function play() {
  $('intro').hidden = true; $('game').hidden = false;
  emulator.setCoreSettings({rewindEnable: false, autoSaveStateEnable: true, restoreAutoSaveStateOnLoad: true,
    autoSaveStateTimerIntervalSeconds: 20});
  if (!emulator.loadGame(romPath)) { quit(); fail(new Error('The game could not start. Remove it and build it again.')); return; }
  emulator.toggleInput(true);
  emulator.resumeAudio();
  if (matchMedia('(pointer: coarse)').matches) $('touch').hidden = false;
  $('screen').focus();
}

async function quit() {
  try { emulator.forceAutoSaveState(); } catch {}
  emulator.quitGame();
  emulator.toggleInput(false);
  await emulator.FSSync();
  $('game').hidden = true; $('intro').hidden = false;
  showMenu();
}

function download(bytes, name) {
  const url = URL.createObjectURL(new Blob([bytes], {type: 'application/octet-stream'}));
  const a = Object.assign(document.createElement('a'), {href: url, download: name});
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

// A save from elsewhere replaces this game's save; the resume point is
// dropped so it cannot restore over the imported progress.
async function importSave(file) {
  const bytes = new Uint8Array(await file.arrayBuffer());
  if (bytes.length !== 131072 && bytes.length !== 65536) { alertInGame('That does not look like an Emerald save (.sav).'); return; }
  emulator.quitGame();
  emulator.FS.writeFile(savePath, bytes);
  const base = romName().replace(/\.gba$/, '');
  try { for (const name of emulator.FS.readdir(paths.autosave)) if (name.startsWith(base)) emulator.FS.unlink(`${paths.autosave}/${name}`); } catch {}
  await emulator.FSSync();
  emulator.loadGame(romPath);
  $('screen').focus();
}

function alertInGame(text) {
  const button = $('import').parentElement, label = button.firstChild.textContent;
  button.firstChild.textContent = text;
  setTimeout(() => { button.firstChild.textContent = label; }, 3500);
}

// Touch controls: an eight-way pad read from the finger's angle, and buttons.
function touchControls() {
  const pad = $('dpad'), held = new Set();
  const set = next => {
    for (const d of held) if (!next.has(d)) emulator.buttonUnpress(d);
    for (const d of next) if (!held.has(d)) emulator.buttonPress(d);
    held.clear(); for (const d of next) held.add(d);
    pad.classList.toggle('down', held.size > 0);
  };
  const read = event => {
    const r = pad.getBoundingClientRect(), x = event.clientX - r.left - r.width / 2, y = event.clientY - r.top - r.height / 2;
    const next = new Set(), dead = r.width * 0.12;
    if (Math.hypot(x, y) > dead) {
      const angle = Math.atan2(y, x) * 180 / Math.PI;   // 0 = right, 90 = down
      if (angle > -67.5 && angle < 67.5) next.add('Right');
      if (angle > 112.5 || angle < -112.5) next.add('Left');
      if (angle > 22.5 && angle < 157.5) next.add('Down');
      if (angle < -22.5 && angle > -157.5) next.add('Up');
    }
    set(next);
  };
  pad.addEventListener('pointerdown', e => { pad.setPointerCapture(e.pointerId); read(e); });
  pad.addEventListener('pointermove', e => { if (pad.hasPointerCapture(e.pointerId)) read(e); });
  for (const type of ['pointerup', 'pointercancel']) pad.addEventListener(type, () => set(new Set()));
  for (const button of document.querySelectorAll('#touch [data-btn]')) {
    const name = button.dataset.btn;
    const release = () => { button.classList.remove('down'); emulator.buttonUnpress(name); };
    button.addEventListener('pointerdown', e => { button.setPointerCapture(e.pointerId); button.classList.add('down'); emulator.buttonPress(name); });
    for (const type of ['pointerup', 'pointercancel']) button.addEventListener(type, release);
  }
  $('touch').addEventListener('contextmenu', e => e.preventDefault());
}

$('choose').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', () => { if ($('file').files[0]) build($('file').files[0]); });
$('play').addEventListener('click', play);
$('forget').addEventListener('click', async () => {
  if ($('forget').dataset.armed !== 'yes') {
    $('forget').dataset.armed = 'yes'; $('forget').textContent = 'Click again to delete the game and its saves';
    return;
  }
  for (const dir of [paths.gamePath, paths.savePath, paths.saveStatePath, paths.autosave]) {
    try { for (const name of emulator.FS.readdir(dir)) if (name.startsWith('Emerald-Arena')) emulator.FS.unlink(`${dir}/${name}`); } catch {}
  }
  await emulator.FSSync();
  $('forget').dataset.armed = ''; $('forget').textContent = 'Remove them';
  showMenu();
});
$('export').addEventListener('click', () => {
  const save = emulator.getSave();
  if (save) download(save, romName().replace(/\.gba$/, '.sav'));
  else alertInGame('No in-game save yet. Save from the field menu first.');
});
$('import').addEventListener('change', () => { if ($('import').files[0]) importSave($('import').files[0]); $('import').value = ''; });
$('mute').addEventListener('click', () => {
  muted = !muted; emulator.setVolume(muted ? 0 : 1);
  $('mute').textContent = muted ? 'Sound off' : 'Sound on'; $('screen').focus();
});
$('fullscreen').addEventListener('click', () => {
  if (document.fullscreenElement) document.exitFullscreen(); else $('game').requestFullscreen?.();
  $('screen').focus();
});
$('touch-toggle').addEventListener('click', () => { $('touch').hidden = !$('touch').hidden; $('screen').focus(); });
$('quit').addEventListener('click', quit);
document.addEventListener('visibilitychange', () => {
  if (!emulator || $('game').hidden) return;
  if (document.hidden) { emulator.pauseGame(); try { emulator.forceAutoSaveState(); } catch {} emulator.FSSync(); }
  else emulator.resumeGame();
});
addEventListener('pagehide', () => { if (emulator && !$('game').hidden) { try { emulator.forceAutoSaveState(); } catch {} emulator.FSSync(); } });
touchControls();
start().catch(fail);
