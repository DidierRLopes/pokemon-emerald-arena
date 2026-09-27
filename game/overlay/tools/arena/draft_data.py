#!/usr/bin/env python3
"""Arena draft data: for each of the arena's 150 animated Pokémon, a credit
cost and a fixed level-100 moveset, written as a C table.

Moves: only ones the Pokémon can legally know (its own and its
pre-evolutions' level-up moves, TMs/HMs, tutors, egg moves) that the arena
plays in real time. The four picks favour strong, accurate, same-type moves,
then type coverage, a mix of ranged and close-range, and one utility move
when the attacks run out.

Moves are also weighed by their arena wind-up, since a slow move lands
less often in real time.

Cost: from base stat total on a line through Bulbasaur 6 and Venusaur 15,
capped at 20 (Mewtwo and the other box legendaries), at least 2, with a
discount for Pokémon whose real-time moveset is thin and a few manual
overrides (Shedinja's Wonder Guard).

Run from the pokeemerald workspace: python3 tools/arena/draft_data.py
"""
import json
import re
import struct
import zlib
from pathlib import Path

ROOT = Path.cwd()
OUT = ROOT / 'src/data/arena_draft.h'
# Grid miniatures are derived from the game's icons, so they are written into
# the workspace at build time and never committed.
MINIS = ROOT / 'src/data/arena_draft_minis.h'


def read(p):
    return (ROOT / p).read_text()


def defines(text, prefix):
    return {m.group(1): int(m.group(2)) for m in re.finditer(r'#define\s+(' + prefix + r'\w+)\s+(\d+)\b', text)}


species_ids = defines(read('include/constants/species.h'), 'SPECIES_')
move_ids = defines(read('include/constants/moves.h'), 'MOVE_')
move_names = {k: v for k, v in re.findall(r'\[(MOVE_\w+)\]\s*=\s*_\("([^"]*)"\)', read('src/data/text/move_names.h'))}

# Move data
moves = {}
for m in re.finditer(r'\[(MOVE_\w+)\]\s*=\s*\{(.*?)\n    \}', read('src/data/battle_moves.h'), re.S):
    body = m.group(2)
    get = lambda f, d='0': (re.search(r'\.' + f + r'\s*=\s*(\w+)', body) or [None, d])[1]
    moves[m.group(1)] = {'effect': get('effect'), 'power': int(get('power')), 'type': get('type'), 'acc': int(get('accuracy'))}

# Arena profiles: move -> kind, range
profiles = {}
kinds = {'ARENA_MOVE_MELEE': 'melee', 'ARENA_MOVE_RUSH': 'rush', 'ARENA_MOVE_PROJECTILE': 'projectile',
         'ARENA_MOVE_CONE': 'cone', 'ARENA_MOVE_SELF': 'self'}
for m in re.finditer(r'\{\s*(MOVE_\w+)\s*,([^}]*)\}', read('src/arena_moves.c')):
    f = [x.strip() for x in m.group(2).split(',')]
    if len(f) > 6 and f[1] in kinds:
        profiles[m.group(1)] = {'kind': kinds[f[1]], 'range': int(f[6]), 'windup': int(f[3])}

# Species data
info = read('src/data/pokemon/species_info.h')
species = {}
for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\n\s*\{(.*?)\n    \},', info, re.S):
    b = m.group(2)
    stat = lambda f: int(re.search(r'\.' + f + r'\s*=\s*(\d+)', b).group(1))
    types = re.search(r'\.types\s*=\s*\{\s*(\w+),\s*(\w+)\s*\}', b).groups()
    abilities = re.search(r'\.abilities\s*=\s*\{\s*(\w+),\s*(\w+)\s*\}', b).groups()
    species[m.group(1)] = {'bst': sum(stat(f) for f in ('baseHP', 'baseAttack', 'baseDefense', 'baseSpeed', 'baseSpAttack', 'baseSpDefense')),
                           'atk': stat('baseAttack'), 'spa': stat('baseSpAttack'), 'types': set(types), 'abilities': abilities}

# Learnsets
lu_ptr = dict(re.findall(r'\[(SPECIES_\w+)\]\s*=\s*(s\w+LevelUpLearnset)', read('src/data/pokemon/level_up_learnset_pointers.h')))
lu_text = read('src/data/pokemon/level_up_learnsets.h')
lu = {name: set(re.findall(r'LEVEL_UP_MOVE\(\s*\d+,\s*(MOVE_\w+)\)', body))
      for name, body in re.findall(r'(s\w+LevelUpLearnset)\[\]\s*=\s*\{(.*?)\};', lu_text, re.S)}
tm = {}
for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\{\s*\.learnset\s*=\s*\{(.*?)\}\s*\}', read('src/data/pokemon/tmhm_learnsets.h'), re.S):
    tm[m.group(1)] = {'MOVE_' + x for x in re.findall(r'\.(\w+)\s*=\s*TRUE', m.group(2))}
