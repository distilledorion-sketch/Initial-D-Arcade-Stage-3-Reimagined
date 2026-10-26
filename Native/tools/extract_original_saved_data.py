"""Export original Saved Data and NetRankings artwork from a verified local dump."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import struct
import texture_bank
from extract_original_menus import export_model_menu, export_menu, ORIGINAL_IMAGE_SHA256


def export(game, output):
    image = game / 'idas3_main_0C020000.bin'
    program = image.read_bytes()
    if hashlib.sha256(program).hexdigest() != ORIGINAL_IMAGE_SHA256:
        raise ValueError('The verified Stage 3 source image is required')
    records = []
    with tempfile.TemporaryDirectory() as temporary:
        for bank in ('v3sS10conf', 'cardcheck', 'cardcheck2', 'cardcheck_car',
                     'cardfont_a', 'cardfont_h', 'cardfont_k', 'password', 'password_sprite'):
            work = Path(temporary) / bank
            if bank == 'password_sprite':
                source = game / 'driveA/HOSTFS/sprite/password'
                manifest = export_menu(source, work, 'twiddled')
                names = ('original_rip.tbl', 'original_rip.bin', 'sprites.idasui', 'textures/textures.idastex')
            else:
                folder = 'cardcheck' if bank == 'cardcheck2' or bank.startswith('cardfont_') else bank
                source = game / 'driveA/HOSTFS/model' / folder
                texture = 'cardfont_tex.tbl' if bank.startswith('cardfont_') else bank+'_tex.tbl'
                manifest = export_model_menu(source, work, 'twiddled', bank+'_pol.tbl', texture)
                names = (bank+'.idasmesh', 'textures/textures.idastex')
            hashes = {}
            for name in names:
                dest = output / bank / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(work / name, dest)
                hashes[name] = hashlib.sha256(dest.read_bytes()).hexdigest()
            records.append(dict(bank=bank, outputs=hashes))
        # Configuration uses m_s10_font and spacing mode5, not the shop's
        # option font (which lacks periods and has different proportions).
        source = game / 'driveA/HOSTFS/font/m_s10_font'
        work = Path(temporary) / 'help_font'
        texture_bank.export_bank(source/'m_s10_font_spr.tbl', source/'m_s10_font_spr.bin.nz', work/'textures', 'twiddled')
        (work/'font.bin').write_bytes((source/'m_s10_font_font.bin.nz').read_bytes())
        spacing = bytearray()
        for index in range(35):
            pointer, trim = struct.unpack_from('<II', program, 0x339714 + index*8 - 0x20000)
            code = struct.unpack_from('<H', program, pointer - 0x0c020000)[0]
            spacing.extend(struct.pack('<HHI', code, 0, trim))
        (work/'spacing.bin').write_bytes(spacing)
        hashes = {}
        for name in ('font.bin', 'spacing.bin', 'textures/textures.idastex'):
            dest = output/'help_font'/name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(work/name, dest)
            hashes[name] = hashlib.sha256(dest.read_bytes()).hexdigest()
        records.append(dict(bank='help_font', outputs=hashes))
    (output/'manifest.json').write_text(json.dumps(dict(source_image_sha256=ORIGINAL_IMAGE_SHA256,
        banks=records, scope='Original artwork only; no executable or personal profile copied.'), indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    export(args.game, args.output)
