"""Export unmodified Stage 3 level notices and end-of-session artwork."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
from extract_original_menus import export_model_menu, ORIGINAL_IMAGE_SHA256


def export(game, output):
    image = game / 'idas3_main_0C020000.bin'
    if hashlib.sha256(image.read_bytes()).hexdigest() != ORIGINAL_IMAGE_SHA256:
        raise ValueError('The verified Stage 3 source image is required')
    specs = [('levelup', 'levelup_dr'), ('levelup', 'levelup_bu'),
             ('levelup', 'levelup_vs'), ('gameover_v3', 'gameover_v3'), ('ejectcard', 'ejectcard')]
    records = []
    with tempfile.TemporaryDirectory() as temporary:
        for folder, bank in specs:
            source = game / 'driveA/HOSTFS/model' / folder
            work = Path(temporary) / bank
            manifest = export_model_menu(source, work, 'twiddled', bank + '_pol.tbl', bank + '_tex.tbl')
            hashes = {}
            for name in (bank + '.idasmesh', 'textures/textures.idastex'):
                dest = output / bank / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(work / name, dest)
                hashes[name] = hashlib.sha256(dest.read_bytes()).hexdigest()
            records.append(dict(bank=bank, chunks=manifest['chunk_count'], textures=manifest['texture_count'], outputs=hashes))
    (output / 'manifest.json').write_text(json.dumps(dict(source_image_sha256=ORIGINAL_IMAGE_SHA256,
        source='driveA/HOSTFS/model', banks=records, scope='Unmodified original artwork; no executable copied.'), indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    export(args.game, args.output)
