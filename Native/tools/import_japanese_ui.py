"""Map exported Japan Rev C UI banks onto the remake's stable English IDs.

Input is an extracted UI directory, not a ROM. No executable, physics, course,
car, sound or key material is copied. English texture indices remain valid;
localized text uses appended textures and retains the existing chunk IDs.
"""
import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path
from extract_original_models import write_binary


def read_mesh(path):
    data = path.read_bytes()
    at = 8
    def take(fmt):
        nonlocal at
        value = struct.unpack_from(fmt, data, at)
        at += struct.calcsize(fmt)
        return value
    assert data[:8] == b'IDAS3M1\0'
    version, count = take('<2I')
    assert version == 1
    chunks = []
    for _ in range(count):
        index, offset, size, batches = take('<4I')
        header = take('<24I')
        result = []
        for _ in range(batches):
            start, nv, ni, nraw = take('<4I')
            words, material = take('<8I'), take('<16I')
            vertices = [take('<I8f2I') for _ in range(nv)]
            indices = take('<' + str(ni) + 'I')
            raw = data[at:at+nraw]
            at += nraw
            result.append(dict(source_offset=start, words=words, material=material,
                               vertices=vertices, indices=indices, source_vertex_bytes=raw))
        chunks.append(dict(index=index, source_offset=offset, source_size=size,
                           header=header, batches=result))
    assert at == len(data)
    return chunks


def read_textures(path):
    data = path.read_bytes()
    assert data[:8] == b'IDAS3T1\0'
    version, count = struct.unpack_from('<2I', data, 8)
    assert version == 1
    at, images = 16, []
    for index in range(count):
        i, w, h, size = struct.unpack_from('<4I', data, at)
        at += 16
        assert i == index and size == w*h*4
        images.append((w, h, data[at:at+size]))
        at += size
    assert at == len(data)
    return images


def write_textures(path, images):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('wb') as out:
        out.write(b'IDAS3T1\0' + struct.pack('<2I', 1, len(images)))
        for i, (w, h, rgba) in enumerate(images):
            out.write(struct.pack('<4I', i, w, h, len(rgba)))
            out.write(rgba)


def ranges(*items):
    # Inclusive destination range and source start, explicitly reviewed by screen.
    return {i: c+i-a for a, b, c in items for i in range(a, b+1)}


MAPPINGS = {
    'game2d': ranges((9, 9, 8), (91, 91, 90), (96, 96, 95),
                     (145, 145, 142), (147, 181, 144)),
    'v3sS00common': {21:21, 51:50},
    'v3sK00common': ranges((31, 39, 30), (42, 42, 40)),
    'v3sA00etc': {2:2, 3:3, 4:4, 6:7, 7:8},
    'v3sS05cars': {**ranges((1, 35, 1)), 37:37, 43:42},
    'v3sK02rival': ranges((1, 22, 1), (24, 24, 24), (41, 41, 50),
                         (47, 47, 56), (75, 107, 84), (109, 113, 118)),
    'adv2d_v3': {34:34, 38:38},
    'start2d': ranges((71, 80, 70), (83, 97, 82), (104, 116, 103), (118, 118, 117)),
    'v3sT13rankin': ranges((8, 16, 7), (32, 38, 31), (42, 43, 40),
                          (46, 47, 44), (49, 50, 47)),
    'select0402': {0:3, 1:4},
    'lecture': ranges((0, 1, 0), (22, 22, 22), (30, 36, 30)),
    'rival_scene': {19:18},
    'gasstand': {10:7, 11:8, 12:9},
}
# Same-ID text banks; deliberately exclude portraits and the keyboard grids,
# whose Japanese layout does not match the existing input controller.
IDENTITY = set('''adv_newtitle adv_title v3sB01course v3sK01course v3sS04maker
v3sS06mission v3sS07tune v3sS11mode v3sT02route v3sT03weather v3sT04time
mode_bunta v3sS08name game2d_intr game2d_ta game2d_vs result v3sT14password
continue v3sK17continue v3sK12shop select adv_caution loadcmn loadvs loadwait ending'''.split())


