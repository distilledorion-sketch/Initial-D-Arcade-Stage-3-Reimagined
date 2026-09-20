#!/usr/bin/env python3
"""Extract original Stage 3 sprite textures and authored RIP vertices.

The assets remain private original artwork. Layout/state identities are not
guessed: exact positions, UVs, colors and tag words are retained for consumers.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import texture_bank
import extract_original_models as models

TABLE = struct.Struct('<4I')
VERTEX = struct.Struct('<5fII')
UI_MAGIC = b'IDAS3U1\0'
V3_BANKS = ['adv_newtitle', 'adv_title', 'v3sA00etc', 'v3sS04maker', 'v3sS05cars',
            'v3sS06mission', 'v3sS07tune', 'v3sS11mode', 'v3sT01course',
            'v3sT02route', 'v3sT03weather', 'v3sT04time', 'adv2d',
            'v3sS00common', 'v3sS00emblem', 'v3sS00minicar', 'v3sT00common', 'v3sB01course',
            'v3sK00common', 'v3sK01course', 'v3sK02rival']
ORIGINAL_IMAGE_SHA256 = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decode_rip(table: bytes, payload: bytes, texture_records: list[dict]) -> list[dict]:
    if not table or len(table) % TABLE.size:
        raise ValueError('RIP table must contain complete nonempty 16-byte records')
    sprites = []
    offset = 0
    for index, (texture, count, tag_a, tag_b) in enumerate(TABLE.iter_unpack(table)):
        if texture >= len(texture_records) or texture > 65535:
            raise ValueError(f'Sprite {index}: texture index outside supplied bank')
        if not 3 <= count <= 4096:
            raise ValueError(f'Sprite {index}: unsupported vertex count {count}')
        if tag_a > 255 or tag_b > 255:
            raise ValueError(f'Sprite {index}: tag cannot be represented in packed vertex descriptor')
        size = count * VERTEX.size
        if offset + size > len(payload):
            raise ValueError(f'Sprite {index}: vertex payload truncated')
        raw = payload[offset:offset + size]
        vertices = []
        packed = texture | (tag_a << 24) | (tag_b << 16)
        for vertex_index, fields in enumerate(VERTEX.iter_unpack(raw)):
            if not all(math.isfinite(value) for value in fields[:5]):
                raise ValueError(f'Sprite {index}: non-finite position or UV')
            if fields[6] != packed:
                raise ValueError(f'Sprite {index}: packed vertex descriptor disagrees with table')
            bits = struct.unpack_from('<7I', raw, vertex_index * VERTEX.size)
            vertices.append(dict(zip(('x', 'y', 'z', 'u', 'v', 'argb', 'descriptor'), fields)) |
                            {'raw_words_hex': [f'{word:08x}' for word in bits]})
        bounds = [min(v['x'] for v in vertices), min(v['y'] for v in vertices),
                  max(v['x'] for v in vertices), max(v['y'] for v in vertices)]
        uv_bounds = [min(v['u'] for v in vertices), min(v['v'] for v in vertices),
                     max(v['u'] for v in vertices), max(v['v'] for v in vertices)]
        xs, ys = {v['x'] for v in vertices}, {v['y'] for v in vertices}
        us, vs = {v['u'] for v in vertices}, {v['v'] for v in vertices}
        rectangular = count == 4 and len(xs) == len(ys) == len(us) == len(vs) == 2
        rectangular &= len({(v['x'], v['y']) for v in vertices}) == count
        rectangular &= len({(v['u'], v['v']) for v in vertices}) == count
        tex = texture_records[texture]
        source_bounds = [uv_bounds[0] * tex['width'], uv_bounds[1] * tex['height'],
                         uv_bounds[2] * tex['width'], uv_bounds[3] * tex['height']]
        # Export an exact texel crop only when edges already land on texel
        # boundaries (allow original decimal-to-float serialization error).
        snapped = [round(value) for value in source_bounds]
        crop = None
        if rectangular and all(abs(a - b) <= 0.001 for a, b in zip(source_bounds, snapped)):
            x0, y0, x1, y1 = snapped
            if 0 <= x0 < x1 <= tex['width'] and 0 <= y0 < y1 <= tex['height']:
                crop = snapped
        sprites.append({
            'index': index, 'texture_index': texture, 'vertex_count': count,
            'tag_a': tag_a, 'tag_b': tag_b, 'packed_descriptor_hex': f'{packed:08x}',
            'payload_offset': offset, 'payload_bytes': size, 'payload_sha256': digest(raw),
            'authored_bounds_xy': bounds, 'uv_bounds': uv_bounds,
            'source_texel_bounds_float': source_bounds, 'exact_texel_crop': crop,
            'axis_aligned_position_and_uv_quad': bool(rectangular),
            'vertices': vertices,
            'identity': 'Unassigned original sprite; filename/index retained',
        })
        offset += size
    if offset != len(payload):
        raise ValueError(f'RIP payload has {len(payload) - offset} unexplained trailing bytes')
    return sprites


def make_ui_pack(sprites: list[dict], payload: bytes) -> bytes:
    result = bytearray(UI_MAGIC + struct.pack('<II', 1, len(sprites)))
    for sprite in sprites:
        result += struct.pack('<5I', sprite['index'], sprite['texture_index'], sprite['vertex_count'],
                              sprite['tag_a'], sprite['tag_b'])
        start = sprite['payload_offset']
        result += payload[start:start + sprite['payload_bytes']]
    return bytes(result)


def crop_rgba(rgba: bytes, width: int, height: int, rect: list[int]) -> bytes:
    x0, y0, x1, y1 = rect
    if len(rgba) != width * height * 4 or not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
        raise ValueError('Invalid exact crop')
    return b''.join(rgba[(y * width + x0) * 4:(y * width + x1) * 4] for y in range(y0, y1))


def choose_payload(folder: Path, stem: str) -> Path:
    plain, nz = folder / f'{stem}.bin', folder / f'{stem}.bin.nz'
    if plain.exists():
        if nz.exists() and plain.read_bytes() != nz.read_bytes():
            raise ValueError(f'Ambiguous differing .bin/.bin.nz copies: {stem}')
        return plain
    if nz.exists():
        return nz
    raise ValueError(f'Missing extracted payload: {stem}')


def export_menu(folder: Path, output: Path, layout: str) -> dict:
    name = folder.name
    rip_table = folder / f'{name}_rip.tbl'
    rip_payload = choose_payload(folder, f'{name}_rip')
    spr_table = folder / f'{name}_spr.tbl'
    spr_payload = choose_payload(folder, f'{name}_spr')
    # Parse metadata and RIP before writing anything for this bank.
    texture_table = spr_table.read_bytes()
    if not texture_table or len(texture_table) % 16:
        raise ValueError('Incomplete sprite texture table')
    tex_records = []
    for index, record in enumerate(struct.iter_unpack('<HHBBHII', texture_table)):
        width, height, fmt, flags, reserved, start, tail = record
        tex_records.append({'index': index, 'width': width, 'height': height, 'format': fmt,
                            'flags': flags, 'reserved': reserved, 'offset': start, 'tail': tail})
    table, raw = rip_table.read_bytes(), rip_payload.read_bytes()
    sprites = decode_rip(table, raw, tex_records)
    textures = texture_bank.export_bank(spr_table, spr_payload, output / 'textures', layout)
    texture_bytes = spr_payload.read_bytes()
    rgba_cache = {}
    crops_dir = output / 'crops'
    crops_dir.mkdir(parents=True, exist_ok=True)
    for sprite in sprites:
        rect = sprite['exact_texel_crop']
        if rect is None:
            continue
        index = sprite['texture_index']
        tex = tex_records[index]
        if index not in rgba_cache:
            start = tex['offset']
            rgba_cache[index] = texture_bank.decode(texture_bytes[start:start + tex['width'] * tex['height'] * 2],
                                                    tex['width'], tex['height'], tex['format'], layout)
        crop = crop_rgba(rgba_cache[index], tex['width'], tex['height'], rect)
        filename = f'crops/sprite_{sprite["index"]:03d}_texture_{index:03d}.png'
        texture_bank.write_png(output / filename, rect[2] - rect[0], rect[3] - rect[1], crop)
        sprite['crop_file'] = filename
        sprite['crop_rgba_sha256'] = digest(crop)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'original_rip.tbl').write_bytes(table)
    (output / 'original_rip.bin').write_bytes(raw)
    pack = make_ui_pack(sprites, raw)
    (output / 'sprites.idasui').write_bytes(pack)
    manifest = {
        'schema': 'idas3-original-menus-v1', 'name': name, 'private_original_assets': True,
        'source_directory': str(folder.resolve()),
        'source_rip_table': str(rip_table.resolve()), 'source_rip_payload': str(rip_payload.resolve()),
        'source_rip_table_sha256': digest(table), 'source_rip_payload_sha256': digest(raw),
        'texture_manifest': 'textures/manifest.json', 'texture_pack': 'textures/textures.idastex',
        'texture_count': len(textures['textures']), 'sprite_count': len(sprites),
        'ui_pack': 'sprites.idasui', 'ui_pack_sha256': digest(pack),
        'layout': layout,
        'layout_evidence': 'Explicit decoder argument; twiddled yields coherent original art in inspected banks. PVR submission-state identity is a separate gate.',
        'rip_evidence': '16-byte records and cumulative 28-byte vertices consume exact payload; packed texture/tag descriptor independently matches every vertex.',
        'coordinate_contract': 'Authored xyzuv retained without translation, scaling or Y inversion. Screen projection, tag-controlled visibility and animation require original draw-state binding.',
        'known_unresolved': ['Meaning of tag_a/tag_b beyond exact packed representation',
                             'Original per-state sprite selection, animation and depth/blending',
                             'Complete original screen projection and surrounding background/model layers'],
        'sprites': sprites,
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    return manifest


def export_menus(source: Path, output: Path, layout: str, banks: list[str] | None = None) -> dict:
    names = banks or sorted(path.name for path in source.iterdir()
                            if path.is_dir() and (path / f'{path.name}_rip.tbl').is_file())
    if not names:
        raise ValueError('No RIP sprite banks found')
    if len(names) != len(set(names)):
        raise ValueError('Duplicate bank selection')
    summaries = []
    for name in names:
        if Path(name).name != name or name in ('.', '..') or '/' in name or '\\' in name:
            raise ValueError('Bank must be a plain folder name')
        result = export_menu(source / name, output / name, layout)
        summaries.append({'name': name, 'manifest': f'{name}/manifest.json', 'ui_pack': f'{name}/sprites.idasui',
                          'texture_pack': f'{name}/textures/textures.idastex',
                          'textures': result['texture_count'], 'sprites': result['sprite_count']})
    index = {'schema': 'idas3-original-menu-index-v1', 'private_original_assets': True,
             'source': str(source.resolve()), 'layout': layout, 'banks': summaries,
             'total_textures': sum(item['textures'] for item in summaries),
             'total_sprites': sum(item['sprites'] for item in summaries)}
    output.mkdir(parents=True, exist_ok=True)
    (output / 'index.json').write_text(json.dumps(index, indent=2) + '\n', encoding='utf-8')
    return index


def export_model_menu(folder: Path, output: Path, layout: str,
                      polygon_table: str | None = None,
                      texture_table_name: str | None = None) -> dict:
    """Preserve authored V3 menu GMP/ICH geometry, without guessing projection."""
    prefix, chunks, sources, polygon = models.parse_model(folder, polygon_table, texture_table_name)
    # Card-check geometry shares a directory and the three card fonts share
    # one texture bank. Resolve the payload from the selected texture table.
    texture_payload = choose_payload(folder, sources['texture_table'].stem)
    output.mkdir(parents=True, exist_ok=True)
    # Texture decoder accepts the extracted payload. Retain encoded source hash
    # as well when an NMZIP wrapper actually occurs in a future bank.
    raw_texture = models.read_payload(texture_payload)
    texture_input = texture_payload
    if raw_texture != texture_payload.read_bytes():
        texture_input = output / 'original_texture_decoded.bin'
        texture_input.write_bytes(raw_texture)
    textures = texture_bank.export_bank(sources['texture_table'], texture_input, output / 'textures', layout)
    mesh = output / f'{prefix}.idasmesh'
    models.write_binary(mesh, chunks)
    (output / 'original_pol.bin').write_bytes(polygon)
    (output / 'original_pol.tbl').write_bytes(sources['table'].read_bytes())
    catalog = []
    for chunk in chunks:
        batches = []
        all_vertices = []
        for index, batch in enumerate(chunk['batches']):
            vertices = batch['vertices']
            all_vertices.extend(vertices)
            batches.append({'index': index, 'source_offset': batch['source_offset'],
                            'ich_words_hex': [f'{w:08x}' for w in batch['words']],
                            'material_words_hex': [f'{w:08x}' for w in batch['material']],
                            'primary_texture_index': batch['material'][9],
                            'vertex_layout_hex': f'{batch["words"][6]:03x}',
                            'source_vertex_sha256': digest(batch['source_vertex_bytes']),
                            'indices': batch['indices'],
                            'vertices': [{'header_hex': f'{v[0]:08x}', 'xyz': list(v[1:4]),
                                          'uv': list(v[7:9]), 'colors_argb_hex': [f'{c:08x}' for c in v[9:11]]}
                                         for v in vertices]})
        bounds = ([[min(v[k] for v in all_vertices), max(v[k] for v in all_vertices)]
                   for k in (1, 2, 3)] if all_vertices else None)
        catalog.append({'index': chunk['index'], 'source_offset': chunk['source_offset'],
                        'source_size': chunk['source_size'], 'header_words_hex': [f'{w:08x}' for w in chunk['header']],
                        'bounds_xyz': bounds, 'vertex_count': len(all_vertices), 'batches': batches,
                        'nongeometry_marker': chunk.get('nongeometry_marker', False),
                        'diagnostic_screen_bounds_at_100x_negative_y': [bounds[0][0] * 100, -bounds[1][1] * 100,
                                                                      bounds[0][1] * 100, -bounds[1][0] * 100] if bounds else None})
    sources['texture_payload'] = texture_payload
    result = {'schema': 'idas3-original-model-menu-v1', 'name': prefix, 'private_original_assets': True,
              'source_directory': str(folder.resolve()),
              'source_files': {k: {'path': str(p.resolve()), 'bytes': p.stat().st_size,
                                   'sha256': digest(p.read_bytes())} for k, p in sources.items() if p.exists()},
              'decoded_polygon_sha256': digest(polygon), 'mesh_pack': mesh.name,
              'mesh_pack_sha256': digest(mesh.read_bytes()), 'texture_pack': 'textures/textures.idastex',
              'texture_manifest': 'textures/manifest.json', 'texture_count': len(textures['textures']),
              'chunk_count': len(chunks), 'chunks': catalog, 'layout': layout,
              'coordinate_transform': 'none', 'uv_transform': 'none',
              'projection_evidence': 'A 6.4 by 4.8 authored title quad numerically fits 640 by 480 at x*100,y*-100. Diagnostic bounds use that fit only; original draw-matrix proof remains required.',
              'known_unresolved': ['Per-state chunk selection and animation/translation',
                                   'Original view/projection matrices and depth state',
                                   'Effective material override, sampling and blend state']}
    (output / 'manifest.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    return result


def export_model_menus(source: Path, output: Path, layout: str, banks: list[str] | None = None) -> dict:
    names = V3_BANKS if banks is None else banks
    if not names or len(names) != len(set(names)):
        raise ValueError('Model bank selection must be nonempty and unique')
    summaries = []
    for name in names:
        if Path(name).name != name or name in ('.', '..') or '/' in name or '\\' in name:
            raise ValueError('Bank must be a plain folder name')
        result = export_model_menu(source / name, output / name, layout)
        summaries.append({'name': name, 'manifest': f'{name}/manifest.json',
                          'mesh_pack': f'{name}/{result["mesh_pack"]}',
                          'texture_pack': f'{name}/textures/textures.idastex',
                          'textures': result['texture_count'], 'chunks': result['chunk_count']})
    index = {'schema': 'idas3-original-model-menu-index-v1', 'private_original_assets': True,
             'source': str(source.resolve()), 'layout': layout, 'banks': summaries,
             'total_textures': sum(item['textures'] for item in summaries),
             'total_chunks': sum(item['chunks'] for item in summaries)}
    output.mkdir(parents=True, exist_ok=True)
    (output / 'index.json').write_text(json.dumps(index, indent=2) + '\n', encoding='utf-8')
    return index


def export_car_menu_mapping(image_path: Path, output: Path) -> dict:
    image = image_path.read_bytes()
    if digest(image) != ORIGINAL_IMAGE_SHA256:
        raise ValueError('Original menu table extraction requires the verified Stage3 image')
    address, image_base = 0x0c2aae80, 0x0c020000
    entries = list(struct.unpack_from('<35I', image, address-image_base))
    if sorted(entries) != list(range(35)):
        raise ValueError('Original car menu table is not the observed complete permutation')
    result = {'schema': 'idas3-original-car-menu-map-v1', 'source_image': str(image_path.resolve()),
              'source_image_sha256': digest(image), 'source_address_hex': f'{address:08x}',
              'source_bytes': 140, 'source_table_sha256': digest(image[address-image_base:address-image_base+140]),
              'original_car_id_to_zero_based_menu_index': entries,
              'original_car_id_to_s05cars_name_chunk': [entry+1 for entry in entries],
              'original_car_id_to_s00minicar_single_chunk': [entry+8 for entry in entries],
              'evidence': ['0C1BE1E4 contains this table address; original code loads index using packed original car identity bits',
                           'All35 original S05cars name chunks independently rendered and visually cross-checked against original car folder identities',
                           'S00minicar single-car chunks8..42 reference textures0..34 in identical menu order; aggregate manufacturer grids corroborate the order'],
              'mapping_scope': 'Artwork identity only; does not establish original navigation order or animation'}
    output.mkdir(parents=True, exist_ok=True)
    (output / 'car_menu_map.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path, help='Original extracted HOSTFS/sprite or HOSTFS/model folder')
    parser.add_argument('--kind', choices=['rip', 'model'], default='rip')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--layout', required=True, choices=['twiddled', 'linear'])
    parser.add_argument('--bank', action='append', help='Repeat to select specific bank folders; default all')
    parser.add_argument('--original-image', type=Path, help='Optional verified original image for exact car menu permutation')
    args = parser.parse_args()
    if args.kind == 'model':
        result = export_model_menus(args.source, args.out, args.layout, args.bank)
        if args.original_image:
            export_car_menu_mapping(args.original_image, args.out)
        print(f'Extracted {len(result["banks"])} original V3 model banks, {result["total_textures"]} lossless textures, '
              f'{result["total_chunks"]} authored chunks. Draw-state binding remains separate.')
        return
    result = export_menus(args.source, args.out, args.layout, args.bank)
    print(f'Extracted {len(result["banks"])} original banks, {result["total_textures"]} lossless textures, '
          f'{result["total_sprites"]} authored sprites. Visibility/state binding remains separate.')


if __name__ == '__main__':
    main()
