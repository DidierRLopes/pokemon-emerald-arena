// Emerald Arena: local-only ROM preparation. No user file is uploaded.
// MIT, German Burgardt. BPS codec follows the public BPS1 format.
const fail = message => { throw new Error(message); };
export async function digest(bytes, algorithm = 'SHA-256') {
  return Array.from(new Uint8Array(await crypto.subtle.digest(algorithm, bytes)),
    b => b.toString(16).padStart(2, '0')).join('');
}
const crcTable = Uint32Array.from({length:256}, (_, n) => {
  for(let i=0;i<8;i++) n=(n>>>1)^((n&1)?0xedb88320:0);
  return n>>>0;
});
export function crc32(bytes) {
  let c=0xffffffff;
  for(const b of bytes) c=crcTable[(c^b)&255]^(c>>>8);
  return (c^0xffffffff)>>>0;
}
export function applyBps(source, patch) {
  const dv=new DataView(patch.buffer,patch.byteOffset,patch.byteLength);
  if(patch.length<16 || new TextDecoder().decode(patch.subarray(0,4))!=='BPS1') fail('Parche inválido.');
  if(crc32(patch.subarray(0,-4))!==dv.getUint32(patch.length-4,true)) fail('Parche incompleto. Volvé a descargar.');
  if(crc32(source)!==dv.getUint32(patch.length-12,true)) fail('Esta ROM no coincide con Esmeralda (USA/Europe).');
  let p=4;
  function variable() {
    let n=0,shift=1;
    for(let i=0;i<8;i++) {
      if(p>=patch.length-12) fail('Parche truncado.');
      const b=patch[p++]; n+=(b&127)*shift;
      if(b&128) return n;
      shift*=128; n+=shift;
    }
    fail('Entero inválido.');
  }
  const originalSize=variable(),size=variable(),metadata=variable();
  if(originalSize!==source.length||size>33554432||p+metadata>patch.length-12) fail('Tamaño incompatible.');
  p+=metadata;
  const out=new Uint8Array(size);let at=0,sourceRelative=0,targetRelative=0;
  while(at<size) {
    const command=variable(),kind=command%4,count=Math.floor(command/4)+1;
    if(count>size-at) fail('Parche fuera de rango.');
    if(kind===0) {
      if(at+count>source.length) fail('Lectura original fuera de rango.');
      out.set(source.subarray(at,at+count),at);at+=count;
    } else if(kind===1) {
      if(p+count>patch.length-12) fail('Datos incompletos.');
      out.set(patch.subarray(p,p+count),at);p+=count;at+=count;
    } else {
      const offset=variable(),delta=Math.floor(offset/2)*(offset%2?-1:1);
      if(kind===2) {
        sourceRelative+=delta;
        if(sourceRelative<0||sourceRelative+count>source.length) fail('Copia original fuera de rango.');
        out.set(source.subarray(sourceRelative,sourceRelative+count),at);sourceRelative+=count;at+=count;
      } else {
        targetRelative+=delta;
        if(targetRelative<0||targetRelative>=at) fail('Copia de destino fuera de rango.');
        for(let i=0;i<count;i++) out[at++]=out[targetRelative++];
      }
    }
  }
  if(p!==patch.length-12||crc32(out)!==dv.getUint32(patch.length-8,true)) fail('El parche no produjo el resultado esperado.');
  return out;
}

// Deterministic PNG decoding: browser color-management cannot change pixels.
export async function decodePng(bytes) {
  if(bytes.length<33||bytes[0]!==137||new TextDecoder().decode(bytes.subarray(1,4))!=='PNG') fail('PNG inválido.');
  const dv=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  let w,h,depth,type,palette,alpha,parts=[],total=0;
  for(let p=8;p+12<=bytes.length;) {
    const n=dv.getUint32(p),kind=new TextDecoder().decode(bytes.subarray(p+4,p+8));
    if(n>4000000||p+12+n>bytes.length) fail('PNG truncado.');
    const data=bytes.subarray(p+8,p+8+n);
    if(kind==='IHDR') {
      w=dv.getUint32(p+8);h=dv.getUint32(p+12);depth=data[8];type=data[9];
      if(!w||!h||w*h>2000000||data[10]||data[11]||data[12]) fail('PNG no compatible.');
    } else if(kind==='PLTE') palette=data;
    else if(kind==='tRNS') alpha=data;
    else if(kind==='IDAT') { parts.push(data);total+=n; }
    p+=n+12;if(kind==='IEND')break;
  }
  const channels={0:1,2:3,3:1,4:2,6:4}[type];
  if(!channels||![1,2,4,8].includes(depth)||(type!==3&&depth!==8)) fail('Profundidad PNG no compatible.');
  const packed=new Uint8Array(total);let at=0;
  for(const part of parts){packed.set(part,at);at+=part.length;}
  const raw=new Uint8Array(await new Response(new Blob([packed]).stream().pipeThrough(new DecompressionStream('deflate'))).arrayBuffer());
  const stride=Math.ceil(w*channels*depth/8),bpp=Math.max(1,Math.ceil(channels*depth/8));
  if(raw.length!==h*(stride+1)) fail('Geometría PNG inválida.');
  const scan=new Uint8Array(h*stride);
  const paeth=(a,b,c)=>{const p=a+b-c,pa=Math.abs(p-a),pb=Math.abs(p-b),pc=Math.abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;};
  for(let y=0;y<h;y++) {
    const filter=raw[y*(stride+1)];if(filter>4)fail('Filtro PNG inválido.');
    for(let x=0;x<stride;x++) {
      const i=y*stride+x,a=x>=bpp?scan[i-bpp]:0,b=y?scan[i-stride]:0,c=y&&x>=bpp?scan[i-stride-bpp]:0;
      scan[i]=(raw[y*(stride+1)+1+x]+[0,a,b,Math.floor((a+b)/2),paeth(a,b,c)][filter])&255;
    }
  }
  const rgba=new Uint8Array(w*h*4);
  for(let y=0;y<h;y++)for(let x=0;x<w;x++) {
    const i=(y*w+x)*4,s=y*stride+x*channels;rgba[i+3]=255;
    if(type===3) {
      const index=(scan[y*stride+Math.floor(x*depth/8)]>>(8-depth-(x*depth)%8))&((1<<depth)-1);
      if(!palette||index*3+2>=palette.length)fail('Paleta inválida.');
      rgba.set(palette.subarray(index*3,index*3+3),i);rgba[i+3]=alpha?.[index]??255;
    } else if(type===6){rgba.set(scan.subarray(s,s+4),i);}
    else if(type===2){rgba.set(scan.subarray(s,s+3),i);}
    else {rgba[i]=rgba[i+1]=rgba[i+2]=scan[s];if(type===4)rgba[i+3]=scan[s+1];}
  }
  return {width:w,height:h,rgba};
}

