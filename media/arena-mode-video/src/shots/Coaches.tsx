// 5. The coaches: a shout leaves the blue coach as a waveform and flies into
// the jumbotron, where it becomes the move. Then both coaches shout at once:
// one merged waveform, cut exactly where the voice changes, each half
// flying to its own side of the screen.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {CLIP_DUO, CLIP_FLAME, SHOUT_ONE, SHOUT_TWO, VOICE_CUT} from '../cues';
import {clamp, easeIn, easeInOut, easeOut, hash, lerp, lin, ring} from '../anim';
import {Header, Jumbotron, Pix, W} from '../fx';
import {SCREEN} from './GameDay';
import {Stadium} from './Stadium';
import {C, PIXEL, SMALL} from '../theme';

// A spoken phrase as waveform bars: syllable-shaped envelope, seeded detail.
const Wave: React.FC<{bars: number; w: number; h: number; color: string; grow: number; seed: number; t: number}> = ({bars, w, h, color, grow, seed, t}) => (
  <svg width={w} height={h} style={{overflow: 'visible', display: 'block', filter: `drop-shadow(0 0 14px ${color})`}}>
    {Array.from({length: bars}, (_, i) => {
      const k = i / bars;
      const syll = Math.pow(Math.sin(k * Math.PI * (3 + (seed % 2))), 2) * Math.sin(k * Math.PI);
      const a = (0.25 + 0.75 * syll) * (0.55 + 0.45 * hash(seed * 100 + i + Math.floor(t * 30))) * clamp(grow * 1.4 - k * 0.4);
      const bh = Math.max(4, a * h);
      return <rect key={i} x={i * (w / bars)} y={(h - bh) / 2} width={(w / bars) * 0.6} height={bh} rx={2} fill={color} />;
    })}
  </svg>
);

const Coach: React.FC<{who: 'brendan' | 'may'; x: number; color: string; shout: number; t: number}> = ({who, x, color, shout, t}) => (
  <div style={{position: 'absolute', left: x - 190, top: 640 + 8 * Math.sin(t * 5 + x), transformOrigin: '50% 100%',
    transform: `${who === 'may' ? 'scaleX(-1)' : ''} rotate(${-9 * shout}deg) scale(${1 + 0.12 * shout}) translateX(${40 * shout}px)`}}>
    {[0, 1, 2].map((r) => (
      <div key={r} style={{position: 'absolute', left: 250, top: 40 - r * 20, width: 90 + r * 70, height: 120 + r * 60, borderRadius: '50%',
        border: `6px solid ${color}`, borderLeftColor: 'transparent', borderTopColor: 'transparent', borderBottomColor: 'transparent',
        opacity: shout * (1 - r * 0.25), transform: `scale(${0.8 + 0.3 * shout})`}} />
    ))}
    <Pix src={`gen/${who}.png`} w={380} style={{filter: `drop-shadow(0 0 22px ${color}) brightness(0.95)`}} />
  </div>
);

