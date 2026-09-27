"""What the agent watches in memory each frame, and its decoding into a
GameState. Offsets follow the structs in the arena's headers (0.10.1)."""
import math
import struct
from dataclasses import dataclass, field

from .gamedata import decode_party

# (name, symbol, bytes)
WATCH = [
    ('tel', 'gRealtimeArenaTelemetry', 60),
    ('mov', 'gArenaMoveTelemetry', 36),
    ('cmb', 'gArenaCombatTelemetry', 28),
    ('ai', 'gArenaAiTelemetry', 44),
    ('phy', 'gArenaPhysicsTelemetry', 56),
    ('mons', 'gBattleMons', 0x58 * 2),
    ('main', 'gMain', 8),
    ('pmenu', 'gPartyMenu', 12),
    ('bpi', 'gBattlerPartyIndexes', 4),
    ('party', 'gPlayerParty', 600),
    ('btf', 'gBattleTypeFlags', 4),
    ('tasks', 'gTasks', 16 * 40),
    ('draft', 'gArenaDraftBattle', 1),
    ('dmode', 'gArenaDraftMode', 1),
    ('p2slot', 'gArenaP2MoveSlot', 1),
    ('dscreen', 'gArenaDraftScreen', 1),
    ('dvoice', 'gArenaDraftVoice', 2),
    ('toast', 'gArenaToastSeq', 2),
]
OPTIONAL = {'gArenaDraftBattle', 'gArenaDraftMode', 'gArenaP2MoveSlot',   # Arena-mode builds only
            'gArenaDraftScreen', 'gArenaDraftVoice', 'gArenaToastSeq'}
KEYS = {'A': 1, 'B': 2, 'SELECT': 4, 'START': 8, 'RIGHT': 16, 'LEFT': 32, 'UP': 64, 'DOWN': 128, 'R': 256, 'L': 512}
FACING = ['down', 'down_right', 'right', 'up_right', 'up', 'up_left', 'left', 'down_left']
PHASES = {0: 'idle', 1: 'winding_up', 2: 'attacking', 3: 'recovering'}
OBSTACLES = [(108, 69, 133, 86), (60, 39, 81, 56), (169, 99, 190, 117), (24, 90, 42, 106),
             (28, 56, 45, 69), (201, 46, 213, 62), (202, 80, 218, 94)]
BOUNDS = (12, 32, 228, 143)
DIRS = {'up': (0, -1), 'down': (0, 1), 'left': (-1, 0), 'right': (1, 0),
        'up_left': (-1, -1), 'up_right': (1, -1), 'down_left': (-1, 1), 'down_right': (1, 1)}


def s16(b, o):
    return struct.unpack_from('<h', b, o)[0]


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


@dataclass
class Fighter:
    x: int
    y: int
    species: int
    name: str
    hp: int
    max_hp: int
    moves: list
    pp: list
    facing: str = 'down'
    phase: str = 'idle'
    action_move: int = 0
    cooldown: int = 0
    dash_cooldown: int = 0


@dataclass
class GameState:
    frame: int
    in_arena: bool
    paused: bool
    selected_slot: int
    me: Fighter
    rival: Fighter
    party: list
    active_party_slot: int
    screen: str               # arena | battle | menu (party menu or a transition)
    menu_cursor: int
    party_input: bool         # the party menu is waiting for a Pokémon to be chosen
    party_submenu: bool       # the SHIFT / SUMMARY / CANCEL list is open
    solid_mask: int
    trainer_battle: bool
    dodges: int = 0           # the arena's count of the player's dodges
    pvp: bool = False         # an Arena-mode 1P vs 1P battle
    draft_screen: int = 0     # 1/2: Player 1/2 drafting, 3: the versus screen
    draft_busy: bool = False  # the draft screen hasn't taken the last request yet
    toast_seq: tuple = (0, 0)
    side: int = 0             # whose point of view this is
    raw: dict = field(repr=False, default_factory=dict)

    def distance(self):
        return math.hypot(self.rival.x - self.me.x, self.rival.y - self.me.y)

    def direction_to_rival(self):
        return direction_name(self.rival.x - self.me.x, self.rival.y - self.me.y)


