// Boots the packaged web client in headless Chromium against a local server and drives it.
//   node boot_test.cjs <dist dir> <Native dir> <manifest.json> <out dir> [script]
// script (optional): comma list of steps: wait:<ms> key:<code>[:<holdMs>] shot:<name> stats log:<label>
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path'), fs = require('fs');
const [dist, native, manifest, outDir, script = 'wait:3000,shot:boot'] = process.argv.slice(2);
if (!outDir) { console.error('usage: boot_test.cjs dist native manifest outDir [script]'); process.exit(2); }
fs.mkdirSync(outDir, { recursive: true });
const port = 20000 + Math.floor(Math.random() * 20000);
const server = spawn('node', [path.join(__dirname, '../tools/serve.mjs'), '--www', dist, '--data', native, '--manifest', manifest, '--port', String(port)], { stdio: 'inherit' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  await sleep(600);
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required'] });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  page.on('console', (m) => console.log('[page]', m.text().slice(0, 300)));
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  const t0 = Date.now();
  // Test-only: the shipped client has no way to skip the disc check, so the harness replaces the gate
  // module (a real GDS-0033 dump is not available here) and picks a dummy file.
  await page.route('**/romgate.js', (route) => route.fulfill({ contentType: 'text/javascript',
    body: 'export async function validateDisc(){ return { verified: true, message: "test stub", format: "stub" }; }' }));
  await page.goto(`http://127.0.0.1:${port}/?${process.env.QUERY ?? 'stats=1&maxlong=1280'}`);
  await page.setInputFiles('#files', { name: 'stub.bin', mimeType: 'application/octet-stream', buffer: Buffer.from('x') });
  try {
    await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 280000 });
  } catch (e) { console.log('timeout waiting for ready'); }
  const err = await page.evaluate(() => window.__error);
  console.log(`ready after ${((Date.now() - t0) / 1000).toFixed(1)} s`, err ? 'ERROR: ' + err : '');
  if (!err) {
    for (const step of script.split(',')) {
      const [op, a, b] = step.split(':');
      if (op === 'wait') await sleep(Number(a));
      else if (op === 'key') { await page.keyboard.down(a); await sleep(Number(b ?? 120)); await page.keyboard.up(a); await sleep(60); }
      else if (op === 'shot') await page.screenshot({ path: path.join(outDir, a + '.png') });
      else if (op === 'stats') console.log('stats', JSON.stringify(await page.evaluate(() => window.__stats), (k, v) => (typeof v === 'number' ? Math.round(v * 100) / 100 : v)));
      else if (op === 'log') console.log('--', a);
      else if (op === 'serverstats') { const st = await page.evaluate(() => fetch('/__stats').then((r) => r.json())); console.log('server', JSON.stringify(st), `body ${(st.bodyBytes / 1048576).toFixed(1)} MiB`); }
      else if (op === 'audio') console.log('audio', JSON.stringify(await page.evaluate(() => window.__audio), (k, v) => (typeof v === 'number' ? Math.round(v * 10000) / 10000 : v)));
      else if (op === 'until') {   // until:<statusField>:<value>:<key>[:<holdMs>] -> tap key every 600 ms until status[field] == value
        const [, field, value, code, hold = '150'] = step.split(':');
        const t = Date.now();
        for (;;) {
          const s = await page.evaluate(() => window.__stats?.status);
          if (s && String(s[field]) === value) break;
          if (Date.now() - t > 150000) { console.log('until timed out', field, value, JSON.stringify(s)); break; }
          await page.keyboard.down(code); await sleep(Number(hold)); await page.keyboard.up(code); await sleep(450);
        }
      }
      else if (op === 'access') {
        const paths = await page.evaluate(() => new Promise((res) => { const w = window.__worker; const h = (e) => { if (e.data.type === 'accessLog') { w.removeEventListener('message', h); res(e.data.paths); } }; w.addEventListener('message', h); w.postMessage({ type: 'accessLog' }); }));
        fs.writeFileSync(path.join(outDir, a + '.txt'), paths.join('\n') + '\n'); console.log('access log', a, paths.length, 'files');
      }
    }
  }
  await browser.close(); server.kill();
  process.exit(err ? 1 : 0);
})().catch((e) => { console.error(e); server.kill(); process.exit(1); });