def bounds(chunk):
    points = [v for b in chunk['batches'] for v in b['vertices']]
    if not points:
        return None
    return [(min(v[k] for v in points), max(v[k] for v in points)) for k in (1, 2)]


def anchor(chunk, original):
    """Keep the existing caller's anchor and fit text without distorting its aspect."""
    src, dst = bounds(chunk), bounds(original)
    if not src or not dst:
        return
    scale = min([1.0] + [(b-a)/(d-c) for (a,b),(c,d) in zip(dst,src) if d-c > 1e-6])
    delta = [(a+b)/2 - scale*(c+d)/2 for (a,b),(c,d) in zip(dst,src)]
    for batch in chunk['batches']:
        raw = bytearray(batch['source_vertex_bytes'])
        stride = len(raw)//len(batch['vertices']) if batch['vertices'] else 0
        for i, value in enumerate(batch['vertices']):
            v = list(value)
            for k in (0,1):
                v[k+1] = struct.unpack('<f', struct.pack('<f', scale*v[k+1]+delta[k]))[0]
            batch['vertices'][i] = tuple(v)
            struct.pack_into('<3f', raw, i*stride+4, *v[1:4])
        batch['source_vertex_bytes'] = bytes(raw)


def import_ui(english, japanese, output):
    audit = []
    for path in sorted(english.rglob('*.idasmesh')):
        relative = path.relative_to(english)
        source = japanese/relative
        if not source.is_file() or path.stem not in IDENTITY | MAPPINGS.keys():
            continue
        original, localized = read_mesh(path), read_mesh(source)
        mapping = MAPPINGS.get(path.stem)
        if mapping is None:
            assert len(original) == len(localized), relative
            mapping = dict(enumerate(range(len(original))))
        textures = read_textures(path.parent/'textures/textures.idastex')
        japanese_textures = read_textures(source.parent/'textures/textures.idastex')
        base_count = len(textures)
        lookup = {image: i for i, image in enumerate(textures)}
        mapped = copy.deepcopy(original)
        changes = []
        for en, ja in sorted(mapping.items()):
            chunk = copy.deepcopy(localized[ja])
            # Skip unchanged artwork, even if source offsets differ by region.
            if len(original[en]['batches']) == len(chunk['batches']) and all(
                a['vertices'] == b['vertices'] and a['indices'] == b['indices'] and
                ((a['material'][9] == b['material'][9] == 0xffffffff) or
                 (a['material'][9] != 0xffffffff and b['material'][9] != 0xffffffff and
                  textures[a['material'][9]] == japanese_textures[b['material'][9]]))
                for a,b in zip(original[en]['batches'],chunk['batches'])):
                continue
            chunk['index'] = en
            anchor(chunk, original[en])
            for batch in chunk['batches']:
                material = list(batch['material'])
                if material[9] != 0xffffffff:
                    image = japanese_textures[material[9]]
                    if image not in lookup:
                        lookup[image] = len(textures)
                        textures.append(image)
                    material[9] = lookup[image]
                batch['material'] = tuple(material)
            mapped[en] = chunk
            changes.append([en, ja])
        if not changes:
            continue
        target = output/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        write_binary(target, mapped)
        write_textures(target.parent/'textures/textures.idastex', textures)
        audit.append(dict(asset=relative.as_posix(), chunks=len(mapped), base_textures=base_count,
                          textures=len(textures), mapping=changes,
                          english_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                          japanese_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    (output.parent/'manifest.json').write_text(json.dumps(dict(version=1, language='ja',
        source='Initial D Arcade Stage 3 Japan Rev C / GDS-0032C', banks=audit), indent=2)+'\n', encoding='utf-8')
    print(f'{len(audit)} UI banks; {sum(len(x["mapping"]) for x in audit)} mapped elements')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--english', type=Path, required=True)
    parser.add_argument('--japanese', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    import_ui(args.english, args.japanese, args.output)
