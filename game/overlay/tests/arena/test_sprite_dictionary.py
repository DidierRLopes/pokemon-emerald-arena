#!/usr/bin/env python3
"""All directions/frames through the actual C decoder; optional raw baseline.

The reference frame is reconstructed independently from the dictionary in
Python. --baseline additionally compares every original 0.5.0 pixel byte.
No assets are checked into source control.
"""
import argparse, ctypes as C, json, struct, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path);args=p.parse_args()
out=ROOT/'.arena-dev/test-sprite-decoder.dylib'
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-dynamiclib',
    '-DARENA_SPRITES_HOST','-I'+str(ROOT/'include'),str(ROOT/'src/arena_sprites.c'),'-o',str(out)],check=True)
class Anim(C.Structure):
    _fields_=[('tiles',C.c_void_p),('durations',C.c_void_p),('frames',C.c_uint16),
              ('ticks',C.c_uint16),('hit',C.c_uint16)]
fn=C.CDLL(str(out)).ArenaSprites_Decode
fn.argtypes=[C.POINTER(Anim),C.c_uint8,C.c_uint8,C.POINTER(C.c_uint32)];fn.restype=C.c_uint8
manifest=json.loads((ROOT/'.arena-dev/pmd/manifest.json').read_text())
frames=0;packed=0;raw=0;baseline_frames=0
for mon in manifest['bundles']:
    for anim in mon['animations']:
        data=Path(anim['binary']).read_bytes();offset,count=struct.unpack_from('<II',data)
        n=len(anim['durations']);assert offset==8+n*8*128 and len(data)==offset+count*32
        table=struct.unpack_from('<'+'H'*(n*8*64),data,8);assert max(table)<count
        buffer=C.create_string_buffer(data);a=Anim(C.addressof(buffer),None,n,0,0)
        decoded=(C.c_uint32*512)()
        baseline=(args.baseline/Path(anim['binary']).name).read_bytes() if args.baseline and (args.baseline/Path(anim['binary']).name).exists() else None
        for direction in range(8):
            for frame in range(n):
                f=direction*n+frame
                reference=b''.join(data[offset+i*32:offset+(i+1)*32] for i in table[f*64:(f+1)*64])
                assert fn(C.byref(a),direction,frame,decoded)==1
                assert bytes(decoded)==reference,(mon['species'],anim['name'],direction,frame)
                if baseline:
                    assert bytes(decoded)==baseline[f*2048:(f+1)*2048]
                    baseline_frames+=1
                frames+=1
        assert fn(C.byref(a),8,0,decoded)==0 and fn(C.byref(a),0,n,decoded)==0
        packed+=len(data);raw+=n*8*2048
print(json.dumps(dict(ok=True,species=len(manifest['bundles']),frames=frames,
    baseline_frames=baseline_frames,packed_bytes=packed,raw_bytes=raw)))
