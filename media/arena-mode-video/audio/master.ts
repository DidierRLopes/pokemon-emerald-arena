// Masters audio/raw.wav to public/soundtrack.wav at -12 LUFS integrated,
// -1.6 dBTP, leaving room for AAC (two-pass ffmpeg loudnorm), then prints the measured result.
import {execFileSync} from 'node:child_process';

const raw = new URL('./raw.wav', import.meta.url).pathname;
const out = new URL('../public/soundtrack.wav', import.meta.url).pathname;
const target = 'I=-12:TP=-1.6:LRA=9';

const probe = execFileSync('sh', ['-c', `ffmpeg -hide_banner -i "${raw}" -af loudnorm=${target}:print_format=json -f null - 2>&1`], {encoding: 'utf8'});
const m = JSON.parse(probe.slice(probe.lastIndexOf('{'), probe.lastIndexOf('}') + 1));
execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', raw, '-af',
  `loudnorm=${target}:measured_I=${m.input_i}:measured_TP=${m.input_tp}:measured_LRA=${m.input_lra}:measured_thresh=${m.input_thresh}:offset=${m.target_offset}:linear=true,aresample=48000`,
  '-c:a', 'pcm_s16le', out]);
const check = execFileSync('sh', ['-c', `ffmpeg -hide_banner -nostats -i "${out}" -af ebur128=peak=true -f null - 2>&1 | tail -12`], {encoding: 'utf8'});
console.log(check.split('\n').filter((l) => /I:|Peak:|LRA:/.test(l)).map((l) => l.trim()).join('\n'));
