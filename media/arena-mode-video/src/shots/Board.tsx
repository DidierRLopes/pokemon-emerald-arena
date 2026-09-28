// 2. The draft board: a wall of all 150 cards. The camera trucks along it as
// the price tags flip over in a wave, then whip-pans to Magikarp's 2 and
// on to Mewtwo's 32, which ignites gold.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import roster from '../../public/gen/roster.json';
import {MEWTWO_IGNITE, TAG_WAVE, WHIP_MAGIKARP, WHIP_MEWTWO} from '../cues';
import {backOut, clamp, easeInOut, easeOut, expoOut, hash, lerp, lin, ring, spring} from '../anim';
import {Camera, Header, Pix, W, H} from '../fx';
import {C, PIXEL, SMALL} from '../theme';

type Mon = {id: string; name: string; cost: number; dex: number};
const MONS = roster as Mon[];
const COLS = 15;
const CW = 116;
const CH = 144;
const GAP = 14;
const WALL_W = COLS * (CW + GAP);
const WALL_H = 10 * (CH + GAP);
const cardPos = (i: number) => ({x: (i % COLS) * (CW + GAP) + CW / 2, y: Math.floor(i / COLS) * (CH + GAP) + CH / 2});
export const MAGIKARP = MONS.findIndex((m) => m.id === 'MAGIKARP');
export const MEWTWO = MONS.findIndex((m) => m.id === 'MEWTWO');

// Price tag colour by tier: cheap grey-blue up to legendary gold.
const tier = (cost: number) => (cost >= 28 ? C.gold : cost >= 18 ? '#ff9a3c' : cost >= 10 ? C.blue : C.grey);

export const Card: React.FC<{m: Mon; flip: number; gold?: number; dim?: number; scale?: number}> = ({m, flip, gold = 0, dim = 0, scale = 1}) => (
  <div style={{width: CW * scale, height: CH * scale, position: 'relative', transformStyle: 'preserve-3d',
    background: gold ? `linear-gradient(160deg, #3a2a08, #1c1406)` : 'linear-gradient(160deg, #26325e, #151d3c)',
    border: `${3 * scale}px solid ${gold ? C.gold : C.line}`, borderRadius: 8 * scale,
    boxShadow: gold ? `0 0 ${60 * gold}px rgba(255,210,74,${0.7 * gold})` : '0 8px 18px rgba(0,0,0,0.45)', filter: dim ? `grayscale(${dim}) brightness(${1 - 0.5 * dim})` : undefined}}>
    <div style={{position: 'absolute', left: 8 * scale, top: 6 * scale, fontFamily: SMALL, fontSize: 13 * scale, color: C.grey}}>#{String(m.dex).padStart(3, '0')}</div>
    <div style={{position: 'absolute', left: (CW * scale - 84 * scale) / 2, top: 18 * scale}}>
      <Pix src={`gen/icons/${m.id}.png`} w={84 * scale} />
    </div>
    {/* the price tag flips over like a scoreboard flap */}
    <div style={{position: 'absolute', left: 10 * scale, right: 10 * scale, bottom: 8 * scale, height: 30 * scale, borderRadius: 5 * scale,
      background: tier(m.cost), transformOrigin: '50% 0%', transform: `rotateX(${(1 - flip) * -95}deg)`, opacity: flip > 0.02 ? 1 : 0,
      display: 'flex', alignItems: 'center', justifyContent: 'center', fontFamily: PIXEL, fontSize: 18 * scale, color: '#0b0f22'}}>
      {m.cost}
    </div>
  </div>
);

