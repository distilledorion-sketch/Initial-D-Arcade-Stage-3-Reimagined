"""Restore all 31 Legend dialogue still tables from the canonical image.

The original partial exporter stopped at 24 (the number of scene kinds),
but the character-indexed table contains 31 Legend rivals plus Bunta mode.
This exporter replaces only the three still-table declarations, retaining
all unrelated dialogue data. No guest execution or asset modification.
"""
import argparse
import hashlib
import re
import struct
from pathlib import Path

SHA = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image: Path, target: Path):
    blob = image.read_bytes()
    if hashlib.sha256(blob).hexdigest() != SHA:
        raise ValueError('Canonical original image required')
    def word(address):
        return struct.unpack_from('<I', blob, address - 0x0c020000)[0]
    records, tables, remaps = [], [], []
    for character in range(31):
        address = word(0x0c31a008 + character * 8)
        count = word(0x0c31a00c + character * 8)
        if not 0 < count <= 32:
            raise ValueError('Unexpected still table extent')
        tables.append((len(records), count))
        for index in range(count):
            records.append([word(address + index * 44 + offset * 4) for offset in range(11)])
            raw = word(word(0x0c31a6a8 + character * 4) + index * 4)
            remaps.append(raw if raw < 128 else -1)
    declarations = {
        'scrollRecords': 'inline constexpr std::array<OriginalScrollRecord,%d> scrollRecords{{%s}};' %
            (len(records), ','.join('{{' + ','.join('0x%08xu' % w for w in row) + '}}' for row in records)),
        'scrollTables': 'inline constexpr std::array<std::pair<std::uint16_t,std::uint16_t>,31> scrollTables{{%s}};' %
            ','.join('{%d,%d}' % row for row in tables),
        'portraitPartRemap': 'inline constexpr std::array<std::int8_t,%d> portraitPartRemap{%s};' %
            (len(remaps), ','.join(map(str, remaps))),
    }
    text = target.read_text(encoding='utf-8')
    for name, declaration in declarations.items():
        text, count = re.subn(r'^inline constexpr .*\b' + name + r'\{.*;$', lambda _: declaration,
                             text, flags=re.MULTILINE)
        if count != 1:
            raise ValueError('Expected one declaration: ' + name)
    target.write_text(text, encoding='utf-8', newline='\n')
    print('Recovered 31 rivals, %d original still records; SHA256 %s' % (len(records), SHA))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('image', type=Path)
    parser.add_argument('target', type=Path)
    args = parser.parse_args()
    export(args.image, args.target)
