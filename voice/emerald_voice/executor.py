"""Frame-accurate execution of controller macros. The brain decides what to
do; these actions decide exactly which keys are held on each frame."""
import math
import threading

from .state import KEYS, DIRS

DIR_KEYS = {name: sum(KEYS[k] for k, on in (('LEFT', dx < 0), ('RIGHT', dx > 0), ('UP', dy < 0), ('DOWN', dy > 0)) if on)
            for name, (dx, dy) in DIRS.items()}


class Action:
    name = 'action'
    ok = True

    def step(self, s):
        """Return (keys, done) for this frame."""
        raise NotImplementedError


class Hold(Action):
    def __init__(self, keys, frames, name='hold'):
        self.keys, self.left, self.name = keys, frames, name

    def step(self, s):
        self.left -= 1
        return self.keys, self.left <= 0


class Tap(Action):
    """Press, then release for a few frames so the game sees a fresh press."""

    def __init__(self, keys, press=4, gap=3, name='tap'):
        self.keys, self.press, self.gap, self.name, self.t = keys, press, gap, name, 0

    def step(self, s):
        self.t += 1
        return (self.keys if self.t <= self.press else 0), self.t >= self.press + self.gap


class Walk(Action):
    """Walk one way until the Pokémon has covered `distance` pixels, or it
    stops making progress (a wall or a rock), or two seconds pass."""

    def __init__(self, direction, distance=64):
        self.keys, self.distance, self.name = DIR_KEYS[direction], distance, f'walk {direction}'
        self.start, self.t, self.last, self.still = None, 0, None, 0

    def step(self, s):
        self.t += 1
        pos = (s.me.x, s.me.y)
        if self.start is None:
            self.start = self.last = pos
        moved = abs(pos[0] - self.start[0]) + abs(pos[1] - self.start[1])
        self.still = self.still + 1 if pos == self.last else 0
        self.last = pos
        if moved >= self.distance or self.t > 120 or (self.t > 10 and self.still > 10):
            return 0, True
        return self.keys, False


class GoTo(Action):
    """Walk to a point along a path around the rocks, re-planned every few
    frames (the target can be the opponent, who moves). Holds the one or two
    direction keys toward the next waypoint. If the body stops moving (a
    shot, a cover, the other Pokémon), it sidesteps briefly and re-plans.
    Done on arrival, when `stop(state)` says so, or after `budget` frames."""

    def __init__(self, target, stop=None, arrive=5, budget=240, name='go', avoid_rival=True):
        self.target, self.stop, self.arrive, self.budget, self.name = target, stop, arrive, budget, name
        self.avoid_rival = avoid_rival
        self.t, self.route, self.last, self.still, self.side_step, self.flip = 0, [], None, 0, 0, 1

    def goal(self, s):
        return self.target(s) if callable(self.target) else self.target

    def step(self, s):
        from . import nav
        self.t += 1
        me = (s.me.x, s.me.y)
        if self.stop and self.stop(s):
            return 0, True
        if self.t > self.budget:
            self.ok = self.stop is None and math.dist(me, self.route[-1] if self.route else me) < 24
            return 0, True
        if self.t == 1 or self.t % 6 == 0 or not self.route:
            extra = [nav.body_rect(s.rival.x, s.rival.y)] if self.avoid_rival else []
            self.route = nav.path(me, self.goal(s), s.solid_mask, extra)
        while len(self.route) > 1 and math.dist(me, self.route[0]) < 5:
            self.route.pop(0)
        wx, wy = self.route[0]
        if len(self.route) == 1 and math.dist(me, (wx, wy)) <= self.arrive:
            return 0, True
        self.still = self.still + 1 if me == self.last else 0
        self.last = me
        if self.side_step:
            self.side_step -= 1
            return self.side_keys, False
        dx, dy = wx - me[0], wy - me[1]
        if self.still > 8:
            # Stuck: step sideways for a moment, alternating sides.
            self.still, self.side_step, self.flip = 0, 10, -self.flip
            sx, sy = -dy * self.flip, dx * self.flip
            self.side_keys = _dir_keys(sx, sy)
            return self.side_keys, False
        return _dir_keys(dx, dy), False


def _dir_keys(dx, dy):
    """One or two direction keys for a heading; a small component is dropped
    so walking nearly straight doesn't zig-zag."""
    keys = 0
    big = max(abs(dx), abs(dy), 1)
    if abs(dx) >= max(2, big * 0.4):
        keys |= KEYS['RIGHT'] if dx > 0 else KEYS['LEFT']
    if abs(dy) >= max(2, big * 0.4):
        keys |= KEYS['DOWN'] if dy > 0 else KEYS['UP']
    return keys


class Wait(Hold):
    def __init__(self, frames, name='wait'):
        super().__init__(0, frames, name)


class Sequence(Action):
    def __init__(self, actions, name='sequence'):
        self.actions, self.name = list(actions), name

    def step(self, s):
        while self.actions:
            keys, done = self.actions[0].step(s)
            if done:
                if not self.actions[0].ok:
                    self.ok = False
                self.actions.pop(0)
            return keys, not self.actions
        return 0, True


class Unpause(Action):
    name = 'unpause'

    def __init__(self):
        self.tap = None

    def step(self, s):
        if not s.paused:
            return 0, True
        if not self.tap or self.tap.t >= self.tap.press + self.tap.gap + 6:
            self.tap = Tap(KEYS['START'], name='start')
        keys, _ = self.tap.step(s)
        return keys, False


