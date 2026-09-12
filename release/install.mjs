#!/usr/bin/env node
import {readFile,writeFile} from 'node:fs/promises';
import {prepareRom} from './installer.mjs';
const [input,output]=process.argv.slice(2);
if(!input||!output){console.error('node install.mjs original.gba Emerald-Arena.gba');process.exit(2);}
try{
  const payload=JSON.parse(await readFile(new URL('./payload.json',import.meta.url),'utf8'));
  const result=await prepareRom(new Uint8Array(await readFile(input)),payload.manifest,
    new Uint8Array(Buffer.from(payload.patch,'base64')),console.log);
  await writeFile(output,result,{flag:'wx'}); // Never replace the original or an existing output.
  console.log('Preparada y verificada: '+output);
}catch(error){console.error(error.message);process.exit(1);}
