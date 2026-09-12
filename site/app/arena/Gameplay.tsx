"use client";

import { useEffect, useRef, useState } from "react";

export default function Gameplay() {
  const video = useRef<HTMLVideoElement>(null);
  const [muted, setMuted] = useState(true);

  useEffect(() => {
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      video.current?.pause();
    }
  }, []);

  async function toggleSound() {
    if (!video.current) return;
    const player = video.current;
    player.muted = !player.muted;
    if (!player.muted) player.currentTime = 0;
    setMuted(player.muted);
    try { await player.play(); } catch { /* Native controls remain available. */ }
  }

  return <>
    <div className="game-frame">
      <video ref={video} src="/emerald-arena-15s.mp4" poster="/arena-poster.png"
        width="960" height="640" autoPlay muted loop playsInline controls preload="metadata"
        aria-label="15 seconds of real Charizard versus Blastoise gameplay, with native game music and sound"
        onVolumeChange={() => setMuted(video.current?.muted ?? true)}>
        <a href="/emerald-arena-15s.mp4">Watch the gameplay clip</a>
      </video>
    </div>
    <div className="playback-controls">
      <button onClick={toggleSound} aria-pressed={!muted}>{muted ? "Sound on" : "Mute"}</button>
      <a href="/emerald-arena-15s.mp4" download>Save the clip ↓</a>
    </div>
  </>;
}
