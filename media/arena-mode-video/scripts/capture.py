"""Records the gameplay clips the jumbotron shows (public/clips/*.mp4, not
committed) from the locally built ARENA-mode ROM, driven by the voice
agent's own brain with typed commands, so every speech bubble and move is
the real thing. Needs the headless mGBA toolkit (~/.config/mgba/headless)
and the ROM plus its symbols in the repo root.

  ~/.config/mgba/headless/venv/bin/python scripts/capture.py [clip ...]
"""
import os
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
REPO = HERE.parents[1]
HEADLESS = Path.home() / '.config/mgba/headless'
sys.path[:0] = [str(HEADLESS / 'lib'), str(REPO / 'voice')]
os.environ.setdefault('DYLD_LIBRARY_PATH', str(HEADLESS / 'lib'))

import mgba.core, mgba.image, mgba.log  # noqa: E402
from emerald_voice.brain import Brain  # noqa: E402
from emerald_voice.bridge import HeadlessBridge  # noqa: E402
from emerald_voice.executor import Executor  # noqa: E402
from emerald_voice.gamedata import GameData, load_symbols  # noqa: E402
from emerald_voice.state import view  # noqa: E402

mgba.log.silence()
ROM = REPO / 'Emerald-Arena-draft.gba'
SYM = REPO / 'Emerald-Arena-draft.sym'
START, A, B, DOWN = 3, 0, 1, 7        # mGBA key numbers
# Still frames saved beside a clip (seconds into it): the thumbnail's.
POSTERS = {'flame': 2.1}

# name: (versus P2?, P1 picks, P2 picks, [(seconds, player, command)], record seconds)
CLIPS = {
    'flame': (False, 'Charizard', None, [(0.2, 1, 'get closer'), (1.2, 1, 'flamethrower')], 4.5),
    'duo': (True, 'Charizard', 'Venusaur', [(0.3, 2, 'giga drain'), (0.4, 1, 'dodge up'), (2.4, 1, 'flamethrower')], 5.0),
    'rollout': (False, 'Golem', None, [(0.2, 1, 'rollout')], 4.0),
    'toss': (False, 'Machamp', None, [(0.2, 1, 'seismic toss')], 5.0),
    'psychic': (False, 'Alakazam', None, [(0.2, 1, 'psychic')], 4.0),
}


class Game:
    def __init__(self):
        self.core = mgba.core.load_path(str(ROM))
        w, h = self.core.desired_video_dimensions()
        self.img = mgba.image.Image(w, h)
        self.core.set_video_buffer(self.img)
        self.core.reset()
        self.mem = self.core.memory
        self.data = GameData(str(REPO / 'workspace'))
        self.syms = load_symbols(str(SYM))
        self.ex, self.ex2 = Executor(), Executor()
        self.bridge = HeadlessBridge(self.core, self.data, self.syms, self.on_frame)
        log = lambda m: print('   ', m, flush=True)
        self.brains = [Brain(self.bridge, self.ex, self.data, log=log),
                       Brain(self.bridge, self.ex2, self.data, log=log, side=1)]
        self.frames = None

    def on_frame(self, s):
        keys = self.ex.on_frame(s)
        if s.pvp:
            self.mem.u16[self.syms['gArenaP2Keys']] = self.ex2.on_frame(view(s, 1))
        return keys

    on_step = None

    def step(self, n=1, extra=0):
        for _ in range(n):
            self.bridge.step(extra)
            if self.on_step:
                self.on_step()
            if self.frames is not None:
                self.frames.append(self.img.to_pil().convert('RGB'))

    def press(self, key, wait=12):
        self.step(4, 1 << key)
        self.step(wait)

    def task(self, name):
        base, fn = self.syms['gTasks'], self.syms[name] & ~1
        return any(self.mem.u8[base + i * 40 + 4] and (self.mem.u32[base + i * 40] & ~1) == fn for i in range(16))

    def say(self, player, text):
        t = threading.Thread(target=self.brains[player - 1].command, args=(text,), daemon=True)
        t.start()
        return t


def record(name, pvp, p1, p2, script, seconds):
    g = Game()
    for i in range(3000):
        g.step()
        if i % 60 == 30 and not g.task('Task_ArenaMenu'):
            g.step(4, 1 << START)
        if g.task('Task_ArenaMenu'):
            break
    g.step(30)
    if pvp:
        g.press(DOWN)
    g.press(A, 80)
    for player, team in ((1, p1), (2, p2)):
        if not team:
            continue
        g.say(1, f'pick {team}').join()
        g.step(30)
        g.say(1, 'done').join()
        g.step(100)
    g.say(1, 'fight').join()
    tel, bm = g.syms['gRealtimeArenaTelemetry'], g.syms['gBattleMons']

    def tank():                                  # nobody faints on camera
        for side in (0, 1):
            g.mem.u16[bm + side * 0x58 + 0x2C] = g.mem.u16[bm + side * 0x58 + 0x28] = 999
    g.on_step = tank
    for i in range(60 * 20):
        g.step()
        if g.mem.u8[tel + 56]:
            break
    print('    arena active after', round(i / 60, 1), 's', flush=True)
    g.step(420)                                  # past the send-out
    g.frames = []
    events = sorted(script)
    for f in range(int(seconds * 60)):
        while events and f >= events[0][0] * 60:
            _, player, text = events.pop(0)
            g.say(player, text)
        g.step()
    frames, g.frames = g.frames, None
    out = HERE / 'public' / 'clips'
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for i, im in enumerate(frames):
            im.save(f'{tmp}/{i:05d}.png')
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', '60', '-i', f'{tmp}/%05d.png',
                        '-vf', 'scale=960:640:flags=neighbor', '-c:v', 'libx264', '-crf', '12', '-pix_fmt', 'yuv420p',
                        str(out / f'{name}.mp4')], check=True)
        frames[len(frames) // 2].save(out / f'{name}.png')
        if name in POSTERS:
            frames[int(POSTERS[name] * 60)].resize((960, 640), 0).save(out / f'{name}-poster.png')
    print(f'{name}: {len(frames)} frames')


if __name__ == '__main__':
    for name in sys.argv[1:] or CLIPS:
        print('clip', name, flush=True)
        record(name, *CLIPS[name])