tutor = {m.group(1): set(re.findall(r'TUTOR\((MOVE_\w+)\)', m.group(2)))
         for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\((.*?)\),?\n', read('src/data/pokemon/tutor_learnsets.h'), re.S)}
egg = {}
for m in re.finditer(r'egg_moves\((\w+),(.*?)\)', read('src/data/pokemon/egg_moves.h'), re.S):
    egg['SPECIES_' + m.group(1)] = set(re.findall(r'MOVE_\w+', m.group(2)))
pre = {}
for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\{(.*?)\}\s*,\s*\n', read('src/data/pokemon/evolution.h'), re.S):
    for target in re.findall(r'(SPECIES_\w+)\s*\}', m.group(2)):
        pre[target] = m.group(1)


def family(sp):
    chain = [sp]
    while chain[-1] in pre:
        chain.append(pre[chain[-1]])
    return chain


def learnable(sp):
    out = set()
    for s in family(sp):
        out |= lu.get(lu_ptr.get(s, ''), set()) | tm.get(s, set()) | tutor.get(s, set()) | egg.get(s, set())
    return out


UTILITY = ['MOVE_SUBSTITUTE', 'MOVE_DOUBLE_TEAM', 'MOVE_AGILITY', 'MOVE_TELEPORT', 'MOVE_REFLECT', 'MOVE_LIGHT_SCREEN',
           'MOVE_GROWTH', 'MOVE_HOWL', 'MOVE_FOCUS_ENERGY', 'MOVE_HARDEN']
RANGED = {'projectile', 'cone'}
TRAPPY = {'MOVE_DIG', 'MOVE_FLY'}          # strong but slow to land; never the only attack


def effective_power(mv, sp):
    d = moves[mv]
    if d['effect'] == 'EFFECT_LEVEL_DAMAGE':
        return 100                          # Seismic Toss / Night Shade at level 100
    if d['effect'] == 'EFFECT_ROLLOUT':
        return 75                           # ramps with distance in the arena
    p = d['power']
    if p <= 1:
        return 0
    return p * (d['acc'] or 100) / 100


def score(mv, sp):
    d, s = moves[mv], species[sp]
    p = effective_power(mv, sp)
    if not p:
        return 0
    stab = 1.5 if d['type'] in s['types'] else 1.0
    special = d['type'] in {'TYPE_FIRE', 'TYPE_WATER', 'TYPE_GRASS', 'TYPE_ELECTRIC', 'TYPE_ICE', 'TYPE_PSYCHIC', 'TYPE_DRAGON', 'TYPE_DARK'}
    stat = s['spa'] if special else s['atk']
    if d['effect'] == 'EFFECT_LEVEL_DAMAGE':
        stat = 100
    # In real time a slow wind-up costs hits: weigh by how fast it comes out.
    speed = 60 / (40 + profiles[mv]['windup'])
    trappy = 0.8 if mv in TRAPPY else 1.0
    return p * stab * (0.5 + stat / 200) * speed * trappy


def pick_moves(sp):
    pool = [m for m in learnable(sp) if m in profiles and m in moves]
    attacks = sorted((m for m in pool if score(m, sp) > 0), key=lambda m: -score(m, sp))
    chosen = []

    def take(m):
        if m and m not in chosen:
            chosen.append(m)

    # 1. the strongest attack, preferring one that is not Dig/Fly
    take(next((m for m in attacks if m not in TRAPPY), attacks[0] if attacks else None))
    # 2. best attack of a new type; prefer the other range class if close in strength
    def best_new(filter_=lambda m: True):
        types = {moves[m]['type'] for m in chosen}
        return next((m for m in attacks if m not in chosen and moves[m]['type'] not in types and filter_(m)), None)
    if chosen:
        other_range = (lambda m: profiles[m]['kind'] not in RANGED) if profiles[chosen[0]]['kind'] in RANGED \
            else (lambda m: profiles[m]['kind'] in RANGED)
        a, b = best_new(), best_new(other_range)
        take(b if b and a and score(b, sp) >= 0.75 * score(a, sp) else a)
    # 3. another new type, else the next best attack
    take(best_new() or next((m for m in attacks if m not in chosen), None))
    # 4. utility, if it has one; Rollout users take Defense Curl
    if 'MOVE_ROLLOUT' in chosen and 'MOVE_DEFENSE_CURL' in pool:
        take('MOVE_DEFENSE_CURL')
    elif len(chosen) < 4:
        take(next((m for m in UTILITY if m in pool), None))
    while len(chosen) < 4:
        m = next((m for m in attacks if m not in chosen), None) or next((m for m in pool if m not in chosen), None)
        if not m:
            break
        take(m)
    return chosen[:4]


OVERRIDES = {'SPECIES_SHEDINJA': 12}         # Wonder Guard: only super-effective hits land


def cost(sp, chosen):
    c = round(0.0435 * species[sp]['bst'] - 7.8)
    attacks = [m for m in chosen if score(m, sp) > 0]
    if len(attacks) < 2:
        c -= 2                                # thin real-time moveset
    return max(2, min(20, OVERRIDES.get(sp, c)))


SAFE_EXTRA = {'ABILITY_SHED_SKIN', 'ABILITY_SYNCHRONIZE', 'ABILITY_TRUANT', 'ABILITY_SPEED_BOOST', 'ABILITY_SOUNDPROOF',
              'ABILITY_LIQUID_OOZE', 'ABILITY_STICKY_HOLD', 'ABILITY_ROUGH_SKIN'}
src = read('src/realtime_arena.c')
a = src.index('static bool8 SupportedBattler')
supported = set(re.findall(r'case (ABILITY_\w+):', src[a:src.index('\n}\n', a)]))


def ability_num(sp):
    ab = species[sp]['abilities']
    for i in (0, 1):
        if ab[i] in supported and ab[i] != 'ABILITY_NONE':
            return i
    for i in (0, 1):
        if ab[i] in SAFE_EXTRA:
            return i
    return 0


roster = json.load(open(ROOT / 'tools/arena/roster.json'))
roster.sort(key=lambda r: int(r['dex']))
rows, report = [], []
for r in roster:
    sp = 'SPECIES_' + r['species']
    chosen = pick_moves(sp)
    c = cost(sp, chosen)
    ab = ability_num(sp)
    rows.append(f'    {{{sp}, {c}, {ab}, {{{", ".join(chosen + ["MOVE_NONE"] * (4 - len(chosen)))}}}}},')
    report.append(f'{r["species"]:<11} {c:>2}  {species[sp]["abilities"][ab][8:]:<13} ' +
                  ', '.join(f'{move_names[m]} ({profiles[m]["kind"]})' for m in chosen))
# ---- 8x8 grid miniatures from the party icons (indexed PNG, first frame)


def read_png_indexed(path):
    data = Path(path).read_bytes()
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', chunk[:10])
        elif kind == b'IDAT':
            idat += chunk
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = (w * depth + 7) // 8
    bpp = max(1, depth // 8)
    rows, prev, i = [], bytearray(stride), 0
    for y in range(h):
        f, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        if depth == 4:
            rows.append([(line[x // 2] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(w)])
        else:
            rows.append(list(line[:w]))
    return rows


def mini(rows):
    frame = rows[:32]
    pts = [(x, y) for y in range(32) for x in range(32) if frame[y][x]]
    out = [[0] * 8 for _ in range(8)]
    if not pts:
        return out
    minX, maxX = min(p[0] for p in pts), max(p[0] for p in pts)
    minY, maxY = min(p[1] for p in pts), max(p[1] for p in pts)
    w, h = maxX - minX + 1, maxY - minY + 1
    s = (max(w, h) + 7) // 8
    offX, offY = (8 - -(-w // s)) // 2, (8 - -(-h // s)) // 2
    for oy in range(8):
        for ox in range(8):
            counts = [0] * 16
            for sy in range(minY + (oy - offY) * s, minY + (oy - offY) * s + s):
                for sx in range(minX + (ox - offX) * s, minX + (ox - offX) * s + s):
                    if minX <= sx <= maxX and minY <= sy <= maxY:
                        counts[frame[sy][sx] & 15] += 1
            best = max(range(1, 16), key=lambda v: counts[v])
            out[oy][ox] = best if counts[best] else 0
    return out


icon_file = dict(re.findall(r'(gMonIcon_\w+)\[\]\s*=\s*INCGFX_U8\("([^"]+)\.png"', read('src/data/graphics/pokemon.h')))
icon_of = dict(re.findall(r'\[(SPECIES_\w+)\]\s*=\s*(gMonIcon_\w+),', read('src/pokemon_icon.c')))
pal_of = {k: int(v) for k, v in re.findall(r'\[(SPECIES_\w+)\]\s*=\s*(\d+),', read('src/pokemon_icon.c').split('gMonIconPaletteIndices')[1])}
mini_rows, pal_rows = [], []
for r in roster:
    sp = 'SPECIES_' + r['species']
    px = mini(read_png_indexed(ROOT / (icon_file[icon_of[sp]] + '.png')))
    data = []
    for y in range(8):
        for x in range(0, 8, 2):
            data.append(px[y][x] | (px[y][x + 1] << 4))
    mini_rows.append('    {' + ', '.join(f'0x{b:02x}' for b in data) + '},')
    pal_rows.append(str(pal_of[sp]))
MINIS.write_text('// Generated at build time by tools/arena/draft_data.py from the party icons; not committed.\n'
                 'static const u8 sArenaDraftMinis[ARENA_DRAFT_COUNT][32] =\n{\n' + '\n'.join(mini_rows) + '\n};\n'
                 'static const u8 sArenaDraftMiniPal[ARENA_DRAFT_COUNT] = {' + ', '.join(pal_rows) + '};\n')

OUT.write_text('// Generated by tools/arena/draft_data.py from this build\'s data; do not edit.\n'
               '// species, credit cost, ability slot, level-100 moveset\n'
               f'#define ARENA_DRAFT_COUNT {len(rows)}\n'
               'static const struct ArenaDraftEntry sArenaDraftRoster[ARENA_DRAFT_COUNT] =\n{\n' + '\n'.join(rows) + '\n};\n')
(ROOT / '.arena-dev/draft-report.txt').write_text('\n'.join(report) + '\n')
print('\n'.join(report))
