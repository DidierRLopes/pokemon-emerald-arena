"""Build the voice fight's starting save state: a fresh practice save
(SELECT on NEW GAME), L+R for the first practice battle, paused on the move
picker. With --dummy the wild Eevee becomes a training dummy: level 70 and
420 HP, but Attack and Sp. Atk 8 so its hits stay soft. Its party entry is
edited the moment L+R creates it, before the battle copies it.

Needs mGBA's Python bindings (built from mGBA 0.10.5 with BUILD_PYTHON=ON).
Usage: practice_state.py ROM MAP STATE_OUT [--dummy]
  e.g. practice_state.py Emerald-Arena-voice.gba Emerald-Arena-voice.map \
       ~/.config/mgba/states/voice-charizard.ss1 --dummy"""
import sys, re, struct
import mgba.core, mgba.image, mgba.gba, mgba.log
from mgba._pylib import lib, ffi
from PIL import Image
mgba.log.silence()
K = mgba.gba.GBA
rom, mapfile, state_out = sys.argv[1:4]
def symbol(name):
    for line in open(mapfile):
        m = re.match(r'\s+0x([0-9a-f]+)\s+' + name + r'$', line)
        if m: return int(m.group(1), 16)
    raise SystemExit('symbol not found: ' + name)
TEL, EP, BM = symbol('gRealtimeArenaTelemetry'), symbol('gEnemyParty'), 0x02024084
core = mgba.core.load_path(rom); w, h = core.desired_video_dimensions()
img = mgba.image.Image(w, h); core.set_video_buffer(img); core.reset(); mem = core.memory
def run(n, *keys):
    core.set_keys(*keys)
    for _ in range(n): core.run_frame()
    core.set_keys()
def press(key, n=5): run(n, key); core.run_frame()
run(1500); press(K.KEY_START); run(600); press(K.KEY_START); run(150); press(K.KEY_SELECT); run(900)
core.set_keys(K.KEY_L, K.KEY_R)
edited = False
for i in range(1200):
    core.run_frame()
    if '--dummy' in sys.argv and not edited and mem.u16[EP + 86] and mem.u8[EP + 84] == 30:     # the wild Eevee now exists
        core.set_keys()
        mem.u8[EP + 84] = 70                                      # level
        mem.u16[EP + 86] = mem.u16[EP + 88] = 420                 # HP, max HP
        mem.u16[EP + 90] = 8; mem.u16[EP + 96] = 8                # Attack, Sp. Atk
        edited = True
    if mem.u8[TEL + 56] and i > 60: break
core.set_keys()
assert mem.u8[TEL + 56] and (edited or '--dummy' not in sys.argv), 'arena did not start'
run(240)
press(K.KEY_START); run(20)
b = BM + 0x58
print('rival: species', mem.u16[b], 'level', mem.u8[b + 0x2A], 'HP', mem.u16[b + 0x28], '/', mem.u16[b + 0x2C], 'atk', mem.u16[b + 2], 'spatk', mem.u16[b + 8], 'def', mem.u16[b + 4])
vf = lib.VFileMemChunk(ffi.NULL, 0)
ok = lib.mCoreSaveStateNamed(core._core, vf, 1 | 8 | 16)
size = vf.size(vf); data = bytes(ffi.buffer(vf.map(vf, size, 1), size)); vf.close(vf)
open(state_out, 'wb').write(data); print('state', state_out, 'ok' if ok else 'FAILED', size)
img.to_pil().convert('RGB').resize((w * 2, h * 2), Image.NEAREST).save(state_out + '.png')
