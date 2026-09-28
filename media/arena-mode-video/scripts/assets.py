"""Builds public/gen/ from the local pokeemerald workspace (not committed):
the draft roster with costs (roster.json), every roster Pokémon's party icon
coloured with its icon palette, a few front sprites for hero shots, and the
two trainer pictures used as the coaches.

Run from this folder: python3 scripts/assets.py  (needs Pillow)."""
import json
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parents[1]
REPO = HERE.parents[1]
WS = REPO / 'workspace'
OUT = HERE / 'public' / 'gen'
HEROES = ['MEWTWO', 'MAGIKARP', 'CHARIZARD', 'GENGAR', 'DRAGONITE', 'VENUSAUR', 'PIKACHU', 'GYARADOS', 'ALAKAZAM', 'MACHAMP']


def read(p):
    return (WS / p).read_text(encoding='utf-8', errors='replace')


def jasc(path):
    lines = Path(path).read_text().split('\n')
    return [tuple(int(v) for v in l.split()) for l in lines[3:3 + int(lines[2])]]


def colour(png, pal, frame_h=None):
    """An indexed PNG drawn with a JASC palette, index 0 transparent."""
    im = Image.open(png)
    idx = im.convert('P') if im.mode != 'P' else im
    w, h = idx.size
    h = frame_h or h
    out = Image.new('RGBA', (w, h))
    px, dst = idx.load(), out.load()
    for y in range(h):
        for x in range(w):
            i = px[x, y]
            if i:
                r, g, b = pal[i % len(pal)]
                dst[x, y] = (r, g, b, 255)
    return out


def main():
    (OUT / 'icons').mkdir(parents=True, exist_ok=True)
    (OUT / 'front').mkdir(exist_ok=True)
    names = dict(re.findall(r'\[SPECIES_(\w+)\]\s*=\s*_\("([^"]*)"\)', read('src/data/text/species_names.h')))
    icon_file = dict(re.findall(r'(gMonIcon_\w+)\[\]\s*=\s*INCGFX_U8\("([^"]+)\.png"', read('src/data/graphics/pokemon.h')))
    icon_of = dict(re.findall(r'\[(SPECIES_\w+)\]\s*=\s*(gMonIcon_\w+),', read('src/pokemon_icon.c')))
    pal_of = {k: int(v) for k, v in re.findall(r'\[(SPECIES_\w+)\]\s*=\s*(\d+),',
                                                read('src/pokemon_icon.c').split('gMonIconPaletteIndices')[1])}
    pals = [jasc(WS / f'graphics/pokemon/icon_palettes/icon_palette_{i}.pal') for i in range(3)]
    # National dex numbers: the order of the NATIONAL_DEX_ enum (NONE is 0).
    enum = read('include/constants/pokedex.h').split('NATIONAL_DEX_NONE')[1]
    nat = {n: i + 1 for i, n in enumerate(re.findall(r'NATIONAL_DEX_(\w+)', enum.split('}')[0]))}
    roster = []
    table = read('src/data/arena_draft.h')
    for m in re.finditer(r'\{\s*SPECIES_(\w+)\s*,\s*(\d+)\s*,\s*\d+\s*,\s*\{([^}]*)\}', table):
        sp, cost, moves = m.group(1), int(m.group(2)), [x.strip().replace('MOVE_', '').replace('_', ' ') for x in m.group(3).split(',')]
        key = 'SPECIES_' + sp
        path = icon_file[icon_of[key]]
        colour(WS / (path + '.png'), pals[pal_of[key]], 32).save(OUT / 'icons' / f'{sp}.png')
        roster.append({'id': sp, 'name': names.get(sp, sp.title()), 'cost': cost, 'dex': int(nat.get(sp, 0)),
                       'moves': [x.title() for x in moves if x != 'NONE'], 'dir': str(Path(path).parent)})
        if sp in HEROES:
            d = WS / Path(path).parent
            colour(d / 'front.png', jasc(d / 'normal.pal'), 64).save(OUT / 'front' / f'{sp}.png')
    for who in ('brendan', 'may'):
        colour(WS / f'graphics/trainers/front_pics/{who}.png', jasc(WS / f'graphics/trainers/palettes/{who}.pal')).save(OUT / f'{who}.png')
    (OUT / 'roster.json').write_text(json.dumps(roster, indent=1))
    print(len(roster), 'roster entries,', len(list((OUT / 'front').glob('*.png'))), 'front sprites')


if __name__ == '__main__':
    sys.exit(main())