export function packSpriteSet(species, sheets) {
  const colors=[0],index=(rgba,i)=>{
    if(!rgba[i+3])return 0;
    if(rgba[i+3]!==255)fail('Alpha parcial no compatible.');
    const c=(rgba[i]>>3)|((rgba[i+1]>>3)<<5)|((rgba[i+2]>>3)<<10);
    let at=colors.indexOf(c,1);
    if(at<0){if(colors.length===16)fail('Paleta demasiado grande.');at=colors.length;colors.push(c);}
    return at;
  };
  for(const a of species.animations) {
    const sheet=sheets.get(a.sha256);
    if(sheet.width!==a.width*a.frames||sheet.height!==a.height*8)fail('Sprites con geometría incorrecta.');
    for(let i=0;i<sheet.rgba.length;i+=4)index(sheet.rgba,i);
  }
  const chunks=[];
  for(const a of species.animations) {
    if(a.offset===null)continue; // An alias uses an earlier ROM tile array.
    const s=sheets.get(a.sha256),tiles=new Uint8Array(a.frames*8*2048);
    for(let d=0;d<8;d++)for(let f=0;f<a.frames;f++)for(let y=0;y<a.height;y++)for(let x=0;x<a.width;x++) {
      const pi=((d*a.height+y)*s.width+f*a.width+x)*4,ci=index(s.rgba,pi);
      if(!ci)continue;
      const tx=x+32-Math.floor(a.width/2),ty=y+32-Math.floor(a.height/2);
      if(tx<0||tx>=64||ty<0||ty>=64)fail('No se permite recortar sprites.');
      const off=(d*a.frames+f)*2048+(Math.floor(ty/8)*8+Math.floor(tx/8))*32+(ty%8)*4+Math.floor(tx%8/2);
      tiles[off]|=ci<<((tx&1)*4);
    }
    chunks.push({offset:a.offset,bytes:tiles,sha256:a.compiled_sha256});
  }
  const pal=new Uint8Array(32);colors.forEach((v,i)=>{pal[i*2]=v&255;pal[i*2+1]=v>>8;});
  chunks.push({offset:species.palette_offset,bytes:pal,sha256:species.palette_sha256});
  return chunks;
}

export async function prepareRom(source, manifest, patch, report=()=>{}, fetchResource=async url=>{
  const response=await fetch(url,{credentials:'omit'});
  if(!response.ok)fail('No se pudo descargar un recurso. Probá nuevamente.');
  const bytes=new Uint8Array(await response.arrayBuffer());
  if(bytes.length>4000000)fail('Recurso demasiado grande.');
  return bytes;
}) {
  report('Comprobando tu Esmeralda…');
  if(source.length!==manifest.source_size||await digest(source)!==manifest.source_sha256)fail('Elegí la ROM limpia de Pokémon Emerald (USA/Europe), sin modificaciones.');
  if(await digest(patch)!==manifest.patch_sha256)fail('Paquete de instalación incompleto.');
  const out=applyBps(source,patch),resources=new Map();
  for(const species of manifest.species)for(const a of species.animations)resources.set(a.sha256,a.url);
  const entries=[...resources],sheets=new Map();let next=0,done=0;
  async function worker(){for(;;){const at=next++;if(at>=entries.length)return;
    const [sha,url]=entries[at],bytes=await fetchResource(url);
    if(await digest(bytes)!==sha)fail('Un recurso cambió: descarga rechazada.');
    sheets.set(sha,await decodePng(bytes));report(`Preparando animaciones ${++done}/${entries.length}…`);
  }}
  await Promise.all(Array.from({length:4},worker));
  for(const species of manifest.species) {
    report(`Integrando ${species.name}…`);
    for(const chunk of packSpriteSet(species,sheets)) {
      if(chunk.offset<0||chunk.offset+chunk.bytes.length>out.length||await digest(chunk.bytes)!==chunk.sha256)fail('Los gráficos no coinciden con la versión validada.');
      if(out.subarray(chunk.offset,chunk.offset+chunk.bytes.length).some(b=>b!==0))fail('Zona de gráficos no vacía.');
      out.set(chunk.bytes,chunk.offset);
    }
  }
  if(await digest(out)!==manifest.target_sha256)fail('La ROM final no coincide con el release validado.');
  report('Lista. Descargá Emerald Arena y abrila en tu emulador GBA.');
  return out;
}
