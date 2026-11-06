// Development server for the web client: static files with HTTP Range support.
//
//   node Web/tools/serve.mjs --www <dist dir> --data <Native dir> --manifest <manifest.json> [--port 8080]
//
// /          -> the built client (see Web/build/package_web.sh)
// /game/...  -> <Native dir>/data/... only (the lazy file loader's base URL; Range requests)
// /game/manifest.json -> the file from Web/tools/make_manifest.py
// Not a production server: no compression, no auth. Put the same files behind any static host that
// supports Range requests (nearly all do) and long-lived caching for /game/.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const args = Object.fromEntries(process.argv.slice(2).reduce((acc, a, i, all) => (a.startsWith('--') ? [...acc, [a.slice(2), all[i + 1]]] : acc), []));
const www = path.resolve(args.www ?? 'dist');
const dataRoot = args.data ? path.resolve(args.data) : null;
const port = Number(args.port ?? 8080);
const manifest = args.manifest ? path.resolve(args.manifest) : null;
const types = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm', '.json': 'application/json', '.webmanifest': 'application/manifest+json', '.css': 'text/css', '.png': 'image/png', '.svg': 'image/svg+xml',
};

// Transfer counters, readable at /__stats: lets tests measure what a client actually downloads.
const counters = { requests: 0, ranged: 0, gzipped: 0, bodyBytes: 0 };
const compressible = new Set(['.idasmesh', '.idastex', '.idasasm', '.dtpk', '.rcl', '.idasdialog', '.json', '.js', '.html', '.wasm']);

function resolve(url) {
  const clean = decodeURIComponent(url.split('?')[0]);
  if (clean === '/game/manifest.json') return manifest ? { file: manifest, immutable: false } : null;
  if (clean.startsWith('/game/')) {
    if (!dataRoot) return null;
    const p = path.resolve(dataRoot, clean.slice('/game/'.length));
    return p.startsWith(path.join(dataRoot, 'data') + path.sep) ? { file: p, immutable: true } : null;
  }
  const p = path.resolve(www, '.' + (clean.endsWith('/') ? clean + 'index.html' : clean));
  return p.startsWith(www + path.sep) || p === www ? { file: p, immutable: false } : null;
}

http.createServer((req, res) => {
  if (req.url === '/__stats') { res.writeHead(200, { 'content-type': 'application/json' }); res.end(JSON.stringify(counters)); return; }
  const target = resolve(req.url);
  if (!target || (req.method !== 'GET' && req.method !== 'HEAD')) { res.writeHead(404); res.end(); return; }
  fs.stat(target.file, (err, st) => {
    if (err || !st.isFile()) { res.writeHead(404); res.end(); return; }
    const headers = {
      'content-type': types[path.extname(target.file)] ?? 'application/octet-stream',
      'accept-ranges': 'bytes',
      'cache-control': target.immutable ? 'public, max-age=31536000, immutable' : 'no-cache',
    };
    let start = 0, end = st.size - 1, status = 200;
    const range = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range ?? '');
    if (range && (range[1] || range[2])) {
      if (range[1] === '') { start = Math.max(0, st.size - Number(range[2])); }
      else { start = Number(range[1]); if (range[2]) end = Math.min(end, Number(range[2])); }
      if (start > end || start >= st.size) { res.writeHead(416, { 'content-range': `bytes */${st.size}` }); res.end(); return; }
      status = 206; headers['content-range'] = `bytes ${start}-${end}/${st.size}`;
    }
    ++counters.requests; if (status === 206) ++counters.ranged;
    const gzip = status === 200 && req.method === 'GET' && st.size > 4096 && compressible.has(path.extname(target.file))
      && /\bgzip\b/.test(req.headers['accept-encoding'] ?? '');
    if (gzip) { delete headers['content-length']; headers['content-encoding'] = 'gzip'; headers.vary = 'accept-encoding'; delete headers['accept-ranges']; ++counters.gzipped; }
    else headers['content-length'] = end - start + 1;
    res.writeHead(status, headers);
    if (req.method === 'HEAD') { res.end(); return; }
    let body = fs.createReadStream(target.file, { start, end });
    if (gzip) body = body.pipe(zlib.createGzip({ level: 6 }));
    body.on('data', (chunk) => { counters.bodyBytes += chunk.length; });
    body.pipe(res);
  });
}).listen(port, () => console.log(`serving ${www}${dataRoot ? ` and ${dataRoot}/data at /game/` : ''} on http://127.0.0.1:${port}/`));
