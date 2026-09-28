// The pre-roll the feed shows as the preview: the product doing its job,
// full frame (a real Flamethrower and its speech bubble), under one big
// lockup. Same night navy as the opening, so playback doesn't flash.
import React from 'react';
import {AbsoluteFill, Img, staticFile} from 'remotion';
import {C, PIXEL} from '../theme';

export const Thumbnail: React.FC = () => (
  <AbsoluteFill style={{background: C.night}}>
    <Img src={staticFile('clips/flame-poster.png')} style={{position: 'absolute', left: 0, top: -100, width: 1920, height: 1280, imageRendering: 'pixelated'}} />
    <AbsoluteFill style={{background: 'radial-gradient(ellipse 1000px 560px at 24% 50%, rgba(7,11,28,0.95) 0%, rgba(7,11,28,0.85) 55%, rgba(7,11,28,0) 100%)'}} />
    <div style={{position: 'absolute', left: 90, top: 340}}>
      <div style={{fontFamily: PIXEL, fontSize: 92, color: C.emerald, lineHeight: 1.25, textShadow: '0 10px 0 #0b4d33, 0 0 50px rgba(57,224,140,0.45)'}}>EMERALD<br />ARENA</div>
      <div style={{display: 'inline-block', marginTop: 44, padding: '22px 38px', border: `8px solid ${C.gold}`, borderRadius: 14, fontFamily: PIXEL,
        fontSize: 76, color: C.gold, background: 'rgba(255,210,74,0.1)', boxShadow: '0 0 50px rgba(255,210,74,0.4)'}}>ARENA MODE</div>
    </div>
  </AbsoluteFill>
);
