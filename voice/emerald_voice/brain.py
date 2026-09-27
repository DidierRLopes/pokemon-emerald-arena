"""From words to play. Jev maps what was said onto the commands that are
possible right now (intent), told what each move needs (a close-range blow
means running up to the opponent first). The agent then plans the way: every
walk, approach, retreat or attack follows a path around the rocks, re-planned
every few frames as the opponent moves, and the executor carries it out frame
by frame. Rules stand in for Jev when it is not available."""
import re
import threading

from . import nav
from .executor import Action, Attack, Dodge, GoTo, Sequence, SelectMove, Switch, Unpause
from .jev import JevError
from .state import DIRS, view

CARDINAL = ['up', 'down', 'left', 'right']


class FireWhen(Action):
    """Fire the selected move if the rival is in reach; otherwise give up
    (the move was not landable in time)."""

    def __init__(self, ready, aim=None):
        self.ready, self.aim, self.name, self.attack = ready, aim, 'attack', None

    def step(self, s):
        if self.attack is None:
            if not self.ready(s):
                self.ok = False
                return 0, True
            self.attack = Attack(self.aim(s) if self.aim else None)
        return self.attack.step(s)


class Brain:
    def __init__(self, bridge, executor, data, jev=None, log=print, verbose=False, reset_state=None, side=0):
        self.bridge, self.executor, self.data, self.jev, self.verbose = bridge, executor, data, jev, verbose
        self.reset_state, self.side = reset_state, side
        self.goal_id = 0
        self.lock = threading.Lock()
        self.log = (lambda m: log(m.replace('  ', '  [P2] ', 1) if m.startswith('  ') else '[P2] ' + m)) if side else log

    def state(self):
        return view(self.bridge.state, self.side)

    def unpause(self):
        # The pause is the whole game's and belongs to Player 1.
        return [] if self.side else [Unpause()]

    # ----- intent ---------------------------------------------------------
    def options(self, s):
        opts = {
            'approach': 'Move toward the opponent: get closer, go at it, charge, attack from up close',
            'retreat': 'Move away from the opponent: go away, get back, back off, run, keep your distance',
            'stop': 'Stop, stay, hold still, wait, cancel',
            'attack': f'Attack now with the currently selected move ({self.move_name(s, s.selected_slot)}): attack, hit it, now, fire',
            'none': 'Not a command: chatter, noise, or unclear speech',
        }
        for d in DIRS:
            opts['move_' + d] = f'Take a few steps {d.replace("_", " and ")} (screen direction)'
        for d in CARDINAL:
            opts['dodge_' + d] = f'Dodge, dash or jump {d} (a quick evasive dash to the {d})'
        for i, move in enumerate(s.me.moves):
            if move and self.data.profiles.get(move):
                p = self.data.profiles[move]
                how = {'melee': 'close range: runs up to the opponent, then strikes',
                       'rush': 'charges at the opponent from mid range',
                       'cone': 'short-range blast in front: gets near, then fires',
                       'projectile': 'long-range shot: fires from a distance',
                       'self': 'affects itself, no target'}.get(p.kind, p.kind)
                opts[f'use_move_{i}'] = f'Use the move {self.data.moves.get(move, "?")} ({how})'
        for mon in s.party:
            if mon.slot != s.active_party_slot and mon.hp > 0 and not mon.is_egg:
                opts[f'switch_{mon.slot}'] = (f'Switch Pokémon: send out {mon.name} (a {self.data.species.get(mon.species, "?")}), '
                                              f'come back and go {mon.name}, change to {mon.name}')
        if s.paused:
            opts['resume'] = 'Resume or unpause the fight, start, go, let\'s go'
        return opts

    def move_name(self, s, slot):
        return self.data.moves.get(s.me.moves[slot], '?') if 0 <= slot < 4 else '?'

    def interpret(self, text, s):
        opts = self.options(s)
        facts = {'transcript': text, 'my_pokemon': s.me.name,
                 'my_moves': [self.data.moves.get(m, '-') for m in s.me.moves],
                 'party': [f'{m.name} ({m.hp}/{m.max_hp} HP)' for m in s.party]}
        if self.jev:
            try:
                choice, conf, probs, ms = self.jev.choice(
                    facts, 'A trainer shouted this to their Pokémon during a real-time battle. Which command did they give?', opts)
                return choice, conf, f'Jev {conf:.2f}, {ms:.0f} ms'
            except JevError as e:
                self.log(f'  Jev unavailable ({e}); using the built-in rules.')
        return self.rules_intent(text, s, opts), 1.0, 'rules'

    def rules_intent(self, text, s, opts):
        t = ' ' + re.sub(r'[^a-z ]', ' ', text.lower()) + ' '
        for mon in s.party:
            if f'switch_{mon.slot}' in opts and mon.name.lower() in t:
                return f'switch_{mon.slot}'
        for i, move in enumerate(s.me.moves):
            name = self.data.moves.get(move, '').lower()
            if f'use_move_{i}' in opts and name and name in t:
                return f'use_move_{i}'
        for key, words in (('stop', ['stop', 'stay', 'wait', 'hold']), ('retreat', ['away', 'back off', 'retreat', 'run', 'get back']),
                           ('approach', ['closer', 'approach', 'charge', 'go at', 'get him', 'get her', 'get it']),
                           ('attack', ['attack', 'hit', 'fire', 'now']), ('resume', ['resume', 'unpause', 'start'])):
            if key in opts and any(f' {w} ' in t or w in t for w in words):
                return key
        dodge = any(w in t for w in ('dodge', 'dash', 'jump', 'evade'))
        vertical = 'up' if ' up ' in t or 'north' in t else 'down' if ' down ' in t or 'south' in t else ''
        horizontal = 'left' if 'left' in t else 'right' if 'right' in t else ''
        if dodge and (vertical or horizontal):
            return 'dodge_' + (horizontal or vertical)
        if vertical or horizontal:
            return 'move_' + ('_'.join(x for x in (vertical, horizontal) if x))
        return 'none'

    # ----- commands -------------------------------------------------------
    def command(self, text):
        s = self.state()
        if not s:
            self.log('  The game is not connected yet.')
            return
        if re.search(r'\b(reset|restart|start over)\b', text.lower()) and self.reset_state:
            self.executor.cancel()
            with self.lock:
                self.goal_id += 1
            ok = self.bridge.load_state(self.reset_state)
            self.log('  → reset the fight' if ok else '  Could not reset: the game is not connected.')
            return
        if s.screen != 'arena' and not (s.screen == 'battle' and s.me.hp == 0):
            self.log('  Not in a real-time battle right now.')
            return
        intent, conf, how = self.interpret(text, s)
        if intent == 'none' or conf < 0.35:
            self.log(f'  ? "{text}" was not understood as a command ({how}).')
            return
        self.log(f'  → {self.describe(intent, s)} ({how})')
        with self.lock:
            self.goal_id += 1
            goal = self.goal_id
        self.start(intent, s, goal)

    def describe(self, intent, s):
        if intent.startswith('use_move_'):
            return 'use ' + self.move_name(s, int(intent[-1]))
        if intent.startswith('switch_'):
            mon = next((m for m in s.party if m.slot == int(intent[7:])), None)
            return 'switch to ' + (mon.name if mon else '?')
        return intent.replace('_', ' ')

    def start(self, intent, s, goal):
        ex = self.executor
        if intent == 'stop':
            ex.cancel()
        elif intent == 'resume':
            ex.run(Sequence(self.unpause()))
        elif intent.startswith('move_'):
            dx, dy = DIRS[intent[5:]]
            goal_pt = nav.toward(s.me.x, s.me.y, dx, dy, s.solid_mask, extra=[nav.body_rect(s.rival.x, s.rival.y)])
            self.run(Sequence(self.unpause() + [GoTo(goal_pt, budget=150, name=intent)]), goal, intent)
        elif intent.startswith('dodge_'):
            ex.run(Sequence(self.unpause() + [Dodge(intent[6:])]))
        elif intent.startswith('switch_'):
            slot = int(intent[7:])
            mon = next(m for m in s.party if m.slot == slot)
            ex.run(Sequence(self.unpause() + [Switch(self.bridge, mon.personality, mon.name)]))
        elif intent == 'approach':
            self.run(Sequence(self.unpause() + [GoTo(lambda st: (st.rival.x, st.rival.y), avoid_rival=False,
                                                     stop=lambda st: st.distance() <= 30, budget=240, name='approach')]),
                     goal, 'get closer')
        elif intent == 'retreat':
            spot = nav.open_spot_away((s.me.x, s.me.y), (s.rival.x, s.rival.y), s.solid_mask)
            # Running for the whole budget is a fine retreat: no failure report.
            ex.run(Sequence(self.unpause() + [GoTo(spot, stop=lambda st: st.distance() >= 120, budget=180,
                                                   name='retreat')]))
        else:
            slot = int(intent[-1]) if intent.startswith('use_move_') else s.selected_slot
            move = s.me.moves[slot]
            profile = self.data.profiles.get(move)
            name = self.move_name(s, slot)
            in_reach = lambda st: self.in_reach(st, profile)
            steps = self.unpause() + [SelectMove(slot)]
            if profile and profile.kind != 'self':
                # Close the distance first (around rocks), then fire.
                steps.append(GoTo(lambda st: (st.rival.x, st.rival.y), stop=in_reach, budget=300, name='close in',
                                  avoid_rival=False))
            aim = (lambda st: st.direction_to_rival()) if name.lower() == 'rollout' else None
            steps.append(FireWhen(in_reach, aim))
            self.run(Sequence(steps), goal, name)

    @staticmethod
    def reach(profile):
        """How close to stand to land a move: melee from right beside the
        rival, beams and projectiles from further back (as the game's AI)."""
        k, r = profile.kind, profile.range
        if k == 'melee':
            return max(20, min(r - 8, 30))
        if k == 'rush':
            return max(40, r - 22)
        if k == 'cone':
            return max(28, min(r - 10, 70))
        if k == 'projectile':
            return max(40, min(r - 10, 110))
        return 10 ** 6

    def in_reach(self, s, profile):
        if not profile or profile.kind == 'self':
            return True
        if s.distance() > self.reach(profile):
            return False
        # Shots and beams need a clear line; blows land from beside the rival.
        return profile.kind == 'melee' or nav.line_clear(s.me.x, s.me.y, s.rival.x, s.rival.y, s.solid_mask, radius=3)

    def run(self, action, goal, what):
        """Start an action and report if it could not be finished."""
        ex = self.executor
        ex.run(action)

        def watch():
            ex.done_event.wait(10)
            if self.goal_id == goal and ex.done_event.is_set() and not ex.last_ok:
                self.log(f'  Could not {"use " + what if what[0].isupper() else what} in time.')
        threading.Thread(target=watch, daemon=True).start()
