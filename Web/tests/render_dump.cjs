// Render a .frame (+ optional .ui) dump in headless Chromium and save a PNG.
//   node render_dump.cjs <out.png> <frame> [ui]
// Serves Web/ at / and the dump files under /d/. Needs Playwright (preinstalled in the workspace).
const http = require('http'), fs = require('fs'), path = require('path');
const { chromium } = require('playwright');
const webRoot = path.resolve(__dirname, '..');
const [out, frameFile, uiFile] = process.argv.slice(2);
if (!out || !frameFile) { console.error('usage: render_dump.cjs out.png frame [ui]'); process.exit(2); }
const types = { '.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm' };
const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  let file = null;
  if (url.startsWith('/d/')) { const m = { [path.basename(frameFile)]: frameFile }; if (uiFile) m[path.basename(uiFile)] = uiFile; file = m[url.slice(3)]; }
  else { const p = path.join(webRoot, url); if (p.startsWith(webRoot)) file = p; }
  fs.readFile(file || '', (err, data) => {
    if (err) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream' }); res.end(data);
  });
}).listen(0, async () => {
  const port = server.address().port;
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
  page.on('console', (m) => console.log('[page]', m.text()));
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  let q = `frame=/d/${path.basename(frameFile)}`; if (uiFile) q += `&ui=/d/${path.basename(uiFile)}`;
  await page.goto(`http://127.0.0.1:${port}/tests/render.html?${q}`);
  await page.waitForFunction(() => window.__result, null, { timeout: 120000 });
  const result = await page.evaluate(() => window.__result);
  console.log(JSON.stringify(result));
  if (result.ok) await page.locator('#c').screenshot({ path: out });
  await browser.close(); server.close();
  process.exit(result.ok ? 0 : 1);
});
