#!/usr/bin/env python3
"""Export only the separately owned source Myogi crow bank and authored paths."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from extract_original_models import parse_model, write_binary
from texture_bank import export_bank

CANONICAL = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image, hostfs, project):
    code = image.read_bytes()
    if hashlib.sha256(code).hexdigest() != CANONICAL:
        raise ValueError('Canonical source image mismatch')
    files = {'model': (0x0c23e624, '/driveA/model/crow/crow_pol'),
             'texture': (0x0c23e640, '/driveA/model/crow/crow_tex'),
             'flight': (0x0c2a3bf4, '/driveA/path/k_ez1_crow_fly.bin'),
             'group': (0x0c2a3c14, '/driveA/path/k_ez1_crow_grp.bin')}
    for address, expected in files.values():
        offset = address - 0x0c020000
        if code[offset:code.index(0, offset)].decode('ascii') != expected:
            raise ValueError('Original crow resource path changed')
    report = {'schema': 'idas3-original-course-crows-v1', 'source_image_sha256': CANONICAL,
              'owner_constructor': '041D20', 'course_constructor': '19EA40',
              'course': 'Myogi day/dry, both directions', 'owner_offset': 1280,
              'readiness': '042620', 'initialize': '042440', 'update': '0424C0', 'draw': '042520',
              'source_files': {k: {'address': hex(v[0]), 'path': v[1]} for k, v in files.items()}}
    assets = project / 'data/original_assets/crows'
    model_dir = project / 'data/original_models/crows'
    assets.mkdir(parents=True, exist_ok=True);model_dir.mkdir(parents=True, exist_ok=True)
    for key, count in [('flight', 900), ('group', 19)]:
        raw = (hostfs / files[key][1][8:]).read_bytes()
        if struct.unpack_from('<2I', raw) != (count, 3) or len(raw) != 8 + count * 12:
            raise ValueError('Authored crow path extent changed')
        values = list(struct.iter_unpack('<3f', raw[8:]))
        target = assets / (key + '.bin');target.write_bytes(raw)
        report[key] = {'records': count, 'sha256': hashlib.sha256(raw).hexdigest(),
                       'file': target.relative_to(project).as_posix(),
                       'bounds': [[min(v[i] for v in values), max(v[i] for v in values)] for i in range(3)]}
    folder = hostfs / 'model/crow'
    _, chunks, _, _ = parse_model(folder, 'crow_pol.tbl', str((folder / 'crow_tex.tbl').resolve()))
    if len(chunks) != 30:raise ValueError('Original wing animation chunk count')
    batches = [batch for chunk in chunks for batch in chunk['batches']]
    # The submitted ICH carries both TCWs. Bit26=0 selects twiddled layout;
    # TCW texture format bits27..29=1 select RGB565 in this source bank.
    if any(((batch['words'][i] >> 26) & 1) or ((batch['words'][i] >> 27) & 7) != 1
           for batch in batches for i in (3, 5)):
        raise ValueError('Original crow TCW texture format/layout changed')
    model = model_dir / 'crow.idasmesh';write_binary(model, chunks)
    texture_manifest = export_bank(folder / 'crow_tex.tbl', folder / 'crow_tex.bin.nz', assets / 'textures', 'twiddled')
    report['model'] = {'file': model.relative_to(project).as_posix(), 'sha256': hashlib.sha256(model.read_bytes()).hexdigest(),
                       'chunks': len(chunks), 'source_sha256': hashlib.sha256((folder / 'crow_pol.bin.nz').read_bytes()).hexdigest()}
    report['model']['animation_frames'] = [{'frame': chunk['index'], 'batches': len(chunk['batches']),
        'vertices': sum(len(batch['vertices']) for batch in chunk['batches']),
        'triangles': sum(len(batch['indices']) // 3 for batch in chunk['batches'])} for chunk in chunks]
    report['textures'] = {'file': 'data/original_assets/crows/textures/textures.idastex',
        'count': len(texture_manifest['textures']), 'records': texture_manifest['textures'],
        'layout_evidence': 'All thirty source ICH TCW pairs: scan-order bit26=0, pixel-format bits27..29=1 (twiddled RGB565).'}
    report['phase_boundary'] = 'Initial nineteen wing phases consume original shared RNG37C778; caller must supply its source-boundary seed.'
    report['native_preserving_adapter'] = 'resetBeforeDrivingSeed reconstructs nineteen preceding LCG states ending at the caller supplied player-initialization seed; it does not write driving state or claim original boot/menu history.'
    report['startup'] = {'race_course_pointer_offset': 1036,
        'course_setup_call': '061B46 -> 062480', 'loading_mode': 'synchronous,042700 stack+4=0',
        'player_setup_call': '061C04 -> 0625E0 -> 159720',
        'player_rng_draw': '15EF5E..15EF78', 'ordinary_course_update': '05FE48..05FE6A -> 19F3C0 -> 0424C0',
        'warmup_crow_updates': 0, 'first_ordinary_update_flight_cursor': 1,
        'boot_seed_boundary': '056A44..056A54 -> 2021E0 platform word -> 1F9EE0; not a fixed race seed'}
    report['geometry_scope'] = 'Nineteen animated flying crows, all submitted once per Myogi day/dry draw, no player/camera/path-index gating in042520.'
    out = project / 'verification/original-course-crows';out.mkdir(parents=True, exist_ok=True)
    (out / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    for key in ['image', 'hostfs', 'project']:p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args();print(json.dumps(export(a.image, a.hostfs, a.project), indent=2))
