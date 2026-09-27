"""Static game knowledge read from the build's own sources: symbol addresses,
species and move names, the arena's move profiles, the text charset, and the
decoding of party Pokémon (whose species sits in encrypted substructures)."""
import re
import struct
from dataclasses import dataclass
from pathlib import Path

KIND_NAMES = {1: 'melee', 2: 'rush', 3: 'projectile', 4: 'cone', 5: 'self'}


def load_symbols(path):
    """Symbol addresses from `arm-none-eabi-nm` output (.sym, which includes
    static functions such as the party menu's callbacks) or a GNU ld map."""
    symbols = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'^([0-9a-f]{8}) [A-Za-z] (\w+)\s*$', line) or \
            re.match(r'\s+0x([0-9a-f]{8})\s+(\w+)(?:\s*=\s*\.)?\s*$', line)
        if m:
            symbols.setdefault(m.group(2), int(m.group(1), 16))
    return symbols


def _defines(text, prefix):
    return {m.group(1): int(m.group(2)) for m in re.finditer(r'#define\s+(' + prefix + r'\w+)\s+(\d+)\b', text)}


def _names(text, prefix):
    return {m.group(1): m.group(2) for m in re.finditer(r'\[(' + prefix + r'\w+)\]\s*=\s*_\("([^"]*)"\)', text)}


@dataclass
class MoveProfile:
    move: int
    kind: str
    windup: int
    active: int
    recovery: int
    range: int


def _title(name):
    return ' '.join(w.capitalize() for w in name.split())


class GameData:
    def __init__(self, source_root):
        root = Path(source_root)
        species_ids = _defines((root / 'include/constants/species.h').read_text(), 'SPECIES_')
        move_ids = _defines((root / 'include/constants/moves.h').read_text(), 'MOVE_')
        species_names = _names((root / 'src/data/text/species_names.h').read_text(), 'SPECIES_')
        move_names = _names((root / 'src/data/text/move_names.h').read_text(), 'MOVE_')
        self.species = {species_ids[k]: _title(v) for k, v in species_names.items() if k in species_ids}
        self.moves = {move_ids[k]: _title(v) for k, v in move_names.items() if k in move_ids}
        self.profiles = {}
        table = (root / 'src/arena_moves.c').read_text()
        for m in re.finditer(r'\{\s*(MOVE_\w+)\s*,([^}]*)\}', table):
            fields = [f.strip() for f in m.group(2).split(',')]
            try:
                kind = {'ARENA_MOVE_MELEE': 1, 'ARENA_MOVE_RUSH': 2, 'ARENA_MOVE_PROJECTILE': 3,
                        'ARENA_MOVE_CONE': 4, 'ARENA_MOVE_SELF': 5}[fields[1]]
                move = move_ids[m.group(1)]
                self.profiles[move] = MoveProfile(move, KIND_NAMES[kind], int(fields[3]), int(fields[4]),
                                                  int(fields[5]), int(fields[6]))
            except (KeyError, ValueError, IndexError):
                continue
        self.charset = {}
        for line in (root / 'charmap.txt').read_text(encoding='utf-8', errors='replace').splitlines():
            m = re.match(r"^'(.)'\s*=\s*([0-9A-Fa-f]{2})\s*$", line)
            if m:
                self.charset.setdefault(int(m.group(2), 16), m.group(1))
        self.charset[0x00] = ' '

    def text(self, raw):
        out = []
        for b in raw:
            if b == 0xFF:
                break
            out.append(self.charset.get(b, '?'))
        return ''.join(out).strip()


# Order of the four 12-byte substructures (Growth, Attacks, EVs, Misc) by personality % 24.
_ORDERS = ['GAEM', 'GAME', 'GEAM', 'GEMA', 'GMAE', 'GMEA', 'AGEM', 'AGME', 'AEGM', 'AEMG', 'AMGE', 'AMEG',
           'EGAM', 'EGMA', 'EAGM', 'EAMG', 'EMGA', 'EMAG', 'MGAE', 'MGEA', 'MAGE', 'MAEG', 'MEGA', 'MEAG']


@dataclass
class PartyMon:
    slot: int                 # index in gPlayerParty, which is also the party menu's slot
    personality: int
    species: int
    name: str
    level: int
    hp: int
    max_hp: int
    is_egg: bool


def decode_party(raw, data):
    """The six 100-byte party entries. Species needs the substructure XOR key."""
    party = []
    for slot in range(6):
        b = raw[slot * 100:(slot + 1) * 100]
        personality, ot_id = struct.unpack_from('<II', b, 0)
        if not personality and not ot_id:
            continue
        key = personality ^ ot_id
        secure = bytearray(b[32:80])
        for i in range(0, 48, 4):
            struct.pack_into('<I', secure, i, struct.unpack_from('<I', secure, i)[0] ^ key)
        growth = _ORDERS[personality % 24].index('G') * 12
        species = struct.unpack_from('<H', secure, growth)[0]
        flags = b[19]
        if not species:
            continue
        level, hp, max_hp = b[84], struct.unpack_from('<H', b, 86)[0], struct.unpack_from('<H', b, 88)[0]
        name = data.text(b[8:18]) or data.species.get(species, '?')
        party.append(PartyMon(slot, personality, species, name.title(), level, hp, max_hp, bool(flags & 4)))
    return party
