// The stadium at night, shared by game day, the coaches, the replays and
// the final whistle: sky, stands full of crowd lights, four floodlight
// towers, the pitch, and the jumbotron above it.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {LIGHT_BANKS} from '../cues';
import {clamp, easeOut, hash, lin} from '../anim';
import {H, W} from '../fx';
import {C} from '../theme';

const TOWERS = [150, 560, 1360, 1770];

export const lightsOn = (t: number, i: number) => easeOut(lin(t, LIGHT_BANKS[i], LIGHT_BANKS[i] + 0.12));

export const Stadium: React.FC<{t: number; always?: boolean; crowd?: number}> = ({t, always = false, crowd = 0}) => {
  const on = (i: number) => (always ? 1 : lightsOn(t, i));
  const all = [0, 1, 2, 3].reduce((s, i) => s + on(i), 0) / 4;
  return (
    <AbsoluteFill>
      <AbsoluteFill style={{background: `linear-gradient(180deg, #02040c 0%, #081024 55%, #0d1a3a 100%)`}} />
      {Array.from({length: 90}, (_, i) => (
        <div key={i} style={{position: 'absolute', left: hash(i) * W, top: hash(i + 50) * 380, width: 2, height: 2, background: 'white',
          opacity: (0.2 + 0.5 * hash(i + 9)) * (1 - 0.6 * all)}} />
      ))}
      {/* the stands: two tiers wrapping the pitch, crowd lights twinkling */}
      <svg width={W} height={H} style={{position: 'absolute', inset: 0}}>
        <polygon points={`0,470 ${W},470 ${W},820 0,820`} fill="#0a1130" />
        <polygon points={`0,560 ${W},560 ${W},860 0,860`} fill="#0d1638" />
        {Array.from({length: 12}, (_, r) => (
          <line key={r} x1={0} x2={W} y1={480 + r * 30} y2={480 + r * 30} stroke="#16214a" strokeWidth={2} />
        ))}
      </svg>
      {Array.from({length: 260}, (_, i) => {
        const x = hash(i * 3.1) * W;
        const y = 485 + hash(i * 5.7) * 330;
        const tw = crowd ? 0.5 + 0.5 * Math.sin(t * (6 + hash(i) * 8) + i) : 0.5;
        const c = hash(i * 2.2) > 0.5 ? C.blue : hash(i * 1.3) > 0.5 ? C.red : '#ffe9a8';
        return <div key={i} style={{position: 'absolute', left: x, top: y, width: 3, height: 3, background: c,
          opacity: (0.15 + 0.7 * all) * tw * (0.5 + crowd * 0.5)}} />;
      })}
      {/* the pitch */}
      <div style={{position: 'absolute', left: -200, right: -200, top: 820, height: 400, background: `linear-gradient(180deg, #0f3a2a, #07261b)`,
        transform: 'perspective(900px) rotateX(58deg)', transformOrigin: '50% 0%'}}>
        {Array.from({length: 9}, (_, i) => (
          <div key={i} style={{position: 'absolute', top: 0, bottom: 0, left: `${(i + 0.5) * 11.1}%`, width: 3, background: 'rgba(255,255,255,0.12)'}} />
        ))}
      </div>
      {/* floodlight towers: poles, bulb banks and their light cones */}
      {TOWERS.map((x, i) => {
        const o = on(i);
        return (
          <React.Fragment key={i}>
            <div style={{position: 'absolute', left: x - 7, top: 150, width: 14, height: 700, background: '#101832'}} />
            <div style={{position: 'absolute', left: x - 80, top: 90, width: 160, height: 90, background: '#141d3a', borderRadius: 6,
              display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: 6, padding: 10, boxSizing: 'border-box',
              boxShadow: `0 0 ${140 * o}px ${40 * o}px rgba(255,236,190,${0.55 * o})`}}>
              {Array.from({length: 10}, (_, k) => (
                <div key={k} style={{borderRadius: 30, background: o > 0.5 ? '#fff6dc' : '#2a3350', boxShadow: `0 0 ${18 * o}px rgba(255,240,200,${o})`}} />
              ))}
            </div>
            <div style={{position: 'absolute', left: x - 600, top: 150, width: 1200, height: 900, opacity: 0.22 * o, mixBlendMode: 'screen',
              background: 'linear-gradient(180deg, rgba(255,244,214,0.9), rgba(255,244,214,0))',
              clipPath: `polygon(${50 - 2}% 0%, ${50 + 2}% 0%, ${50 + 34 - (i < 2 ? 20 : 0)}% 100%, ${50 - 34 + (i < 2 ? 0 : 20)}% 100%)`}} />
          </React.Fragment>
        );
      })}
      <AbsoluteFill style={{background: `radial-gradient(ellipse at 50% 40%, rgba(120,150,255,${0.08 * all}), rgba(0,0,0,0) 60%)`}} />
      <AbsoluteFill style={{background: 'linear-gradient(180deg, rgba(0,0,0,0) 60%, rgba(7,11,28,0.55))', opacity: clamp(1)}} />
    </AbsoluteFill>
  );
};
