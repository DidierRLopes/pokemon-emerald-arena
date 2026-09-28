// 1. The salary cap: chip columns land on the beat on a felt table under one
// hanging light while the camera tilts from overhead to a low 3/4 view;
// the counter slams 100 on the downbeat.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {BEAT, CAP_SLAM, COIN_DROPS, COIN_RAIN} from '../cues';
import {backOut, clamp, easeIn, easeInOut, hash, lerp, lin, ring} from '../anim';
import {Coin, W} from '../fx';
import {C, PIXEL} from '../theme';

// Centre-out: the middle column lands first, then pairs spreading outwards.
const SLOT = [0, -1, 1, -2, 2, -3, 3];
const COLS = COIN_DROPS.map((t, i) => ({t, x: W / 2 + SLOT[i] * 250 + (hash(i) - 0.5) * 30, n: 9 + Math.round(hash(i + 4) * 4), depth: Math.abs(SLOT[i])}));
// Chips raining onto the columns on the sixteenths until the slam.
const RAIN = Array.from({length: Math.round((COIN_RAIN[1] - COIN_RAIN[0]) / (BEAT / 4))}, (_, k) => ({t: COIN_RAIN[0] + (k * BEAT) / 4, col: Math.floor(hash(k * 5.3) * 7)}));
const extra = (t: number, col: number) => RAIN.filter((r) => r.col === col && t >= r.t).length;

