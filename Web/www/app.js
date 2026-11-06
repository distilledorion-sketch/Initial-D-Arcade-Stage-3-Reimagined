// Main-thread shell: owns the page, forwards input to the game worker and shows load/stat overlays.
// URL options: ?data=<base url of /data, default /game/>  ?scale=<0.25..1>  ?maxlong=<px>  ?stats=1
import { PAD } from './abi.js';
import { validateDisc } from './romgate.js';
import { initTouch } from './touch.js';
import { AudioOutput } from './audio.js';
const audio = new AudioOutput();

const params = new URLSearchParams(location.search);
const dataBase = new URL(params.get('data') ?? '/game/', location.href).href;
const coarse = matchMedia('(pointer: coarse)').matches;
const renderScale = Number(params.get('scale') ?? (coarse ? 0.75 : 1));
const maxLong = Number(params.get('maxlong') ?? (coarse ? 1280 : 1920));

const canvas = document.getElementById('game');
const overlay = document.getElementById('overlay');
const message = document.getElementById('message');
const bar = overlay.querySelector('#bar > div');
const errorBox = document.getElementById('error');
const statsBox = document.getElementById('stats');

// Windows virtual-key codes the core reads (event.code -> VK).
const VK = { Backspace: 8, Tab: 9, Enter: 13, NumpadEnter: 13, ShiftLeft: 16, ShiftRight: 16, ControlLeft: 17, ControlRight: 17,
  AltLeft: 18, AltRight: 18, Pause: 19, Escape: 27, Space: 32, PageUp: 33, PageDown: 34, End: 35, Home: 36,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Insert: 45, Delete: 46, Semicolon: 186, Equal: 187,
  Comma: 188, Minus: 189, Period: 190, Slash: 191, Backquote: 192, BracketLeft: 219, Backslash: 220, BracketRight: 221, Quote: 222 };
for (let i = 0; i < 10; ++i) { VK['Digit' + i] = 48 + i; VK['Numpad' + i] = 96 + i; }
for (let i = 0; i < 26; ++i) VK['Key' + String.fromCharCode(65 + i)] = 65 + i;
for (let i = 1; i <= 12; ++i) VK['F' + i] = 111 + i;

// Everything the worker needs, as one latest-state record. `sources` merge keyboard, gamepad and touch.
export const state = {
  keys: new Uint32Array(8), padConnected: false, padButtons: 0, thumbLX: 0, thumbLY: 0, thumbRX: 0, thumbRY: 0,
  leftTrigger: 0, rightTrigger: 0, gear: 0,
};
const sources = { keyboard: new Uint32Array(8), pad: { connected: false, buttons: 0, lx: 0, ly: 0, rx: 0, ry: 0, lt: 0, rt: 0 },
  touch: { active: false, buttons: 0, lx: 0, ly: 0, lt: 0, rt: 0, keys: new Uint32Array(8), gear: 0 } };
export const inputSources = sources;

// Presses are latched until a message carrying them has been sent: a tap whose press and release both land
// between two main-thread frames would otherwise never be seen by the game.
const latch = { keys: new Uint32Array(8), buttons: 0 };
let worker = null, dirty = true;
const touch = initTouch({ root: document.getElementById('touch'), sources, latch, markDirty: () => { dirty = true; } });
// Show the on-screen controls on touch devices from the start, and anywhere else on the first touch.
if (coarse || params.has('touch')) touch.show();
addEventListener('pointerdown', (e) => { if (e.pointerType === 'touch') touch.show(); }, { capture: true, passive: true });
export function markInputDirty() { dirty = true; }

function mergeInput() {
  const k = sources.keyboard, p = sources.pad, t = sources.touch;
  for (let i = 0; i < 8; ++i) state.keys[i] = k[i] | t.keys[i] | latch.keys[i];
  state.padConnected = p.connected || t.active;
  state.padButtons = p.buttons | t.buttons | latch.buttons;
  const pick = (a, b) => (Math.abs(b) > Math.abs(a) ? b : a);
  state.thumbLX = pick(p.lx, t.lx); state.thumbLY = pick(p.ly, t.ly); state.thumbRX = p.rx; state.thumbRY = p.ry;
  state.leftTrigger = Math.max(p.lt, t.lt); state.rightTrigger = Math.max(p.rt, t.rt);
  state.gear = t.gear;
}

