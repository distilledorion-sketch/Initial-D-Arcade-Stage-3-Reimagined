// Saves must survive a page reload (IndexedDB-backed /saves).
//   node persist_test.cjs <dist> <Native> <manifest>
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path'), assert = require('assert');
const [dist, native, manifest] = process.argv.slice(2);
const port = 20000 + Math.floor(Math.random() * 20000);
const server = spawn('node', [path.join(__dirname, '../tools/serve.mjs'), '--www', dist, '--data', native, '--manifest', manifest, '--port', String(port)], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  await sleep(600);
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const ctx = await browser.newContext({ viewport: { width: 640, height: 360 } });
  const stub = (route) => route.fulfill({ contentType: 'text/javascript', body: 'export async function validateDisc(){ return { verified: true, message: "test stub" }; }' });
  const boot = async () => {
    const page = await ctx.newPage();
    await page.route('**/romgate.js', stub);
    await page.goto(`http://127.0.0.1:${port}/?maxlong=320`);
    await page.setInputFiles('#files', { name: 'stub.bin', mimeType: 'application/octet-stream', buffer: Buffer.from('x') });
    await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 120000 });
    assert.equal(await page.evaluate(() => window.__error), undefined);
    return page;
  };
  const list = async (page) => {
    await page.evaluate(() => window.__worker.postMessage({ type: 'listSaves' }));
    await page.waitForFunction(() => window.__saves, null, { timeout: 10000 });
    return page.evaluate(() => { const r = window.__saves; window.__saves = undefined; return r; });
  };
  // Session 1: let the core run so it writes its files, then force a flush.
  let page = await boot();
  await sleep(3000);
  await page.evaluate(() => window.__worker.postMessage({ type: 'save' }));
  await sleep(2000);
  const first = await list(page);
  console.log('session 1 save files:', first.map((f) => `${f.path.replace('/saves/', '')}(${f.size})`).join(', ') || '(none)');
  assert.ok(first.length > 0, 'core wrote no save files in session 1');
  const idb = await page.evaluate(() => new Promise((res, rej) => { const r = indexedDB.open('/saves'); r.onerror = () => rej(r.error); r.onsuccess = () => { const tx = r.result.transaction('FILE_DATA'); const c = tx.objectStore('FILE_DATA').count(); c.onsuccess = () => res(c.result); }; }));
  console.log('IndexedDB entries after flush:', idb);
  assert.ok(idb >= first.length, 'IndexedDB holds fewer entries than the files written');
  await page.close();
  // Session 2: a fresh page in the same browser profile must start with the same files already present.
  page = await boot();
  const second = await list(page);
  const key = (l) => l.map((f) => `${f.path}:${f.size}`).sort().join('|');
  console.log('session 2 save files:', second.length);
  assert.equal(key(second), key(first), 'saves differ after reload');
  console.log('PERSISTENCE OK');
  await browser.close(); server.kill();
})().catch((e) => { console.error('FAIL', e.message); server.kill(); process.exit(1); });
