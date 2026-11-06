// GDS-0033 startup check, ported from Assets/Scripts/Idas3RomValidation.cs.
//
// Same rules as the desktop game: only the original disc's bytes unlock startup. The file's name is
// never enough; its exact size and SHA-256 must match (the CHD, or all three CUE/BIN tracks plus a
// CUE sheet describing exactly the required layout). Nothing is remembered between launches. The
// files are read locally in the browser and are never uploaded anywhere.
//
// Like the desktop gate this is a check inside the client the player runs, not a technical barrier.

export const GDS0033 = {
  chd: { name: 'gds-0033.chd', bytes: 236940011, sha256: '9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e' },
  cue: { name: 'gds-0033.cue', maxBytes: 16384 },
  tracks: [
    { name: 'gds-0033-track1.bin', bytes: 1058400, sha256: 'e96f586ec0f6c47533b4856761894d0a7538ba3d50f88c6262083d04309127d1' },
    { name: 'gds-0033-track2.bin', bytes: 5049744, sha256: '8e047c4648d21506cf545a60ced9a62774d63ea8f076d1cae38a2088106779bc' },
    { name: 'gds-0033-track3.bin', bytes: 1185760800, sha256: '0ca6f50ed433d92ea365222e0759c84952c84f35b8d43d6244717882ed0526d1' },
  ],
  cueLayout: 'FILE "GDS-0033-TRACK1.BIN" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n' +
    'FILE "GDS-0033-TRACK2.BIN" BINARY\nTRACK 02 AUDIO\nPREGAP 00:02:00\nINDEX 01 00:00:00\n' +
    'FILE "GDS-0033-TRACK3.BIN" BINARY\nTRACK 03 MODE1/2352\nINDEX 01 00:00:00\n',
};

// ---- streaming SHA-256 --------------------------------------------------------------------------
const K = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
  0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
  0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
  0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
  0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]);

export class Sha256 {
  constructor() {
    this.h = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
    this.w = new Uint32Array(64);
    this.block = new Uint8Array(64);
    this.filled = 0;
    this.length = 0;            // bytes hashed so far (a Number: exact below 2^53)
  }
  #compress(bytes, offset) {
    const w = this.w, h = this.h;
    for (let i = 0; i < 16; ++i) {
      const o = offset + i * 4;
      w[i] = (bytes[o] << 24) | (bytes[o + 1] << 16) | (bytes[o + 2] << 8) | bytes[o + 3];
    }
    for (let i = 16; i < 64; ++i) {
      const a = w[i - 15], b = w[i - 2];
      const s0 = ((a >>> 7) | (a << 25)) ^ ((a >>> 18) | (a << 14)) ^ (a >>> 3);
      const s1 = ((b >>> 17) | (b << 15)) ^ ((b >>> 19) | (b << 13)) ^ (b >>> 10);
      w[i] = (w[i - 16] + s0 + w[i - 7] + s1) | 0;
    }
    let a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (let i = 0; i < 64; ++i) {
      const S1 = ((e >>> 6) | (e << 26)) ^ ((e >>> 11) | (e << 21)) ^ ((e >>> 25) | (e << 7));
      const ch = (e & f) ^ (~e & g);
      const t1 = (hh + S1 + ch + K[i] + w[i]) | 0;
      const S0 = ((a >>> 2) | (a << 30)) ^ ((a >>> 13) | (a << 19)) ^ ((a >>> 22) | (a << 10));
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) | 0;
      hh = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  update(bytes) {
    this.length += bytes.length;
    let i = 0;
    if (this.filled) {
      const take = Math.min(64 - this.filled, bytes.length);
      this.block.set(bytes.subarray(0, take), this.filled);
      this.filled += take; i = take;
      if (this.filled < 64) return this;
      this.#compress(this.block, 0); this.filled = 0;
    }
    for (; i + 64 <= bytes.length; i += 64) this.#compress(bytes, i);
    if (i < bytes.length) { this.block.set(bytes.subarray(i)); this.filled = bytes.length - i; }
    return this;
  }
  hex() {
    const bits = this.length * 8;
    const pad = new Uint8Array(((this.filled < 56 ? 56 : 120) - this.filled) + 8);
    pad[0] = 0x80;
    const view = new DataView(pad.buffer);
    view.setUint32(pad.length - 8, Math.floor(bits / 4294967296));
    view.setUint32(pad.length - 4, bits >>> 0);
    const saved = this.length;
    this.update(pad);
    this.length = saved;
    return Array.from(this.h, (v) => (v >>> 0).toString(16).padStart(8, '0')).join('');
  }
}

