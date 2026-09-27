"""From words to play. Jev first maps what was said onto the commands that
are possible right now (intent). Goals that take more than one input
(approach, retreat, use a move) then run a control loop: each step Jev picks
one controller macro from facts computed about the arena, and the executor
carries it out frame by frame. A rule-based policy stands in when Jev is not
available, so the game never waits on the network."""
import re
import threading
import time

from .executor import Attack, DIR_KEYS, Dodge, Hold, Sequence, SelectMove, Switch, Tap, Unpause, Wait, Walk
from .jev import JevError
from .state import DIRS, KEYS, blocked, direction_name

CARDINAL = ['up', 'down', 'left', 'right']


class Brain:
    def __init__(self, bridge, executor, data, jev=None, log=print, verbose=False):
        self.bridge, self.executor, self.data, self.jev, self.log, self.verbose = bridge, executor, data, jev, log, verbose
        self.goal_id = 0
        self.lock = threading.Lock()

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
                opts[f'use_move_{i}'] = f'Use the move {self.data.moves.get(move, "?")} ({p.kind}, range {p.range}px)'
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
        s = self.bridge.state
        if not s:
            self.log('  The game is not connected yet.')
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
            ex.run(Unpause())
        elif intent.startswith('move_'):
            ex.run(Sequence([Unpause(), Walk(intent[5:])]))
        elif intent.startswith('dodge_'):
            ex.run(Sequence([Unpause(), Dodge(intent[6:])]))
        elif intent.startswith('switch_'):
            slot = int(intent[7:])
            mon = next(m for m in s.party if m.slot == slot)
            ex.run(Sequence([Unpause(), Switch(self.bridge, mon.personality, mon.name)]))
        else:
            slot = int(intent[-1]) if intent.startswith('use_move_') else s.selected_slot
            goal_kind = 'use_move' if intent.startswith('use_move_') or intent == 'attack' else intent
            threading.Thread(target=self.control, args=(goal_kind, slot, goal), daemon=True).start()

    # ----- control loop -----------------------------------------------------
    def facts(self, s, goal, slot):
        profile = self.data.profiles.get(s.me.moves[slot]) if goal == 'use_move' else None
        dist = round(s.distance())
        f = {'goal': {'approach': 'get close to the opponent (within about 36 px)',
                      'retreat': 'get far from the opponent (more than 110 px away)',
                      'use_move': f'hit the opponent with {self.move_name(s, slot)}'}[goal],
             'me': {'x': s.me.x, 'y': s.me.y, 'hp': f'{s.me.hp}/{s.me.max_hp}', 'attack_ready': s.me.cooldown == 0,
                    'dash_ready': s.me.dash_cooldown == 0},
             'opponent': {'x': s.rival.x, 'y': s.rival.y, 'hp': f'{s.rival.hp}/{s.rival.max_hp}', 'doing': s.rival.phase},
             'distance_px': dist, 'direction_to_opponent': s.direction_to_rival(),
             'direction_away_from_opponent': direction_name(s.me.x - s.rival.x, s.me.y - s.rival.y),
             'blocked_directions': [d for d in DIRS if blocked(s, d)]}
        # For each open direction, the distance to the opponent after a short
        # walk that way, best first for this goal: Jev weighs these instead of
        # doing geometry.
        steps = []
        for d, (dx, dy) in DIRS.items():
            if d in f['blocked_directions']:
                continue
            k = 0.7071 if dx and dy else 1.0
            after = ((s.me.x + dx * 24 * k - s.rival.x) ** 2 + (s.me.y + dy * 24 * k - s.rival.y) ** 2) ** 0.5
            steps.append((d, round(after)))
        steps.sort(key=lambda t: -t[1] if goal == 'retreat' else t[1])
        f['open_directions_best_first'] = [f'{d} ({after} px from opponent after)' for d, after in steps]
        if profile:
            f['move'] = {'name': self.move_name(s, slot), 'kind': profile.kind, 'range_px': profile.range,
                         'in_range': dist <= profile.range or profile.kind == 'self', 'selected': s.selected_slot == slot}
        return f

    def macro_options(self, goal):
        opts = {'move_' + d: f'Walk {d.replace("_", " and ")} for a moment' for d in DIRS}
        for d in CARDINAL:
            opts['dash_' + d] = f'Quick dash {d}: fast, evades attacks, has a cooldown'
        if goal == 'use_move':
            opts['attack'] = 'Fire the selected move now (only when it is in range and ready)'
        opts['wait'] = 'Stand still for a moment'
        opts['done'] = 'The goal is achieved; stop controlling'
        return opts

    def rules_macro(self, s, goal, f):
        free = [d for d in DIRS if d not in f['blocked_directions']]
        if goal == 'approach':
            if f['distance_px'] <= 36:
                return 'done'
            want = f['direction_to_opponent']
        elif goal == 'retreat':
            if f['distance_px'] >= 110:
                return 'done'
            want = f['direction_away_from_opponent']
        else:
            m = f['move']
            if m['in_range']:
                return 'attack' if s.me.cooldown == 0 else 'wait'
            want = f['direction_to_opponent']
        if want in free:
            return 'move_' + want
        # Steer around: the free direction closest in angle to the one wanted.
        order = ['right', 'down_right', 'down', 'down_left', 'left', 'up_left', 'up', 'up_right']
        i = order.index(want)
        for step in (1, -1, 2, -2, 3, -3):
            for d in free:
                if order.index(d) == (i + step) % 8:
                    return 'move_' + d
        return 'wait'

    def control(self, goal, slot, goal_id):
        ex = self.executor
        started, fired, decisions = time.time(), False, 0
        budget = {'approach': 4.0, 'retreat': 3.0, 'use_move': 6.0}[goal]
        if goal == 'use_move':
            ex.run(Sequence([Unpause(), SelectMove(slot)]))
            ex.done_event.wait(3)
            if self.goal_id != goal_id:
                return
        while self.goal_id == goal_id and time.time() - started < budget:
            s = self.bridge.state
            if not s or s.screen != 'arena':
                return
            f = self.facts(s, goal, slot)
            macro, how = None, 'rules'
            # Whether the goal is met is arithmetic; Jev decides how to get there.
            if (goal == 'approach' and f['distance_px'] <= 36) or (goal == 'retreat' and f['distance_px'] >= 110):
                macro, how = 'done', 'goal reached'
            elif self.jev:
                try:
                    macro, conf, _, ms = self.jev.choice(f, 'You control a Pokémon in a real-time top-down arena (screen '
                                                         'coordinates, y grows downward). Pick the next controller action '
                                                         'that best achieves the goal. open_directions_best_first ranks '
                                                         'the walkable directions for this goal; dash when the opponent '
                                                         'is attacking and close.', self.macro_options(goal))
                    how = f'Jev {conf:.2f} {ms:.0f} ms'
                except JevError as e:
                    self.log(f'  Jev unavailable ({e}); rules take over.')
                    self.jev = None
            if macro is None:
                macro = self.rules_macro(s, goal, f)
            if self.goal_id != goal_id:
                return
            decisions += 1
            if self.verbose:
                self.log(f'    [{goal}] {f["distance_px"]:>3} px {f["direction_to_opponent"]:>10} → {macro} ({how})')
            if macro == 'done':
                break
            if macro == 'attack':
                if goal == 'use_move' and not f['move']['in_range']:
                    macro = 'move_' + f['direction_to_opponent']
                else:
                    move = s.me.moves[slot]
                    aim = f['direction_to_opponent'] if self.data.moves.get(move, '').lower() == 'rollout' else None
                    ex.run(Attack(aim))
                    ex.done_event.wait(1)
                    fired = True
                    break
            if macro.startswith('move_'):
                ex.run(Hold(DIR_KEYS[macro[5:]], 20, macro))
            elif macro.startswith('dash_'):
                ex.run(Dodge(macro[5:]))
            elif macro == 'wait':
                ex.run(Wait(8))
            # Decide again as soon as the previous macro is mostly under way.
            time.sleep(0.05 if self.jev else 0.12)
        if self.goal_id == goal_id:
            if goal == 'use_move' and not fired:
                self.log(f'  Could not land {self.move_name(self.bridge.state, slot)} in time.')
            elif self.verbose:
                self.log(f'  [{goal}] finished after {decisions} decisions.')
