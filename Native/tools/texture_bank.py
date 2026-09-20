#!/usr/bin/env python3
"""Decode original direct-16-bit texture banks, preserving texels and provenance.

Layout is explicit: a .tbl record's flags alone do not establish the submitted
PVR scan-order state. Use the corresponding original material/loader evidence.
No resampling, replacement artwork, AI-generated pixels or alpha invention.
"""
from __future__ import annotations
import argparse, hashlib, json, math, struct, zlib
from pathlib import Path

def morton(x: int,y: int,size: int) -> int:
    result=0
    for bit in range(size.bit_length()-1):
        result|=((y>>bit)&1)<<(2*bit)
        result|=((x>>bit)&1)<<(2*bit+1)
    return result

def texel_index(x: int,y: int,width: int,height: int,layout: str) -> int:
    if layout=='linear':return y*width+x
    if layout!='twiddled':raise ValueError('Unknown texture layout')
    block=min(width,height)
    block_number=(y//block)*(width//block)+(x//block)
    return block_number*block*block+morton(x%block,y%block,block)

def expand(value: int,fmt: int) -> tuple[int,int,int,int]:
    e5=lambda x:(x<<3)|(x>>2)
    e6=lambda x:(x<<2)|(x>>4)
    if fmt==0:return e5((value>>10)&31),e5((value>>5)&31),e5(value&31),255 if value&0x8000 else 0
    if fmt==1:return e5((value>>11)&31),e6((value>>5)&63),e5(value&31),255
    if fmt==2:return ((value>>8)&15)*17,((value>>4)&15)*17,(value&15)*17,((value>>12)&15)*17
    raise ValueError(f'Unsupported direct texture format {fmt}; palette/VQ not guessed')

def pack_pixel(rgba: bytes|tuple[int,int,int,int],fmt:int)->int:
    r,g,b,a=rgba
    if fmt==0:return (int(a>0)<<15)|((r>>3)<<10)|((g>>3)<<5)|(b>>3)
    if fmt==1:return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
    if fmt==2:return ((a>>4)<<12)|((r>>4)<<8)|((g>>4)<<4)|(b>>4)
    raise ValueError('Unsupported format')

def decode(raw:bytes,width:int,height:int,fmt:int,layout:str)->bytes:
    if any(v<8 or v>2048 or v&(v-1) for v in (width,height)):
        raise ValueError('Invalid texture dimensions')
    if len(raw)!=width*height*2:raise ValueError('Unexpected direct texture byte length')
    out=bytearray(width*height*4)
    for y in range(height):
        for x in range(width):
            source=texel_index(x,y,width,height,layout)*2
            px=struct.unpack_from('<H',raw,source)[0]
            dest=(y*width+x)*4
            out[dest:dest+4]=bytes(expand(px,fmt))
            # Exact inverse confirms all original 16-bit channel information,
            # including hidden RGB under alpha=0, survives export.
            if pack_pixel(out[dest:dest+4],fmt)!=px:raise AssertionError('Pixel roundtrip failed')
    return bytes(out)

def decode_vq(raw:bytes,width:int,height:int,fmt:int)->bytes:
    """Vector-quantised texels: a 256-entry codebook of 2x2 blocks, then one
    index byte per block in the same twiddled order the direct path uses.

    The flag byte above the format is 3 for these. Verified against
    model/navi/navi_df, whose 512x512 block decodes to that course's own
    navigation line; 2048 + (w/2)*(h/2) is exactly the payload length."""
    blocks=(width//2)*(height//2)
    if len(raw)!=2048+blocks:raise ValueError('Unexpected VQ texture byte length')
    book=raw[:2048];indices=raw[2048:]
    out=bytearray(width*height*4)
    bw,bh=width//2,height//2
    for by in range(bh):
        for bx in range(bw):
            entry=indices[texel_index(bx,by,bw,bh,'twiddled')]
            texels=struct.unpack_from('<4H',book,entry*8)
            # Within a block the codebook holds the same twiddled order.
            for k,(dx,dy) in enumerate(((0,0),(0,1),(1,0),(1,1))):
                dest=((by*2+dy)*width+(bx*2+dx))*4
                out[dest:dest+4]=bytes(expand(texels[k],fmt))
    return bytes(out)

def write_png(path:Path,width:int,height:int,rgba:bytes):
    def chunk(kind:bytes,payload:bytes)->bytes:
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
    rows=b''.join(b'\x00'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')
    path.write_bytes(data)

def write_native_pack(path:Path,records:list[dict]):
    """Bounded native RGBA asset stream; no console texture hardware required."""
    with path.open('wb') as output:
        output.write(b'IDAS3T1\0'+struct.pack('<II',1,len(records)))
        for record in records:
            rgba=record['rgba']
            if len(rgba)!=record['width']*record['height']*4:raise ValueError('RGBA byte size mismatch')
            output.write(struct.pack('<4I',record['index'],record['width'],record['height'],len(rgba)))
            output.write(rgba)

def export_bank(table:Path,payload:Path,out:Path,layout:str)->dict:
    metadata=table.read_bytes();raw=payload.read_bytes()
    if not metadata or len(metadata)%16:raise ValueError('Invalid texture table record length')
    records=[]
    for i in range(len(metadata)//16):
        w,h,fmt,flags,reserved,offset,tail=struct.unpack_from('<HHBBHII',metadata,i*16)
        end=offset+w*h*2
        if offset>len(raw) or end>len(raw):raise ValueError(f'Texture {i} exceeds bank payload')
        image=decode(raw[offset:end],w,h,fmt,layout)
        records.append({'index':i,'width':w,'height':h,'format':fmt,'flags':flags,'reserved':reserved,'offset':offset,'tail':tail,'layout':layout,'source_texels_sha256':hashlib.sha256(raw[offset:end]).hexdigest(),'rgba_sha256':hashlib.sha256(image).hexdigest(),'rgba':image})
    out.mkdir(parents=True,exist_ok=True)
    write_native_pack(out/'textures.idastex',records)
    for record in records:
        name=f'texture_{record["index"]:03d}.png';write_png(out/name,record['width'],record['height'],record.pop('rgba'));record['file']=name
    manifest={'schema':'idas3-direct-textures-v1','table':str(table.resolve()),'payload':str(payload.resolve()),'table_sha256':hashlib.sha256(metadata).hexdigest(),'payload_sha256':hashlib.sha256(raw).hexdigest(),'pixel_roundtrip':'All original 16-bit texels verified by exact decode/repack','layout_evidence':'Explicit layout parameter; verify material TCW scan-order before binding.','private_original_assets':True,'textures':records}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--table',type=Path,required=True);p.add_argument('--payload',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--layout',choices=['twiddled','linear'],required=True)
    args=p.parse_args();manifest=export_bank(args.table,args.payload,args.out,args.layout)
    print(f'Exported {len(manifest["textures"])} original textures; texel roundtrip verified. Material layout still requires matching scan-order evidence.')
