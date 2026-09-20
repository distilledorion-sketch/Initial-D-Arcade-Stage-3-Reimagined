"""Repair note-on pitch registers the ARM capture recorded one note late.

The score harness reads a voice's parameter block during the key-on dispatch.
For a channel the driver programs through its periodic modulation update --
the vibrato channels, the only ones whose PLFOS depth is non-zero -- the pitch
word has not been rewritten yet at that moment, so the capture keeps the value
the previous note left behind. Every note on such a channel is then scored with
its predecessor's pitch, and the very first one is scored with an untouched
register (rate 1024). The melody plays shifted by one note.

The correct pitch for a note is exactly what the following note-on captured, so
the repair is a shift within the affected channel. Only the final note has no
successor; its pitch is re-encoded from its interval against the preceding
corrected value. A channel is only touched when the lagged reading explains its
intervals better than the direct one, and the result is re-checked afterwards.

Development tool. It rewrites generated .idms score data in place.
"""
import argparse, math, pathlib, shutil, struct, collections

RECORD = 112
KEY_ON = 1

def rate_of(pitch):
    octave = ((pitch >> 11 & 0xF) ^ 8) - 8
    base = 1024 | (pitch & 0x3FF)
    return base >> -octave if octave < 0 else base << octave

def encode(rate):
    octave = max(-8, min(7, int(math.floor(math.log2(rate / 1024.0)))))
    base = int(round(rate / (2.0 ** octave)))
    if base > 2047: octave += 1; base = int(round(rate / (2.0 ** octave)))
    fns = max(0, min(1023, base - 1024))
    return (((octave + 8) ^ 8) << 11) | fns

# The pitch word is a 10-bit mantissa and an octave, so an interval lands a
# little off its exact ratio. A fiftieth of a semitone is far below anything
# audible and still an order of magnitude tighter than a real wrong note.
TOLERANCE = 0.03

def agreement(rows):
    """How many intervals the direct and the one-late reading each explain."""
    direct = lagged = 0
    for i in range(1, len(rows)):
        step = 12 * math.log2(rows[i][1] / rows[i - 1][1])
        if abs(step - (rows[i][0] - rows[i - 1][0])) < TOLERANCE: direct += 1
        if i >= 2 and abs(step - (rows[i - 1][0] - rows[i - 2][0])) < TOLERANCE: lagged += 1
    return direct, lagged

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('scores', type=pathlib.Path, nargs='+')
    p.add_argument('--check', action='store_true', help='report without rewriting')
    a = p.parse_args()
    failed = False
    for path in a.scores:
        raw = bytearray(path.read_bytes())
        groups = collections.defaultdict(list)
        for at in range(32, len(raw), RECORD):
            kind, tick, command, channel, note, layer, sample, mask = struct.unpack_from('<8I', raw, at)
            if kind != KEY_ON: continue
            pitch = struct.unpack_from('<I', raw, at + 32 + 6 * 4)[0] & 0xFFFF
            groups[(channel, sample)].append((at, note, rate_of(pitch)))
        for (channel, sample), entries in sorted(groups.items()):
            if len(entries) < 8: continue
            rows = [(n, r) for _, n, r in entries]
            direct, lagged = agreement(rows)
            if lagged <= direct: continue
            print(f"{path.name} channel {channel}: {len(rows)} notes, "
                  f"direct {direct}, one-late {lagged} -> repairing")
            if a.check: failed = True; continue
            # Every note takes the pitch its successor recorded; the last is
            # re-encoded from its own interval against the corrected value.
            pitches = [struct.unpack_from('<I', raw, at + 32 + 6 * 4)[0] for at, _, _ in entries]
            fixed = pitches[1:]
            tail = rate_of(fixed[-1] & 0xFFFF) * 2.0 ** ((entries[-1][1] - entries[-2][1]) / 12.0)
            fixed.append((pitches[-1] & ~0xFFFF) | encode(tail))
            for (at, _, _), value in zip(entries, fixed):
                struct.pack_into('<I', raw, at + 32 + 6 * 4, value)
            after = [(n, rate_of(v & 0xFFFF)) for (_, n, _), v in zip(entries, fixed)]
            d2, l2 = agreement(after)
            print(f"    after: direct {d2}, one-late {l2} of {len(after) - 1}")
            intervals = len(after) - 1
            if d2 < intervals * 0.95 or d2 <= l2:
                raise SystemExit(f'repair did not resolve the lag: {d2} of {intervals}')
        if not a.check:
            backup = path.with_suffix(path.suffix + '.lagged')
            if not backup.exists(): shutil.copyfile(path, backup)
            path.write_bytes(bytes(raw))
    raise SystemExit(1 if failed else 0)

main()
