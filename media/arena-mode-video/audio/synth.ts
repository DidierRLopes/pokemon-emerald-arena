// The score, synthesized from src/cues.ts so every hit lands on its picture.
// A GBA-flavoured chiptune (pulse leads, triangle bass, noise drums) with
// sound design on the cues. Writes audio/raw.wav; audio/master.ts masters it.
import {writeFileSync} from 'node:fs';
import {
  BAR, BEAT, CAP_SLAM, COIN_DROPS, COIN_RAIN, SHOT, CONFETTI, IMPACTS, LIGHT_BANKS, LOCKUP, MEWTWO_IGNITE, OVER_CAP, PICKS,
  REPLAYS, RISERS, SHOUT_ONE, SHOUT_TWO, SONG, TAG_WAVE, VOICE_CUT, VS_SLAM, WHIP_MAGIKARP, WHIP_MEWTWO, WINS_SLAM,
} from '../src/cues.ts';

const SR = 44100;
const N = Math.ceil((SONG + 1.5) * SR);
const L = new Float32Array(N);
const R = new Float32Array(N);

let seed = 7;
const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff) * 2 - 1;
const midi = (n: number) => 440 * Math.pow(2, (n - 69) / 12);

function add(t0: number, dur: number, pan: number, f: (t: number, i: number) => number) {
  const s0 = Math.max(0, Math.floor(t0 * SR));
  const s1 = Math.min(N, Math.floor((t0 + dur) * SR));
  const gl = Math.cos(((pan + 1) * Math.PI) / 4);
  const gr = Math.sin(((pan + 1) * Math.PI) / 4);
  for (let i = s0; i < s1; i++) {
    const v = f((i - s0) / SR, i - s0);
    L[i] += v * gl;
    R[i] += v * gr;
  }
}

const env = (t: number, a: number, d: number, dur: number) =>
  t < a ? t / a : t > dur - d ? Math.max(0, (dur - t) / d) : 1;

function pulse(t0: number, dur: number, note: number, vol: number, duty = 0.5, pan = 0, slide = 0) {
  let ph = 0;
  add(t0, dur, pan, (t) => {
    ph += (midi(note + slide * t) / SR) % 1;
    const s = ph % 1 < duty ? 1 : -1;
    return s * vol * env(t, 0.004, Math.min(0.08, dur * 0.5), dur) * Math.exp(-t * 2.2);
  });
}

function tri(t0: number, dur: number, note: number, vol: number) {
  let ph = 0;
  add(t0, dur, 0, (t) => {
    ph += midi(note) / SR;
    const p = ph % 1;
    return (4 * Math.abs(p - 0.5) - 1) * vol * env(t, 0.003, 0.03, dur);
  });
}

function noise(t0: number, dur: number, vol: number, decay: number, pan = 0, tone = 1) {
  let lp = 0;
  add(t0, dur, pan, (t) => {
    const n = rnd();
    lp += (n - lp) * tone;
    return lp * vol * Math.exp(-t * decay);
  });
}

function kick(t0: number, vol = 0.9) {
  let ph = 0;
  add(t0, 0.28, 0, (t) => {
    const f = 45 + 110 * Math.exp(-t * 28);
    ph += f / SR;
    return Math.sin(ph * 2 * Math.PI) * vol * Math.exp(-t * 9);
  });
}

function snare(t0: number, vol = 0.45) {
  noise(t0, 0.2, vol, 18, 0, 0.8);
  pulse(t0, 0.08, 50, vol * 0.4, 0.5, 0, -24);
}

const hat = (t0: number, vol = 0.12) => noise(t0, 0.05, vol, 70, 0.3, 1);

function impact(t0: number, vol = 1) {
  let ph = 0;
  add(t0, 1.6, 0, (t) => {
    const f = 62 * Math.exp(-t * 1.2) + 26;
    ph += f / SR;
    return Math.sin(ph * 2 * Math.PI) * vol * Math.exp(-t * 2.4);
  });
  noise(t0, 0.9, vol * 0.55, 5, -0.2, 0.35);
  noise(t0, 0.9, vol * 0.55, 5, 0.2, 0.35);
  kick(t0, vol);
}