export const Cap: React.FC<{t: number}> = ({t}) => {
  const tilt = lerp(1, 0.34, easeInOut(lin(t, 0, CAP_SLAM)));           // overhead -> low 3/4
  const push = lerp(0.92, 1.12, easeInOut(lin(t, 0, 4)));
  const landed = COIN_DROPS.filter((d) => t >= d).length;
  const rained = RAIN.filter((r) => t >= r.t).length;
  const count = t >= CAP_SLAM ? 100 : Math.round((landed / COIN_DROPS.length) * 60 + (rained / RAIN.length) * 37);
  const slam = t >= CAP_SLAM ? backOut(lin(t, CAP_SLAM, CAP_SLAM + 0.3)) : 0;
  const flash = ring(t, [CAP_SLAM], 0.6);
  const out = easeIn(lin(t, 3.75, 4.0));                                 // whip up into the board
  return (
    <AbsoluteFill style={{background: C.night, filter: out ? `blur(${out * 18}px)` : undefined, transform: `translateY(${-out * 700}px)`}}>
      {/* the felt and the pool of light */}
      <AbsoluteFill style={{background: `radial-gradient(ellipse ${900 * push}px ${900 * push * lerp(1, 0.55, 1 - tilt)}px at 50% 60%, #243568 0%, #121a3a 45%, ${C.night} 80%)`}} />
      <AbsoluteFill style={{background: `radial-gradient(ellipse 600px 380px at 50% 58%, rgba(255,210,74,${0.08 + 0.25 * flash}) 0%, rgba(0,0,0,0) 70%)`}} />
      {/* the table: a felt oval with a brass rim, flattening as the camera tilts */}
      <div style={{position: 'absolute', left: W / 2 - 1050 * push, top: 720 - 360 * push * tilt - (1 - tilt) * 40, width: 2100 * push, height: 720 * push * tilt + 80,
        borderRadius: '50%', background: 'radial-gradient(ellipse at 50% 40%, #2b4a8a 0%, #1a2c5c 55%, #101a3c 100%)',
        boxShadow: `inset 0 0 0 ${10 * push}px #6b5320, inset 0 0 0 ${14 * push}px #3a2c10, 0 ${30 + 40 * (1 - tilt)}px 80px rgba(0,0,0,0.7)`}} />
      {COLS.map((c, i) => {
        const size = lerp(210, 165, c.depth / 3) * push;
        const landedK = t >= c.t ? 1 : clamp((t - (c.t - 0.3)) / 0.3);
        const yBase = 760 - (1 - tilt) * 40 - c.depth * 30 * (1 - tilt);
        return <div key={`s${i}`} style={{position: 'absolute', left: W / 2 + (c.x - W / 2) * push - size * 0.6, top: yBase + size * 0.5 * tilt * 0.6 - 6,
          width: size * 1.2, height: size * 0.5 * tilt + 18, borderRadius: '50%', background: 'rgba(0,0,0,0.55)', filter: 'blur(10px)', opacity: landedK}} />;
      })}
      {/* dust in the light, in front of everything */}
      {Array.from({length: 40}, (_, i) => {
        const px = 500 + hash(i * 3) * 920;
        const py = (hash(i * 7) * 900 + t * (20 + hash(i) * 30)) % 900;
        return <div key={i} style={{position: 'absolute', left: px, top: py, width: 3, height: 3, borderRadius: 3,
          background: 'rgba(255,230,160,0.5)', filter: 'blur(1px)', opacity: 0.2 + 0.4 * hash(i * 11)}} />;
      })}
      {/* the chip columns, back ones smaller, each landing on its beat */}
      {COLS.slice().sort((a, b) => b.depth - a.depth).map((c, i) => {
        const fall = clamp((t - (c.t - 0.3)) / 0.3);
        if (fall <= 0) return null;
        const size = lerp(210, 165, c.depth / 3) * push;
        const coinH = size * 0.5 * tilt;
        const band = size * 0.12 * Math.sqrt(1 - tilt * tilt);
        const step = Math.max(4, band * 1.05);
        const yBase = 760 - (1 - tilt) * 40 - c.depth * 30 * (1 - tilt);
        const drop = (1 - easeIn(fall)) * -900;
        const squash = t >= c.t ? 1 - 0.18 * ring(t, [c.t], 0.25) : 1;
        return (
          <div key={i} style={{position: 'absolute', left: W / 2 + (c.x - W / 2) * push - size / 2, top: yBase + drop - coinH,
            transform: `scaleY(${squash})`, transformOrigin: 'bottom'}}>
            {Array.from({length: c.n + extra(t, COLS.indexOf(c))}, (_, k) => (
              <div key={k} style={{position: 'absolute', left: (hash(i * 40 + k) - 0.5) * 6, top: -k * step}}>
                <Coin size={size} tilt={tilt} glow={k === c.n + extra(t, COLS.indexOf(c)) - 1 ? 0.4 + flash : 0} />
              </div>
            ))}
          </div>
        );
      })}
      {RAIN.map((r, k) => {
        const a = clamp((t - (r.t - 0.22)) / 0.22);
        if (a <= 0 || a >= 1) return null;
        const c = COLS[r.col];
        const size = lerp(210, 165, c.depth / 3) * push * 0.9;
        return <div key={k} style={{position: 'absolute', left: W / 2 + (c.x - W / 2) * push - size / 2, top: lerp(-120, 520, easeIn(a))}}>
          <Coin size={size} tilt={0.5 + 0.4 * Math.sin(a * 8 + k)} glow={0.5} />
        </div>;
      })}
      {/* the counter: it is the header */}
      <div style={{position: 'absolute', left: 0, right: 0, top: 36, textAlign: 'center', transform: `scale(${1 + 0.35 * (1 - slam) * (t >= CAP_SLAM ? 1 : 0)})`}}>
        <div style={{fontFamily: PIXEL, fontSize: 40, color: C.white, letterSpacing: 6, textShadow: '0 4px 0 rgba(0,0,0,0.6)'}}>SALARY CAP</div>
        <div style={{fontFamily: PIXEL, fontSize: 132, color: C.gold, marginTop: 14,
          textShadow: `0 10px 0 ${C.goldDeep}, 0 0 ${40 + 120 * flash}px rgba(255,210,74,${0.35 + 0.5 * flash})`}}>
          {count}
        </div>
        <div style={{fontFamily: PIXEL, fontSize: 34, color: C.white, marginTop: 16, letterSpacing: 6, opacity: t >= CAP_SLAM ? 1 : 0.35}}>CREDITS</div>
      </div>
    </AbsoluteFill>
  );
};
