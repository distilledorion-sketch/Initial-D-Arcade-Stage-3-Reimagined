// Parsers for the raw dumps written by headless_scene.cpp ("g<name>" -> .frame, "u<name>" -> .ui),
// turning them into the same SceneFrame / UiFrame objects abi.js builds from live wasm memory, so the
// renderer can be exercised and regression-tested without running the core.
//
// A .frame dump embeds Idas3SceneFrame as compiled: 256 bytes from a 64-bit build, 232 from wasm32.
// The embedded pointers are meaningless; counts, cameras and the sections that follow are not.

const STRUCTS = {
  256: { cameras: 112 },   // 64-bit: seven 8-byte pointers
  232: { cameras: 84 },    // wasm32: seven 4-byte pointers
};

export function parseFrameDump(buffer) {
  const dv = new DataView(buffer);
  if (dv.getUint32(0, true) !== 0x31464449) throw new Error('not an IDF1 frame dump');
  const size = dv.getUint32(4, true);
  const layout = STRUCTS[size];
  if (!layout) throw new Error('unknown Idas3SceneFrame size ' + size);
  const h = 4;                                    // struct starts after the magic
  const u32 = (o) => dv.getUint32(h + o, true);
  const width = u32(24), height = u32(28), vertexCount = u32(32), rangeCount = u32(36);
  const textureCount = u32(40), overlayCount = u32(44), viewCount = u32(48);
  let p = h + size;

  const cameras = [];
  for (let i = 0; i < 2; ++i) {
    const c = h + layout.cameras + i * 72;
    const f = (k) => dv.getFloat32(c + k * 4, true);
    cameras.push({
      eye: [f(0), f(1), f(2)], target: [f(3), f(4), f(5)], up: [f(6), f(7), f(8)],
      fov: f(9), aspect: f(10), near: f(11), far: f(12), viewport: [f(13), f(14), f(15), f(16)],
      leftHanded: dv.getUint32(c + 68, true),
    });
  }
  const take = (Type, count) => {
    // dump sections are not aligned; copy into a fresh aligned buffer
    const bytes = count * Type.BYTES_PER_ELEMENT;
    const out = new Type(buffer.slice(p, p + bytes));
    p += bytes;
    return out;
  };
  const vertices = take(Float32Array, vertexCount * 16);
  const ranges = take(Uint32Array, rangeCount * 16);
  const textures = [];
  for (let i = 0; i < textureCount; ++i) {
    const w = dv.getUint32(p, true), hh = dv.getUint32(p + 4, true); p += 8;
    textures.push({ width: w, height: hh, pixels: take(Uint32Array, w * hh) });
  }
  let behind = false;
  for (let i = 0; i < overlayCount; ++i) {
    const w = dv.getUint32(p, true), hh = dv.getUint32(p + 4, true), flags = dv.getUint32(p + 8, true);
    p += 12 + w * hh * 4;
    behind ||= (flags & 1) !== 0;
  }
  const frameConstants = take(Float32Array, 184);
  const lightConstants = take(Uint32Array, 1248);
  const fogConstants = take(Uint32Array, 136);
  if (p !== buffer.byteLength) throw new Error(`dump has ${buffer.byteLength - p} unread bytes`);
  return {
    width, height, viewCount, fade: u32(52), textureGeneration: Number(dv.getBigUint64(h + 16, true)),
    cameras, behind, vertexCount, vertices, rangeCount,
    rangesU32: ranges, rangesF32: new Float32Array(ranges.buffer),
    geometryIds: null,                              // not in dumps: every range streams
    textureCount, texture: (i) => textures[i] ?? null,
    frameConstants, lightConstants, fogConstants,
  };
}

export function parseUiDump(buffer) {
  const dv = new DataView(buffer);
  if (dv.getUint32(0, true) !== 0x31554449) throw new Error('not an IDU1 UI dump');
  const h = 4;
  const width = dv.getUint32(h + 4, true), height = dv.getUint32(h + 8, true);
  const drawCount = dv.getUint32(h + 12, true), vertexCount = dv.getUint32(h + 16, true);
  const textureCount = dv.getUint32(h + 20, true);
  let p = h + 40;
  const draws = buffer.slice(p, p + drawCount * 48); p += drawCount * 48;
  const vertices = new Uint8Array(buffer.slice(p, p + vertexCount * 24)); p += vertexCount * 24;
  const textures = [];
  for (let i = 0; i < textureCount; ++i) {
    const w = dv.getUint32(p, true), hh = dv.getUint32(p + 4, true), bytes = dv.getUint32(p + 8, true); p += 12;
    textures.push({ width: w, height: hh, rgba: new Uint8Array(buffer.slice(p, p + bytes)) });
    p += bytes;
  }
  if (p !== buffer.byteLength) throw new Error(`UI dump has ${buffer.byteLength - p} unread bytes`);
  return {
    width, height, drawCount, vertexCount, textureCount,
    drawsU32: new Uint32Array(draws), drawsF32: new Float32Array(draws),
    vertices, texture: (id) => textures[id],
  };
}
