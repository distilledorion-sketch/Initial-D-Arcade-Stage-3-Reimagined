// WebGL2 renderer for the native scene frame (Idas3SceneFrame) and UI draw lists.
//
// It mirrors Assets/Scripts/Idas3SceneRenderer.cs + Idas3UnityUi.cs:
//   - ranges are drawn per view in source list order 0 (opaque/native), 4 (punch-through), 2
//     (translucent), each in submission order;
//   - blend, depth and cull state come from the PowerVR words packed on every range;
//   - static geometry has a stable 64-bit id and is kept on the GPU, dynamic geometry streams;
//   - the scene is rendered to an offscreen target (the core already sized it to the window,
//     scaled by the quality setting) and presented to the canvas.
//
// Frame objects are plain JS, built by abi.js from wasm memory or by the test loader from a dump:
//   SceneFrame { width, height, viewCount, fade, textureGeneration, cameras[2],
//                vertices:Float32Array(n*16), rangeCount, rangesU32, rangesF32, geometryIds|null,
//                textureCount, texture(i)->{width,height,pixels:Uint32Array}|null,
//                frameConstants:Float32Array(184), lightConstants:Uint32Array(1248),
//                fogConstants:Uint32Array(136), behind:boolean }
//   UiFrame    { width, height, drawCount, draws:Uint32Array+f32, vertices:ArrayBuffer view,
//                vertexCount, textureCount, texture(i)->{width,height,rgba:Uint8Array} }

import { SCENE_VS, SCENE_FS, UI_VS, UI_FS, BLIT_VS, BLIT_FS } from './shaders.js';

const SOURCE_LIST_ORDER = [0, 4, 2];
const RANGE_WORDS = 16;
const VERTEX_FLOATS = 16;
const VERTEX_BYTES = 64;
const SLAB_VERTICES = 1 << 17;      // 8 MiB of vertices per shared static-geometry buffer

