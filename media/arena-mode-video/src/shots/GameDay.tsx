// 4. Game day: the camera cranes up the stadium as the floodlight banks
// clunk on with the kick; the jumbotron boots and VS slams between the two
// drafted teams.
import React from 'react';
import {AbsoluteFill} from 'remotion';
import {LIGHT_BANKS, PICKS, VS_SLAM} from '../cues';
import {backOut, easeInOut, easeOut, lerp, lin, ring} from '../anim';
import {Jumbotron, Pix, W} from '../fx';
import {Crowd} from './Replays';
import {Stadium} from './Stadium';
import {C, PIXEL} from '../theme';

export const COM_TEAM = ['BLASTOISE', 'MACHAMP', 'SCIZOR', 'LAPRAS', 'ARCANINE', 'STARMIE'];
export const SCREEN = {w: 960, h: 480, x: W / 2, y: 330};

export const VersusCard: React.FC<{t: number; slam: number}> = ({t, slam}) => {
  const hit = ring(t, [VS_SLAM], 0.5);
  const side = (team: string[], color: string, left: boolean) => (
    <div style={{position: 'absolute', top: 0, bottom: 0, [left ? 'left' : 'right']: 0, width: '50%',
      background: `linear-gradient(${left ? 90 : 270}deg, ${color}55, ${color}11)`, display: 'flex', flexDirection: 'column', justifyContent: 'center',
      alignItems: left ? 'flex-start' : 'flex-end', padding: '0 34px', boxSizing: 'border-box',
      transform: `translateX(${(left ? -1 : 1) * (1 - easeOut(lin(t, LIGHT_BANKS[0] + 0.2, VS_SLAM))) * 480}px)`}}>
      <div style={{fontFamily: PIXEL, fontSize: 30, color, marginBottom: 26, textShadow: '0 4px 0 rgba(0,0,0,0.6)'}}>{left ? 'PLAYER 1' : 'COM'}</div>
      <div style={{display: 'grid', gridTemplateColumns: 'repeat(3, 92px)', gap: 6}}>
        {team.map((id) => <Pix key={id} src={`gen/icons/${id}.png`} w={92} />)}
      </div>
    </div>
  );
  return (
    <div style={{position: 'absolute', inset: 0, background: '#070b1c'}}>
      {side(PICKS.map((p) => p.id), C.blue, true)}
      {side(COM_TEAM, C.red, false)}
      <div style={{position: 'absolute', left: '50%', top: '50%', transform: `translate(-50%, -50%) scale(${slam ? lerp(3, 1, slam) : 0}) rotate(${-6 * (1 - slam)}deg)`,
        fontFamily: PIXEL, fontSize: 130, color: C.gold, textShadow: `0 10px 0 ${C.red}, 0 0 ${30 + 80 * hit}px rgba(255,210,74,0.8)`}}>VS</div>
    </div>
  );
};

export const GameDay: React.FC<{t: number}> = ({t}) => {
  const crane = easeInOut(lin(t, 14, VS_SLAM));
  const boot = easeOut(lin(t, LIGHT_BANKS[0], LIGHT_BANKS[0] + 0.2));
  const slam = t >= VS_SLAM ? backOut(lin(t, VS_SLAM, VS_SLAM + 0.25)) : 0;
  const punch = lerp(1, 1.85, easeInOut(lin(t, VS_SLAM, 16)));
  return (
    <AbsoluteFill style={{transform: `translateY(${lerp(200, 0, crane)}px) scale(${lerp(1.1, 1, crane) * punch})`, transformOrigin: `50% ${SCREEN.y / 10.8}%`}}>
      <Stadium t={t} />
      <div style={{position: 'absolute', left: SCREEN.x - SCREEN.w / 2 - 20, top: SCREEN.y - SCREEN.h / 2 - 20,
        transform: `perspective(1400px) rotateX(${lerp(14, 0, crane)}deg)`}}>
        <Jumbotron w={SCREEN.w} h={SCREEN.h} glow={boot} power={boot}>
          {/* boot line, then the card */}
          <div style={{position: 'absolute', left: 0, right: 0, top: '50%', height: 4, background: 'white', transform: `scaleY(${1 + 120 * boot})`,
            opacity: boot < 1 ? 1 - boot : 0}} />
          {boot > 0.5 ? <VersusCard t={t} slam={slam} /> : null}
        </Jumbotron>
      </div>
      <Crowd t={t} shift={-40 * crane} />
    </AbsoluteFill>
  );
};