def direction_name(dx, dy):
    if not dx and not dy:
        return 'none'
    angle = math.degrees(math.atan2(dy, dx))   # screen y grows down
    names = ['right', 'down_right', 'down', 'down_left', 'left', 'up_left', 'up', 'up_right']
    return names[int(((angle + 360 + 22.5) % 360) // 45)]


def blocked(state, direction, reach=18):
    """Whether moving `reach` px that way runs into a wall or a standing rock."""
    dx, dy = DIRS[direction]
    x, y = state.me.x + dx * reach, state.me.y + dy * reach
    if not (BOUNDS[0] <= x <= BOUNDS[2] and BOUNDS[1] <= y <= BOUNDS[3]):
        return True
    for i, (l, t, r, b) in enumerate(OBSTACLES):
        if state.solid_mask & (1 << i) and l - 10 <= x <= r + 10 and t - 10 <= y <= b + 10:
            return True
    return False


def parse(frame, chunks, data, symbols):
    tel, mov, cmb, ai, phy, mons, main, pmenu, bpi, party_raw, btf, tasks = (chunks[n] for n, _, _ in WATCH[:12])
    funcs = {u32(tasks, i * 40) & ~1 for i in range(16) if tasks[i * 40 + 4]}

    def fighter(side):
        b = mons[side * 0x58:(side + 1) * 0x58]
        species = u16(b, 0)
        moves = [u16(b, 0x0C + 2 * i) for i in range(4)]
        return Fighter(x=s16(tel, 44 + 2 * side), y=s16(tel, 48 + 2 * side), species=species,
                       name=data.species.get(species, '?'), hp=u16(b, 0x28), max_hp=u16(b, 0x2C),
                       moves=moves, pp=[b[0x24 + i] for i in range(4)],
                       facing=FACING[ai[33 + side] % 8], phase=PHASES.get(mov[26 + side], 'idle'),
                       action_move=u16(mov, 32 + 2 * side), cooldown=u16(cmb, 12 + 2 * side),
                       dash_cooldown=u16(cmb, 20 + 2 * side))

    callback2 = u32(main, 4) & ~1
    # The game swaps party entries on a switch, so the Pokémon out in battle is
    # found by its personality value rather than by a party index.
    party = decode_party(party_raw, data)
    personality = u32(mons, 0x48)
    active_slot = next((m.slot for m in party if m.personality == personality), u16(bpi, 0))
    in_arena = bool(tel[56])
    screen = 'arena' if in_arena else 'battle' if callback2 == (symbols.get('BattleMainCB2', -1) & ~1) else 'menu'
    return GameState(frame=frame, in_arena=in_arena, paused=bool(tel[57]), selected_slot=tel[59],
                     me=fighter(0), rival=fighter(1), party=party,
                     active_party_slot=active_slot, screen=screen, menu_cursor=struct.unpack_from('<b', pmenu, 9)[0],
                     party_input=(symbols.get('Task_HandleChooseMonInput', -1) & ~1) in funcs,
                     party_submenu=(symbols.get('Task_HandleSelectionMenuInput', -1) & ~1) in funcs,
                     solid_mask=phy[54], trainer_battle=bool(u32(btf, 0) & 8), dodges=u32(tel, 28),
                     pvp=bool(chunks.get('draft', b'\0')[0]) and chunks.get('dmode', b'\0')[0] == 2,
                     draft_screen=chunks.get('dscreen', b'\0')[0], draft_busy=bool(chunks.get('dvoice', b'\0\0')[0]),
                     toast_seq=tuple(chunks.get('toast', b'\0\0')[:2]),
                     raw=chunks)


def view(state, side):
    """The same frame from Player 2's side: its Pokémon is `me`."""
    if not state or side == 0:
        return state
    import dataclasses
    return dataclasses.replace(state, me=state.rival, rival=state.me, side=1, party=[],
                               selected_slot=state.raw.get('p2slot', b'\0')[0], dodges=0)
