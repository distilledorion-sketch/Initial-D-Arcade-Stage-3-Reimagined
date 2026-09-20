#!/usr/bin/env python3
"""Recover private Stage 3 card/menu geometry, including shared font banks.

This exports source artwork only. It does not invent a card reader, a screen
state machine, or a replacement for local save slots.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

from extract_original_menus import export_model_menu, digest, ORIGINAL_IMAGE_SHA256


def export_cards(game: Path, output: Path) -> dict:
    image = game / 'idas3_main_0C020000.bin'
    if digest(image.read_bytes()) != ORIGINAL_IMAGE_SHA256:
        raise ValueError('Card menus require the verified Stage 3 image')
    model_root = game / 'driveA/HOSTFS/model'
    # Bank 17 (v3sS09conf) is an empty placeholder. The Stage 3 card-check
    # dispatcher 10CE60 resolves bank 18 for the actual profile/options panels.
    specs = [('v3sS01card', 'v3sS01card', None),
             ('ejectcard', 'ejectcard', None),
             ('v3sS10conf', 'v3sS10conf', None),
             ('cardcheck', 'cardcheck', None),
             ('cardcheck', 'cardcheck2', None),
             ('cardcheck', 'cardfont_a', 'cardfont_tex.tbl'),
             ('cardcheck', 'cardfont_h', 'cardfont_tex.tbl'),
             ('cardcheck', 'cardfont_k', 'cardfont_tex.tbl')]
    banks = []
    for folder, name, texture_table in specs:
        result = export_model_menu(model_root / folder, output / name, 'twiddled',
                                   name + '_pol.tbl', texture_table)
        banks.append({'name': name, 'manifest': name + '/manifest.json',
                      'chunks': result['chunk_count'], 'textures': result['texture_count'],
                      'markers': [c['index'] for c in result['chunks']
                                  if c['nongeometry_marker']]})
    result = {'schema': 'idas3-original-card-menu-index-v1',
              'private_original_assets': True, 'source_image': str(image.resolve()),
              'source_image_sha256': ORIGINAL_IMAGE_SHA256, 'banks': banks,
              'scope': 'Unmodified geometry/texture extraction; screen ownership and animation not implemented'}
    (output / 'index.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='Private game files directory')
    parser.add_argument('output', type=Path, help='Export directory')
    args = parser.parse_args()
    print(json.dumps(export_cards(args.game, args.output), indent=2))
