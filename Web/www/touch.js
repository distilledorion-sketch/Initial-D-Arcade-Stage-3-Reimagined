// On-screen controls. Two layouts, switched by the game state the worker reports:
//   driving: a steering zone (drag sideways from where the thumb lands), brake and throttle pedals,
//            shift up/down, camera view, pause
//   menus:   D-pad, OK, Back, camera/Y
// Driving feeds the core as a virtual gamepad (left stick X, triggers, B/X for shifting) because
// that is the path the core's own controller handling expects; menus use the keyboard path.
import { PAD } from './abi.js';

const VK = { LEFT: 37, UP: 38, RIGHT: 39, DOWN: 40, ENTER: 13, ESC: 27, C: 67, H: 72 };

export function initTouch({ root, sources, latch, markDirty }) {
  const t = sources.touch;
  let visible = false, driving = false;
  root.innerHTML = `
    <div id="steerZone" class="zone drive"></div>
    <div id="steerRing" class="ring"><i></i></div>
    <div class="ctl drive pedal brake" data-ctl="brake"><span>BRAKE</span></div>
    <div class="ctl drive pedal gas" data-ctl="gas"><span>GAS</span></div>
    <div class="ctl drive shift up" data-ctl="up"><span>&#9650;</span></div>
    <div class="ctl drive shift down" data-ctl="down"><span>&#9660;</span></div>
    <div class="ctl drive small view" data-ctl="view"><span>VIEW</span></div>
    <div class="ctl drive small lights" data-ctl="lights"><span>LIGHTS</span></div>
    <div class="ctl small pause" data-ctl="pause"><span>&#10074;&#10074;</span></div>
    <div class="ctl menu dpad up" data-ctl="d-up"><span>&#9650;</span></div>
    <div class="ctl menu dpad down" data-ctl="d-down"><span>&#9660;</span></div>
    <div class="ctl menu dpad left" data-ctl="d-left"><span>&#9664;</span></div>
    <div class="ctl menu dpad right" data-ctl="d-right"><span>&#9654;</span></div>
    <div class="ctl menu round ok" data-ctl="ok"><span>OK</span></div>
    <div class="ctl menu round back" data-ctl="back"><span>BACK</span></div>
    <div class="ctl menu small view" data-ctl="m-view"><span>Y</span></div>`;

  const setKey = (vk, down) => {
    const w = vk >> 5, b = 1 << (vk & 31);
    t.keys[w] = down ? t.keys[w] | b : t.keys[w] & ~b;
    if (down) latch.keys[w] |= b;
  };
  const setButton = (bit, down) => {
    t.buttons = down ? t.buttons | bit : t.buttons & ~bit;
    if (down) latch.buttons |= bit;
  };
  // control -> how it affects the merged input
  const actions = {
    brake: (down) => { t.lt = down ? 255 : 0; },
    gas: (down) => { t.rt = down ? 255 : 0; },
    up: (down) => setButton(PAD.B, down),
    down: (down) => setButton(PAD.X, down),
    view: (down) => setKey(VK.C, down),
    'm-view': (down) => setKey(VK.C, down),
    lights: (down) => setKey(VK.H, down),
    pause: (down) => setKey(VK.ESC, down),
    'd-up': (down) => setKey(VK.UP, down), 'd-down': (down) => setKey(VK.DOWN, down),
    'd-left': (down) => setKey(VK.LEFT, down), 'd-right': (down) => setKey(VK.RIGHT, down),
    ok: (down) => setKey(VK.ENTER, down), back: (down) => setKey(VK.ESC, down),
  };

  for (const el of root.querySelectorAll('.ctl')) {
    const act = actions[el.dataset.ctl];
    const owners = new Set();
    const release = (e) => {
      if (!owners.delete(e.pointerId)) return;
      if (owners.size === 0) { act(false); el.classList.remove('held'); markDirty(); }
    };
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault(); el.setPointerCapture(e.pointerId);
      owners.add(e.pointerId); act(true); el.classList.add('held'); markDirty();
      if (navigator.vibrate) navigator.vibrate(8);
    });
    el.addEventListener('pointerup', release);
    el.addEventListener('pointercancel', release);
    el.addEventListener('lostpointercapture', release);
  }

  // Steering: the thumb's first contact is the neutral point; sideways travel is the stick.
  const zone = root.querySelector('#steerZone'), ring = root.querySelector('#steerRing');
  let pointer = null, originX = 0;
  const travel = () => Math.max(60, Math.min(innerWidth, innerHeight) * 0.16);   // px for full lock
  const steer = (x) => {
    const v = Math.max(-1, Math.min(1, (x - originX) / travel()));
    t.lx = Math.round(v * 32767);
    ring.firstElementChild.style.transform = `translateX(${v * travel()}px)`;
    markDirty();
  };
  zone.addEventListener('pointerdown', (e) => {
    if (pointer !== null) return;
    e.preventDefault(); zone.setPointerCapture(e.pointerId);
    pointer = e.pointerId; originX = e.clientX;
    ring.style.display = 'block'; ring.style.left = `${e.clientX}px`; ring.style.top = `${e.clientY}px`;
    steer(e.clientX);
  });
  zone.addEventListener('pointermove', (e) => { if (e.pointerId === pointer) steer(e.clientX); });
  const endSteer = (e) => {
    if (e.pointerId !== pointer) return;
    pointer = null; t.lx = 0; ring.style.display = 'none'; markDirty();
  };
  zone.addEventListener('pointerup', endSteer);
  zone.addEventListener('pointercancel', endSteer);
  zone.addEventListener('lostpointercapture', endSteer);

  function apply() {
    root.classList.toggle('on', visible);
    root.classList.toggle('mode-drive', driving);
    root.classList.toggle('mode-menu', !driving);
    // The virtual pad exists only while its layout is on screen; otherwise the keyboard path is the one in use.
    t.active = visible && driving;
    if (!t.active) { t.buttons = 0; t.lx = 0; t.lt = 0; t.rt = 0; }
    markDirty();
  }
  return {
    /** Show the controls (called on the first touch, or always on coarse-pointer devices). */
    show() { if (!visible) { visible = true; apply(); } },
    /** Switch layouts: driving = true while a race can be driven. */
    setDriving(value) { if (driving !== value) { driving = value; releaseAll(); apply(); } },
    get visible() { return visible; },
  };
  function releaseAll() {
    t.keys.fill(0); t.buttons = 0; t.lt = 0; t.rt = 0; t.lx = 0;
    for (const el of root.querySelectorAll('.held')) el.classList.remove('held');
    ring.style.display = 'none'; pointer = null;
  }
}
