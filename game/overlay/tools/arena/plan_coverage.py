#!/usr/bin/env python3
"""Generate an auditable early-adventure roster from Emerald's own tables.

Coverage means art, not a promise that every move/ability supports arena mode.
All native encounter methods are included, even fishing rods acquired later.
Evolution descendants are included so the imported art survives levelling up.
"""
import argparse, json, re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--write-catalog',action='store_true');args=p.parse_args()
routes={f'MAP_ROUTE{i}' for i in list(range(101,111))+[116]}
places={'MAP_PETALBURG_WOODS','MAP_RUSTURF_TUNNEL','MAP_PETALBURG_CITY',
        'MAP_DEWFORD_TOWN','MAP_SLATEPORT_CITY'}
encounters=json.loads((ROOT/'src/data/wild_encounters.json').read_text())
maps=[];species={'TREECKO','TORCHIC','MUDKIP'};levels={}
for group in encounters['wild_encounter_groups']:
    if not group['for_maps']:continue
    for e in group['encounters']:
        name=e['map']
        if name not in routes|places and not name.startswith('MAP_GRANITE_CAVE_'):continue
        entries=[]
        for method in ('land_mons','water_mons','fishing_mons','rock_smash_mons'):
            if method not in e:continue
            mons=e[method]['mons']
            names=sorted({m['species'].removeprefix('SPECIES_') for m in mons})
            species.update(names)
            for m in mons:
                s=m['species'].removeprefix('SPECIES_');levels[s]=max(levels.get(s,1),m['max_level'])
            entries.append(dict(method=method,species=names))
        maps.append(dict(map=name,encounters=entries))
natural=sorted(species)
evo={}
for name,body in re.findall(r'\[SPECIES_(\w+)\]\s*=\s*(.*?)(?=\n    \[SPECIES_|\n};)',
                           (ROOT/'src/data/pokemon/evolution.h').read_text(),re.S):
    evo[name]=re.findall(r'SPECIES_(\w+)',body)
while True:
    added={t for s in species for t in evo.get(s,[])}-species
    if not added:break
    species.update(added)
national=re.findall(r'^\s+NATIONAL_DEX_(\w+)\s*,',
    (ROOT/'include/constants/pokedex.h').read_text().split('// Hoenn Pokédex order')[0],re.M)
dex={name:i for i,name in enumerate(national)}
assert dex['TREECKO']==252 and dex['SWAMPERT']==260
catalog_path=ROOT/'tools/arena/roster.json'
catalog=json.loads(catalog_path.read_text());present={m['species'] for m in catalog}
new=[]
for s in sorted(species-present,key=dex.get):
    new.append(dict(dex=f'{dex[s]:04}',species=s,level=max(10,levels.get(s,30)),poses={'Attack':'Charge'}))
coverage=dict(milestone='Early Hoenn: routes 101–110 and 116, Petalburg Woods, Rusturf Tunnel, Granite Cave, and nearby town waters',
    native_species=natural,evolution_families=sorted(species),maps=maps,
    note='Art coverage only. Trainer/double/link battles and unsupported mechanics remain classic.')
if args.write_catalog:
    catalog_path.write_text(json.dumps(catalog+new,indent=2)+'\n')
    (ROOT/'tools/arena/coverage.json').write_text(json.dumps(coverage,indent=2)+'\n')
print(json.dumps(dict(maps=len(maps),wild_and_starters=len(natural),with_evolutions=len(species),
    new=len(new),total_roster=len(catalog)+len(new),new_species=[n['species'] for n in new]),indent=2))
