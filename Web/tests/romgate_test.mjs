// node Web/tests/romgate_test.mjs   (Node >= 20)
// Verifies the SHA-256 against Node's crypto and every accept/reject path of the disc check using a
// synthetic spec. A real GDS-0033 dump is not available here, so acceptance of the real disc is the
// one thing this cannot show.
import { createHash, randomBytes } from 'node:crypto';
import assert from 'node:assert/strict';
import { Sha256, matchesFile, cueMatches, validateDisc, GDS0033 } from '../www/romgate.js';

const sha = (b) => createHash('sha256').update(b).digest('hex');

// 1. Streaming SHA-256 equals Node's for awkward sizes and chunkings (block boundaries, padding edge cases).
for (const size of [0, 1, 3, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 1000, 65536, 1_000_003]) {
  const data = randomBytes(size);
  for (const chunk of [1, 7, 63, 64, 65, 4096, size || 1]) {
    const h = new Sha256();
    for (let i = 0; i < data.length; i += chunk) h.update(data.subarray(i, i + chunk));
    assert.equal(h.hex(), sha(data), `size ${size} chunk ${chunk}`);
  }
}
console.log('sha256: ok');

// 2. Cue normalisation matches the C# rules.
const goodCue = GDS0033.cueLayout.replace(/\n/g, '\r\n');
assert.ok(cueMatches(goodCue));
assert.ok(cueMatches('REM made by x\n  file   "gds-0033-track1.bin"  binary \n' + GDS0033.cueLayout.split('\n').slice(1).join('\n').replace('TRACK 01', 'track  01')));
assert.ok(!cueMatches(GDS0033.cueLayout.replace('PREGAP 00:02:00', 'PREGAP 00:03:00')));
assert.ok(!cueMatches(GDS0033.cueLayout.replace('TRACK3', 'TRACK4')));
console.log('cue: ok');

// 3. Disc validation against a synthetic spec with the same structure.
const mk = (n) => randomBytes(n);
const chdData = mk(5000), t1 = mk(100), t2 = mk(200), t3 = mk(300_000);
const spec = {
  chd: { name: 'gds-0033.chd', bytes: chdData.length, sha256: sha(chdData) },
  cue: GDS0033.cue, cueLayout: GDS0033.cueLayout,
  tracks: [t1, t2, t3].map((d, i) => ({ name: `gds-0033-track${i + 1}.bin`, bytes: d.length, sha256: sha(d) })),
};
const F = (name, data) => new File([data], name);
const files = {
  chd: F('GDS-0033.CHD', chdData), cue: F('gds-0033.cue', GDS0033.cueLayout),
  t1: F('gds-0033-track1.bin', t1), t2: F('gds-0033-track2.bin', t2), t3: F('gds-0033-track3.bin', t3),
};
const run = (list, s = spec) => validateDisc(list, undefined, s);

let r = await run([files.chd]);
assert.deepEqual([r.verified, r.format], [true, 'CHD']);                       // CHD alone, name case-insensitive
r = await run([files.cue, files.t1, files.t2, files.t3]);
assert.deepEqual([r.verified, r.format], [true, 'CUE/BIN']);                   // CUE/BIN set
r = await run([F('gds-0033.chd', mk(5000))]);
assert.equal(r.verified, false); assert.match(r.message, /does not match/);    // same size, wrong bytes
r = await run([F('gds-0033.chd', mk(4999))]);
assert.equal(r.verified, false);                                               // wrong size
r = await run([F('other-name.chd', chdData)]);
assert.equal(r.verified, false); assert.match(r.message, /Choose/);            // a filename alone is not enough, nor is a renamed file
r = await run([F('gds-0033.chd', mk(5000)), files.cue, files.t1, files.t2, files.t3]);
assert.equal(r.verified, true);                                                // bad CHD, but a complete valid CUE/BIN set still counts
r = await run([files.cue, files.t1, files.t2]);
assert.equal(r.verified, false); assert.match(r.message, /Missing gds-0033-track3/);
r = await run([files.cue, files.t1, files.t2, F('gds-0033-track3.bin', mk(300_000))]);
assert.equal(r.verified, false); assert.match(r.message, /track3.*does not match/);
r = await run([F('gds-0033.cue', GDS0033.cueLayout.replace('MODE1/2352', 'MODE2/2352')), files.t1, files.t2, files.t3]);
assert.equal(r.verified, false); assert.match(r.message, /cue does not describe/);
r = await run([F('gds-0033.cue', 'x'.repeat(20000)), files.t1, files.t2, files.t3]);
assert.equal(r.verified, false);                                               // oversize cue rejected
// Extra bytes appended / truncated track
r = await run([files.cue, files.t1, files.t2, F('gds-0033-track3.bin', Buffer.concat([t3, Buffer.from([0])]))]);
assert.equal(r.verified, false);
console.log('validateDisc (synthetic spec): ok');

// 4. With the real spec, nothing synthetic can pass.
for (const list of [[files.chd], [files.cue, files.t1, files.t2, files.t3], [F('gds-0033.chd', mk(236940011 % 1000))]]) {
  assert.equal((await validateDisc(list)).verified, false);
}
// Progress is monotonic and ends at 1 for a good file.
const seen = []; await matchesFile(files.t3, t3.length, sha(t3), (p) => seen.push(p));
assert.ok(seen.every((v, i) => i === 0 || v >= seen[i - 1]) && seen.at(-1) === 1);
console.log('real spec rejects synthetic data; progress ok');
console.log('ALL OK');
