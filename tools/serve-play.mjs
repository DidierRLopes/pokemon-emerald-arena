// Serve play/dist locally with the cross-origin isolation headers the
// emulator needs. --no-isolation leaves them out, to try the service-worker
// fallback that hosts like GitHub Pages rely on.
// Usage: node tools/serve-play.mjs [--port 8123] [--no-isolation]
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {extname, normalize} from 'node:path';
import {fileURLToPath} from 'node:url';

const dist = fileURLToPath(new URL('../play/dist/', import.meta.url));
const port = Number(process.argv[process.argv.indexOf('--port') + 1]) || 8123;
const isolate = !process.argv.includes('--no-isolation');
const types = {'.html': 'text/html; charset=utf-8', '.mjs': 'text/javascript', '.js': 'text/javascript',
  '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.txt': 'text/plain; charset=utf-8'};

createServer(async (request, response) => {
  let path = normalize(decodeURIComponent(new URL(request.url, 'http://x').pathname)).replace(/^[/\\]+/, '');
  if (!path || path.endsWith('/')) path += 'index.html';
  if (path.startsWith('..')) { response.writeHead(403).end(); return; }
  try {
    const body = await readFile(dist + path);
    const headers = {'Content-Type': types[extname(path)] || 'application/octet-stream', 'Cache-Control': 'no-cache'};
    if (isolate) Object.assign(headers, {'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp'});
    response.writeHead(200, headers).end(body);
  } catch {
    response.writeHead(404).end('Not found');
  }
}).listen(port, '127.0.0.1', () => console.log(`Emerald Arena player: http://localhost:${port}/${isolate ? '' : ' (no isolation headers)'}`));
