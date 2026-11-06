// JS side of the native core's C ABI, for the WebAssembly (wasm32) build.
//
// The Unity host reads Idas3SceneFrame etc. with 64-bit pointer layouts (Native/src/
// unity_scene_capture.h, `#pragma pack(8)`). In wasm32 pointers are 4 bytes, so the structs that
// contain pointers are smaller; the offsets below are the wasm32 layout, checked against a real
// dump from the same build (Web/tests). Structs without pointers (input, status, UI) are identical
// to the 64-bit ABI.
//
// Everything returned here is a *view into wasm memory* that is only valid until the next call into
// the core (the core owns and reuses those buffers, and growing the heap detaches old ArrayBuffers).
// The renderer consumes a frame immediately, so nothing is copied unless it has to be.

export const ABI = {
  frame: {
    size: 232,
    off: {
      size: 0, version: 4, frameGeneration: 8, textureGeneration: 16, width: 24, height: 28,
      vertexCount: 32, rangeCount: 36, textureCount: 40, overlayCount: 44, viewCount: 48, screenFade: 52,
      vertices: 56, ranges: 60, textures: 64, overlays: 68, frameConstants: 72, lightConstants: 76,
      fogConstants: 80, cameras: 84,
    },
  },
  camera: { size: 72 },        // 17 floats (eye3,target3,up3,fov,aspect,near,far,viewport4) + u32 flags
  texture: { stride: 24, width: 0, height: 4, pixelCount: 8, pixels: 16 },  // u64 forces 8-byte alignment
  overlay: { stride: 20, surface: 0, width: 4, height: 8, flags: 12 },
  input: { size: 88 },
  status: { size: 80 },
  ui: { frameSize: 40, drawStride: 48, vertexStride: 24, textureInfoSize: 16 },
};

// Idas3UnityInput.flags
export const INPUT_FOCUSED = 1;
export const INPUT_CONTROLS_BLOCKED = 2;

// XInput button bits understood by the core's gamepad path.
export const PAD = {
  UP: 0x0001, DOWN: 0x0002, LEFT: 0x0004, RIGHT: 0x0008, START: 0x0010, BACK: 0x0020,
  LTHUMB: 0x0040, RTHUMB: 0x0080, LSHOULDER: 0x0100, RSHOULDER: 0x0200,
  A: 0x1000, B: 0x2000, X: 0x4000, Y: 0x8000,
};

/** A fresh, neutral input record. Mutate and pass to Core.step(). */
export function newInput() {
  return {
    focused: true, controlsBlocked: false, gear: 0, deltaSeconds: 1 / 60,
    keys: new Uint32Array(8),          // held Windows virtual-key bits 0..255
    padConnected: false, padButtons: 0, thumbLX: 0, thumbLY: 0, thumbRX: 0, thumbRY: 0,
    leftTrigger: 0, rightTrigger: 0,   // 0..255
    width: 0, height: 0,               // >0 asks the core to resize its render size
  };
}
export const setKey = (input, vk, down) => {
  if (down) input.keys[vk >> 5] |= 1 << (vk & 31); else input.keys[vk >> 5] &= ~(1 << (vk & 31));
};

export class Core {
  /**
   * @param {object} mod an instantiated Emscripten module (see Web/build/link_wasm.sh web)
   */
  constructor(mod) {
    this.mod = mod;
    this.frameBuf = mod._malloc(ABI.frame.size);
    this.inputBuf = mod._malloc(ABI.input.size);
    this.statusBuf = mod._malloc(ABI.status.size);
    this.uiFrameBuf = mod._malloc(ABI.ui.frameSize);
    this.infoBuf = mod._malloc(ABI.ui.textureInfoSize);
    this.errorBuf = mod._malloc(1024);
    this.uiDraws = { ptr: 0, bytes: 0 };
    this.uiVerts = { ptr: 0, bytes: 0 };
    this.uiTex = { ptr: 0, bytes: 0 };
    this.initialized = false;
  }

