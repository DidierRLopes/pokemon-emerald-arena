const release = 'https://github.com/GBurgardt/pokemon-emerald-arena/releases/download/v0.3.1/Emerald-Arena-0.3.1.zip';
export default function Arena() {
  return <main>
    <nav><a className="wordmark" href="/arena">EMERALD<span>ARENA</span></a><a className="repo" href="https://github.com/GBurgardt/pokemon-emerald-arena">Source ↗</a></nav>
    <section className="hero">
      <div className="intro">
        <div className="eyebrow"><i /> PLAYABLE DEMO · GAME BOY ADVANCE</div>
        <h1>Pokémon<br />Emerald.<br /><em>In real time.</em></h1>
        <p className="lead">Move. Dodge. Attack. Break the arena.<br />Your team keeps its experience.</p>
        <a className="cta" href={release}>Download the demo <span>↓</span></a>
        <a className="prepare-link" href="/prepare.html">Or prepare your ROM in the browser ↗</a>
        <p className="fine">Requires your own Emerald ROM (USA/Europe).<br />Your file stays on your device.</p>
      </div>
      <div className="stage">
        <div className="stage-label"><span>RUNNING INSIDE THE GBA GAME.</span><span>GBA · 240 × 160</span></div>
        <Gameplay />
        <p className="caption"><span className="live-dot" /> REAL GAMEPLAY <span>12 Pokémon. A different pace.</span></p>
      </div>
    </section>
    <section className="details" aria-label="Three things to try">
      <div><span>01</span><h2>You make the moves.</h2><p>Eight directions, a dodge, and attacks with their own reach and timing.</p></div>
      <div><span>02</span><h2>Break your way through.</h2><p>Rocks, wood and crystals. Impacts, flying fragments and chain explosions.</p></div>
      <div><span>03</span><h2>Still your team.</h2><p>Native HP, PP and experience. Classic battles are one button away.</p></div>
    </section>
    <footer><span>An experiment by German Burgardt.</span><span>Playable demo · Not all moves are adapted yet.</span></footer>
  </main>;
}
import Gameplay from './Gameplay';
