// The headline-number still: 150 Pokémon, 100 credits, one golden Mewtwo.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import roster from '../public/gen/roster.json';
import {Coin, Pix} from './fx';
import {Card} from './shots/Board';
import {C, PIXEL} from './theme';

type Mon = {id: string; name: string; cost: number; dex: number};
const mons = roster as Mon[];

export const KeyArt: React.FC = () => (
  <AbsoluteFill style={{background: `radial-gradient(ellipse at 60% 50%, #22306a, ${C.night} 75%)`, overflow: 'hidden'}}>
    <div style={{position: 'absolute', left: 900, top: -60, display: 'grid', gridTemplateColumns: 'repeat(8, 128px)', gap: 12,
      transform: 'perspective(1600px) rotateY(-24deg) rotateX(8deg)', opacity: 0.55}}>
      {mons.slice(0, 64).map((m) => <Card key={m.id} m={m} flip={1} />)}
    </div>
    <div style={{position: 'absolute', left: 1180, top: 330, transform: 'rotate(6deg)'}}>
      <Card m={mons.find((m) => m.id === 'MEWTWO')!} flip={1} gold={1} scale={2.6} />
    </div>
    <div style={{position: 'absolute', left: 1290, top: 110, filter: 'drop-shadow(0 0 30px rgba(255,210,74,0.6))'}}>
      <Pix src="gen/front/MEWTWO.png" w={360} />
    </div>
    <div style={{position: 'absolute', left: 110, top: 250}}>
      <div style={{fontFamily: PIXEL, fontSize: 150, color: C.white, lineHeight: 1}}>150</div>
      <div style={{fontFamily: PIXEL, fontSize: 44, color: C.grey, marginTop: 20}}>POKéMON · LV.100</div>
      <div style={{fontFamily: PIXEL, fontSize: 150, color: C.gold, lineHeight: 1, marginTop: 70, textShadow: `0 10px 0 ${C.goldDeep}`}}>100</div>
      <div style={{fontFamily: PIXEL, fontSize: 44, color: C.grey, marginTop: 20}}>CREDITS · 6 PICKS</div>
    </div>
    <div style={{position: 'absolute', left: 760, top: 820}}>
      {[0, 1, 2, 3, 4, 5, 6].map((k) => <div key={k} style={{position: 'absolute', top: -k * 12}}><Coin size={150} tilt={0.36} /></div>)}
    </div>
    <div style={{position: 'absolute', left: 110, bottom: 70, fontFamily: PIXEL, fontSize: 32, color: C.emerald}}>EMERALD ARENA · ARENA MODE</div>
  </AbsoluteFill>
);
