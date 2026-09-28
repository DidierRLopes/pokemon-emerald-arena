// 7. Final whistle: PLAYER 1 WINS! on the jumbotron, confetti cannons on the
// beat, then the camera pushes into the screen for the lockup.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {CONFETTI, FADE_OUT, LOCKUP, WINS_SLAM} from '../cues';
import {backOut, easeInOut, easeOut, hash, lerp, lin, ring} from '../anim';
import {Clip, H, Jumbotron, W} from '../fx';
import {Crowd} from './Replays';
import {Stadium} from './Stadium';
import {C, PIXEL, SMALL} from '../theme';

const COLORS = [C.gold, C.blue, C.red, C.emerald, C.white];

const Confetti: React.FC<{t: number}> = ({t}) => (
  <>
    {CONFETTI.flatMap((t0, b) =>
      [0, 1].flatMap((side) =>
        Array.from({length: 46}, (_, i) => {
          const dt = t - t0;
          if (dt < 0 || dt > 3.2) return [];
          const seed = b * 1000 + side * 500 + i;
          const ang = (side ? -1 : 1) * (0.55 + hash(seed) * 0.5);
          const v = 1300 + hash(seed + 1) * 900;
          const x0 = side ? W + 20 : -20;
          const drag = 1 - Math.exp(-dt * 2.2);
          const x = x0 + (Math.cos(Math.PI / 2 - ang) * v * drag) / 2.2 * (side ? 1 : 1);
          const y = H + 20 - (Math.sin(Math.PI / 2 - Math.abs(ang)) * v * drag) / 2.2 + 260 * dt * dt;
          return [<div key={seed} style={{position: 'absolute', left: x, top: y, width: 14, height: 22, background: COLORS[i % 5],
            transform: `rotate(${dt * (400 + hash(seed + 2) * 600)}deg) scaleX(${Math.cos(dt * 12 + i)})`, opacity: 1 - lin(dt, 2.6, 3.2)}} />];
        }),
      ),
    )}
  </>
);

export const Finale: React.FC<{t: number}> = ({t}) => {
  const slam = backOut(lin(t, WINS_SLAM, WINS_SLAM + 0.3));
  const pulse = 0.5 + 0.5 * Math.sin((t - WINS_SLAM) * Math.PI * 4);
  const dive = easeInOut(lin(t, LOCKUP - 0.4, LOCKUP + 0.3));
  const lock = easeOut(lin(t, LOCKUP, LOCKUP + 0.5));
  const fade = lin(t, FADE_OUT[0], FADE_OUT[1]);
  return (
    <AbsoluteFill style={{opacity: 1 - fade}}>
      <AbsoluteFill style={{transform: `scale(${lerp(1, 2.35, dive)})`, transformOrigin: '50% 38%'}}>
        <Stadium t={t} always crowd={1} />
        <div style={{position: 'absolute', left: W / 2 - 700, top: 90}}>
          <Jumbotron w={1360} h={680} glow={1.4 + ring(t, [WINS_SLAM], 0.8)}>
            <div style={{position: 'absolute', inset: 0, background: 'radial-gradient(ellipse at 50% 40%, #1b2a5c, #070b1c 75%)',
              display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', opacity: 1 - lock}}>
              <div style={{fontFamily: PIXEL, fontSize: 70, color: C.blue, textShadow: '0 8px 0 #0a1c3a', transform: `scale(${slam})`}}>PLAYER 1</div>
              <div style={{fontFamily: PIXEL, fontSize: 190, color: pulse > 0.5 ? C.gold : '#fff3c4', marginTop: 34,
                textShadow: `0 14px 0 ${C.red}, 0 0 60px rgba(255,210,74,0.8)`, transform: `scale(${slam})`}}>WINS!</div>
            </div>
          </Jumbotron>
        </div>
        <Crowd t={t} shift={0} />
      </AbsoluteFill>
      <Confetti t={t} />
      {/* the lockup over live gameplay, once the camera is inside the screen */}
      {t >= LOCKUP - 0.1 ? (
        <AbsoluteFill style={{opacity: lock}}>
          <Clip clip="duo" at={LOCKUP - 0.1} from={2.4}
            style={{width: 1920, height: 1280, marginTop: -100, transform: `scale(${1.05 + 0.03 * lin(t, LOCKUP, 33)})`}} />
          <AbsoluteFill style={{background: 'radial-gradient(ellipse at 50% 50%, rgba(7,11,28,0.72), rgba(7,11,28,0.93) 70%)'}} />
          <AbsoluteFill style={{display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center'}}>
            <div style={{fontFamily: PIXEL, fontSize: 124, color: C.emerald, letterSpacing: 4, textShadow: `0 12px 0 #0b4d33, 0 0 60px rgba(57,224,140,0.5)`,
              transform: `translateY(${(1 - lock) * 40}px)`}}>EMERALD ARENA</div>
            <div style={{marginTop: 52, padding: '24px 46px', border: `8px solid ${C.gold}`, borderRadius: 14, fontFamily: PIXEL, fontSize: 84, color: C.gold,
              background: 'rgba(255,210,74,0.1)', boxShadow: '0 0 50px rgba(255,210,74,0.4)', transform: `scale(${backOut(lin(t, LOCKUP + 0.15, LOCKUP + 0.5))})`}}>ARENA MODE</div>
            <div style={{marginTop: 60, fontFamily: PIXEL, fontSize: 38, color: C.white, letterSpacing: 2, opacity: lin(t, LOCKUP + 0.4, LOCKUP + 0.7)}}>
              <span style={{color: C.blue}}>1P VS COM</span>  ·  <span style={{color: C.red}}>1P VS 1P</span>  ·  VOICE
            </div>
            <div style={{marginTop: 44, fontFamily: SMALL, fontSize: 50, color: C.white, opacity: lin(t, LOCKUP + 0.6, LOCKUP + 0.9)}}>
              github.com/DidierRLopes/pokemon-emerald-arena
            </div>
          </AbsoluteFill>
        </AbsoluteFill>
      ) : null}
    </AbsoluteFill>
  );
};