/** True when `file` (a Blob/File) has exactly `bytes` bytes and SHA-256 `sha256`. */
export async function matchesFile(file, bytes, sha256, onProgress, signal) {
  if (file.size !== bytes) return false;
  const sha = new Sha256();
  const reader = file.stream().getReader();
  let total = 0;
  for (;;) {
    if (signal?.aborted) throw new DOMException('cancelled', 'AbortError');
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
    if (total > bytes) { await reader.cancel(); return false; }
    sha.update(value);
    onProgress?.(total / bytes);
  }
  return total === bytes && sha.hex() === sha256.toLowerCase();
}

/** Same normalisation as ValidCue: trim, collapse whitespace, upper-case, drop blank and REM lines. */
export function cueMatches(text, expected = GDS0033.cueLayout) {
  const layout = [];
  for (const raw of text.split(/\r\n|\r|\n/)) {
    const line = raw.trim().replace(/\s+/g, ' ').toUpperCase();
    if (line.length === 0 || line.startsWith('REM ')) continue;
    layout.push(line);
  }
  return layout.map((l) => l + '\n').join('') === expected;
}

/**
 * Validate the player's selection.
 * @param {File[]} selection files the player picked (any names; matched case-insensitively)
 * @param {(fraction:number)=>void} [onProgress]
 * @param {object} [spec] defaults to GDS0033 (a different spec exists only for testing this code)
 * @returns {Promise<{verified:boolean, message:string, format?:string}>}
 */
export async function validateDisc(selection, onProgress, spec = GDS0033, signal) {
  const byName = new Map();
  for (const f of selection) byName.set(f.name.toLowerCase(), f);
  const chd = byName.get(spec.chd.name);
  let unreadableChd = false;
  if (chd) {
    try {
      if (await matchesFile(chd, spec.chd.bytes, spec.chd.sha256, onProgress, signal)) return { verified: true, message: 'GDS-0033 verified.', format: 'CHD' };
    } catch (e) {
      if (e?.name === 'AbortError') throw e;
      unreadableChd = true;                     // the complete CUE/BIN dump is an independent valid input
    }
  }
  const cue = byName.get(spec.cue.name);
  if (!cue) {
    return { verified: false, message: chd ? (unreadableChd ? 'Could not read gds-0033.chd.' : 'gds-0033.chd does not match the required GDS-0033 disc.') : 'Choose gds-0033.chd.' };
  }
  try {
    if (cue.size > spec.cue.maxBytes || !cueMatches(await cue.text(), spec.cueLayout)) {
      return { verified: false, message: 'gds-0033.cue does not describe the required three-track disc.' };
    }
    const total = spec.tracks.reduce((n, t) => n + t.bytes, 0);
    let completed = 0;
    for (const track of spec.tracks) {
      const file = byName.get(track.name);
      if (!file) return { verified: false, message: `Missing ${track.name}.` };
      const prior = completed;
      if (!(await matchesFile(file, track.bytes, track.sha256, (v) => onProgress?.((prior + v * track.bytes) / total), signal))) {
        return { verified: false, message: `${track.name} does not match the required GDS-0033 disc.` };
      }
      completed += track.bytes;
    }
    return { verified: true, message: 'GDS-0033 verified.', format: 'CUE/BIN' };
  } catch (e) {
    if (e?.name === 'AbortError') throw e;
    return { verified: false, message: 'Could not read GDS-0033. Check that the files are accessible.' };
  }
}
