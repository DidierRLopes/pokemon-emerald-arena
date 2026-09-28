// The one timing source. Every event is in song seconds at 120 BPM (a beat
// is 0.5 s, a bar 2 s). The scenes and audio/synth.ts both read this file,
// so moving a cue moves its picture and its sound together.

export const BPM = 120;
export const BEAT = 60 / BPM;
export const BAR = BEAT * 4;
export const FPS = 60;
// The first 0.3 s hold the thumbnail; picture and music start after it.
export const PREROLL = 0.3;
export const SONG = 33;
export const TOTAL = PREROLL + SONG;

// Shots: [start, end) in song seconds.
export const SHOT = {
  cap: [0, 4],
  board: [4, 10],
  picks: [10, 14],
  gameday: [14, 16],
  coaches: [16, 22],
  replays: [22, 28],
  finale: [28, 33],
} as const;

// 1. Salary cap: chip columns land centre-out on the eighths, then chips rain
// onto them until the counter slams 100.
export const COIN_DROPS = [0, 0.25, 0.5, 0.75, 1, 1.25, 1.5];
export const COIN_RAIN = [1.75, 3.25];
export const CAP_SLAM = 3.5;

// 2. The board: price tags flip in a wave, whip to Magikarp, then Mewtwo.
export const TAG_WAVE = [4.0, 6.0];
export const WHIP_MAGIKARP = 7.0;
export const WHIP_MEWTWO = 7.75;
export const MEWTWO_IGNITE = 8.0;

// 3. The picks: chips slide onto cards; Mewtwo is over the cap.
export const PICKS = [
  {t: 10.0, id: 'CHARIZARD'},
  {t: 10.5, id: 'GENGAR'},
  {t: 11.0, id: 'GYARADOS'},
  {t: 11.5, id: 'ALAKAZAM'},
  {t: 12.0, id: 'PIKACHU'},
  {t: 13.25, id: 'DRAGONITE'},
];
export const OVER_CAP = {t: 12.5, stamp: 12.75, id: 'MEWTWO'};

// 4. Game day: four floodlight banks on the kick, then VS.
export const LIGHT_BANKS = [14.0, 14.25, 14.5, 14.75];
export const VS_SLAM = 15.0;

// 5. The coaches: one shout, then two at once, cut where the voice changes.
export const SHOUT_ONE = 16.25;
export const CLIP_FLAME = 16.75;
export const SHOUT_TWO = 19.0;
export const VOICE_CUT = 19.75;
export const CLIP_DUO = 20.25;

// 6. Replays: full-frame, a cut every three beats, each on its impact frame.
export const REPLAYS = [
  {t: 22.0, clip: 'rollout', from: 0.7},
  {t: 22.75, clip: 'toss', from: 1.75},
  {t: 23.5, clip: 'psychic', from: 1.0},
  {t: 24.25, clip: 'flame', from: 1.75},
  {t: 25.0, clip: 'duo', from: 3.05},
  {t: 25.75, clip: 'rollout', from: 1.5},
  {t: 26.5, clip: 'toss', from: 2.3},
  {t: 27.25, clip: 'psychic', from: 1.7},
];

// 7. Final whistle: WINS!, confetti cannons, the lockup.
export const WINS_SLAM = 28.0;
export const CONFETTI = [28.0, 28.5, 29.0];
export const LOCKUP = 29.75;
export const FADE_OUT = [32.4, 33.0];

// Every hard hit, for the impact sound and the lens (shake, colour split).
export const IMPACTS = [CAP_SLAM, WHIP_MAGIKARP, MEWTWO_IGNITE, OVER_CAP.stamp, VS_SLAM, VOICE_CUT, 22.0, WINS_SLAM, LOCKUP];
// Risers end on these.
export const RISERS = [
  {from: 2.0, to: CAP_SLAM},
  {from: 12.0, to: 14.0},
  {from: 20.5, to: 22.0},
  {from: 26.5, to: WINS_SLAM},
];
