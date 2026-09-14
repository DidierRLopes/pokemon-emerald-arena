// Rebuild the single-file player installer. No ROM is read or packaged here.
import {readFile,writeFile} from 'node:fs/promises';
const root=new URL('../',import.meta.url);
const template=await readFile(new URL('tools/installer.html',root),'utf8');
const runtime=(await readFile(new URL('release/installer.mjs',root),'utf8')).replace(/^export /gm,'');
const payload=await readFile(new URL('release/payload.json',root),'utf8');
JSON.parse(payload);
const html=template.replace('/* ARENA_RUNTIME */',()=>runtime).replace('/* ARENA_PAYLOAD */',()=>payload.trim());
const destination=new URL('release/Prepare-Emerald-Arena.html',root);
if(process.argv.includes('--check')){
  if(await readFile(destination,'utf8')!==html)throw new Error('Installer is stale. Run node tools/build-installer.mjs.');
}else await writeFile(destination,html);
