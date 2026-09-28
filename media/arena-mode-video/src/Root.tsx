import React from 'react';
import {Composition, Still} from 'remotion';
import {FPS, TOTAL} from './cues';
import {KeyArt} from './KeyArt';
import {Video} from './Video';

export const Root: React.FC = () => (
  <>
    <Composition id="ArenaMode" component={Video} durationInFrames={Math.round(TOTAL * FPS)} fps={FPS} width={1920} height={1080} />
    <Still id="KeyArt" component={KeyArt} width={1920} height={1080} />
  </>
);
