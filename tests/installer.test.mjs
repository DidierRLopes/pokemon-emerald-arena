import {test} from 'node:test';
import assert from 'node:assert/strict';
import {deflateSync} from 'node:zlib';
import {readFile} from 'node:fs/promises';
import {applyBps,crc32,decodePng,digest,packSpriteSet,prepareRom} from '../release/installer.mjs';

const variable=n=>{const out=[];for(;;){let b=n%128;n=Math.floor(n/128);if(!n){out.push(b|128);return out;}out.push(b);n--;}};
const le=n=>[n&255,(n>>>8)&255,(n>>>16)&255,n>>>24];
const makePatch=(source,target,commands)=>{
  const body=Uint8Array.from([66,80,83,49,...variable(source.length),...variable(target.length),128,
    ...commands,...le(crc32(source)),...le(crc32(target))]);
  return Uint8Array.from([...body,...le(crc32(body))]);
};
test('BPS literal roundtrip and checksums',()=>{
  const source=Uint8Array.from([1,2,3]),target=Uint8Array.from([4,5,6,7]);
  const patch=makePatch(source,target,[...variable((target.length-1)*4+1),...target]);
  assert.deepEqual(applyBps(source,patch),target);
  assert.throws(()=>applyBps(Uint8Array.from([9,9,9]),patch));
  const corrupt=patch.slice();corrupt[8]^=1;assert.throws(()=>applyBps(source,corrupt));
});
test('BPS source read, relative source copy and overlapping target copy',()=>{
  const s=Uint8Array.from([1,2,3,4]),t=Uint8Array.from([1,2,3,4,3,4,3,4,3,4]);
  const p=makePatch(s,t,[...variable(3*4),...variable(1*4+2),...variable(2*2),...variable(3*4+3),...variable(4*2)]);
  assert.deepEqual(applyBps(s,p),t);
});
const chunk=(name,data)=>{const type=Buffer.from(name);const raw=Buffer.concat([type,data]);
  const size=Buffer.alloc(4),crc=Buffer.alloc(4);size.writeUInt32BE(data.length);crc.writeUInt32BE(crc32(raw));return Buffer.concat([size,raw,crc]);};
test('PNG exact RGBA and GBA palette/tile packing',async()=>{
  const h=Buffer.alloc(13);h.writeUInt32BE(1);h.writeUInt32BE(8,4);h[8]=8;h[9]=6;
  const raw=Buffer.from(Array.from({length:8},()=>[0,248,0,0,255]).flat());
  const png=Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',h),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
  const sheet=await decodePng(png);assert.equal(sheet.rgba.length,32);assert.deepEqual([...sheet.rgba.subarray(0,4)],[248,0,0,255]);
  const packed=packSpriteSet({palette_offset:0,palette_sha256:'test',animations:[{sha256:'red',width:1,height:1,frames:1,offset:32}]},new Map([['red',sheet]]));
  assert.equal(packed[0].bytes.length,16384);assert.equal(packed[0].bytes.reduce((a,b)=>a+(b!==0),0),8);
  assert.equal(packed[1].bytes[2],31);
});
test('release rejects wrong ROM before any network access',async()=>{
  const {manifest,patch}=JSON.parse(await readFile(new URL('../release/payload.json',import.meta.url)));
  let fetched=false;
  await assert.rejects(prepareRom(new Uint8Array(10),manifest,Buffer.from(patch,'base64'),()=>{},async()=>{fetched=true;}));
  assert.equal(fetched,false);
  assert.equal(await digest(Buffer.from(patch,'base64')),manifest.patch_sha256);
  assert.equal(manifest.species.length,12);
  assert.equal(manifest.target_size,33554432);
});
