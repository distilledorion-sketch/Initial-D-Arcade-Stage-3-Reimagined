"""Store the streamed songs as uncompressed 16-bit PCM instead of 4-bit ADPCM.

Every stream ships as Yamaha 4-bit ADPCM (SPSD codec 3). The player already
expands each one to 16-bit at load, so this changes what sits on disk, not what
the mixer receives: the Yamaha expansion is deterministic, and the check below
requires the rewritten file to decode to the identical sample sequence. It
costs four times the disk and removes the decode from load.

The header is copied verbatim apart from the four fields the codec decides:
the codec byte, the sample-byte count, and the loop word, whose stored value
carries a codec-dependent bias (16384 frames for ADPCM, 4096 for PCM16) that
has to be rebiased so the loop point lands on the same frame.

Development tool. It rewrites generated stream data in place; the originals are
byte-identical to the game's own sound/stream files and recoverable from there.
"""
import argparse, pathlib, struct

SCALES = (230, 230, 230, 230, 307, 409, 512, 614)
HEADER = 64
BIAS = {0: 4096, 1: 8192, 3: 16384}

def clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v

def parse(data):
    if len(data) < HEADER or data[:4] != b'SPSD': raise ValueError('not an SPSD stream')
    u16 = lambda a: data[a] | (data[a + 1] << 8)
    u32 = lambda a: u16(a) | (u16(a + 2) << 16)
    codec, flags, index, size = data[8], data[9], u16(10), u32(12)
    channels = 2 if flags & 3 else 1
    per = size // channels
    frames = per * 2 if codec == 3 else per // 2 if codec == 0 else per
    return dict(codec=codec, channels=channels, index=index, size=size, per=per,
                frames=frames, rate=u16(42), looping=bool(flags & 128), loopWord=u32(44))

def decode(data, info):
    """The player's own expansion, per channel, planar or 8192-byte blocked."""
    codec, channels, per, index = info['codec'], info['channels'], info['per'], info['index']
    def byte(channel, at):
        if index == 13:
            group, within = divmod(at, 8192)
            block = min(8192, per - group * 8192)
            off = group * 8192 * channels + channel * block + within
        else:
            off = channel * per + at
        return data[HEADER + off]
    out = [0] * (info['frames'] * channels)
    for channel in range(channels):
        history, step = 0, 127
        for frame in range(info['frames']):
            if codec == 0:
                word = byte(channel, frame * 2) | (byte(channel, frame * 2 + 1) << 8)
                sample = word - 65536 if word >= 32768 else word
            elif codec == 1:
                raw = byte(channel, frame)
                sample = (raw - 256 if raw >= 128 else raw) * 256
            else:
                nibble = (byte(channel, frame // 2) >> ((frame & 1) * 4)) & 15
                delta = min(32767, ((nibble & 7) * 2 + 1) * step // 8)
                history = clamp(history * 254 // 256 + (-delta if nibble & 8 else delta), -32768, 32767)
                step = clamp(step * SCALES[nibble & 7] // 256, 127, 24576)
                sample = history
            out[frame * channels + channel] = sample
    return out

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('streams', type=pathlib.Path, nargs='+')
    p.add_argument('--check', action='store_true', help='report without rewriting')
    a = p.parse_args()
    pending = 0
    for path in a.streams:
        data = path.read_bytes()
        info = parse(data)
        if info['codec'] == 0:
            print(f"{path.name:<32} already uncompressed"); continue
        pending += 1
        if a.check:
            print(f"{path.name:<32} codec {info['codec']} -> would expand to PCM16"); continue
        samples = decode(data, info)
        frames, channels = info['frames'], info['channels']
        body = bytearray(frames * 2 * channels)
        # Planar, matching the index 255 layout these streams already declare.
        for channel in range(channels):
            base = channel * frames * 2
            for frame in range(frames):
                struct.pack_into('<h', body, base + frame * 2, samples[frame * channels + channel])
        out = bytearray(data[:HEADER]) + body
        out[8] = 0
        struct.pack_into('<I', out, 12, frames * 2 * channels)
        struct.pack_into('<H', out, 10, 255)
        struct.pack_into('<I', out, 44, info['loopWord'] + BIAS[info['codec']] - BIAS[0])
        after = parse(bytes(out))
        if after['frames'] != frames or after['channels'] != channels or after['rate'] != info['rate']:
            raise SystemExit(f'{path.name}: rewritten geometry does not match')
        if after['loopWord'] + BIAS[0] != info['loopWord'] + BIAS[info['codec']]:
            raise SystemExit(f'{path.name}: loop point moved')
        if decode(bytes(out), after) != samples:
            raise SystemExit(f'{path.name}: rewritten stream does not decode identically')
        path.write_bytes(bytes(out))
        print(f"{path.name:<32} codec {info['codec']} -> 0  {len(data)/1e6:.1f}MB -> {len(out)/1e6:.1f}MB  "
              f"{frames} frames verified identical")
    raise SystemExit(1 if (a.check and pending) else 0)

main()
