"""Capture every factory paint through the original player/material setup."""
import argparse, concurrent.futures, hashlib, json, struct, subprocess
from pathlib import Path
from extract_original_models import parse_model, VERTEX_SIZE
from export_original_assembly import IMAGE_HASH, operations_matrix
from export_car_presentation import KINDS, CHANNELS, WHEEL_CALLS

def run(args):
    result = subprocess.run([str(v) for v in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)

def pack_geometry(output, capture, night, parts, maximum_phase):
    programs, wheels = [], []
    with (output / 'car.idasasm').open('wb') as file:
        file.write(b'IDAS3A1\0' + struct.pack('<II', 1, len(capture['draws'])))
        for index, draw in enumerate(capture['draws']):
            file.write(struct.pack('<I16f', draw['chunk'], *operations_matrix(draw['operations'])))
            if draw['return_pc'] in WHEEL_CALLS:
                wheels.append(WHEEL_CALLS.index(draw['return_pc']))
            operations, dynamic = [], False
            for op in draw['operations']:
                channel = CHANNELS.get(op['source'], 0)
                if op['source'] == 0x0c02795e:
                    channel = 2 + (1, 0, 3, 2)[draw['semantic'] - 60]
                dynamic |= channel != 0
                operations.append(struct.pack('<Ii16f', KINDS[op['type']], channel, *(op['values'] + [0.] * (16 - len(op['values'])))))
            if dynamic:
                programs.append(struct.pack('<II', index, len(operations)) + b''.join(operations))
    if sorted(wheels) != [0, 1, 2, 3]:
        raise ValueError('Original paint appearance lost a wheel')
    (output / 'wheel_motion.bin').write_bytes(b'ID3MOT1\0' + struct.pack('<II', 1, len(programs)) + b''.join(programs))
    config = capture['config_word']
    transforms = [struct.unpack_from('<9f', parts, (config & 7) * 36), struct.unpack_from('<9f', parts, 0xd8 + ((config >> 13) & 7) * 36)]
    (output / 'plate.bin').write_bytes(b'ID3PLT1\0' + struct.pack('<III18f', 1, 1, config, *transforms[0], *transforms[1]))
    indices = [i for i, draw in enumerate(capture['draws']) if draw['return_pc'] in (0xc027372, 0xc027430)]
    if len(indices) > 1:
        raise ValueError('Factory paint headlight variant is ambiguous')
    with (output / 'headlights.bin').open('wb') as file:
        file.write(b'ID3POP1\0' + struct.pack('<IIII', 1, indices[0] if indices else 0xffffffff, maximum_phase, 2))
        for state in (capture, night):
            selected = [draw for draw in state['draws'] if draw['return_pc'] in (0xc027372, 0xc027430)]
            if len(selected) > 1 or bool(selected) != bool(indices):
                raise ValueError('Factory paint headlight selection ambiguous')
            if not selected:
                file.write(struct.pack('<II', 0xffffffff, 0))
                continue
            draw = selected[0]
            file.write(struct.pack('<II', draw['chunk'], len(draw['operations'])))
            for op in draw['operations']:
                file.write(struct.pack('<II16f', KINDS[op['type']], int(op['source'] == 0xc02736a), *(op['values'] + [0.] * (16 - len(op['values'])))))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'hostfs', 'image', 'material-exe', 'geometry-exe', 'work'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    program = args.image.read_bytes()
    if hashlib.sha256(program).hexdigest() != IMAGE_HASH:
        raise ValueError('Canonical image identity mismatch')
    word = lambda address: struct.unpack_from('<I', program, address - 0x0c020000)[0]
    catalog = json.loads((args.root / 'data/original_models/car_catalog.json').read_text())['cars']

    def capture(car):
        index, folder = car['car_index'], car['folder']
        count = word(0x0c30ecd4 + index * 32)  #133B00 ->0DDB40
        if not 1 <= count <= 8:
            raise ValueError('Original factory color count outside config field')
        work = args.work / folder
        work.mkdir(parents=True, exist_ok=True)
        base = args.root / 'data/original_models' / folder
        _, chunks, paths, source = parse_model(args.hostfs / 'model/car' / folder)
        raw = work / 'source.bin'
        raw.write_bytes(source)
        descriptors, allowed = [], set()
        for chunk in chunks:
            if chunk.get('nongeometry_marker'):
                continue
            pos, end, material, batch = chunk['source_offset'] + 96, chunk['source_offset'] + chunk['source_size'], None, 0
            while pos < end:
                words = struct.unpack_from('<8I', source, pos)
                command = (words[0] >> 8) & 15
                if command == 5:
                    material = pos
                    allowed.update(range(pos, pos + 64))
                    pos += 64
                    continue
                if command != 7 or material is None:
                    raise ValueError('Unexpected polygon source command')
                allowed.update(range(pos, pos + 32))
                old = struct.unpack_from('<16I', source, material) + struct.unpack_from('<8I', source, pos)
                descriptors.append((chunk['index'], batch, material, pos, old))
                batch += 1
                pos += 32 + VERTEX_SIZE[words[6]] * words[7]
        reference_draws = json.loads((base / 'fresh_player/source_capture.json').read_text())['draws']
        chunk_count = json.loads((base / 'model_manifest.json').read_text())['chunk_count']
        parts = (args.hostfs / 'parts' / f'{folder}.bin').read_bytes()
        phase = word(word(0xc19145c) + index * 4)
        records = []
        for color in range(count):
            output = base / 'colors' / f'color_{color:02}'
            output.mkdir(parents=True, exist_ok=True)
            changed, metadata, geometry = work / 'changed.bin', work / 'material.json', work / f'geometry_{color:02}.json'
            run([args.material_exe, args.image, raw, paths['table'], index, -1, changed, metadata, color])
            run([args.geometry_exe, args.image, args.hostfs / 'parts' / f'{folder}.bin', geometry, index, chunk_count, -1, 0, color])
            draw = json.loads(geometry.read_text())
            night_path = work / f'night_{color:02}.json'
            run([args.geometry_exe, args.image, args.hostfs / 'parts' / f'{folder}.bin', night_path, index, chunk_count, -1, 1, color])
            night = json.loads(night_path.read_text())
            pack_geometry(output, draw, night, parts, phase)
            (output / 'source_capture.json').write_text(geometry.read_text())
            (output / 'night_capture.json').write_text(night_path.read_text())
            after = changed.read_bytes()
            if len(after) != len(source) or any(a != b and i not in allowed for i, (a, b) in enumerate(zip(source, after))):
                raise ValueError('Paint setup changed bytes outside material descriptors')
            patches = []
            for chunk, batch, material, pos, old in descriptors:
                new = struct.unpack_from('<16I', after, material) + struct.unpack_from('<8I', after, pos)
                if old != new:
                    patches.append(struct.pack('<II48I', chunk, batch, *old, *new))
            packed = b'ID3CMP1\0' + struct.pack('<II', 1, len(patches)) + b''.join(patches)
            if color == 0 and packed != (base / 'fresh_player/materials.bin').read_bytes():
                raise ValueError('Default factory color differs from released source appearance')
            (output / 'materials.bin').write_bytes(packed)
            record = json.loads(metadata.read_text())
            palette = word(0x0c33b1a0 + index * 4) + color * 36
            r, g, b = program[palette - 0x0c020000:palette - 0x0c020000 + 3]
            if record['rgb'] != [r, g, b] or (draw['config_word'] >> 25) & 7 != color:
                raise ValueError('Source palette/config color mismatch')
            record.update(color=color, palette_address=f'{palette:08X}', rgb24=(r << 16) | (g << 8) | b,
                          material_patches=len(patches), material_sha256=hashlib.sha256(packed).hexdigest(),
                          geometry_sha256=hashlib.sha256(geometry.read_bytes()).hexdigest(), geometry_matches_default=draw['draws'] == reference_draws,
                          geometry_instructions=draw['preset_instructions'] + draw['original_instructions'])
            records.append(record)
        result = dict(car=index, folder=folder, count=count, colors=records)
        (base / 'colors/manifest.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f'car{index}: {count} original paint/material variants; {sum(not item["geometry_matches_default"] for item in records)} alternate assemblies', flush=True)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        cars = sorted(executor.map(capture, catalog), key=lambda item: item['car'])
    manifest = dict(schema='idas3-original-factory-paint-v1', source_image_sha256=IMAGE_HASH, cars=cars,
                    source='133B00/0DDB40 counts; profile64 through0630B4..06316E;0267C0 and029040..029AD0 materials;1911A0 palette;026D80 geometry.',
                    boundaries='CPU-only original instruction captures. Every factory color includes its complete assembly, wheel/headlight programs, plate placement and material state, including color-specific geometry. Explicit capture hooks documented in the capture tools; no hardware image equivalence claim.')
    (args.root / 'data/original_models/car_colors_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    header = '#pragma once\n#include <array>\n#include <cstdint>\nnamespace idas3::original {\n'
    header += '// Exact133B00/0DDB40 counts and1911A0 factory palettes. Generated from the canonical image.\n'
    header += 'inline constexpr std::array<unsigned,35> originalCarColorCounts={' + ','.join(str(car['count']) for car in cars) + '};\n'
    header += 'inline constexpr std::array<std::array<std::uint32_t,8>,35> originalCarPaintRgb={{\n'
    for car in cars:
        header += '    {' + ','.join(f'0x{c["rgb24"]:06x}u' for c in car['colors']) + '},\n'
    header += '}};\n}\n'
    (args.root / 'src/original_car_color_catalog.h').write_text(header)
    print(f'Captured {sum(car["count"] for car in cars)} original factory paints across35cars.', flush=True)

if __name__ == '__main__':
    main()