function riser(from: number, to: number, vol = 0.35) {
  const dur = to - from;
  let lp = 0;
  add(from, dur, 0, (t) => {
    const k = t / dur;
    lp += (rnd() - lp) * (0.02 + 0.5 * k * k);
    return lp * vol * k * k;
  });
  let ph = 0;
  add(from, dur, 0, (t) => {
    const k = t / dur;
    ph += midi(60 + 24 * k) / SR;
    return (ph % 1 < 0.25 ? 1 : -1) * 0.06 * k;
  });
}

function whoosh(t0: number, vol = 0.4, pan = 0) {
  let lp = 0;
  add(t0 - 0.25, 0.5, pan, (t) => {
    const k = t / 0.5;
    lp += (rnd() - lp) * (0.05 + 0.4 * Math.sin(k * Math.PI));
    return lp * vol * Math.sin(k * Math.PI);
  });
}

const coin = (t0: number, lift = 0) => {
  pulse(t0, 0.07, 83 + lift, 0.16, 0.5, 0.2);
  pulse(t0 + 0.07, 0.25, 88 + lift, 0.16, 0.5, 0.2);
};

// ---- the progression: Am  F  C  G (a heroic i-VI-III-VII), one per bar
const CHORDS = [[57, 60, 64], [53, 57, 60], [48, 52, 55], [55, 59, 62]];
const ROOTS = [45, 41, 48, 43];
const chordAt = (t: number) => Math.floor(t / BAR) % 4;
const LEAD = [69, 72, 76, 74, 72, 76, 79, 77, 76, 74, 72, 71, 72, 74, 76, 72];

for (let b = 0; b * BAR < SONG; b++) {
  const t = b * BAR;
  const c = CHORDS[b % 4];
  // Arpeggio throughout, brighter as it builds.
  const arpVol = t < 4 ? 0.05 : t < 16 ? 0.06 : t < SHOT.finale[0] ? 0.07 : 0.05;
  for (let s = 0; s < 16; s++) {
    const tt = t + (s * BEAT) / 4;
    if (tt >= SONG - 0.5 || (tt >= 15.5 && tt < 16)) continue;
    pulse(tt, BEAT / 4, c[s % 3] + 12 * (Math.floor(s / 3) % 2), arpVol, 0.125, s % 2 ? 0.35 : -0.35);
  }
  // Drums and bass from the board on.
  if (t >= 4 && t < SHOT.finale[0]) {
    for (let q = 0; q < 4; q++) {
      const tt = t + q * BEAT;
      if (tt >= 14 && tt < 16) continue;
      kick(tt, t >= 16 ? 0.9 : 0.7);
      if (t >= 16 && (q === 1 || q === 3)) snare(tt);
      for (let h = 0; h < (t >= 16 ? 4 : 2); h++) hat(tt + (h * BEAT) / (t >= 16 ? 4 : 2), h % 2 ? 0.07 : 0.11);
      for (let e = 0; e < 2; e++) tri(tt + (e * BEAT) / 2, BEAT / 2 - 0.02, ROOTS[b % 4] - 12 + (e ? 12 : 0), 0.32);
    }
  }
  // The lead melody over the coaches and the replays (an octave up at the peak).
  if (t >= 16 && t < SHOT.finale[0]) {
    const peak = t >= SHOT.replays[0];
    for (let i = 0; i < 4; i++) {
      const n = LEAD[(b * 4 + i) % LEAD.length] + (peak ? 12 : 0);
      pulse(t + i * BEAT, BEAT * 0.9, n, peak ? 0.1 : 0.12, 0.5, 0, 0);
      pulse(t + i * BEAT + 0.012, BEAT * 0.9, n + 0.08, 0.05, 0.25, 0.4);
    }
  }
}
// Snare fill into the finale.
for (let i = 0; i < 8; i++) snare(SHOT.finale[0] - 1 + i * (BEAT / 4), 0.25 + i * 0.04);