class SelectMove(Action):
    """Cycle R until the arena's selected move is the wanted slot."""

    def __init__(self, slot):
        self.slot, self.name, self.tap, self.tries = slot, f'select slot {slot}', None, 0

    def step(self, s):
        if s.selected_slot == self.slot:
            return 0, True
        if self.tries > 10:
            self.ok = False
            return 0, True
        if not self.tap or self.tap.t >= self.tap.press + self.tap.gap + 3:
            self.tap, self.tries = Tap(KEYS['R'], press=3, gap=2), self.tries + 1
        keys, _ = self.tap.step(s)
        return keys, False


class Attack(Action):
    """A alone aims at the rival. Rollout rolls where the body faces, so it
    holds the direction of the rival with A."""

    def __init__(self, aim=None):
        self.aim, self.name, self.t = aim, 'attack', 0

    def step(self, s):
        self.t += 1
        keys = KEYS['A'] | (DIR_KEYS[self.aim] if self.aim else 0)
        return (keys if self.t <= 4 else 0), self.t >= 7


class Dodge(Action):
    """B with a direction. Waits out the dash cooldown, and presses again if
    the game did not register the dodge (it can be busy mid-attack)."""

    def __init__(self, direction):
        self.keys, self.name = KEYS['B'] | DIR_KEYS[direction], f'dodge {direction}'
        self.start, self.t, self.tap, self.tries = None, 0, None, 0

    def step(self, s):
        self.t += 1
        if self.start is None:
            self.start = s.dodges
        if s.dodges > self.start:
            return 0, True
        if self.t > 120 or self.tries >= 4:
            self.ok = False
            return 0, True
        if s.me.dash_cooldown and not self.tap:
            return 0, False
        if not self.tap or self.tap.t >= 12:
            self.tap, self.tries = Tap(self.keys, press=4, gap=2), self.tries + 1
        return self.tap.step(s)[0], False


class Switch(Action):
    """Switch Pokémon through the game's own party menu: the arena is asked
    to open it (or the faint prompt is answered), the cursor is moved to the
    wanted party slot once the menu takes input, and SHIFT is chosen. Done
    when the arena is back; backs out if the game refuses the switch."""

    def __init__(self, bridge, personality, name):
        # The party menu reorders the party (the Pokémon in battle first), so
        # the target is remembered by its personality value and its row is
        # looked up once the menu is open.
        self.bridge, self.personality, self.name = bridge, personality, f'switch to {name}'
        self.phase, self.t, self.t0, self.tap, self.requested, self.presses = 'request', 0, 0, None, False, 0

    def _tap(self, s, key, every):
        if not self.tap or self.tap.t >= every:
            self.tap = Tap(KEYS[key], press=3, gap=2)
        return self.tap.step(s)[0]

    def _go(self, phase):
        self.phase, self.t0, self.tap = phase, self.t, None

    def step(self, s):
        self.t += 1
        if self.t > 60 * 25:
            self.ok = False
            return 0, True
        if self.phase == 'request':
            if s.party_input:
                self._go('navigate')
            elif s.screen == 'arena' and not self.requested:
                self.bridge.write_byte('gArenaVoiceRequest', 1)
                self.requested = True
            elif s.screen == 'battle' and s.me.hp == 0:
                return self._tap(s, 'A', 40), False     # "Use next POKéMON?" -> YES
            return 0, False
        if self.phase == 'navigate':
            if s.party_submenu:
                self._go('submenu')
                return 0, False
            if not s.party_input:
                return 0, False
            row = next((m.slot for m in s.party if m.personality == self.personality), None)
            if row is None:
                self.ok = False
                self._go('back_out')
                return 0, False
            if s.menu_cursor == row:
                return self._tap(s, 'A', 30), False
            if self.presses > 14:
                self.ok = False
                self._go('back_out')
                return 0, False
            if not self.tap or self.tap.t >= 10:
                self.presses += 1
            return self._tap(s, 'DOWN', 10), False
        if self.phase == 'submenu':
            # SHIFT (or SEND OUT after a faint) is the first entry.
            if not s.party_submenu:
                self._go('return')
                return 0, False
            return self._tap(s, 'A', 30), False
        if self.phase == 'return':
            if s.screen == 'arena':
                return 0, True
            if s.party_input and self.t - self.t0 > 20:
                self.ok = False          # the game refused, e.g. it is already in battle
                self._go('back_out')
            return 0, False
        if self.phase == 'back_out':
            if s.screen != 'menu' and not s.party_input:
                return 0, True
            return self._tap(s, 'B', 20), False
        return 0, True


class Executor:
    """Runs one action at a time; a new action replaces the current one."""

    def __init__(self):
        self.lock = threading.Lock()
        self.current = None
        self.done_event = threading.Event()
        self.done_event.set()
        self.last_ok = True

    def run(self, action):
        with self.lock:
            self.current = action
            self.done_event.clear()

    def cancel(self):
        with self.lock:
            self.current = None
            self.done_event.set()

    def busy(self):
        return not self.done_event.is_set()

    def on_frame(self, s):
        with self.lock:
            if not self.current:
                return 0
            keys, done = self.current.step(s)
            if done:
                self.last_ok = self.current.ok
                self.current = None
                self.done_event.set()
            return keys