export class Renderer {
  constructor(canvas, options = {}) {
    const gl = canvas.getContext('webgl2', {
      alpha: false, antialias: false, depth: false, stencil: false, desynchronized: false,
      powerPreference: 'high-performance', preserveDrawingBuffer: !!options.preserveDrawingBuffer,
    });
    if (!gl) throw new Error('WebGL2 is required');
    this.gl = gl;
    this.canvas = canvas;
    this.trackLighting = options.trackLighting ?? 1;
    this.maxAnisotropy = options.maxAnisotropy ?? 4;
    this.stats = { draws: 0, rangesDrawn: 0, uploadedVertices: 0, cacheHits: 0, uiDraws: 0 };

    // Reversed-Z needs a float depth buffer; fall back is not provided (all WebGL2 devices have it).
    gl.getExtension('EXT_color_buffer_float');
    this.aniso = gl.getExtension('EXT_texture_filter_anisotropic');
    this.anisoMax = this.aniso ? Math.min(this.maxAnisotropy, gl.getParameter(this.aniso.MAX_TEXTURE_MAX_ANISOTROPY_EXT)) : 1;

    this.scene = this.#program(SCENE_VS, SCENE_FS, [
      'uView', 'uPcw', 'uTsp', 'uGmp', 'uViewMask', 'uOriginal', 'uEmissive', 'uBillboard', 'uLightScope',
      'uCourseLightRange', 'uNativeSource', 'uFlatShade', 'uLightOverride', 'uGloss', 'uDepth', 'uTrackSurface',
      'uTrackLighting', 'uTex']);
    this.ui = this.#program(UI_VS, UI_FS, ['uCanvas', 'uClip', 'uTint', 'uOpacity', 'uTsp', 'uPcw', 'uOriginal', 'uSourceIsOne', 'uTex']);
    this.blit = this.#program(BLIT_VS, BLIT_FS, ['uTex']);
    for (const [block, binding] of [['FrameBlock', 0], ['LightBlock', 1], ['FogBlock', 2]]) {
      const index = gl.getUniformBlockIndex(this.scene.program, block);
      if (index !== gl.INVALID_INDEX) gl.uniformBlockBinding(this.scene.program, index, binding);
    }
    this.frameUbo = this.#ubo(0, 46 * 16);
    this.lightUbo = this.#ubo(1, 312 * 16);
    this.fogUbo = this.#ubo(2, 34 * 16);

    this.dynamicBuffer = gl.createBuffer();
    this.uiBuffer = gl.createBuffer();
    this.uiUploaded = null;          // the ui.vertices view whose bytes are in uiBuffer
    this.emptyVao = gl.createVertexArray();
    this.vao = gl.createVertexArray();
    this.white = this.#solidTexture([255, 255, 255, 255]);

    this.geometry = new Map();       // id (BigInt) -> { slab, first, lastUsed }
    this.slabs = [];                 // large shared vertex buffers: { buffer, capacity, used, live }
    this.slab = null;                // slab new static geometry is appended to
    this.attribBuffer = null;        // buffer the scene VAO's attribute pointers currently address
    this.boundTexture = null;
    this.boundSampler = null;
    this.textures = new Map();       // scene texture index -> WebGLTexture
    this.textureGeneration = -1;
    this.uiTextures = new Map();     // ui texture id -> WebGLTexture
    this.samplers = new Map();
    this.frameNumber = 0;
    this.scratch = new Float32Array(1 << 20);
    this.target = null;
    this.state = {};
  }

  // ---- GL plumbing -------------------------------------------------------------------------
  #compile(type, source) {
    const gl = this.gl;
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(shader);
      throw new Error(`${type === gl.VERTEX_SHADER ? 'vertex' : 'fragment'} shader failed:\n${log}`);
    }
    return shader;
  }
  #program(vs, fs, uniformNames) {
    const gl = this.gl;
    const program = gl.createProgram();
    gl.attachShader(program, this.#compile(gl.VERTEX_SHADER, vs));
    gl.attachShader(program, this.#compile(gl.FRAGMENT_SHADER, fs));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('program link failed:\n' + gl.getProgramInfoLog(program));
    const u = {};
    for (const name of uniformNames) u[name] = gl.getUniformLocation(program, name);
    return { program, u, last: Object.create(null) };
  }
  // Uniform values persist per program, so a write is skipped when the shadow copy already matches.
  #u1i(s, name, v) { if (s.last[name] !== v) { s.last[name] = v; this.gl.uniform1i(s.u[name], v); } }
  #u1ui(s, name, v) { if (s.last[name] !== v) { s.last[name] = v; this.gl.uniform1ui(s.u[name], v); } }
  #u1f(s, name, v) { if (s.last[name] !== v) { s.last[name] = v; this.gl.uniform1f(s.u[name], v); } }
  #u4f(s, name, a, b, c, d) {
    const l = s.last[name];
    if (l && l[0] === a && l[1] === b && l[2] === c && l[3] === d) return;
    s.last[name] = [a, b, c, d];
    this.gl.uniform4f(s.u[name], a, b, c, d);
  }
  #ubo(binding, bytes) {
    const gl = this.gl;
    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.UNIFORM_BUFFER, buffer);
    gl.bufferData(gl.UNIFORM_BUFFER, bytes, gl.DYNAMIC_DRAW);
    gl.bindBufferBase(gl.UNIFORM_BUFFER, binding, buffer);
    return buffer;
  }
  #solidTexture(rgba) {
    const gl = this.gl;
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(rgba));
    return texture;
  }

  /** (Re)create the offscreen target. The core renders at this size, so call before stepping it. */
  resize(width, height) {
    if (this.target && this.target.width === width && this.target.height === height) return;
    const gl = this.gl;
    if (this.target) {
      gl.deleteFramebuffer(this.target.fbo);
      gl.deleteTexture(this.target.color);
      gl.deleteRenderbuffer(this.target.depth);
    }
    const color = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, color);
    gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, width, height);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    const depth = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, depth);
    gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT32F, width, height);
    const fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, color, 0);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, depth);
    const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (status !== gl.FRAMEBUFFER_COMPLETE) throw new Error('render target incomplete: 0x' + status.toString(16));
    this.target = { width, height, color, depth, fbo };
  }

  // ---- cached GL state (WebGL calls are not free; skip no-ops) ------------------------------
  #set(key, value, apply) {
    if (this.state[key] === value) return;
    this.state[key] = value;
    apply();
  }
  #enable(cap, on) {
    const gl = this.gl;
    this.#set('cap' + cap, on, () => (on ? gl.enable(cap) : gl.disable(cap)));
  }

  // Source blend factor codes (tsp bits) -> GL, as Factor() in Idas3SceneRenderer.cs.
  #blendFactor(code, isSource, isAlpha) {
    const gl = this.gl;
    switch (code) {
      case 0: return gl.ZERO;
      case 1: return gl.ONE;
      case 2: return isSource ? (isAlpha ? gl.DST_ALPHA : gl.DST_COLOR) : (isAlpha ? gl.SRC_ALPHA : gl.SRC_COLOR);
      case 3: return isSource ? (isAlpha ? gl.ONE_MINUS_DST_ALPHA : gl.ONE_MINUS_DST_COLOR) : (isAlpha ? gl.ONE_MINUS_SRC_ALPHA : gl.ONE_MINUS_SRC_COLOR);
      case 4: return gl.SRC_ALPHA;
      case 5: return gl.ONE_MINUS_SRC_ALPHA;
      case 6: return gl.DST_ALPHA;
      default: return gl.ONE_MINUS_DST_ALPHA;
    }
  }

  // Source ISP depth-compare index -> GL function on a REVERSED depth buffer (near = 1). The
  // source/Unity table is [Never, Greater, Equal, GreaterEqual, Less, NotEqual, LessEqual, Always]
  // in "closer" terms (Less = closer); reversed depth flips every ordered comparison.
  #depthFunc(index) {
    const gl = this.gl;
    return [gl.NEVER, gl.LESS, gl.EQUAL, gl.LEQUAL, gl.GREATER, gl.NOTEQUAL, gl.GEQUAL, gl.ALWAYS][index & 7];
  }

  #sampler(sampler, filtered) {
    const key = sampler | (filtered ? 32 : 0);
    let s = this.samplers.get(key);
    if (s) return s;
    const gl = this.gl;
    s = gl.createSampler();
    const wrap = (clamp, mirror) => (clamp ? gl.CLAMP_TO_EDGE : mirror ? gl.MIRRORED_REPEAT : gl.REPEAT);
    gl.samplerParameteri(s, gl.TEXTURE_WRAP_S, wrap(sampler & 1, sampler & 4));
    gl.samplerParameteri(s, gl.TEXTURE_WRAP_T, wrap(sampler & 2, sampler & 8));
    gl.samplerParameteri(s, gl.TEXTURE_MIN_FILTER, filtered ? gl.LINEAR_MIPMAP_LINEAR : gl.NEAREST);
    gl.samplerParameteri(s, gl.TEXTURE_MAG_FILTER, filtered ? gl.LINEAR : gl.NEAREST);
    if (filtered && this.aniso) gl.samplerParameterf(s, this.aniso.TEXTURE_MAX_ANISOTROPY_EXT, this.anisoMax);
    this.samplers.set(key, s);
    return s;
  }

  #sceneTexture(frame, index) {
    let texture = this.textures.get(index);
    if (texture) return texture;
    const image = index < frame.textureCount ? frame.texture(index) : null;
    const gl = this.gl;
    if (!image || !image.width || !image.height) { this.textures.set(index, this.white); return this.white; }
    texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    // Source pixels are 0xAARRGGBB words. Little-endian memory order is B,G,R,A; the shader
    // reads .bgra, so the bytes go up untouched (no per-pixel swizzle on the CPU).
    const bytes = new Uint8Array(image.pixels.buffer, image.pixels.byteOffset, image.width * image.height * 4);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, image.width, image.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, bytes);
    gl.generateMipmap(gl.TEXTURE_2D);
    this.textures.set(index, texture);
    return texture;
  }

  // ---- scene -----------------------------------------------------------------------------
  /**
   * Render one frame into the offscreen target.
   * @param {object} frame SceneFrame (see header). @param {object|null} ui UiFrame.
   */
  render(frame, ui) {
    const gl = this.gl;
    this.stats = { draws: 0, rangesDrawn: 0, uploadedVertices: 0, cacheHits: 0, uiDraws: 0 };
    this.resize(frame.width, frame.height);
    if (this.textureGeneration !== frame.textureGeneration) {
      for (const t of this.textures.values()) if (t !== this.white) gl.deleteTexture(t);
      this.textures.clear();
      this.textureGeneration = frame.textureGeneration;
    }
    ++this.frameNumber;

    const words = frame.frameConstants.slice();            // may be adjusted for the main viewport
    const cameras = frame.cameras.map((c) => ({ ...c, viewport: c.viewport.slice() }));
    this.#expandMainViewport(cameras[0], frame.width, frame.height, words);
    gl.bindBuffer(gl.UNIFORM_BUFFER, this.frameUbo); gl.bufferSubData(gl.UNIFORM_BUFFER, 0, words);
    gl.bindBuffer(gl.UNIFORM_BUFFER, this.lightUbo); gl.bufferSubData(gl.UNIFORM_BUFFER, 0, frame.lightConstants);
    gl.bindBuffer(gl.UNIFORM_BUFFER, this.fogUbo); gl.bufferSubData(gl.UNIFORM_BUFFER, 0, frame.fogConstants);

    const sourceLighting = this.#sourceLightingReady(frame, words);
    const prepared = this.#prepareGeometry(frame);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.target.fbo);
    this.state = {};
    gl.viewport(0, 0, frame.width, frame.height);
    gl.disable(gl.SCISSOR_TEST);
    gl.depthMask(true);
    gl.clearColor(words[20], words[21], words[22], 1);
    gl.clearDepth(0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    if (ui) this.#drawUi(ui, frame, true);

    this.#drawView(frame, prepared, cameras[0], 0, words, sourceLighting, !!frame.behind);
    if (frame.viewCount > 1) this.#drawView(frame, prepared, cameras[1], 1, words, sourceLighting, false);

    if (ui) this.#drawUi(ui, frame, false);
    if ((frame.fade >>> 24) !== 0) this.#drawFade(frame);
    this.#evict();
  }

  // The fixed authored main viewport is widened on a larger display so it reveals more world
  // (ExpandMainViewport). A panel aperture (leftHanded bit 1) keeps its rectangle.
  #expandMainViewport(c, width, height, words) {
    const [vx, vy, vw, vh] = c.viewport;
    if (vx === 0 && vy === 0 && vw === width && vh === height) return;
    if (vw <= 0 || vh <= 0 || (c.leftHanded & 2)) return;
    const sx = vw / width, sy = vh / height;
    const tx = (2 * vx + vw) / width - 1, ty = 1 - (2 * vy + vh) / height;
    for (let row = 0; row < 4; ++row) {
      const i = row * 4;
      words[i] = sx * words[i] + tx * words[i + 3];
      words[i + 1] = sy * words[i + 1] + ty * words[i + 3];
    }
    c.fov = 2 * Math.atan(Math.tan(c.fov * 0.5) / sy);
    c.aspect *= sy / sx;
    c.viewport = [0, 0, width, height];
  }

  // Course lighting in both active views -> shared "native source" shader variants.
  #sourceLightingReady(frame, words) {
    const light = frame.lightConstants;
    if (words[43] !== 0 || light[156 + 152] === 0) return false;
    return frame.viewCount < 2 || (words[92 + 43] === 0 && light[5 * 156 + 152] !== 0);
  }

  // Split ranges into GPU-cached (stable id) and streamed; upload the streamed ones once.
  #newSlab(capacity) {
    const gl = this.gl;
    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, capacity * VERTEX_BYTES, gl.STATIC_DRAW);
    const slab = { buffer, capacity, used: 0, live: 0 };
    this.slabs.push(slab);
    return slab;
  }

  /** Cached geometry is appended to large shared buffers so a draw is just drawArrays(first, count). */
  #allocateStatic(count) {
    if (!this.slab || this.slab.used + count > this.slab.capacity) this.slab = this.#newSlab(Math.max(SLAB_VERTICES, count));
    const slab = this.slab, first = slab.used;
    slab.used += count; ++slab.live;
    return { slab, first };
  }

  #prepareGeometry(frame) {
    const gl = this.gl;
    const u32 = frame.rangesU32;
    const n = frame.rangeCount;
    const entries = new Array(n);
    let dynamicVertices = 0;
    for (let i = 0; i < n; ++i) {
      const o = i * RANGE_WORDS;
      const count = u32[o + 1];
      if (!count || !u32[o + 10]) { entries[i] = null; continue; }
      const id = frame.geometryIds ? frame.geometryIds[i] : 0n;
      if (id) {
        let cached = this.geometry.get(id);
        if (!cached) {
          const { slab, first } = this.#allocateStatic(count);
          gl.bindBuffer(gl.ARRAY_BUFFER, slab.buffer);
          gl.bufferSubData(gl.ARRAY_BUFFER, first * VERTEX_BYTES, frame.vertices.subarray(u32[o] * VERTEX_FLOATS, (u32[o] + count) * VERTEX_FLOATS));
          cached = { slab, first, lastUsed: 0 };
          this.geometry.set(id, cached);
          this.stats.uploadedVertices += count;
        } else ++this.stats.cacheHits;
        cached.lastUsed = this.frameNumber;
        entries[i] = { buffer: cached.slab.buffer, first: cached.first };
      } else {
        entries[i] = { buffer: this.dynamicBuffer, first: dynamicVertices, source: u32[o], count };
        dynamicVertices += count;
      }
    }
    if (dynamicVertices) {
      const floats = dynamicVertices * VERTEX_FLOATS;
      if (this.scratch.length < floats) this.scratch = new Float32Array(Math.max(floats, this.scratch.length * 2));
      for (const e of entries) {
        if (!e || e.buffer !== this.dynamicBuffer) continue;
        this.scratch.set(frame.vertices.subarray(e.source * VERTEX_FLOATS, (e.source + e.count) * VERTEX_FLOATS), e.first * VERTEX_FLOATS);
      }
      gl.bindBuffer(gl.ARRAY_BUFFER, this.dynamicBuffer);
      gl.bufferData(gl.ARRAY_BUFFER, this.scratch.subarray(0, floats), gl.STREAM_DRAW);
      this.stats.uploadedVertices += dynamicVertices;
    }
    return entries;
  }

  #evict() {
    if (this.frameNumber % 120) return;
    const gl = this.gl;
    for (const [id, g] of this.geometry) {
      if (this.frameNumber - g.lastUsed <= 300) continue;
      this.geometry.delete(id);
      const slab = g.slab;
      if (--slab.live > 0) continue;
      if (slab === this.slab) { slab.used = 0; continue; }          // empty and still being filled: reuse it
      gl.deleteBuffer(slab.buffer);
      this.slabs.splice(this.slabs.indexOf(slab), 1);
    }
  }

  #drawView(frame, prepared, camera, view, words, sourceLighting, depthOnlyClear) {
    const gl = this.gl;
    const W = frame.width, H = frame.height;
    const [vx, vy, vw, vh] = camera.viewport;
    const x = Math.round(vx), w = Math.max(1, Math.round(vw));
    const h = Math.max(1, Math.round(vh)), y = Math.round(H - (vy + vh));
    gl.viewport(x, y, w, h);
    gl.enable(gl.SCISSOR_TEST);
    gl.scissor(x, y, w, h);
    gl.depthMask(true);
    this.state.depthMask = true;
    if (depthOnlyClear) {
      gl.clearDepth(0);
      gl.clear(gl.DEPTH_BUFFER_BIT);
    } else {
      gl.clearColor(words[20], words[21], words[22], 1);
      gl.clearDepth(0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    }

    const s = this.scene;
    gl.useProgram(s.program);
    gl.uniform1i(s.u.uTex, 0);
    gl.uniform1i(s.u.uView, view);
    gl.uniform1f(s.u.uTrackLighting, this.trackLighting);
    // Reversed-Z coefficients from this camera's planes (double precision, as in the Unity host).
    const near = camera.near, far = camera.far;
    gl.uniform2f(s.u.uDepth, (near * far) / (far - near), near / (far - near));
    gl.bindVertexArray(this.vao);
    for (let a = 0; a < 5; ++a) gl.enableVertexAttribArray(a);
    gl.activeTexture(gl.TEXTURE0);
    this.attribBuffer = null;          // the UI passes re-point the shared VAO between views
    this.boundTexture = null;
    this.boundSampler = null;
    this.#enable(gl.DEPTH_TEST, true);
    this.#enable(gl.CULL_FACE, false);

    const u32 = frame.rangesU32, f32 = frame.rangesF32;
    const leftHanded = (camera.leftHanded & 1) !== 0;
    const filteredAlways = H > 480;
    const viewBit = 1 << view;
    for (const list of SOURCE_LIST_ORDER) {
      for (let i = 0; i < frame.rangeCount; ++i) {
        const entry = prepared[i];
        if (!entry) continue;
        const o = i * RANGE_WORDS;
        const flags = u32[o + 7], viewMask = u32[o + 10];
        if (!(viewMask & viewBit)) continue;
        const original = (flags & 1) !== 0;
        const pcw = u32[o + 4];
        const rangeList = original ? (pcw >>> 24) & 7 : 0;
        if (rangeList !== list) continue;
        const tsp = u32[o + 3], isp = u32[o + 5], gmp = u32[o + 6], scope = u32[o + 9];
        const billboard = (flags & 4) !== 0;

        const isCourse = scope === 1 || (flags & 16) !== 0;
        const cullMode = original && (isCourse || (flags & 32) !== 0) && !billboard ? (isp >>> 27) & 3 : 0;
        // Source triangle rejection used the world-space facing sign. Facing > 0 (CCW seen from the
        // eye) is rejected by mode 2, facing < 0 by mode 3. A left-handed camera mirrors the image,
        // which flips the screen winding of the same triangles.
        if (cullMode >= 2) {
          this.#enable(gl.CULL_FACE, true);
          const cullFront = (cullMode === 2) !== leftHanded;
          this.#set('cullFace', cullFront, () => gl.cullFace(cullFront ? gl.FRONT : gl.BACK));
        } else this.#enable(gl.CULL_FACE, false);

        // Blend
        const src = original ? tsp >>> 29 : 1, dst = original ? (tsp >>> 26) & 7 : 0;
        if (src === 1 && dst === 0) this.#enable(gl.BLEND, false);
        else {
          this.#enable(gl.BLEND, true);
          const sc = this.#blendFactor(src, true, false), dc = this.#blendFactor(dst, false, false);
          const sa = this.#blendFactor(src, true, true), da = this.#blendFactor(dst, false, true);
          this.#set('blend', `${sc},${dc},${sa},${da}`, () => gl.blendFuncSeparate(sc, dc, sa, da));
        }

        // Depth
        const compare = !original || rangeList === 2 ? 6 : isp >>> 29;
        const func = this.#depthFunc(compare);
        this.#set('depthFunc', func, () => gl.depthFunc(func));
        const write = original && rangeList === 2 ? false : (!original || rangeList === 4 || ((isp >>> 26) & 1) === 0);
        this.#set('depthMask', write, () => gl.depthMask(write));

        // Texture + sampler
        const sampler = original
          ? ((tsp >>> 16) & 1) | (((tsp >>> 15) & 1) << 1) | (((tsp >>> 18) & 1) << 2) | (((tsp >>> 17) & 1) << 3) | ((((tsp >>> 13) & 3) !== 0 ? 1 : 0) << 4)
          : 16;
        const filtered = (sampler & 16) !== 0 || filteredAlways;
        const samplerObject = this.#sampler(sampler & 15, filtered);
        if (samplerObject !== this.boundSampler) { gl.bindSampler(0, samplerObject); this.boundSampler = samplerObject; }

        // Per-range uniforms
        const noClip = sourceLighting && original && scope === 1 && (flags & 8) === 0 && viewMask === 3 && rangeList !== 4;
        const nativeSource = sourceLighting && original && scope === 1 && (flags & 8) === 0 ? (noClip ? 2 : 1) : 0;
        const g = u32[o + 8] & 255;
        const gloss = (1 + (g & 31) / 32) * Math.pow(2, (g >> 5) - 1);
        this.#u1ui(s, 'uPcw', pcw);
        this.#u1ui(s, 'uTsp', tsp);
        this.#u1ui(s, 'uGmp', gmp);
        this.#u1ui(s, 'uViewMask', viewMask);
        this.#u1i(s, 'uOriginal', flags & 1);
        this.#u1i(s, 'uEmissive', (flags >> 1) & 1);
        this.#u1i(s, 'uBillboard', billboard ? 1 : 0);
        this.#u1i(s, 'uLightScope', scope);
        this.#u1i(s, 'uCourseLightRange', scope !== 0 ? 1 : 0);
        this.#u1i(s, 'uNativeSource', nativeSource);
        this.#u1i(s, 'uFlatShade', original && (pcw & 2) === 0 ? 1 : 0);
        this.#u4f(s, 'uLightOverride', f32[o + 12], f32[o + 13], f32[o + 14], (flags & 8) !== 0 ? 1 : 0);
        this.#u1f(s, 'uGloss', gloss);
        const trackSurface = isCourse && rangeList !== 2 && (flags & 2) === 0 && (gmp & 512) === 0 ? 1 : 0;
        this.#u1f(s, 'uTrackSurface', trackSurface);

        // Texture: only the bind actually changes between most neighbouring ranges
        const texture = this.#sceneTexture(frame, u32[o + 2]);
        if (texture !== this.boundTexture) { gl.bindTexture(gl.TEXTURE_2D, texture); this.boundTexture = texture; }

        // Geometry: attribute pointers are set once per shared buffer; ranges are addressed with `first`
        if (entry.buffer !== this.attribBuffer) {
          gl.bindBuffer(gl.ARRAY_BUFFER, entry.buffer);
          gl.vertexAttribPointer(0, 3, gl.FLOAT, false, VERTEX_BYTES, 0);
          gl.vertexAttribPointer(1, 3, gl.FLOAT, false, VERTEX_BYTES, 12);
          gl.vertexAttribPointer(2, 4, gl.FLOAT, false, VERTEX_BYTES, 24);
          gl.vertexAttribPointer(3, 2, gl.FLOAT, false, VERTEX_BYTES, 40);
          gl.vertexAttribPointer(4, 4, gl.FLOAT, false, VERTEX_BYTES, 48);
          this.attribBuffer = entry.buffer;
        }
        gl.drawArrays(gl.TRIANGLES, entry.first, u32[o + 1]);
        ++this.stats.draws; ++this.stats.rangesDrawn;
      }
    }
    gl.disable(gl.SCISSOR_TEST);
    this.#enable(gl.CULL_FACE, false);
    this.#enable(gl.BLEND, false);
    gl.depthMask(true);
    this.state.depthMask = true;
  }

  // ---- UI --------------------------------------------------------------------------------
  #uiTexture(ui, id) {
    let texture = this.uiTextures.get(id);
    if (texture) return texture;
    const gl = this.gl;
    const image = ui.texture(id);
    texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, image.width, image.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, image.rgba);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    this.uiTextures.set(id, texture);
    return texture;
  }

  // background=true draws flag-2 batches (behind the 3D scene); false draws the rest.
  #drawUi(ui, frame, background) {
    const gl = this.gl;
    if (!ui.drawCount) return;
    const p = this.ui;
    gl.useProgram(p.program);
    gl.viewport(0, 0, frame.width, frame.height);
    gl.disable(gl.SCISSOR_TEST);
    gl.disable(gl.DEPTH_TEST);
    this.state.capDEPTH = false;
    gl.depthMask(false);
    this.state.depthMask = false;
    gl.disable(gl.CULL_FACE);
    this.state.capCULL = false;
    gl.enable(gl.BLEND);
    this.state.capBLEND = true;
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.uiBuffer);
    if (this.uiUploaded !== ui.vertices) {
      gl.bufferData(gl.ARRAY_BUFFER, ui.vertices, gl.STREAM_DRAW);
      this.uiUploaded = ui.vertices;
    }
    // Position/uv as floats; the two colours as normalised bytes in memory order B,G,R,A.
    gl.enableVertexAttribArray(0); gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 24, 0);
    gl.enableVertexAttribArray(1); gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, 24, 16);
    gl.enableVertexAttribArray(2); gl.vertexAttribPointer(2, 2, gl.FLOAT, false, 24, 8);
    gl.enableVertexAttribArray(3); gl.vertexAttribPointer(3, 4, gl.UNSIGNED_BYTE, true, 24, 20);
    gl.disableVertexAttribArray(4);
    gl.uniform2f(p.u.uCanvas, ui.width, ui.height);
    gl.uniform1i(p.u.uTex, 0);
    gl.uniform4f(p.u.uTint, 1, 1, 1, 1);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindSampler(0, null);

    const d = ui.drawsU32, df = ui.drawsF32;
    for (let i = 0; i < ui.drawCount; ++i) {
      const o = i * 12;
      const flags = d[o + 5];
      const isBackground = (flags & 2) !== 0;
      if (isBackground !== background) continue;
      const count = d[o + 1];
      if (!count) continue;
      const tsp = d[o + 3], pcw = d[o + 4];
      const original = (flags & 1) !== 0;
      const src = original ? tsp >>> 29 : 4;
      let dst = original ? (tsp >>> 26) & 7 : 5;
      if (flags & 4) dst = 1;
      const sf = this.#uiBlend(src, true), df_ = this.#uiBlend(dst, false);
      this.#set('uiBlend', `${sf},${df_}`, () => gl.blendFunc(sf, df_));
      gl.bindTexture(gl.TEXTURE_2D, this.#uiTexture(ui, d[o + 2]));
      gl.uniform1ui(p.u.uTsp, tsp);
      gl.uniform1ui(p.u.uPcw, pcw);
      gl.uniform1i(p.u.uOriginal, original ? 1 : 0);
      gl.uniform1i(p.u.uSourceIsOne, src === 1 ? 1 : 0);
      gl.uniform1f(p.u.uOpacity, df[o + 6]);
      gl.uniform4f(p.u.uClip, df[o + 8], df[o + 9], df[o + 10], df[o + 11]);
      gl.drawArrays(gl.TRIANGLES, d[o], count);
      ++this.stats.uiDraws;
    }
    this.state = {};
    gl.disable(gl.BLEND);
    gl.depthMask(true);
  }
  #uiBlend(factor, isSource) {
    const gl = this.gl;
    switch (factor) {
      case 0: return gl.ZERO;
      case 1: return gl.ONE;
      case 2: return isSource ? gl.DST_COLOR : gl.SRC_COLOR;
      case 3: return isSource ? gl.ONE_MINUS_DST_COLOR : gl.ONE_MINUS_SRC_COLOR;
      case 4: return gl.SRC_ALPHA;
      case 5: return gl.ONE_MINUS_SRC_ALPHA;
      case 6: return gl.DST_ALPHA;
      default: return gl.ONE_MINUS_DST_ALPHA;
    }
  }

  // Full-screen colour fade (screenFadeArgb), alpha-blended over everything.
  #drawFade(frame) {
    const gl = this.gl;
    const argb = frame.fade >>> 0;
    const p = this.ui;
    gl.useProgram(p.program);
    gl.viewport(0, 0, frame.width, frame.height);
    gl.disable(gl.DEPTH_TEST);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    const quad = new ArrayBuffer(6 * 24);
    const f = new Float32Array(quad), u = new Uint32Array(quad);
    const W = frame.width, H = frame.height;
    const corners = [[0, 0], [W, 0], [0, H], [W, 0], [W, H], [0, H]];
    corners.forEach(([x, y], i) => { f[i * 6] = x; f[i * 6 + 1] = y; u[i * 6 + 4] = 0xffffffff; u[i * 6 + 5] = 0; });
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.uiBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STREAM_DRAW);
    this.uiUploaded = null;                          // the shared buffer now holds the fade quad
    gl.enableVertexAttribArray(0); gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 24, 0);
    gl.enableVertexAttribArray(1); gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, 24, 16);
    gl.enableVertexAttribArray(2); gl.vertexAttribPointer(2, 2, gl.FLOAT, false, 24, 8);
    gl.enableVertexAttribArray(3); gl.vertexAttribPointer(3, 4, gl.UNSIGNED_BYTE, true, 24, 20);
    gl.uniform2f(p.u.uCanvas, W, H);
    gl.uniform4f(p.u.uClip, 0, 0, W, H);
    gl.uniform4f(p.u.uTint, ((argb >>> 16) & 255) / 255, ((argb >>> 8) & 255) / 255, (argb & 255) / 255, (argb >>> 24) / 255);
    gl.uniform1f(p.u.uOpacity, 1);
    gl.uniform1ui(p.u.uTsp, 1 << 20);
    gl.uniform1ui(p.u.uPcw, 0);
    gl.uniform1i(p.u.uOriginal, 1);
    gl.uniform1i(p.u.uSourceIsOne, 0);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.white);
    gl.bindSampler(0, null);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.disable(gl.BLEND);
    this.state = {};
  }

  // ---- present ---------------------------------------------------------------------------
  /** Draw the render target to the canvas, letterboxed to the source aspect. */
  present() {
    const gl = this.gl;
    if (!this.target) return;
    const cw = this.canvas.width, ch = this.canvas.height;
    const sourceAspect = this.target.width / this.target.height, outputAspect = cw / ch;
    let w = cw, h = ch;
    if (outputAspect > sourceAspect) w = Math.round(ch * sourceAspect); else h = Math.round(cw / sourceAspect);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.disable(gl.SCISSOR_TEST);
    gl.viewport(0, 0, cw, ch);
    gl.clearColor(0, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.viewport(Math.floor((cw - w) / 2), Math.floor((ch - h) / 2), w, h);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.BLEND);
    gl.useProgram(this.blit.program);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.target.color);
    gl.bindSampler(0, null);
    gl.uniform1i(this.blit.u.uTex, 0);
    gl.bindVertexArray(this.emptyVao);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    this.state = {};
  }
}
