#!/usr/bin/env python3
"""Export the retail TITLE3 dialogue scene and its exact script/font tables."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from extract_original_menus import export_model_menu
import texture_bank

CANONICAL='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image: Path, hostfs: Path, project: Path):
    program=image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=CANONICAL: raise ValueError('Unknown original image')
    def u(address): return struct.unpack_from('<I',program,address-0x0c020000)[0]
    def string(address):
        begin=address-0x0c020000
        return program[begin:program.index(0,begin)].decode('ascii')
    root=project/'data/original_assets/attract'
    bank=export_model_menu(hostfs/'model/gasstand',root/'gasstand','twiddled')
    fontpath=hostfs/'font/alphabet'
    font=texture_bank.export_bank(fontpath/'alphabet_spr.tbl',fontpath/'alphabet_spr.bin.nz',root/'alphabet/textures','twiddled')
    scripts=[]
    for variant in range(3):
        address=u(0x0c2fbf58+variant*8)
        scripts.append([{'speaker':u(address+16*i),'lines':[string(u(address+16*i+j))for j in (4,8,12)],'address':f'{address+16*i:08X}'}for i in range(u(0x0c2fbf5c+variant*8))])
    chars=[string(u(0x0c3786a0+8*i))for i in range(62)]
    widths=[u(0x0c3786a4+8*i)for i in range(62)]
    literals={}
    for address in (0x0c0d0f30,0x0c0d0f34,0x0c0d0f38,0x0c0d095c,0x0c0d1130,0x0c0d1134,0x0c0d1138,0x0c0d113c,0x0c0d114c,0x0c0d1208,0x0c0d1210,0x0c0d1214,0x0c0d13b0,0x0c0d13b4,0x0c0d13b8,0x0c0d13bc,0x0c0d13c0,0x0c0d13c4,0x0c0d13cc,0x0c0d13d0,0x0c0d13d4,0x0c0d13d8,0x0c0d14f0,0x0c0d14f8,0x0c0d14fc,0x0c0d1500,0x0c112e28,0x0c112f88):
        literals[f'{address:08X}']=u(address)
    source=['#pragma once','#include <array>','#include <bit>','#include <string_view>','namespace idas3::original::gasstand_data {','constexpr float word(unsigned u){return std::bit_cast<float>(u);}','struct Message {unsigned speaker;std::array<std::string_view,3> lines;};']
    for variant,rows in enumerate(scripts):
        source.append(f'inline constexpr std::array<Message,{len(rows)}> script{variant}{{{{')
        for row in rows: source.append('{'+str(row['speaker'])+', {'+','.join(json.dumps(s)for s in row['lines'])+'}},')
        source.append('}};')
    source+=['inline constexpr std::array<char,62> fontCharacters{'+','.join(str(ord(c))for c in chars)+'};',
             'inline constexpr std::array<unsigned,62> fontWidthWords{'+','.join(f'0x{x:08x}u'for x in widths)+'};',
             'inline constexpr std::array<unsigned,41> brandOrder{'+','.join(str(u(0x0c2fbeb4+4*i))for i in range(41))+'};',
             'inline constexpr std::array<std::array<float,2>,3> speakerXY{{'+','.join('{'+','.join('word(0x%08xu)'%u(0x0c2fbf70+8*i+4*j)for j in range(2))+'}'for i in range(3))+'}};']
    source += [f'inline constexpr float lit_{a}=word(0x{value:08x}u);'for a,value in literals.items()]
    source += ['}']
    (project/'src/original_gasstand_data.h').write_text('\n'.join(source)+'\n')
    report={'schema':'idas3-original-gasstand-v1','source_image_sha256':CANONICAL,'script_table':'0C2FBF58','private_original_assets':True,'source_draw':'0C0D0F60','scripts':scripts,'font_characters':chars,'font_width_words':widths,'literal_words':literals,'gasstand_chunks':bank['chunk_count'],'gasstand_textures':bank['texture_count'],'alphabet_textures':len(font['textures']),
            'existing_shared_bank':'data/original_assets/menus/v3/v3sA00etc','asset_identity':'Original58-chunk layered2D dialogue art, not a3D gas station model.'}
    (root/'gasstand/scene_provenance.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True)
    a=p.parse_args();r=export(a.image,a.hostfs,a.project);print(f"Exported {r['gasstand_chunks']} original chunks, {r['gasstand_textures']} textures, {r['alphabet_textures']} source font glyphs and3 dialogue scripts")