function pollGamepad() {
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  const pad = [...pads].find((g) => g && g.connected);
  const p = sources.pad;
  if (!pad) { if (p.connected) { Object.assign(p, { connected: false, buttons: 0, lx: 0, ly: 0, rx: 0, ry: 0, lt: 0, rt: 0 }); dirty = true; } return; }
  const b = pad.buttons, down = (i) => !!(b[i] && b[i].pressed);
  const map = [[0, PAD.A], [1, PAD.B], [2, PAD.X], [3, PAD.Y], [4, PAD.LSHOULDER], [5, PAD.RSHOULDER], [8, PAD.BACK], [9, PAD.START],
    [10, PAD.LTHUMB], [11, PAD.RTHUMB], [12, PAD.UP], [13, PAD.DOWN], [14, PAD.LEFT], [15, PAD.RIGHT]];
  let buttons = 0;
  for (const [i, bit] of map) if (down(i)) buttons |= bit;
  const axis = (i) => Math.round(Math.max(-1, Math.min(1, pad.axes[i] ?? 0)) * 32767);
  const next = { connected: true, buttons, lx: axis(0), ly: -axis(1), rx: axis(2), ry: -axis(3),
    lt: Math.round((b[6]?.value ?? 0) * 255), rt: Math.round((b[7]?.value ?? 0) * 255) };
  for (const key of Object.keys(next)) if (p[key] !== next[key]) { Object.assign(p, next); dirty = true; break; }
}

// ---- keyboard --------------------------------------------------------------------------------
function key(event, down) {
  const vk = VK[event.code];
  if (vk === undefined) return;
  if (event.metaKey || (event.ctrlKey && event.code !== 'ControlLeft' && event.code !== 'ControlRight')) return;
  event.preventDefault();
  const word = vk >> 5, bit = 1 << (vk & 31);
  const before = sources.keyboard[word];
  sources.keyboard[word] = down ? before | bit : before & ~bit;
  if (down) latch.keys[word] |= bit;
  if (before !== sources.keyboard[word] || down) dirty = true;
}
addEventListener('keydown', (e) => key(e, true));
addEventListener('keyup', (e) => key(e, false));
addEventListener('blur', () => { sources.keyboard.fill(0); dirty = true; worker?.postMessage({ type: 'focus', focused: false }); });
addEventListener('focus', () => worker?.postMessage({ type: 'focus', focused: true }));
document.addEventListener('visibilitychange', () => {
  worker?.postMessage({ type: 'focus', focused: !document.hidden });
  if (document.hidden) worker?.postMessage({ type: 'save' });          // flush saves when the tab is backgrounded
});
addEventListener('pagehide', () => worker?.postMessage({ type: 'save' }));
addEventListener('contextmenu', (e) => e.preventDefault());

// ---- sizing ----------------------------------------------------------------------------------
function sizes() {
  const dpr = Math.min(devicePixelRatio || 1, 3);
  const canvasWidth = Math.max(2, Math.round(innerWidth * dpr)), canvasHeight = Math.max(2, Math.round(innerHeight * dpr));
  let w = canvasWidth * renderScale, h = canvasHeight * renderScale;
  const long = Math.max(w, h);
  if (long > maxLong) { w *= maxLong / long; h *= maxLong / long; }
  const even = (v) => Math.max(2, Math.round(v / 2) * 2);
  return { canvasWidth, canvasHeight, width: even(w), height: even(h) };
}
let resizeTimer = 0;
addEventListener('resize', () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(() => worker?.postMessage({ type: 'resize', ...sizes() }), 120);
});

// ---- worker ----------------------------------------------------------------------------------
function setPhase(text, progress) {
  message.textContent = text;
  if (progress !== undefined) bar.style.width = `${Math.round(progress * 100)}%`;
}
function fail(text) {
  overlay.classList.remove('hidden');
  setPhase('The game stopped.', 0);
  errorBox.textContent = text;
}

