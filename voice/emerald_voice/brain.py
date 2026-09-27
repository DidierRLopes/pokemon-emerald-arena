"""From words to play. Jev maps what was said onto the commands that are
possible right now (intent), told what each move needs (a close-range blow
means running up to the opponent first). The agent then plans the way: every
walk, approach, retreat or attack follows a path around the rocks, re-planned
every few frames as the opponent moves, and the executor carries it out frame
by frame. Rules stand in for Jev when it is not available."""
import difflib
import re
import threading
import time

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
        if self.bridge.state and self.bridge.state.draft_screen:
            self.draft_command(text, self.bridge.state)
            return
        if s.screen != 'arena' and not (s.screen == 'battle' and s.me.hp == 0):
            self.log('  Not in a real-time battle right now.')
            return
        clauses = self.split_chain(text)
        if len(clauses) > 1:
            # Understand every part at once (Jev calls run in parallel).
            from concurrent.futures import ThreadPoolExecutor
            with ThreadPoolExecutor(len(clauses)) as pool:
                parts = list(pool.map(lambda c: self.interpret(c, s), clauses))
            chain = [(c, p) for c, p in zip(clauses, parts) if p[0] != 'none' and p[1] >= 0.35]
            if len(chain) >= 2:
                chain = chain[:self.QUEUE]
                self.log('  → ' + ', then '.join(self.describe(p[0], s) for _, p in chain)
                         + ' (' + ', '.join(p[2] for _, p in chain) + ')')
                self.toast(', '.join(c.strip().rstrip('.,') for c, _ in chain))
                with self.lock:
                    self.goal_id += 1
                    goal = self.goal_id
                threading.Thread(target=self.run_chain, args=(chain, goal), daemon=True).start()
                return
        intent, conf, how = self.interpret(text, s)
        if intent == 'none' or conf < 0.35:
            self.log(f'  ? "{text}" was not understood as a command ({how}).')
            return
        self.log(f'  → {self.describe(intent, s)} ({how})')
        self.toast(text)
        with self.lock:
            self.goal_id += 1
            goal = self.goal_id
        self.start(intent, s, goal)

    # ----- chains: "retreat and then shadow ball" ------------------------------
    QUEUE = 2       # at most two commands queued from one sentence

    @staticmethod
    def split_chain(text):
        """The parts of a chained order. "then" (and "after that", "followed
        by") always separates; a plain "and" or comma only if nothing else
        does, and each part must then read as a command of its own."""
        t = text.strip().rstrip('.!?')
        parts = [p for p in re.split(r'\s*(?:,?\s*\band then\b|,?\s*\bthen\b|\bafter that\b|\bfollowed by\b|\band after\b)\s*',
                                     t, flags=re.I) if p and p.strip(' ,')]
        if len(parts) < 2 and not re.search(r'\b(come back|return)\b', t, flags=re.I):
            parts = [p for p in re.split(r'\s*(?:,\s*and\b|\band\b|,)\s*', t, flags=re.I) if p and p.strip(' ,')]
        return [p.strip(' ,') for p in parts]

    def run_chain(self, chain, goal):
        """Each command once the one before it has finished (or given up)."""
        for n, (clause, (intent, conf, how)) in enumerate(chain):
            if self.goal_id != goal:
                return
            s = self.state()
            if not s or s.screen != 'arena':
                return
            if n and chain[n - 1][1][0].startswith('switch_'):
                # A new Pokémon has other moves: read the next part again.
                intent, conf, how = self.interpret(clause, s)
                if intent == 'none' or conf < 0.35:
                    return
            if n:
                self.log(f'  → next: {self.describe(intent, s)}')
            self.start(intent, s, goal)
            time.sleep(0.1)
            deadline = time.time() + 12
            while self.goal_id == goal and time.time() < deadline:
                if self.executor.done_event.wait(0.05):
                    break

    # ----- trainer call-outs ------------------------------------------------
    def toast(self, text):
        """What was said, in a speech bubble from this player's trainer."""
        live = self.bridge.state
        if not live:
            return
        line = text.strip().rstrip('.').strip()
        if not line:
            return
        line = line[0].upper() + line[1:]
        if line[-1] not in '!?':
            line += '!'
        data = self.data.encode(line, limit=26)
        self.bridge.write_bytes('gArenaToastText', data, offset=28 * self.side)
        self.bridge.write_bytes('gArenaToastSeq', [(live.toast_seq[self.side] + 1) & 255], offset=self.side)

    # ----- the draft screens ----------------------------------------------------
    PICK, REMOVE, DONE, SHOW = 1, 2, 3, 4

    def find_species(self, text):
        """Roster indexes of the Pokémon named in `text`, in spoken order;
        forgiving of Whisper's spellings ("mew two", "gengard")."""
        names = {re.sub(r'[^a-z]', '', self.data.species.get(sp, '').lower()): i
                 for i, sp in enumerate(self.data.roster)}
        words = re.sub(r'[^a-z ]', ' ', text.lower()).split()
        found, i = [], 0
        while i < len(words):
            best = None
            for n in (3, 2, 1):
                if i + n > len(words):
                    continue
                joined = ''.join(words[i:i + n])
                if len(joined) < 3:
                    continue
                if joined in names:
                    best = (1.0, n, names[joined])
                    break
                match = difflib.get_close_matches(joined, names, n=1, cutoff=0.8)
                if match:
                    score = difflib.SequenceMatcher(None, joined, match[0]).ratio()
                    if not best or score > best[0]:
                        best = (score, n, names[match[0]])
            if best:
                if best[2] not in found:
                    found.append(best[2])
                i += best[1]
            else:
                i += 1
        return found

    def draft_request(self, op, index=0):
        """One request to the draft screen, after the previous one is taken."""
        for _ in range(60):
            live = self.bridge.state
            if live and not live.draft_busy:
                break
            time.sleep(1 / 60)
        self.bridge.write_bytes('gArenaDraftVoice', [index], offset=1)
        self.bridge.write_bytes('gArenaDraftVoice', [op])
        time.sleep(3 / 60)

    def draft_command(self, text, s):
        t = ' ' + re.sub(r"[^a-z' ]", ' ', text.lower()) + ' '
        done = re.search(r"\b(done|ready|finish(ed)?|that's (it|all)|confirm|let'?s (go|fight|battle)|start( the)? (battle|fight)|fight|battle)\b", t)
        if s.draft_screen == 3:
            if done or re.search(r'\b(go|start|begin)\b', t):
                self.log('  → start the battle')
                self.draft_request(self.DONE)
            else:
                self.log(f'  ? "{text}": say "fight" to start the battle.')
            return
        picks = self.find_species(text)
        remove = re.search(r"\b(remove|drop|undo|take (out|off)|get rid|unpick|delete|without|kick|swap out)\b", t)
        show = re.search(r"\b(show|look|what about|what's|info|details|see|check)\b", t) and not re.search(
            r'\b(pick|choose|take|add|want|get|draft|select|go with)\b', t)
        op = self.REMOVE if remove else self.SHOW if show else self.PICK
        verb = {self.PICK: 'pick', self.REMOVE: 'remove', self.SHOW: 'show'}[op]
        for index in picks:
            self.log(f'  → {verb} {self.data.species.get(self.data.roster[index], "?")}')
            self.draft_request(op, index)
        if done and op != self.REMOVE:
            self.log('  → done drafting')
            self.draft_request(self.DONE)
        if not picks and not done:
            self.log(f'  ? "{text}": no Pokémon name heard (say e.g. "pick Charizard", "remove Pikachu", "done").')

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
