import React from 'react';
import {Audio, Sequence, staticFile, useCurrentFrame} from 'remotion';
import {FPS, PREROLL, SHOT} from './cues';
import {Lens} from './fx';
import {Board} from './shots/Board';
import {Cap} from './shots/Cap';
import {Coaches} from './shots/Coaches';
import {Finale} from './shots/Finale';
import {GameDay} from './shots/GameDay';
import {Picks} from './shots/Picks';
import {Replays} from './shots/Replays';
import {Thumbnail} from './shots/Thumbnail';

const inShot = (t: number, s: readonly [number, number]) => t >= s[0] && t < s[1];

// Song time drives everything; the first PREROLL seconds hold the thumbnail.
export const Video: React.FC = () => {
  const t = useCurrentFrame() / FPS - PREROLL;
  const shot =
    t < 0 ? <Thumbnail /> :
    inShot(t, SHOT.cap) ? <Cap t={t} /> :
    inShot(t, SHOT.board) ? <Board t={t} /> :
    inShot(t, SHOT.picks) ? <Picks t={t} /> :
    inShot(t, SHOT.gameday) ? <GameDay t={t} /> :
    inShot(t, SHOT.coaches) ? <Coaches t={t} /> :
    inShot(t, SHOT.replays) ? <Replays t={t} /> :
    <Finale t={t} />;
  return (
    <>
      <Lens t={t}>{shot}</Lens>
      <Sequence from={Math.round(PREROLL * FPS)}>
        <Audio src={staticFile('soundtrack.wav')} />
      </Sequence>
    </>
  );
};
