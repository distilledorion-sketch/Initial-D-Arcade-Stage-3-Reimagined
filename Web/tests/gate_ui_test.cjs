// The real gate in the real page (no stub): wrong files are refused and the game never starts.
//   node gate_ui_test.cjs <dist> <Native> <manifest> <out dir>
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path'), fs = require('fs'), assert = require('assert');
const [dist, native, manifest, outDir] = process.argv.slice(2);
fs.mkdirSync(outDir, { recursive: true });
const port = 20000 + Math.floor(Math.random() * 20000);
const server = spawn('node', [path.join(__dirname, '../tools/serve.mjs'), '--www', dist, '--data', native, '--manifest', manifest, '--port', String(port)], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  await sleep(600);
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 900, height: 600 } });
  await page.goto(`http://127.0.0.1:${port}/`);
  await page.screenshot({ path: path.join(outDir, 'gate.png') });
  assert.equal(await page.evaluate(() => typeof window.__worker), 'undefined', 'game must not start before verification');
  // 1. right name, wrong bytes (and wrong size)
  await page.setInputFiles('#files', { name: 'GDS-0033.chd', mimeType: 'application/octet-stream', buffer: Buffer.from('not the disc') });
  await page.waitForFunction(() => document.getElementById('gateMessage').className === 'bad');
  const m1 = await page.evaluate(() => document.getElementById('gateMessage').textContent);
  console.log('wrong chd ->', m1); assert.match(m1, /does not match/);
  // 2. wrong name entirely
  await page.setInputFiles('#files', { name: 'something.chd', mimeType: 'application/octet-stream', buffer: Buffer.from('x') });
  await page.waitForFunction(() => /Choose/.test(document.getElementById('gateMessage').textContent));
  console.log('wrong name ->', await page.evaluate(() => document.getElementById('gateMessage').textContent));
  // 3. a cue that does not describe the disc
  await page.setInputFiles('#files', [{ name: 'gds-0033.cue', mimeType: 'text/plain', buffer: Buffer.from('FILE "x.bin" BINARY\n') }]);
  await page.waitForFunction(() => /cue does not describe/.test(document.getElementById('gateMessage').textContent));
  console.log('bad cue    ->', await page.evaluate(() => document.getElementById('gateMessage').textContent));
  await sleep(500);
  assert.equal(await page.evaluate(() => typeof window.__worker), 'undefined', 'game must still not be running');
  assert.equal(await page.evaluate(() => document.getElementById('gate').classList.contains('hidden')), false);
  await page.screenshot({ path: path.join(outDir, 'gate_rejected.png') });
  console.log('GATE UI OK: nothing started');
  await browser.close(); server.kill();
})().catch(async (e) => { console.error('FAIL', e.message); server.kill(); process.exit(1); });
