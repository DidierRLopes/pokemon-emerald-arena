// 3. The picks: each card drops into a bench seat as chips arc from your pile
// up into its price tag; the credits run down. Mewtwo swoops at the last
// seat, is stamped OVER THE CAP mid-air and bounces off; Dragonite fits.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import roster from '../../public/gen/roster.json';
import {OVER_CAP, PICKS} from '../cues';
import {backOut, clamp, easeIn, easeInOut, easeOut, hash, lerp, lin, ring, spring} from '../anim';
import {Coin, Header, W} from '../fx';
import {Card} from './Board';
import {C, PIXEL} from '../theme';

type Mon = {id: string; name: string; cost: number; dex: number};
const byId = Object.fromEntries((roster as Mon[]).map((m) => [m.id, m]));
const SCALE = 2.05;
const CARD_W = 116 * SCALE;
const CARD_H = 144 * SCALE;
const SEAT = (i: number) => ({x: W / 2 + (i - 2.5) * 292, y: 500});
const PILE = {x: W / 2, y: 960};

export const creditsLeft = (t: number) =>
  100 - PICKS.filter((p) => t >= p.t + 0.1).reduce((s, p) => s + byId[p.id].cost, 0);

export const Picks: React.FC<{t: number}> = ({t}) => {
  const left = creditsLeft(t);
  const push = lerp(1.0, 1.05, easeInOut(lin(t, 10, 14)));
  const out = easeIn(lin(t, 13.72, 14.0));
  const mew = byId[OVER_CAP.id];
  const stamp = t >= OVER_CAP.stamp ? backOut(lin(t, OVER_CAP.stamp, OVER_CAP.stamp + 0.16), 1.2) : 0;
  // Mewtwo's path: in from the top, at the empty seat, then knocked away.
  const seat6 = SEAT(5);
  const swoop = easeOut(lin(t, OVER_CAP.t, OVER_CAP.stamp));
  const knock = easeIn(lin(t, OVER_CAP.stamp + 0.42, OVER_CAP.stamp + 0.72));
  const mx = lerp(lerp(W / 2 + 200, seat6.x - 320, swoop), seat6.x - 100, knock);
  const my = lerp(lerp(-420, seat6.y - 330, swoop), seat6.y + 900, knock * knock) - Math.sin(knock * Math.PI) * 140;
  return (
    <AbsoluteFill style={{background: C.night, opacity: 1 - out}}>
      <AbsoluteFill style={{transform: `scale(${push})`}}>
        <AbsoluteFill style={{background: `radial-gradient(ellipse 1300px 700px at 50% 62%, #22336a 0%, #111936 55%, ${C.night} 85%)`}} />
        {/* the bench */}
        <div style={{position: 'absolute', left: SEAT(0).x - 150, right: W - SEAT(5).x - 150, top: SEAT(0).y + CARD_H / 2 - 10, height: 34, borderRadius: 8,
          background: 'linear-gradient(180deg, #3a4a86, #1c2650)', boxShadow: '0 20px 40px rgba(0,0,0,0.6)'}} />
        {Array.from({length: 6}, (_, i) => {
          const s = SEAT(i);
          return <div key={i} style={{position: 'absolute', left: s.x - CARD_W / 2 - 8, top: s.y - CARD_H / 2 - 8, width: CARD_W + 16, height: CARD_H + 16,
            borderRadius: 16, border: `4px dashed ${C.line}`, background: 'rgba(20,27,54,0.5)'}} />;
        })}
        {/* your chip pile, shrinking as you spend */}
        {Array.from({length: Math.max(1, Math.round(left / 7))}, (_, k) => (
          <div key={k} style={{position: 'absolute', left: PILE.x - 80 + (hash(k) - 0.5) * 8, top: PILE.y - k * 11}}>
            <Coin size={160} tilt={0.36} />
          </div>
        ))}
        {PICKS.map((p, i) => {
          if (t < p.t - 0.25) return null;
          const s = SEAT(i);
          const land = spring(t, p.t - 0.25, 12, 9);
          const stamped = t >= p.t + 0.12 ? backOut(lin(t, p.t + 0.12, p.t + 0.28)) : 0;
          const paid = ring(t, [p.t + 0.08], 0.4);
          return (
            <React.Fragment key={p.id}>
              <div style={{position: 'absolute', left: s.x - CARD_W / 2, top: lerp(-420, s.y - CARD_H / 2, clamp(land, 0, 1.15)),
                transform: `rotate(${(1 - clamp(land)) * (i % 2 ? 16 : -16)}deg)`, filter: paid ? `drop-shadow(0 0 ${30 * paid}px rgba(255,210,74,0.9))` : undefined}}>
                <Card m={byId[p.id]} flip={1} scale={SCALE} />
                <div style={{position: 'absolute', left: -10, top: CARD_H * 0.58, padding: '10px 14px', border: `5px solid ${C.emerald}`, color: C.emerald, borderRadius: 8,
                  fontFamily: PIXEL, fontSize: 26, transform: `rotate(-12deg) scale(${stamped ? 1 + (1 - stamped) * 1.4 : 0})`, opacity: stamped ? 1 : 0,
                  background: 'rgba(7,11,28,0.8)'}}>PICKED</div>
              </div>
              {/* the chips paying for it: from the pile up into the price tag */}
              {Array.from({length: 5}, (_, k) => {
                const k0 = p.t - 0.26 + k * 0.04;
                const a = clamp((t - k0) / 0.3);
                if (a <= 0 || a >= 1) return null;
                const tx = s.x;
                const ty = s.y + CARD_H / 2 - 36;
                return <div key={k} style={{position: 'absolute', left: lerp(PILE.x, tx, a) - 40, top: lerp(PILE.y - 40, ty, a) - Math.sin(a * Math.PI) * 220,
                  transform: `scale(${lerp(1, 0.45, a)})`}}>
                  <Coin size={80} tilt={0.5 + 0.5 * Math.sin(a * 9 + k)} glow={0.8} />
                </div>;
              })}
            </React.Fragment>
          );
        })}
        {/* Mewtwo: swoops at the last seat, stamped mid-air, knocked away */}
        {t >= OVER_CAP.t && t < OVER_CAP.stamp + 0.75 ? (
          <div style={{position: 'absolute', left: mx - 150, top: my, transform: `rotate(${lerp(-8, 50, knock)}deg)`}}>
            <Card m={mew} flip={1} scale={2.3} gold={stamp ? 0 : 0.9} dim={stamp ? 0.6 : 0} />
            <div style={{position: 'absolute', left: -70, top: 120, padding: '16px 24px', border: `9px solid ${C.red}`, color: C.red, borderRadius: 10,
              fontFamily: PIXEL, fontSize: 44, whiteSpace: 'nowrap', background: 'rgba(7,11,28,0.85)',
              transform: `rotate(-16deg) scale(${stamp ? 1 + (1 - stamp) * 2.2 : 0})`, opacity: stamp ? 1 : 0,
              boxShadow: `0 0 ${50 * ring(t, [OVER_CAP.stamp], 0.6)}px rgba(255,91,75,0.9)`}}>OVER THE CAP</div>
          </div>
        ) : null}
      </AbsoluteFill>
      <Header text="CREDITS LEFT" live={String(left)} appear={lin(t, 10, 10.3)} />
    </AbsoluteFill>
  );
};