export const Coaches: React.FC<{t: number}> = ({t}) => {
  // Into the screen when a voice lands, back out for the next shout.
  const inFlame = easeInOut(lin(t, CLIP_FLAME, CLIP_FLAME + 0.4)) * (1 - easeInOut(lin(t, SHOUT_TWO - 0.35, SHOUT_TWO)));
  const inDuo = easeInOut(lin(t, CLIP_DUO, CLIP_DUO + 0.4));
  const dive = Math.max(inFlame, inDuo);
  const push = lerp(1, 1.7, dive);
  // The first shout: out of Brendan, into the screen.
  const one = lin(t, SHOUT_ONE, CLIP_FLAME);
  const onePath = easeInOut(one);
  const oneShow = t >= SHOUT_ONE && t < CLIP_FLAME + 0.1;
  // The second: both coaches into one merged strip at the centre.
  const build = easeOut(lin(t, SHOUT_TWO, SHOUT_TWO + 0.6));
  const cut = t >= VOICE_CUT;
  const split = easeIn(lin(t, VOICE_CUT, CLIP_DUO));
  const strip = t >= SHOUT_TWO && t < CLIP_DUO + 0.05;
  const blade = lin(t, SHOUT_TWO + 0.6, VOICE_CUT);
  const clip = t >= CLIP_DUO ? 'duo' : t >= CLIP_FLAME ? 'flame' : undefined;
  const from = t >= CLIP_DUO ? 0.2 : 1.0;
  const bx = lerp(340, SCREEN.x - 220, onePath);
  const by = lerp(720, SCREEN.y + 140, onePath) - Math.sin(onePath * Math.PI) * 180;
  return (
    <AbsoluteFill>
    <AbsoluteFill style={{transform: `translateY(${(540 - SCREEN.y) * dive}px) scale(${push})`, transformOrigin: `50% ${(SCREEN.y / 1080) * 100}%`}}>
      <Stadium t={t} always crowd={0.6} />
      <div style={{position: 'absolute', left: SCREEN.x - SCREEN.w / 2 - 20, top: SCREEN.y - SCREEN.h / 2 - 20}}>
        <Jumbotron clip={clip} at={t >= CLIP_DUO ? CLIP_DUO : CLIP_FLAME} from={from} w={SCREEN.w} h={SCREEN.h} glow={1 + ring(t, [CLIP_FLAME, CLIP_DUO], 0.5)}>
          {!clip || (t >= CLIP_FLAME + 3.4 && t < CLIP_DUO) ? (
            // Between clips: the jumbotron listens (a live input meter).
            <div style={{position: 'absolute', inset: 0, background: '#060a18', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 10}}>
              {Array.from({length: 24}, (_, i) => (
                <div key={i} style={{width: 18, height: 30 + 200 * hash(i + Math.floor(t * 12)) * (strip ? 1 : 0.3), background: i < 12 ? C.blue : C.red, opacity: 0.8}} />
              ))}
            </div>
          ) : null}
        </Jumbotron>
      </div>
      <Coach who="brendan" x={250} color={C.blue} t={t} shout={Math.max(ring(t, [SHOUT_ONE], 0.6), ring(t, [SHOUT_TWO], 0.8))} />
      <Coach who="may" x={W - 250} color={C.red} t={t} shout={ring(t, [SHOUT_TWO], 0.8)} />
      {/* shout one flies into the screen */}
      {oneShow ? (
        <div style={{position: 'absolute', left: bx - 200, top: by - 60, transform: `scale(${lerp(1, 0.45, onePath)})`, opacity: 1 - lin(t, CLIP_FLAME - 0.05, CLIP_FLAME + 0.1)}}>
          <Wave bars={40} w={400} h={120} color={C.blue} grow={lin(t, SHOUT_ONE, SHOUT_ONE + 0.25)} seed={1} t={t} />
        </div>
      ) : null}
      {/* shout two: one strip, two voices, cut where the colour changes */}
      {strip ? (
        <>
          <div style={{position: 'absolute', left: W / 2 - 460 - split * 250, top: 700 - split * 380, transform: `scale(${lerp(1, 0.4, split)})`, transformOrigin: 'right center'}}>
            <Wave bars={44} w={460} h={140} color={C.blue} grow={build} seed={2} t={t} />
          </div>
          <div style={{position: 'absolute', left: W / 2 + split * 250, top: 700 - split * 380, transform: `scale(${lerp(1, 0.4, split)})`, transformOrigin: 'left center'}}>
            <Wave bars={44} w={460} h={140} color={C.red} grow={build} seed={3} t={t} />
          </div>
          {/* the blade: scans in, strikes at the boundary */}
          {!cut || t < VOICE_CUT + 0.2 ? (
            <div style={{position: 'absolute', left: lerp(W / 2 - 520, W / 2, easeInOut(blade)) - 3, top: 600, width: 6, height: 340,
              background: 'white', boxShadow: '0 0 30px 8px rgba(255,255,255,0.8)', opacity: cut ? 1 - lin(t, VOICE_CUT, VOICE_CUT + 0.2) : 0.9,
              transform: `scaleY(${cut ? 1.4 : 1})`}} />
          ) : null}
        </>
      ) : null}
    </AbsoluteFill>
      {/* type lives outside the camera move, in clear space */}
      <div style={{opacity: 1 - dive}}>
        <Header text={t < SHOUT_TWO - 0.35 ? 'YOUR VOICE IS THE CONTROLLER' : 'TWO COACHES · ONE MIC'} center y={890}
          appear={t < SHOUT_TWO - 0.35 ? lin(t, 16.0, 16.3) : lin(t, SHOUT_TWO - 0.35, SHOUT_TWO)} color={t < SHOUT_TWO - 0.35 ? C.blue : C.white} />
      </div>
      {/* the one exact claim, with its source */}
      <div style={{position: 'absolute', left: '50%', bottom: 30, transform: 'translateX(-50%)', padding: '14px 30px 12px', borderRadius: 10,
        background: 'rgba(5,8,20,0.85)', border: `3px solid ${C.blue}66`, textAlign: 'center', whiteSpace: 'nowrap',
        opacity: lin(t, CLIP_FLAME + 0.4, CLIP_FLAME + 0.6) * (1 - lin(t, SHOUT_TWO - 0.5, SHOUT_TWO - 0.3))}}>
        <div style={{fontFamily: PIXEL, fontSize: 40, color: C.white}}>HEARD IN <span style={{color: C.gold}}>120 MS</span></div>
        <div style={{fontFamily: SMALL, fontSize: 20, color: C.grey, marginTop: 8}}>WHISPER ON-DEVICE, MEDIAN OF 261 COMMANDS · JEV PICKS THE MOVE IN 215 MS (MEDIAN, 101 CALLS)</div>
      </div>
    </AbsoluteFill>
  );
};
