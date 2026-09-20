"""Decode the five original day-rain skies using their retail program palettes."""
import hashlib,json,struct
from pathlib import Path
from texture_bank import texel_index,write_png,write_native_pack
from export_original_assembly import IMAGE_HASH

COURSE_PALETTES={0:20,1:21,2:22,3:23,5:24}

def export_palette_bank(table:Path,payload:Path,out:Path,image:Path,palette_id:int):
    program=image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical program palette required')
    metadata=table.read_bytes();raw=payload.read_bytes()
    if len(metadata)!=16:raise ValueError('Only the proven single-texture course sky format is supported')
    w,h,fmt,flags,reserved,offset,tail=struct.unpack('<HHBBHII',metadata)
    if (w,h,fmt,flags,reserved,offset,tail)!=(512,256,7,7,0,0,0) or len(raw)!=w*h:
        raise ValueError('Original day-rain palette texture schema changed')
    if palette_id not in COURSE_PALETTES.values():raise ValueError('Unproven course palette selector')
    palette_table=struct.unpack_from('<I',program,0xc191aa8-0xc020000)[0]
    address=struct.unpack_from('<I',program,palette_table-0xc020000+palette_id*4)[0]
    palette=program[address-0xc020000:address-0xc020000+1024]
    colors=struct.unpack('<256I',palette)
    rgba=bytearray(w*h*4)
    for y in range(h):
        for x in range(w):
            index=raw[texel_index(x,y,w,h,'twiddled')]
            argb=colors[index];target=(y*w+x)*4
            rgba[target:target+4]=bytes(((argb>>16)&255,(argb>>8)&255,argb&255,argb>>24))
            r,g,b,a=rgba[target:target+4]
            if (a<<24|r<<16|g<<8|b)!=argb:raise AssertionError('Palette channel roundtrip failed')
    out.mkdir(parents=True,exist_ok=True)
    write_native_pack(out/'textures.idastex',[{'index':0,'width':w,'height':h,'rgba':rgba}])
    write_png(out/'texture_000.png',w,h,rgba)
    (out/'original_indices.pal8').write_bytes(raw);(out/'original_palette.argb32').write_bytes(palette)
    manifest={'schema':'idas3-original-course-pal8-v1','source_image_sha256':IMAGE_HASH,
      'table':str(table.resolve()),'payload':str(payload.resolve()),'table_sha256':hashlib.sha256(metadata).hexdigest(),
      'payload_sha256':hashlib.sha256(raw).hexdigest(),'palette_id':palette_id,'palette_address':f'{address:08X}',
      'palette_sha256':hashlib.sha256(palette).hexdigest(),'palette_format':'ARGB8888',
      'loader_evidence':'2136A0 source0707 produces PAL8 format30000000,size131072; 1F7420 produces TSP53/TCW30000000 (twiddled).',
      'palette_evidence':'Course night constructor calls191A40(id,0); it selects33B898[id], sets ARGB8888, and uploads256 words to palette bank0 through1F618C.',
      'validation':'All output pixels map their unchanged source8-bit index through the exact original32-bit palette; channel roundtrip checked. Original indices and palette retained.',
      'textures':[{'index':0,'width':w,'height':h,'format':fmt,'flags':flags,'file':'texture_000.png','rgba_sha256':hashlib.sha256(rgba).hexdigest()}]}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest
