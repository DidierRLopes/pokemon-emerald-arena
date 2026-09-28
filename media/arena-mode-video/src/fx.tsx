import React from 'react';
import {AbsoluteFill, Img, OffthreadVideo, Sequence, staticFile, useCurrentFrame} from 'remotion';
import {FPS, IMPACTS, LOCKUP, PREROLL} from './cues';
import {hash, ring} from './anim';
import {C, PIXEL} from './theme';

export const W = 1920;
export const H = 1080;

// A perspective stage and a camera inside it: everything a shot draws sits
// in this 3D space, so camera moves give real parallax.
export const Camera: React.FC<{
  x?: number; y?: number; z?: number; rx?: number; ry?: number; rz?: number; s?: number; persp?: number;
  children: React.ReactNode;
}> = ({x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, s = 1, persp = 1600, children}) => (
  <AbsoluteFill style={{perspective: persp, perspectiveOrigin: '50% 50%', overflow: 'hidden'}}>
    <AbsoluteFill
      style={{
        transformStyle: 'preserve-3d',
        transform: `translate3d(${x}px, ${y}px, ${z}px) rotateX(${rx}deg) rotateY(${ry}deg) rotateZ(${rz}deg) scale(${s})`,
      }}
    >
      {children}
    </AbsoluteFill>
  </AbsoluteFill>
);

// Something placed at a point in the 3D space (centre-anchored).
export const Place: React.FC<{x: number; y: number; z?: number; rx?: number; ry?: number; s?: number; style?: React.CSSProperties; children: React.ReactNode}> = ({
  x, y, z = 0, rx = 0, ry = 0, s = 1, style, children,
}) => (
  <div
    style={{
      position: 'absolute', left: x, top: y, transformStyle: 'preserve-3d',
      transform: `translate(-50%, -50%) translateZ(${z}px) rotateX(${rx}deg) rotateY(${ry}deg) scale(${s})`,
      ...style,
    }}
  >
    {children}
  </div>
);

// Pixel art, kept crisp at any scale.
export const Pix: React.FC<{src: string; w: number; h?: number; style?: React.CSSProperties}> = ({src, w, h, style}) => (
  <Img src={staticFile(src)} style={{width: w, height: h ?? w, imageRendering: 'pixelated', display: 'block', ...style}} />
);

// A gold credit coin seen at `tilt` (1 from above, towards 0 edge-on).
export const Coin: React.FC<{size: number; tilt: number; thick?: number; glow?: number}> = ({size, tilt, thick = 0.12, glow = 0}) => {
  const ry = size * 0.5 * Math.max(0.12, tilt);
  const band = size * thick * Math.sqrt(1 - Math.min(1, tilt) ** 2);
  return (
    <svg width={size + 4} height={ry * 2 + band + 4} style={{overflow: 'visible', display: 'block', filter: glow ? `drop-shadow(0 0 ${12 * glow}px rgba(255,210,74,${0.8 * glow}))` : undefined}}>
      <ellipse cx={size / 2 + 2} cy={ry + band + 2} rx={size / 2} ry={ry} fill={C.goldDeep} />
      <rect x={2} y={ry + 2} width={size} height={band} fill={C.goldDeep} />
      <ellipse cx={size / 2 + 2} cy={ry + 2} rx={size / 2} ry={ry} fill={C.gold} />
      <ellipse cx={size / 2 + 2} cy={ry + 2} rx={size * 0.36} ry={ry * 0.72} fill="none" stroke={C.goldDeep} strokeWidth={size * 0.05} />
      <text x={size / 2 + 2} y={ry + 2 + ry * 0.36} fontFamily={PIXEL} fontSize={size * 0.34} textAnchor="middle" fill={C.goldDeep}
        transform={`translate(0 ${(ry + 2) * (1 - tilt)}) scale(1 ${tilt})`}>C</text>
      <ellipse cx={size * 0.34 + 2} cy={ry * 0.62 + 2} rx={size * 0.1} ry={ry * 0.14} fill="rgba(255,255,255,0.55)" />
    </svg>
  );
};

// A gameplay clip that starts playing at song time `at`, from `from` seconds
// into the clip (a video follows the timeline, so it needs its own sequence).
export const Clip: React.FC<{clip: string; at: number; from?: number; style?: React.CSSProperties}> = ({clip, at, from = 0, style}) => (
  <Sequence key={`${clip}@${at}`} from={Math.round((at + PREROLL) * FPS)} layout="none">
    <OffthreadVideo src={staticFile(`clips/${clip}.mp4`)} startFrom={Math.round(from * FPS)} muted style={{imageRendering: 'pixelated', ...style}} />
  </Sequence>
);

// A gameplay clip on an LED jumbotron: pixels kept square, LED dot mask,
// scanline sheen and a glass reflection.
export const Jumbotron: React.FC<{
  clip?: string; at?: number; from?: number; w: number; h: number; children?: React.ReactNode; glow?: number; power?: number;
}> = ({clip, at = 0, from = 0, w, h, children, glow = 1, power = 1}) => (
  <div style={{position: 'relative', width: w + 40, height: h + 40, background: '#05070f', borderRadius: 10,
    boxShadow: `0 0 ${90 * glow}px rgba(88,182,255,${0.28 * glow}), inset 0 0 0 3px #26314f, 0 30px 60px rgba(0,0,0,0.6)`}}>
    <div style={{position: 'absolute', left: 20, top: 20, width: w, height: h, overflow: 'hidden', background: '#000', opacity: power}}>
      {clip ? <Clip clip={clip} at={at} from={from} style={{width: w, height: h, filter: 'saturate(1.25) contrast(1.08)'}} /> : null}
      {children}
      <div style={{position: 'absolute', inset: 0, backgroundImage: 'radial-gradient(circle at 50% 50%, rgba(0,0,0,0) 45%, rgba(0,0,0,0.55) 72%)',
        backgroundSize: '6px 6px', mixBlendMode: 'multiply'}} />
      <div style={{position: 'absolute', inset: 0, background: 'linear-gradient(120deg, rgba(255,255,255,0.10) 0%, rgba(255,255,255,0.02) 35%, rgba(255,255,255,0) 60%)'}} />
    </div>
  </div>
);

// Film grain, vignette, and the lens on every hit: flash, shake and a
// red/cyan colour split, all driven by the impact cues.
export const Lens: React.FC<{t: number; children: React.ReactNode}> = ({t, children}) => {
  const frame = useCurrentFrame();
  const hit = ring(t, IMPACTS, 0.35);
  const sx = (hash(frame * 1.7) - 0.5) * 34 * hit;
  const sy = (hash(frame * 2.3 + 9) - 0.5) * 26 * hit;
  const split = 10 * hit;
  return (
    <AbsoluteFill style={{background: C.night}}>
      <AbsoluteFill style={{transform: `translate(${sx}px, ${sy}px) scale(${1 + 0.02 * hit})`,
        filter: split > 0.5 ? `drop-shadow(${split}px 0 0 rgba(255,40,60,0.55)) drop-shadow(${-split}px 0 0 rgba(40,220,255,0.55))` : undefined}}>
        {children}
      </AbsoluteFill>
      <AbsoluteFill style={{background: 'white', opacity: 0.28 * ring(t, IMPACTS.filter((h) => h !== LOCKUP), 0.14), mixBlendMode: 'screen'}} />
      <AbsoluteFill style={{background: 'radial-gradient(ellipse at 50% 50%, rgba(0,0,0,0) 55%, rgba(0,0,0,0.62) 100%)'}} />
      <AbsoluteFill style={{opacity: 0.09, mixBlendMode: 'overlay'}}>
        <svg width={W} height={H}>
          <filter id="grain"><feTurbulence type="fractalNoise" baseFrequency="0.85" numOctaves="2" seed={frame % 23} /></filter>
          <rect width={W} height={H} filter="url(#grain)" />
        </svg>
      </AbsoluteFill>
    </AbsoluteFill>
  );
};

// One orientation header per shot: big pixel type on a dark plate so it
// reads over anything, typing on fast, with an optional live part.
export const Header: React.FC<{text: string; live?: string; appear: number; color?: string; x?: number; y?: number; center?: boolean}> = ({
  text, live, appear, color = C.white, x = 80, y = 70, center = false,
}) => {
  const k = Math.min(1, Math.max(0, appear));
  const shown = Math.ceil(text.length * Math.min(1, k * 3));
  return (
    <div style={{position: 'absolute', left: center ? '50%' : x, top: y, transform: center ? 'translateX(-50%)' : undefined,
      padding: '18px 28px 16px', background: 'rgba(5,8,20,0.78)', borderRadius: 10, border: `3px solid ${color}55`,
      fontFamily: PIXEL, fontSize: 42, color, letterSpacing: 2, whiteSpace: 'nowrap', textShadow: '0 4px 0 rgba(0,0,0,0.6)',
      opacity: Math.min(1, k * 5), boxShadow: '0 12px 30px rgba(0,0,0,0.5)'}}>
      <span style={{display: 'inline-block', width: 16, height: 40, background: color, marginRight: 24, verticalAlign: 'top'}} />
      {text.slice(0, shown)}
      {live ? <span style={{color: C.gold, marginLeft: 30}}>{live}</span> : null}
    </div>
  );
};
