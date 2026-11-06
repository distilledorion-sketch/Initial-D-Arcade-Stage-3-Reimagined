// Drives the packaged client with emulated touch only: menus via the on-screen OK button, then pedals + steering.
//   node touch_test.cjs <dist> <Native> <manifest> <outDir>
const { chromium, devices } = require('playwright');
const { spawn } = require('child_process');
const path = require('path'), fs = require('fs');
const [dist, native, manifest, outDir] = process.argv.slice(2);
fs.mkdirSync(outDir, { recursive: true });
const port = 20000 + Math.floor(Math.random() * 20000);
const server = spawn('node', [path.join(__dirname, '../tools/serve.mjs'), '--www', dist, '--data', native, '--manifest', manifest, '--port', String(port)], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  await sleep(600);
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const ctx = await browser.newContext({ viewport: { width: 844, height: 390 }, hasTouch: true, isMobile: true, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  page.on('console', (m) => { if (/error|fail/i.test(m.text()) && !/\[core\]/.test(m.text())) console.log('[page]', m.text().slice(0, 200)); });
  await page.route('**/romgate.js', (route) => route.fulfill({ contentType: 'text/javascript',
    body: 'export async function validateDisc(){ return { verified: true, message: "test stub", format: "stub" }; }' }));
  await page.goto(`http://127.0.0.1:${port}/?stats=1&maxlong=480`);
  await page.setInputFiles('#files', { name: 'stub.bin', mimeType: 'application/octet-stream', buffer: Buffer.from('x') });
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 120000 });
  const status = () => page.evaluate(() => window.__stats?.status);
  const center = async (sel) => { const b = await page.locator(sel).boundingBox(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; };
  const visible = (sel) => page.locator(sel).isVisible();
  console.log('touch layer shown:', await page.evaluate(() => document.getElementById('touch').classList.contains('on')));
  await sleep(1500);
  console.log('menu layout: OK visible', await visible('[data-ctl=ok]'), '| GAS visible', await visible('[data-ctl=gas]'));
  await page.screenshot({ path: path.join(outDir, 'touch_menu.png') });

  // Menus: tap OK (a real touch tap) until the race phase is reached.
  const t0 = Date.now(); let taps = 0;
  for (;;) {
    const s = await status();
    if (s && s.racePhase === 2) break;
    if (Date.now() - t0 > 150000) { console.log('timeout in menus', JSON.stringify(s)); break; }
    const c = await visible('[data-ctl=ok]') ? await center('[data-ctl=ok]') : null;
    if (c) { await page.touchscreen.tap(c.x, c.y); ++taps; }
    await sleep(500);
  }
  console.log('reached race with', taps, 'taps; status', JSON.stringify(await status()));
  await sleep(4000);   // let the countdown finish
  const st = await status();
  console.log('race status', JSON.stringify({ racePhase: st.racePhase, flags: st.flags, speed: st.speed }));
  await page.waitForFunction(() => window.__mode && window.__mode.driving, null, { timeout: 20000 }).catch(() => {});
  console.log('driving layout: GAS visible', await visible('[data-ctl=gas]'), '| OK visible', await visible('[data-ctl=ok]'));
  await page.screenshot({ path: path.join(outDir, 'touch_drive_idle.png') });

  // Multi-touch via CDP: finger 1 holds GAS, finger 2 drags in the steering zone.
  const cdp = await ctx.newCDPSession(page);
  const gas = await center('[data-ctl=gas]');
  const zone = await page.locator('#steerZone').boundingBox();
  const sx = zone.x + zone.width * 0.4, sy = zone.y + zone.height * 0.5;
  const touch = (type, pts) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts });
  await touch('touchStart', [{ x: gas.x, y: gas.y, id: 1 }, { x: sx, y: sy, id: 2 }]);
  for (let i = 1; i <= 8; ++i) { await touch('touchMove', [{ x: gas.x, y: gas.y, id: 1 }, { x: sx + i * 12, y: sy, id: 2 }]); await sleep(60); }
  await sleep(3000);
  const mid = await status();
  const pad = await page.evaluate(() => ({ lt: 0 }));
  await page.screenshot({ path: path.join(outDir, 'touch_drive_gas_steer.png') });
  console.log('while holding gas + steering right: speed', mid.speed.toFixed(1), 'rpm', mid.rpm.toFixed(0), 'ticks', mid.simulationTicks);
  await touch('touchEnd', []);
  await sleep(500);
  const ok = mid.speed > st.speed + 1 && mid.rpm > 1500;
  console.log(ok ? 'TOUCH DRIVING OK' : 'TOUCH DRIVING FAILED');
  await browser.close(); server.kill();
  process.exit(ok ? 0 : 1);
})().catch((e) => { console.error('FAIL', e); server.kill(); process.exit(1); });
