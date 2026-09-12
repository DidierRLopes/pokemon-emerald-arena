import assert from "node:assert/strict";
import { readFile, stat } from "node:fs/promises";
import test from "node:test";

async function render(path) {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(
    new Request("https://emerald-arena.example" + path, { headers: { accept: "text/html", host: "emerald-arena.example" } }),
    { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } },
    { waitUntil() {}, passThroughOnException() {} },
  );
}

for (const path of ["/", "/arena"]) {
  test(path + " serves the English product, download and real video", async () => {
    const response = await render(path);
    assert.equal(response.status, 200);
    const html = await response.text();
    assert.match(html, /<html lang="en"/);
    assert.match(html, /Pokémon/);
    assert.match(html, /Download the demo/);
    assert.match(html, /releases\/download\/v0\.3\.1\/Emerald-Arena-0\.3\.1\.zip/);
    assert.match(html, /href="\/prepare\.html"/);
    assert.match(html, /<video[^>]*src="\/emerald-arena-17s\.mp4"/);
    assert.match(html, /17 seconds: walking through Emerald/);
    for (const attribute of ["autoPlay", "muted", "loop", "playsInline", "controls"]) {
      assert.match(html, new RegExp("<video[^>]*" + attribute, "i"));
    }
    assert.match(html, /Sound on/);
    assert.match(html, /https:\/\/emerald-arena\.example\/og\.png/);
    assert.match(html, /summary_large_image/);
    assert.doesNotMatch(html, /codex-preview|SkeletonPreview|Your site is taking shape/);
  });
}

test("release assets are present and the installer stays local-only", async () => {
  for (const name of ["emerald-arena-17s.mp4", "emerald-arena-15s.mp4", "arena-poster.png", "og.png", "prepare.html"]) {
    assert.ok((await stat(new URL("../public/" + name, import.meta.url))).size > 1000);
  }
  const installer = await readFile(new URL("../public/prepare.html", import.meta.url), "utf8");
  assert.match(installer, /lang="en"/);
  assert.match(installer, /Your file is never uploaded/);
  assert.match(installer, /crypto\.subtle\.digest/);
  assert.match(installer, /download\.download='Emerald-Arena-0\.3\.1\.gba'/);
  assert.match(installer, /\[hidden\]\{display:none!important\}/);
  assert.doesNotMatch(installer, /XMLHttpRequest|FormData|method:[ ]*['"]POST/);
});
