"""Preserve the original A8 banks the dialogue and post-race scenes request.

The source BGM table at 31DF04 names one bank per cue: ids 3 to 33 are the 31
rivals' own themes, which is the enemy number plus three the next-rival step
asks for, and 34 to 38 are the after-race tracks. This copies each named bank
out of the game's sound pack unchanged, records its identity and the cue's
command and level, and writes a manifest.

This is preservation only. Playing these banks additionally needs their voice
events, which the three menu banks have as .idms files produced by running the
original ARM sound driver. That harness is not in this project and nothing here
synthesises it.
"""
import argparse, csv, hashlib, json, struct
from pathlib import Path


def inspect(raw, name):
    if raw[:4] != b'DTPK':
        raise ValueError(f'{name}: not a DTPK bank')
    size = struct.unpack_from('<I', raw, 8)[0]
    if size != len(raw):
        raise ValueError(f'{name}: DTPK size {size} does not match {len(raw)} bytes')
    # The header words are recorded as found; this tool does not interpret the
    # bank layout beyond its identity and size.
    header = list(struct.unpack_from('<16I', raw, 0x20))
    return dict(bytes=len(raw), header_0x20=[f'{word:08x}' for word in header],
                sha256=hashlib.sha256(raw).hexdigest())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hostfs', type=Path, required=True)
    p.add_argument('--project', type=Path, required=True)
    p.add_argument('--descriptors', type=Path, required=True,
                   help='the recovered 31DF04 table as id,filename,command,slot_mask,source_level')
    a = p.parse_args()
    destination = a.project / 'data/original_audio/music'
    destination.mkdir(parents=True, exist_ok=True)

    records = []
    with a.descriptors.open(newline='') as handle:
        for row in csv.DictReader(handle):
            cue = int(row['id'])
            stem = row['filename'].removesuffix('.bin')
            if cue < 3:
                continue   # TYPE, SELECT and RESULT are already preserved as selection banks.
            source = a.hostfs / 'sound/pack' / (stem + '.bin.nz')
            if not source.exists():
                raise FileNotFoundError(f'cue {cue} names {stem} but {source} is missing')
            raw = source.read_bytes()
            record = inspect(raw, stem)
            (destination / (stem + '.dtpk')).write_bytes(raw)
            record.update(cue=cue, name=stem,
                          source_command=int(row['command'], 16),
                          slot_mask=int(row['slot_mask'], 16),
                          source_level=int(row['source_level'], 16))
            records.append(record)

    manifest = dict(
        schema='idas3-original-dialogue-music-v1',
        source_table='31DF04 + 80*id, each record opening with its filename inline',
        banks=records,
        limitations='Banks only. Playing them needs the per-voice event capture the '
                    'three menu banks have as .idms; that harness is not in this project.')
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Preserved {len(records)} original A8 banks named by the source BGM table')


if __name__ == '__main__':
    main()