export function startGame() {
  const offscreen = canvas.transferControlToOffscreen();
  worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
  window.__worker = worker;
  worker.onerror = (e) => fail(`worker error: ${e.message}`);
  worker.onmessage = (event) => {
    const m = event.data;
    if (m.type === 'phase') setPhase(m.text, m.progress);
    else if (m.type === 'ready') { overlay.classList.add('hidden'); window.__ready = true; sndButton.classList.add('available'); }
    else if (m.type === 'saves') window.__saves = m.files;
    else if (m.type === 'mode') { touch.setDriving(m.driving); window.__mode = m; }
    else if (m.type === 'error') { window.__error = m.message; fail(m.message); }
    else if (m.type === 'stats') {
      window.__stats = m;
      if (statsBox.style.display === 'block') {
        const s = m.status;
        statsBox.textContent = `${m.fps.toFixed(0)} fps  step ${m.stepMs.toFixed(1)} ms  render ${m.renderMs.toFixed(1)} ms  draws ${m.draws}\n` +
          `core ${m.frameSize.join('x')}  heap ${m.heapMB.toFixed(0)} MB  data cache ${m.files.cachedMB.toFixed(0)} MB\n` +
          `files: ${m.files.requests} req, ${(m.files.bytes / 1048576).toFixed(0)} MB, blocked ${m.files.blockedMs.toFixed(0)} ms\n` +
          `stage ${s.frontendStage} race ${s.racePhase} course ${s.course} car ${s.car}`;
      }
    }
  };
  audio.start(worker);                         // before the core starts, so the port is in place for the first sound
  const s = sizes();
  worker.postMessage({ type: 'start', canvas: offscreen, manifestUrl: new URL('manifest.json', dataBase).href, dataBase,
    width: s.width, height: s.height, canvasWidth: s.canvasWidth, canvasHeight: s.canvasHeight,
    preserveDrawingBuffer: params.has('preserve') }, [offscreen]);
  const loop = () => {
    pollGamepad();
    if (dirty) {
      dirty = false; mergeInput();
      worker.postMessage({ type: 'input', ...state, keys: state.keys.slice() });
      // The latched presses are now in the worker; send the true (possibly released) state next frame.
      if (latch.buttons || latch.keys.some(Boolean)) { latch.keys.fill(0); latch.buttons = 0; dirty = true; }
    }
    requestAnimationFrame(loop);
  };
  requestAnimationFrame(loop);
}

// Sound toggle (also M).
const sndButton = document.getElementById('snd');
const showSound = () => { sndButton.textContent = audio.muted ? 'Sound off' : 'Sound on'; };
sndButton.addEventListener('click', () => { audio.setMuted(!audio.muted); showSound(); });
addEventListener('keydown', (e) => { if (e.code === 'KeyM' && !e.repeat) { audio.setMuted(!audio.muted); showSound(); } });
showSound();
if (coarse || params.has('touch')) document.body.classList.add('touchui');

// Fullscreen where the browser allows it (not iPhone Safari: add the page to the home screen instead).
const fsButton = document.getElementById('fs');
const root = document.documentElement;
if (root.requestFullscreen && (coarse || params.has('touch'))) {
  fsButton.classList.add('available');
  fsButton.addEventListener('click', async () => {
    try { await root.requestFullscreen({ navigationUI: 'hide' }); await screen.orientation?.lock?.('landscape'); } catch { /* optional */ }
  });
  document.addEventListener('fullscreenchange', () => fsButton.classList.toggle('available', !document.fullscreenElement));
}

addEventListener('keydown', (e) => {
  if (e.code === 'Backquote' && e.shiftKey) statsBox.style.display = statsBox.style.display === 'block' ? 'none' : 'block';
});
if (params.has('stats')) statsBox.style.display = 'block';
// ---- GDS-0033 check ---------------------------------------------------------------------------
// The game starts only after the player's own disc files verify (see romgate.js). Nothing is
// remembered between visits and there is deliberately no option to skip it.
const gate = document.getElementById('gate');
const gateMessage = document.getElementById('gateMessage');
const gateBar = document.getElementById('gateBar');
const picker = document.getElementById('files');
let checking = false;
async function check(fileList) {
  if (checking || !fileList.length) return;
  checking = true;
  gateMessage.className = ''; gateMessage.textContent = 'Verifying… keep this page open.';
  gateBar.style.visibility = 'visible'; gateBar.firstElementChild.style.width = '0%';
  try {
    const result = await validateDisc([...fileList], (f) => { gateBar.firstElementChild.style.width = `${Math.round(f * 100)}%`; });
    if (result.verified) {
      gate.classList.add('hidden');
      overlay.classList.remove('hidden');
      startGame();
    } else {
      gateMessage.className = 'bad'; gateMessage.textContent = result.message; gateBar.style.visibility = 'hidden';
    }
  } catch (e) {
    gateMessage.className = 'bad'; gateMessage.textContent = 'Verification was interrupted.'; gateBar.style.visibility = 'hidden';
  }
  checking = false; picker.value = '';
}
document.getElementById('pick').addEventListener('click', () => picker.click());
picker.addEventListener('change', () => check(picker.files));
for (const type of ['dragenter', 'dragover']) gate.addEventListener(type, (e) => { e.preventDefault(); gate.classList.add('drag'); });
for (const type of ['dragleave', 'drop']) gate.addEventListener(type, (e) => { e.preventDefault(); gate.classList.remove('drag'); });
gate.addEventListener('drop', (e) => check(e.dataTransfer.files));