  // ---- helpers -----------------------------------------------------------------------------
  #grow(slot, bytes) {
    if (slot.bytes >= bytes) return slot.ptr;
    if (slot.ptr) this.mod._free(slot.ptr);
    slot.bytes = Math.max(bytes, 4096, slot.bytes * 2);
    slot.ptr = this.mod._malloc(slot.bytes);
    return slot.ptr;
  }
  #cstring(text) {
    const m = this.mod;
    const n = m.lengthBytesUTF8(text) + 1;
    const p = m._malloc(n);
    m.stringToUTF8(text, p, n);
    return p;
  }
  /** The last error text the core recorded (empty when none). */
  error() {
    const m = this.mod;
    m._Idas3UnityCopyError(this.errorBuf, 1024);
    return m.UTF8ToString(this.errorBuf);
  }
  #check(ok, what) {
    if (!ok) throw new Error(`${what}: ${this.error() || 'no error text'}`);
  }

  // ---- lifecycle ---------------------------------------------------------------------------
  /** assetRoot must contain data/ (in the wasm filesystem); saveRoot is where saves are written. */
  initialize(assetRoot, saveRoot, width, height) {
    const m = this.mod;
    const a = this.#cstring(assetRoot), s = this.#cstring(saveRoot);
    const ok = m._Idas3SceneInitialize(a, s, width | 0, height | 0);
    m._free(a); m._free(s);
    this.#check(ok, 'Idas3SceneInitialize');
    this.initialized = true;
  }
  shutdown() {
    if (this.initialized) this.mod._Idas3SceneShutdown();
    this.initialized = false;
  }

  /** Advance one fixed step with the given input record (see newInput). */
  step(input) {
    const m = this.mod;
    const dv = new DataView(m.HEAPU8.buffer);
    const p = this.inputBuf;
    dv.setUint32(p + 0, ABI.input.size, true);
    dv.setUint32(p + 4, (input.focused ? INPUT_FOCUSED : 0) | (input.controlsBlocked ? INPUT_CONTROLS_BLOCKED : 0)
      | ((input.gear & 7) << 8), true);
    dv.setFloat64(p + 8, input.deltaSeconds, true);
    for (let i = 0; i < 8; ++i) dv.setUint32(p + 16 + i * 4, input.keys[i] >>> 0, true);
    dv.setUint32(p + 48, input.padButtons >>> 0, true);
    dv.setInt32(p + 52, input.thumbLX | 0, true);
    dv.setInt32(p + 56, input.thumbLY | 0, true);
    dv.setInt32(p + 60, input.thumbRX | 0, true);
    dv.setInt32(p + 64, input.thumbRY | 0, true);
    dv.setUint32(p + 68, input.leftTrigger >>> 0, true);
    dv.setUint32(p + 72, input.rightTrigger >>> 0, true);
    dv.setUint32(p + 76, input.padConnected ? 1 : 0, true);
    dv.setInt32(p + 80, input.width | 0, true);
    dv.setInt32(p + 84, input.height | 0, true);
    this.#check(m._Idas3SceneStep(p), 'Idas3SceneStep');
  }

  /** Idas3UnityStatus as a plain object. */
  status() {
    const m = this.mod;
    const dv = new DataView(m.HEAPU8.buffer);
    const p = this.statusBuf;
    dv.setUint32(p, ABI.status.size, true);
    this.#check(m._Idas3UnityGetStatus(p), 'Idas3UnityGetStatus');
    return {
      state: dv.getUint32(p + 4, true),
      renderedFrames: Number(dv.getBigUint64(p + 8, true)),
      simulationTicks: Number(dv.getBigUint64(p + 16, true)),
      textureGeneration: Number(dv.getBigUint64(p + 24, true)),
      width: dv.getInt32(p + 32, true), height: dv.getInt32(p + 36, true),
      frontendStage: dv.getInt32(p + 40, true), attractChild: dv.getInt32(p + 44, true),
      course: dv.getInt32(p + 48, true), car: dv.getInt32(p + 52, true), racePhase: dv.getInt32(p + 56, true),
      flags: dv.getUint32(p + 60, true),
      speed: dv.getFloat32(p + 64, true), rpm: dv.getFloat32(p + 68, true),
      lastEventId: dv.getUint32(p + 72, true),
    };
  }

  // ---- audio -------------------------------------------------------------------------------
  /** Let the core's mixer fill its PCM ring (Idas3UnityReadAudio drains it). */
  enableAudio() {
    const m = this.mod;
    this.audioBuf = m._malloc(8192 * 2 * 4);
    this.audioStats = m._malloc(64);
    m._Idas3UnitySetAudioRunning(1);
    this.audioProduced = this.#producedFrames();
  }
  #producedFrames() {
    const m = this.mod;
    new DataView(m.HEAPU8.buffer).setUint32(this.audioStats, 64, true);
    m._Idas3UnityGetAudioStatistics(this.audioStats);
    return new DataView(m.HEAPU8.buffer).getBigUint64(this.audioStats + 8, true);
  }
  /**
   * The PCM the last step produced, as a fresh interleaved stereo Float32Array at 44.1 kHz, or null.
   * The ring needs ~1024 frames queued before it releases audio, so the first steps return null.
   */
  readAudio() {
    const produced = this.#producedFrames();
    let n = Number(produced - this.audioProduced);
    this.audioProduced = produced;
    if (n <= 0) return null;
    n = Math.min(n, 8192);
    const count = this.mod._Idas3UnityReadAudio(this.audioBuf, n);
    return count > 0 ? new Float32Array(this.mod.HEAPU8.buffer, this.audioBuf, count * 2).slice() : null;
  }

  // ---- scene frame -------------------------------------------------------------------------
  /** The current Idas3SceneFrame as the object Renderer.render() expects. */
  frame() {
    const m = this.mod;
    const f = ABI.frame.off;
    new DataView(m.HEAPU8.buffer).setUint32(this.frameBuf, ABI.frame.size, true);
    this.#check(m._Idas3SceneGetFrame(this.frameBuf), 'Idas3SceneGetFrame');

    const buffer = m.HEAPU8.buffer;                    // fetched after the call: the heap may have grown
    const dv = new DataView(buffer);
    const p = this.frameBuf;
    const u32 = (o) => dv.getUint32(p + o, true);
    const rangeCount = u32(f.rangeCount), vertexCount = u32(f.vertexCount);
    const generation = dv.getBigUint64(p + f.frameGeneration, true);

    const cameras = [];
    for (let i = 0; i < 2; ++i) {
      const c = p + f.cameras + i * ABI.camera.size;
      const fl = (k) => dv.getFloat32(c + k * 4, true);
      cameras.push({
        eye: [fl(0), fl(1), fl(2)], target: [fl(3), fl(4), fl(5)], up: [fl(6), fl(7), fl(8)],
        fov: fl(9), aspect: fl(10), near: fl(11), far: fl(12),
        viewport: [fl(13), fl(14), fl(15), fl(16)], leftHanded: dv.getUint32(c + 68, true),
      });
    }

    const rangesPtr = u32(f.ranges);
    let geometryIds = null;
    if (rangeCount) {
      const ids = m._Idas3SceneGetGeometryIds(generation, rangeCount);
      if (ids) geometryIds = new BigUint64Array(buffer, ids, rangeCount);
    }

    let behind = false;
    const overlays = u32(f.overlays), overlayCount = u32(f.overlayCount);
    for (let i = 0; i < overlayCount; ++i) {
      behind ||= (dv.getUint32(overlays + i * ABI.overlay.stride + ABI.overlay.flags, true) & 1) !== 0;
    }

    const textures = u32(f.textures), textureCount = u32(f.textureCount);
    const T = ABI.texture;
    return {
      width: u32(f.width), height: u32(f.height), viewCount: u32(f.viewCount),
      fade: u32(f.screenFade), textureGeneration: Number(dv.getBigUint64(p + f.textureGeneration, true)),
      frameGeneration: Number(generation),
      cameras, behind,
      vertexCount, vertices: new Float32Array(buffer, u32(f.vertices), vertexCount * 16),
      rangeCount, rangesU32: new Uint32Array(buffer, rangesPtr, rangeCount * 16),
      rangesF32: new Float32Array(buffer, rangesPtr, rangeCount * 16),
      geometryIds,
      textureCount,
      texture: (i) => {
        if (i >= textureCount) return null;
        const t = textures + i * T.stride;
        const width = dv.getUint32(t + T.width, true), height = dv.getUint32(t + T.height, true);
        const ptr = dv.getUint32(t + T.pixels, true);
        if (!ptr || !width || !height) return null;
        return { width, height, pixels: new Uint32Array(buffer, ptr, width * height) };
      },
      frameConstants: new Float32Array(buffer, u32(f.frameConstants), 184),
      lightConstants: new Uint32Array(buffer, u32(f.lightConstants), 1248),
      fogConstants: new Uint32Array(buffer, u32(f.fogConstants), 136),
    };
  }

  // ---- UI draw lists -----------------------------------------------------------------------
  /** The captured UI as {draws, vertices} the renderer expects, or null when none is available. */
  ui() {
    const m = this.mod;
    const U = ABI.ui;
    new DataView(m.HEAPU8.buffer).setUint32(this.uiFrameBuf, U.frameSize, true);
    if (!m._Idas3UiGetFrame(this.uiFrameBuf)) return null;
    const dv = new DataView(m.HEAPU8.buffer);
    const p = this.uiFrameBuf;
    const width = dv.getUint32(p + 4, true), height = dv.getUint32(p + 8, true);
    const drawCount = dv.getUint32(p + 12, true), vertexCount = dv.getUint32(p + 16, true);
    const textureCount = dv.getUint32(p + 20, true);
    if (!width || !height || drawCount > 100000 || vertexCount > 1000000) throw new Error('invalid native UI frame bounds');

    const drawsPtr = this.#grow(this.uiDraws, drawCount * U.drawStride);
    const vertsPtr = this.#grow(this.uiVerts, vertexCount * U.vertexStride);
    if (drawCount && m._Idas3UiCopyDraws(drawsPtr, drawCount) !== drawCount) throw new Error('UI draw transfer failed');
    if (vertexCount && m._Idas3UiCopyVertices(vertsPtr, vertexCount) !== vertexCount) throw new Error('UI vertex transfer failed');

    const buffer = m.HEAPU8.buffer;
    return {
      width, height, drawCount, vertexCount, textureCount,
      revision: Number(dv.getBigUint64(p + 32, true)),
      drawsU32: new Uint32Array(buffer, drawsPtr, drawCount * 12),
      drawsF32: new Float32Array(buffer, drawsPtr, drawCount * 12),
      vertices: new Uint8Array(buffer, vertsPtr, vertexCount * U.vertexStride),
      // Source textures are append-only and never change once published; the renderer caches by id.
      texture: (id) => {
        const info = this.infoBuf;
        const d0 = new DataView(m.HEAPU8.buffer);
        d0.setUint32(info, U.textureInfoSize, true);
        if (!m._Idas3UiGetTextureInfo(id, info)) throw new Error('invalid UI texture ' + id);
        const d1 = new DataView(m.HEAPU8.buffer);
        const w = d1.getUint32(info + 4, true), h = d1.getUint32(info + 8, true), bytes = d1.getUint32(info + 12, true);
        if (!w || !h || w > 2048 || h > 2048 || bytes !== w * h * 4) throw new Error('invalid UI source texture ' + id);
        const ptr = this.#grow(this.uiTex, bytes);
        if (m._Idas3UiCopyTextureRGBA(id, ptr, bytes) !== bytes) throw new Error('UI texture transfer failed');
        return { width: w, height: h, rgba: new Uint8Array(m.HEAPU8.buffer, ptr, bytes) };
      },
    };
  }
}
