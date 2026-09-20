"""Store a music bank's samples as uncompressed 16-bit PCM.

Roughly half the instruments in the sequenced scores are 4-bit Yamaha ADPCM or
8-bit PCM. The bank loader already expands every one of them to 16-bit at load,
so this moves the expansion off the critical path rather than changing what the
voice plays: the check below re-decodes the rewritten bank and requires every
sample, both lanes of a stereo pair included, to come back identical.

Only the sample table's location and length words move, plus the total-size
word in the DTPK header, which the loader checks against the file. Everything
else ahead of the first sample byte -- header, score, layers and the table --
is copied verbatim, and layers address samples by index, so nothing needs
rebasing. Loop points are in sample units and are unaffected.

Development tool. It rewrites generated bank data in place; the originals are
byte-identical to the game's own sound/pack files and recoverable from there.
"""
import argparse, pathlib, struct

SCALE = (230, 230, 230, 230, 307, 409, 512, 614)

def clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v

def frames_of(encoding, length):
    return length // 2 if encoding == 0 else length if encoding == 1 else length * 2

def decode(data, encoding, offset, frames):
    if encoding == 0:
        return list(struct.unpack_from(f'<{frames}h', data, offset))
    if encoding == 1:
        return [struct.unpack_from('<b', data, offset + k)[0] * 256 for k in range(frames)]
    out = [0] * frames
    previous, quantizer = 0, 127
    for k in range(frames):
        nibble = (data[offset + k // 2] >> ((k & 1) * 4)) & 15
        index = nibble & 7
        delta = min(32767, (quantizer * (index * 2 + 1)) >> 3)
        previous = clamp(previous + (-delta if nibble & 8 else delta), -32768, 32767)
        quantizer = clamp((quantizer * SCALE[index]) >> 8, 127, 24576)
        out[k] = previous
    return out

def samples_of(data):
    table = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<I', data, table)[0] + 1
    entries = []
    for i in range(count):
        at = table + 4 + 16 * i
        location, packed, channels, length = struct.unpack_from('<4I', data, at)
        entries.append(dict(at=at, location=location, channels=channels, length=length,
                            offset=location & 0x7FFFFF, encoding=(location >> 23) & 3,
                            flags=location & 0x7F800000, stereo=channels == 0x80))
    return table, count, entries

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('banks', type=pathlib.Path, nargs='+')
    p.add_argument('--check', action='store_true', help='report without rewriting')
    a = p.parse_args()
    pending = 0
    for path in a.banks:
        data = path.read_bytes()
        table, count, entries = samples_of(data)
        compressed = [e for e in entries if e['encoding'] != 0]
        if not compressed:
            print(f"{path.name:<18} all {count} samples already uncompressed"); continue
        pending += 1
        if a.check:
            print(f"{path.name:<18} {len(compressed)} of {count} samples compressed"); continue
        before = {}
        for e in entries:
            n = frames_of(e['encoding'], e['length'])
            before[e['at']] = [decode(data, e['encoding'], e['offset'], n)]
            if e['stereo']:
                before[e['at']].append(decode(data, e['encoding'], e['offset'] + e['length'], n))
        first = min(e['offset'] for e in entries)
        out = bytearray(data[:first])
        for e in sorted(entries, key=lambda x: x['offset']):
            while len(out) % 4: out.append(0)
            lanes = before[e['at']]
            e['newOffset'], e['newLength'] = len(out), len(lanes[0]) * 2
            for lane in lanes:
                out += struct.pack(f'<{len(lane)}h', *lane)
            if e['newOffset'] & ~0x7FFFFF: raise SystemExit(f'{path.name}: expanded bank exceeds the 23-bit sample offset')
            struct.pack_into('<I', out, e['at'], e['newOffset'] | (e['flags'] & ~(3 << 23)))
            struct.pack_into('<I', out, e['at'] + 12, e['newLength'])
        # The DTPK header carries the total file size and the loader requires
        # it to match, so it moves with the expansion.
        struct.pack_into('<I', out, 8, len(out))
        rebuilt = bytes(out)
        if struct.unpack_from('<I', rebuilt, 8)[0] != len(rebuilt):
            raise SystemExit(f'{path.name}: header size does not match the file')
        _, recount, reentries = samples_of(rebuilt)
        if recount != count: raise SystemExit(f'{path.name}: sample count changed')
        for e, r in zip(entries, reentries):
            if r['encoding'] != 0: raise SystemExit(f'{path.name}: sample still compressed')
            if r['offset'] < table + 4 + 16 * count: raise SystemExit(f'{path.name}: sample overlaps the table')
            n = frames_of(0, r['length'])
            lanes = [decode(rebuilt, 0, r['offset'], n)]
            if r['stereo']: lanes.append(decode(rebuilt, 0, r['offset'] + r['length'], n))
            if lanes != before[e['at']]: raise SystemExit(f'{path.name}: sample {e["at"]} decodes differently')
        path.write_bytes(rebuilt)
        print(f"{path.name:<18} {len(compressed)}/{count} expanded  "
              f"{len(data)/1e6:.2f}MB -> {len(rebuilt)/1e6:.2f}MB  all samples verified identical")
    raise SystemExit(1 if (a.check and pending) else 0)

main()
