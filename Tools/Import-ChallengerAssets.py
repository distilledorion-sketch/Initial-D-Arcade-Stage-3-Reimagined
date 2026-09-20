"""Exact original interrupt02 textures and persistent notification sound bank.

The source uses negative V; the Unity RawImages flip V at runtime. No painting,
resizing or replacement artwork is performed here. PACK20 voice parameters are
captured separately by Native/tools/import_original_oneshot.py (bank20).
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys, zlib

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / 'Native/tools'))
from extract_original_models import read_payload
from texture_bank import decode, write_png

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hostfs', type=Path, default=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/driveA/HOSTFS'))
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
source = args.hostfs / 'model/interrupt'
table = (source / 'interrupt02_tex.tbl').read_bytes()
raw = read_payload(source / 'interrupt02_tex.bin.nz')
assert len(table) == 80 and len(raw) == 204800
dest = PROJECT / 'Assets/Resources/Challenger'
dest.mkdir(parents=True, exist_ok=True)
records = []
for i in range(5):
    w, h, fmt, flags, reserved, offset, tail = struct.unpack_from('<HHBBHII', table, i * 16)
    pixels = decode(raw[offset:offset + w*h*2], w, h, fmt, 'twiddled')
    path = dest / f'interrupt_{i}.png'
    if args.check:
        png = path.read_bytes(); at = 8; compressed = bytearray()
        assert png[:8] == b'\x89PNG\r\n\x1a\n'
        while at < len(png):
            count = struct.unpack_from('>I', png, at)[0]
            if png[at+4:at+8] == b'IDAT': compressed.extend(png[at+8:at+8+count])
            at += count + 12
        rows = zlib.decompress(compressed)
        assert rows == b''.join(b'\0' + pixels[y*w*4:(y+1)*w*4] for y in range(h)), path
    else:
        write_png(path, w, h, pixels)
    records.append(dict(file=path.name, index=i, width=w, height=h, rgbaSha256=hashlib.sha256(pixels).hexdigest(), fileSha256=hashlib.sha256(path.read_bytes()).hexdigest()))
sound = (args.hostfs / 'sound/pack/PACK20.bin.nz').read_bytes()
target = PROJECT / 'Native/data/original_audio/race/PACK20.dtpk'
assert sound[:4] == b'DTPK'
if args.check:
    assert target.read_bytes() == sound
else:
    target.write_bytes(sound)
    (dest / 'provenance.json').write_text(json.dumps(dict(source=str(source), tableSha256=hashlib.sha256(table).hexdigest(), textures=records,
        soundSource=str(args.hostfs / 'sound/pack/PACK20.bin.nz'), soundSha256=hashlib.sha256(sound).hexdigest(),
        cueEvidence='Original interrupt owner0C16CCC4 calls0C141F40(3,1). Cue table0C31EB5C contains0x000300A9: PACK20 cue3.',
        coordinateTransform='Negative original V is flipped by Unity RawImage UV rect.'), indent=2) + '\n')
print('PASS: five exact original textures and unchanged PACK20 bank' if args.check else 'Imported original challenger assets')