export const Board: React.FC<{t: number}> = ({t}) => {
  // Where the camera looks (wall coordinates) and how close.
  const truck = easeInOut(lin(t, 4, WHIP_MAGIKARP));
  const mk = cardPos(MAGIKARP);
  const mw = cardPos(MEWTWO);
  const toK = expoOut(lin(t, WHIP_MAGIKARP - 0.12, WHIP_MAGIKARP + 0.28));
  const toW = expoOut(lin(t, WHIP_MEWTWO - 0.12, WHIP_MEWTWO + 0.28));
  let fx = lerp(WALL_W * 0.34, WALL_W * 0.62, truck);
  let fy = lerp(WALL_H * 0.36, WALL_H * 0.52, truck);
  let zoom = lerp(0.85, 1.0, truck);
  let ry = lerp(-16, 14, truck);
  fx = lerp(lerp(fx, mk.x, toK), mw.x, toW);
  fy = lerp(lerp(fy, mk.y, toK), mw.y, toW);
  zoom = lerp(lerp(zoom, 3.1, toK), 3.3 + 0.7 * easeInOut(lin(t, MEWTWO_IGNITE, 10)), toW);
  ry = lerp(lerp(ry, 4, toK), lerp(-10, 10, easeInOut(lin(t, MEWTWO_IGNITE, 10))), toW);
  const whip = Math.max(ring(t, [WHIP_MAGIKARP], 0.3), ring(t, [WHIP_MEWTWO], 0.3));
  const whipBlur = whip * 8;
  const ignite = t >= MEWTWO_IGNITE ? easeOut(lin(t, MEWTWO_IGNITE, MEWTWO_IGNITE + 0.4)) : 0;
  const counted = Math.round(150 * clamp((t - TAG_WAVE[0]) / (TAG_WAVE[1] - TAG_WAVE[0])));
  const enter = easeOut(lin(t, 4.0, 4.5));
  return (
    <AbsoluteFill style={{background: `radial-gradient(ellipse at 50% 30%, #1b2550 0%, ${C.night} 70%)`}}>
      {/* a spotlight sweeping the wall */}
      <AbsoluteFill style={{filter: whipBlur ? `blur(${whipBlur}px)` : undefined}}>
        <Camera ry={ry} rx={4} s={zoom * lerp(1.4, 1, enter)} x={0} y={0}>
          <div style={{position: 'absolute', left: W / 2 - fx, top: H / 2 - fy, width: WALL_W, height: WALL_H, transformStyle: 'preserve-3d'}}>
            {MONS.map((m, i) => {
              const p = cardPos(i);
              const wave = TAG_WAVE[0] + ((i % COLS) + Math.floor(i / COLS) * 1.4) / (COLS + 14) * (TAG_WAVE[1] - TAG_WAVE[0]);
              const flip = clamp(spring(t, wave, 16, 9));
              const isW = i === MEWTWO;
              const lift = isW ? 60 * ignite : i === MAGIKARP ? 30 * toK * (1 - toW) : 0;
              return (
                <div key={m.id} style={{position: 'absolute', left: p.x - CW / 2, top: p.y - CH / 2, transform: `translateZ(${lift}px)`, transformStyle: 'preserve-3d'}}>
                  <Card m={m} flip={flip} gold={isW ? ignite : 0} dim={toW > 0.5 && !isW ? 0.6 * ignite : 0} />
                </div>
              );
            })}
            {/* sweeping light over the wall */}
            <div style={{position: 'absolute', inset: -200, pointerEvents: 'none',
              background: `radial-gradient(ellipse 500px 900px at ${lerp(-10, 110, lin(t, 4, 7.5))}% 50%, rgba(160,200,255,0.16), rgba(0,0,0,0) 70%)`}} />
          </div>
        </Camera>
      </AbsoluteFill>
      {/* speed lines on the whips */}
      {whip > 0.05 ? Array.from({length: 18}, (_, i) => (
        <div key={i} style={{position: 'absolute', left: -200 + hash(i * 3) * 600, top: hash(i * 7) * H, width: 900 + hash(i) * 900, height: 3 + hash(i * 2) * 5,
          background: 'linear-gradient(90deg, rgba(255,255,255,0), rgba(255,255,255,0.8), rgba(255,255,255,0))', opacity: whip}} />
      )) : null}
      {/* Magikarp flops up out of its own card: all that for 2 credits */}
      {t >= WHIP_MAGIKARP + 0.1 && t < WHIP_MEWTWO ? (
        <div style={{position: 'absolute', left: W / 2 - 170, top: H / 2 - 330 - 90 * Math.abs(Math.sin((t - WHIP_MAGIKARP) * 7)),
          transform: `scale(${backOut(lin(t, WHIP_MAGIKARP + 0.1, WHIP_MAGIKARP + 0.35))}) rotate(${Math.sin(t * 14) * 18}deg)`}}>
          <Pix src="gen/front/MAGIKARP.png" w={340} />
        </div>
      ) : null}
      {/* Mewtwo: gold rays and the sprite rising out of its card */}
      {t >= WHIP_MEWTWO + 0.2 ? (
        <>
          <AbsoluteFill style={{opacity: ignite * 0.9, background: `repeating-conic-gradient(from ${t * 40}deg at 50% 50%, rgba(255,210,74,0.22) 0deg 8deg, rgba(0,0,0,0) 8deg 22deg)`,
            maskImage: 'radial-gradient(circle at 50% 50%, black 0%, transparent 60%)', WebkitMaskImage: 'radial-gradient(circle at 50% 50%, black 0%, transparent 60%)'}} />
          {/* Mewtwo rises out of its card, huge */}
          <div style={{position: 'absolute', left: W / 2 - 280, top: lerp(H / 2 - 120, H / 2 - 540, easeOut(lin(t, MEWTWO_IGNITE, MEWTWO_IGNITE + 0.5))) - 20 * Math.sin(t * 2.5),
            transform: `scale(${lerp(0.4, 1, easeOut(lin(t, MEWTWO_IGNITE, MEWTWO_IGNITE + 0.5)))})`, filter: `drop-shadow(0 0 40px rgba(255,210,74,${0.7 * ignite}))`}}>
            <Pix src="gen/front/MEWTWO.png" w={560} />
          </div>
          {Array.from({length: 26}, (_, i) => {
            const a = hash(i) * Math.PI * 2;
            const r = 120 + ((t - MEWTWO_IGNITE) * (160 + hash(i + 3) * 260)) % 520;
            return <div key={i} style={{position: 'absolute', left: W / 2 + Math.cos(a) * r, top: H / 2 + Math.sin(a) * r * 0.7, width: 8, height: 8,
              background: C.gold, opacity: ignite * (1 - r / 640), boxShadow: '0 0 12px rgba(255,210,74,0.9)'}} />;
          })}
        </>
      ) : null}
      <Header text="POKéMON · LV. 100 · EACH PRICED" live={`${counted}/150`} appear={lin(t, 4.0, 4.3)} />
    </AbsoluteFill>
  );
};