// ---- sound design on the cues
COIN_DROPS.forEach((t, i) => coin(t, i));
for (let t = COIN_RAIN[0], i = 0; t < COIN_RAIN[1]; t += BEAT / 4, i++) pulse(t, 0.05, 88 + (i % 5), 0.05, 0.5, (i % 3) - 1);
for (const r of RISERS) riser(r.from, r.to);
for (const t of IMPACTS) impact(t, t === WINS_SLAM || t === VS_SLAM ? 1.1 : 0.85);
// Tag wave: a rising run of blips.
for (let i = 0; i < 16; i++) pulse(TAG_WAVE[0] + (i * (TAG_WAVE[1] - TAG_WAVE[0])) / 16, 0.06, 72 + i, 0.07, 0.25, i % 2 ? 0.5 : -0.5);
whoosh(WHIP_MAGIKARP, 0.45, -0.4);
pulse(WHIP_MAGIKARP + 0.25, 0.2, 60, 0.12, 0.5, 0, -12);            // a small, sad "2"
whoosh(WHIP_MEWTWO, 0.5, 0.4);
for (const n of [57, 64, 69, 76]) pulse(MEWTWO_IGNITE, 1.4, n, 0.07, 0.5, 0);   // gold chord
for (const p of PICKS) {
  coin(p.t, 5);
  for (const n of CHORDS[chordAt(p.t)]) pulse(p.t, 0.18, n + 12, 0.07, 0.5, 0);
}
// Over the cap: a low buzzer.
pulse(OVER_CAP.stamp, 0.45, 38, 0.2, 0.5, 0);
pulse(OVER_CAP.stamp, 0.45, 38.6, 0.2, 0.3, 0);
for (const t of LIGHT_BANKS) {
  kick(t, 0.8);
  noise(t, 0.3, 0.3, 10, 0, 0.2);
}
noise(VS_SLAM, 2.2, 0.35, 1.5, 0, 1);                               // crash
whoosh(SHOUT_ONE, 0.4, -0.6);
whoosh(SHOUT_TWO, 0.4, -0.6);
whoosh(SHOUT_TWO + 0.1, 0.4, 0.6);
noise(VOICE_CUT, 0.12, 0.5, 30, 0, 1);                             // the cut
for (const r of REPLAYS) noise(r.t, 0.08, 0.25, 40, 0, 1);         // replay cut ticks
noise(WINS_SLAM, 2.5, 0.4, 1.2, 0, 1);
for (const t of CONFETTI) {
  noise(t, 0.15, 0.45, 25, -0.5, 0.9);
  noise(t + 0.03, 0.15, 0.45, 25, 0.5, 0.9);
}
// The final chord, ringing out under the lockup.
for (const n of [45, 57, 60, 64, 69]) pulse(LOCKUP, 3.0, n, 0.06, 0.5, 0);
for (const n of [57, 64, 69]) tri(LOCKUP, 2.6, n - 12, 0.12);
// Slam: an extra hit on the counter.
pulse(CAP_SLAM, 0.5, 45, 0.18, 0.5, 0, -12);

// ---- write 16-bit stereo WAV (soft clip; master.ts sets the loudness)
const out = Buffer.alloc(44 + N * 4);
out.write('RIFF', 0);
out.writeUInt32LE(36 + N * 4, 4);
out.write('WAVEfmt ', 8);
out.writeUInt32LE(16, 16);
out.writeUInt16LE(1, 20);
out.writeUInt16LE(2, 22);
out.writeUInt32LE(SR, 24);
out.writeUInt32LE(SR * 4, 28);
out.writeUInt16LE(4, 32);
out.writeUInt16LE(16, 34);
out.write('data', 36);
out.writeUInt32LE(N * 4, 40);
for (let i = 0; i < N; i++) {
  const fade = i > (SONG + 0.8) * SR ? Math.max(0, 1 - (i / SR - SONG - 0.8) / 0.7) : 1;
  out.writeInt16LE(Math.round(Math.tanh(L[i] * 0.8) * fade * 32000), 44 + i * 4);
  out.writeInt16LE(Math.round(Math.tanh(R[i] * 0.8) * fade * 32000), 46 + i * 4);
}
writeFileSync(new URL('./raw.wav', import.meta.url), out);
console.log(`raw.wav: ${(N / SR).toFixed(1)} s`);
