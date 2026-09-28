// 6. Replays: the camera swings around the jumbotron, a speed ramp on every
// cut, an LED wipe between clips, the crowd in silhouette in front.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {BEAT, REPLAYS} from '../cues';
import {easeOut, expoOut, hash, lerp, lin, ring} from '../anim';
import {Header, Jumbotron, W} from '../fx';
import {Stadium} from './Stadium';
import {C, PIXEL} from '../theme';

export const Crowd: React.FC<{t: number; shift: number}> = ({t, shift}) => (
  <>
    {[0, 1].map((row) => (
      <div key={row} style={{position: 'absolute', left: -200 + shift * (row ? 1.6 : 1), bottom: row ? -40 : 30, width: W + 400, height: 220}}>
        {Array.from({length: 26}, (_, i) => {
          const x = i * 88 + hash(i + row * 50) * 40;
          const bob = Math.max(0, Math.sin(t * 7 + i)) * 14 * (hash(i * 3 + row) > 0.5 ? 1 : 0.3);
          const arm = hash(i * 7 + row) > 0.7;
          return (
            <div key={i} style={{position: 'absolute', left: x, bottom: bob}}>
              <div style={{width: 64, height: 64, borderRadius: 32, background: row ? '#03050c' : '#070b18', marginLeft: 20}} />
              <div style={{width: 104, height: 120, borderRadius: '40px 40px 0 0', background: row ? '#03050c' : '#070b18', marginTop: -6}} />
              {arm ? <div style={{position: 'absolute', left: 70, top: -70, width: 18, height: 110, borderRadius: 9, background: row ? '#03050c' : '#070b18',
                transform: `rotate(${20 + 10 * Math.sin(t * 9 + i)}deg)`}} /> : null}
            </div>
          );
        })}
      </div>
    ))}
  </>
);

export const Replays: React.FC<{t: number}> = ({t}) => {
  const k = REPLAYS.filter((r) => t >= r.t).length - 1;
  const r = REPLAYS[Math.max(0, k)];
  const seg = lin(t, r.t, r.t + 0.75);
  // A speed ramp on every cut: a fast swing that settles.
  const ry = lerp(k % 2 ? 11 : -11, k % 2 ? -3 : 3, expoOut(seg));
  const zoom = lerp(1.2, 1.0, expoOut(seg)) * lerp(1, 1.04, lin(t, 22, 28));
  const wipe = lin(t, r.t, r.t + 0.14);
  const flashes = Array.from({length: 14}, (_, i) => ({x: hash(i) * W, y: 900 + hash(i + 7) * 160, t0: 22 + i * (BEAT * 0.75) + hash(i * 3) * 0.2}));
  const GW = 1320;
  const GH = 880;
  return (
    <AbsoluteFill>
      <Stadium t={t} always crowd={1} />
      <AbsoluteFill style={{transform: `scale(${zoom})`}}>
        <div style={{position: 'absolute', left: W / 2 - GW / 2 - 20, top: 150, transform: `perspective(2200px) rotateY(${ry}deg)`, transformOrigin: '50% 50%'}}>
          <Jumbotron clip={r.clip} at={r.t} from={r.from} w={GW} h={GH} glow={1.3}>
            {wipe < 1 ? Array.from({length: 24}, (_, i) => (
              <div key={i} style={{position: 'absolute', top: 0, bottom: 0, left: `${(i / 24) * 100}%`, width: `${100 / 24 + 0.2}%`,
                background: i % 2 ? C.blue : C.emerald, opacity: Math.max(0, 1 - Math.abs(wipe * 1.6 - i / 24) * 4)}} />
            )) : null}
          </Jumbotron>
        </div>
      </AbsoluteFill>
      {/* REPLAY bug in the margin, clear of the game's own HUD */}
      <div style={{position: 'absolute', left: 60, top: 190, fontFamily: PIXEL, fontSize: 30, color: 'white', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 16,
        padding: '18px 16px', background: 'rgba(5,8,20,0.8)', borderRadius: 10, border: `3px solid ${C.red}88`}}>
        <div style={{width: 22, height: 22, borderRadius: 11, background: C.red, opacity: Math.sin(t * 8) > 0 ? 1 : 0.25}} />
        {'REPLAY'.split('').map((ch, i) => <div key={i}>{ch}</div>)}
      </div>
      {flashes.map((f, i) => {
        const v = ring(t, [f.t0], 0.15);
        return v ? <div key={i} style={{position: 'absolute', left: f.x, top: f.y, width: 110, height: 110, borderRadius: 55, marginLeft: -55, marginTop: -55,
          background: 'radial-gradient(circle, rgba(255,255,255,0.95), rgba(255,255,255,0) 70%)', opacity: v}} /> : null;
      })}
      <div style={{position: 'absolute', left: 0, right: 0, bottom: -170, height: 260}}>
        <Crowd t={t} shift={(ry / 11) * -50} />
      </div>
      <Header text="REAL-TIME BATTLES · INSIDE THE GBA" center y={36} appear={lin(t, 22.0, 22.3)} color={C.emerald} />
    </AbsoluteFill>
  );
};
