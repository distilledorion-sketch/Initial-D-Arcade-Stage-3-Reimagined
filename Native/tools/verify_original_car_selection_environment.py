#!/usr/bin/env python3
"""Verify selector1 aliases the existing native selector3 image; no re-export."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from texture_bank import decode

IMAGE_SHA256 = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def verify(image, hostfs, project):
    code = image.read_bytes()
    if hashlib.sha256(code).hexdigest() != IMAGE_SHA256:
        raise ValueError('Canonical image identity mismatch')
    entries = []
    for selector in range(4):
        pointer, = struct.unpack_from('<I', code, 0x0c2ef204 + 4 * selector - 0x0c020000)
        offset = pointer - 0x0c020000
        filename = code[offset:code.index(0, offset)].decode('ascii')
        if pointer != 0x0c23b334 or filename != '/driveA/binary/j_env_select128_b.bin.nz':
            raise ValueError('Source environment alias changed')
        entries.append({'selector': selector, 'pointer': hex(pointer), 'path': filename})
    source = hostfs / 'binary/j_env_select128_b.bin.nz'
    pixels = source.read_bytes()
    rgba = decode(pixels, 128, 128, 1, 'twiddled')
    relative = 'data/original_assets/tuning/environment/textures.idastex'
    exported = (project / relative).read_bytes()
    expected_header = b'IDAS3T1\0' + struct.pack('<6I', 1, 1, 0, 128, 128, 65536)
    if exported != expected_header + rgba:
        raise ValueError('Existing exported environment differs from original texels')
    result = {
        'schema': 'idas3-original-car-selection-environment-v1',
        'original_image_sha256': IMAGE_SHA256,
        'source_selector_table': '0C2EF204', 'entries': entries,
        'parent_setup': '11D820..11D858:029DA0(car,1),029040(car),car+224=2',
        'pointer_chain': ['parent+124 car array', '12D1E0 stack argument0',
                          'child+440', '12DC60 Init:10F040 r6 at12E046'],
        'selected_refresh': '12E4A0:029000(profile),028660(color),0286A0(1),029040',
        'driver_entry_variant': '11E4EA..11E4FA explicitly sets0286A0(car,0) before selector3/rebuild/layer2',
        'native_texture': relative, 'native_asset_reused': True,
        'source_texture_sha256': hashlib.sha256(pixels).hexdigest(),
        'native_texture_sha256': hashlib.sha256(exported).hexdigest(),
        'rgba_sha256': hashlib.sha256(rgba).hexdigest(),
        'dimensions': [128, 128], 'source_descriptor': '00800080 00000101 pixels 00000000',
        'checked_texels': 16384,
        'verification': 'Exact source RGB565 decode/repack and full existing native pack byte comparison',
        'source_test': 'tests/original_car_selection_environment_tests.cpp',
        'remaining_boundary': 'ELAN instance packet, generated environment coordinates and GPU filtering parity remain unproved',
    }
    output = project / 'verification/original-car-selection-environment'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', required=True, type=Path)
    parser.add_argument('--hostfs', required=True, type=Path)
    parser.add_argument('--project', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.image, args.hostfs, args.project), indent=2))
