// Pure functions of song time; nothing here reads frames.
export const clamp = (x: number, a = 0, b = 1) => Math.min(b, Math.max(a, x));
export const lin = (t: number, a: number, b: number) => clamp((t - a) / (b - a));
export const lerp = (a: number, b: number, k: number) => a + (b - a) * k;
export const easeOut = (x: number) => 1 - Math.pow(1 - clamp(x), 3);
export const easeIn = (x: number) => Math.pow(clamp(x), 3);
export const easeInOut = (x: number) => {
  const k = clamp(x);
  return k < 0.5 ? 4 * k * k * k : 1 - Math.pow(-2 * k + 2, 3) / 2;
};
export const expoOut = (x: number) => (x >= 1 ? 1 : 1 - Math.pow(2, -10 * clamp(x)));
export const backOut = (x: number, s = 1.9) => {
  const k = clamp(x) - 1;
  return 1 + (s + 1) * k * k * k + s * k * k;
};
// A damped spring from 0 to 1 starting at t0.
export const spring = (t: number, t0: number, freq = 9, damp = 7) => {
  const x = t - t0;
  if (x <= 0) return 0;
  return 1 - Math.exp(-damp * x) * Math.cos(freq * x);
};
// Deterministic noise in [0, 1).
export const hash = (n: number) => {
  const s = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return s - Math.floor(s);
};
// How hard the latest hit still rings, 1 at the hit, fading over `len` s.
export const ring = (t: number, hits: readonly number[], len = 0.45) => {
  let v = 0;
  for (const h of hits) if (t >= h && t < h + len) v = Math.max(v, 1 - (t - h) / len);
  return v * v;
};
