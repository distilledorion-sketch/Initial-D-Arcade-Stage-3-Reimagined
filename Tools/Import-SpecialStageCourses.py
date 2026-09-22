"""Decode the three additional Special Stage layouts from a local disc extraction.
No synthesized scenery, collision, road paths or checkpoint positions.

PAC/SMD/GIM fields traced through the extracted SLPM_652.68 executable:
0017c770, 001c5320, 001c54f0, 00175e30, 00175fb0, 0014c480,
0014d550, 0014d1d0, 00164a50, 0015f340.
GS address bit permutations checked against PCSX2 GSTables.cpp (hardware layout).
"""
from pathlib import Path
import struct, math, json, hashlib, zlib, functools
import argparse

def decompress(b):
    # SLPM_652.68: 001c5320 (init), 001c54f0 (decode).
    assert b[:3] == bytes.fromhex('12 3d da')
    size, packed = struct.unpack_from('<II', b, 4)
    if b[3] == 0:
        assert len(b) == size + 12
        return b[12:]
    assert b[3] == 1 and packed == len(b)-12
    ring = bytearray(65536)
    pos, at, flags = 0xfefd, 12, 0
    out = bytearray()
    while len(out) < size:
        flags >>= 1
        if not flags & 0x100:
            flags = b[at] | 0xff00
            at += 1
        if flags & 1:
            values = [b[at]]
            at += 1
            for v in values:
                out.append(v); ring[pos] = v; pos = (pos+1)&65535
        else:
            source, count = struct.unpack_from('<HB', b, at)
            at += 3
            for i in range(count+4):
                v = ring[(source+i)&65535]
                out.append(v); ring[pos] = v; pos = (pos+1)&65535
    assert len(out) == size and at == len(b), (size,len(out),at,len(b))
    return bytes(out)

def pac(path):
    b = path.read_bytes()
    assert b[:4] == b'PAC\0'
    n, _, table = struct.unpack_from('<III', b, 4)
    for i in range(n):
        name, off, size, kind, flags = struct.unpack_from('<16sIIII', b, table+i*32)
        assert off >= table+n*32 and off+size <= len(b)
        data = b[off:off+size]
        yield name.rstrip(b'\0').decode('ascii'), kind, decompress(data) if flags&1 else data



def png(path,w,h,rgba):
    def chunk(t,b):
        return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
    raw=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

def gs_address(x,y,width,psm):
    # Address in bits; pages=8192 bytes, blocks=256 bytes.
    if psm==0:
        pw,ph,bw,bh,bits=64,32,8,8,32
        mapping=((0,),(3,),(1,),(2,),(4,),(5,))
    elif psm==19:
        pw,ph,bw,bh,bits=128,64,16,16,8
        mapping=((5,),(3,),(0,),(4,),(1,),(2,5,6),(6,),(7,))
    elif psm==20:
        pw,ph,bw,bh,bits=128,128,32,16,4
        mapping=((6,),(3,),(4,),(0,),(5,),(1,),(2,6,7),(7,),(8,))
    else:
        raise ValueError(('GS format',psm))
    page=(y//ph)*max(1,(width+pw-1)//pw)+x//pw
    bx,by=(x%pw)//bw,(y%ph)//bh
    if psm==20:
        block=(by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2)
    else:
        block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2)
    xy=(x%bw)|((y%bh)<<(bw-1).bit_length())
    col=sum((sum((xy>>t)&1 for t in terms)%2)<<bit for bit,terms in enumerate(mapping))
    return page*65536+block*2048+col*bits

@functools.lru_cache(maxsize=32)
def texture_permutation(w,h,tw,th,fmt):
    # Simulate the original PSMCT32 transfer, then read as indexed pixels.
    uploaded={gs_address(x,y,tw,0)//8:(y*tw+x)*4 for y in range(th) for x in range(tw)}
    result=[]
    for y in range(h):
        for x in range(w):
            bit=gs_address(x,y,w,fmt)
            byte=bit//8
            assert byte//4*4 in uploaded, ('unwritten GS texel',w,h,tw,th,x,y)
            result.append((uploaded[byte//4*4]+byte%4,bit%8))
    return result

def texture(b,path):
    assert b[:4]==b'GIM\0'
    levels,palettes,regs=struct.unpack_from('<3H',b,32)
    assert levels==1 and palettes in (0,1)
    w,h,fmt,tw,th,transfer,off=struct.unpack_from('<6HI',b,48)
    assert fmt in (0,19,20) and transfer in (0,19,20)
    bits={0:32,19:8,20:4}[transfer]
    n=tw*th*bits//8
    pixels=b[off:off+n]; assert len(pixels)==n
    palette=[]
    if palettes:
        pw,ph,pfmt,_,_,_,po=struct.unpack_from('<6HI',b,64)
        assert pfmt==0 and pw*ph==({19:256,20:16}[fmt])
        for i in range(pw*ph):
            # CSM1 exchanges index bits 3/4 on 256-entry CLUTs.
            j=(i&~24)|((i&8)<<1)|((i&16)>>1) if fmt==19 else i
            r,g,bl,a=struct.unpack_from('<4B',b,po+j*4)
            palette.append(bytes((r,g,bl,min(255,a*2))))
    rgba=bytearray()
    if fmt==0:
        assert transfer==0 and tw==w and th==h
        for r,g,bl,a in struct.iter_unpack('<4B',pixels):rgba.extend((r,g,bl,min(255,a*2)))
    elif fmt==transfer:
        assert w==tw and h==th
        for i in range(w*h):
            index=pixels[i] if fmt==19 else (pixels[i//2]>>((i%2)*4))&15
            rgba.extend(palette[index])
    else:
        assert transfer==0
        for off,shift in texture_permutation(w,h,tw,th,fmt):
            rgba.extend(palette[(pixels[off]>>shift)&(255 if fmt==19 else 15)])
    png(path,w,h,rgba)
    return dict(width=w,height=h,format=fmt,transfer=transfer,registerCount=regs)

def meshes(b):
    assert b[:8]==b'SMD\x000.00'
    declared_tri,declared_vert,nt,nm,to,mo=struct.unpack_from('<6I',b,8)
    assert to==64 and mo==to+nt*16
    names=[b[to+i*16:to+(i+1)*16].split(b'\0')[0].decode('ascii') for i in range(nt)]
    total_vert=total_tri=0
    for m in range(nm):
        start,qwords,tex,_,flags,tri,vert,_=struct.unpack_from('<8I',b,mo+m*32)
        assert tex<len(names) and flags in (0,1) and start+qwords*16<=len(b)
        at=start; positions=[];uvs=[];colors=[];indices=[];source_indices=[];batch={};programs=set()
        while at<start+qwords*16:
            word,=struct.unpack_from('<I',b,at);at+=4
            cmd,num,imm=word>>24&127,word>>16&255,word&65535
            if cmd==0x6c and imm==0x8001:
                batch['positions']=list(struct.iter_unpack('<3fI',b[at:at+num*16]));at+=num*16
            elif cmd==0x74 and imm==0x8002:
                batch['uv']=list(struct.iter_unpack('<2f',b[at:at+num*8]));at+=num*8
            elif cmd==0x6e and imm==0xc003:
                batch['colors']=[tuple(b[at+i*4:at+i*4+4]) for i in range(num)];at+=num*4
            elif cmd==0x6c and imm==0x8000:
                assert num==1;at+=16
            elif cmd==0x20:
                at+=4
            elif cmd in (0,1,5):
                pass
            elif cmd==0x14:
                programs.add(imm)
                p,u,c=batch['positions'],batch['uv'],batch['colors']
                assert len(p)==len(u)==len(c)
                base=len(positions)
                for i,(x,y,z,adc) in enumerate(p):
                    assert all(math.isfinite(v) for v in (x,y,z,*u[i]))
                    # Source V=0 is at the base of the tree cards; V=-1 at top.
                    positions.append((x,y,z));uvs.append((u[i][0],-u[i][1]));colors.append(c[i])
                    if not adc&0x8000:
                        assert i>=2
                        # Keep two-sided strips as drawn. Paired skins below
                        # use the explicit ADC winding for facing rejection.
                        forward=i%2==0
                        indices.extend((base+i-2,base+i-1,base+i) if forward else (base+i-1,base+i-2,base+i))
                        source_indices.extend((base+i-2,base+i-1,base+i) if adc&4 else (base+i-1,base+i-2,base+i))
                batch={}
            else:
                raise ValueError(('Unsupported VIF',hex(cmd),hex(imm),at-4))
        assert at==start+qwords*16 and len(positions)==vert and len(indices)//3==tri
        total_vert+=vert;total_tri+=tri
        yield dict(flags=flags,texture=names[tex],positions=positions,uv=uvs,colors=colors,indices=indices,sourceIndices=source_indices,programs=sorted(programs))
    assert total_vert==declared_vert and total_tri==declared_tri


# The two longer PS2 routes are distinct from D3's short Myogi/Usui circuits.
# Source IDs are from SLPM_652.68's course table at 0x24cd00.
COURSES = (
    dict(id=12, sourceId=6, source='MYOUGI', slug='myogi_special', folder='MYOGI_SPECIAL', title='Myogi (Special Stage)', donor='Shomaru', condition=12),
    dict(id=13, sourceId=7, source='USUI', slug='usui_special', folder='USUI_SPECIAL', title='Usui (Special Stage)', donor='Happogahara', condition=8),
    dict(id=14, sourceId=8, source='MOMIJI', slug='momiji', folder='MOMIJI', title='Momiji Line', donor='Akagi', condition=4),
)


def scenery_sections(cif, model_names, path_points):
    """CIF4 night geometry windows, consumed by SLPM_652.68 00180f10.

    00162420 selects the second (night) table; 00180210 attaches each
    28-byte record to its crsNN model. Offsets 12/16 are inclusive source
    path limits, independent of driving direction (00161c90/00163210).
    """
    count=struct.unpack_from('<I',cif,0x2c0)[0]
    table=0x2c4+(count+1)*2+count*28
    assert table+count*28==len(cif)
    result=[]
    for i in range(count):
        number,start,end,visible_start,visible_end,gate,persistent=struct.unpack_from('<7i',cif,table+i*28)
        name=f'crs{number:02d}'
        assert number==i and name in model_names
        assert 0<=visible_start<=start<=end<=visible_end<=path_points
        assert persistent==0, 'Persistent scenery needs a separate visibility policy'
        result.append(dict(model=name,firstNode=visible_start,lastNode=visible_end))
    return result


def separate_paired_skins(models):
    """Cull only proven, coincident opposite faces; leave other scenery two-sided.

    Matching a quad also handles front/back skins with opposite diagonals.
    Winding comes from the authored strip control, not batch vertex parity.
    ENN2 part bit1 selects the existing world-space facing test in Unity.
    """
    from collections import defaultdict
    def normal(points):
        a,b,c=points;u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
        return (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
    pairs=0
    for name,parts in models:
        if not (name.startswith('gate') or name.startswith('crs') and not name.startswith('crslod')):continue
        groups=defaultdict(list);paired=[set() for _ in parts]
        for p,part in enumerate(parts):
            ps,indices=part['positions'],part['sourceIndices']
            for i in range(0,len(indices)-3,6):
                points=tuple(sorted(set(ps[j] for j in indices[i:i+6])))
                if len(points)!=4:continue
                n=normal([ps[j] for j in indices[i:i+3]])
                groups[points].append((p,i,n))
        for group in groups.values():
            if len(group)!=2:continue
            a,b=group
            if sum(x*y for x,y in zip(a[2],b[2]))>=-1e-12:continue
            for p,i,_ in group:paired[p].update((i,i+3))
            pairs+=1
        new=[]
        for part,selected in zip(parts,paired):
            if not selected:new.append(part);continue
            for cull in (False,True):
                indices=[j for i in range(0,len(part['indices']),3) if (i in selected)==cull
                    for j in (part['sourceIndices'] if cull else part['indices'])[i:i+3]]
                if not indices:continue
                used=list(dict.fromkeys(indices));remap={j:i for i,j in enumerate(used)}
                p=dict(part);p['flags']|=2 if cull else 0
                for key in ('positions','uv','colors'):p[key]=[part[key][i] for i in used]
                p['indices']=[remap[j] for j in indices];new.append(p)
        parts[:]=new
    return pairs


def export(root, out, course, native):
    out.mkdir(parents=True, exist_ok=True)
    (out/'textures').mkdir(exist_ok=True)
    source=root/'COURSE'/f"{course['source']}_NIT.PAC"
    crs=root/'CRS_DATA'
    report=dict(courseId=course['id'], title=course['title'], source=source.name,
        sourceId=course['sourceId'], sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        textures={}, models=[], geometry=course['slug']+'.bin')
    models=[]
    for name, kind, b in pac(source):
        if kind==1:
            report['textures'][name]=texture(b,out/'textures'/f'{name}.png')
        elif kind==3:
            parts=list(meshes(b)); models.append((name,parts))
            report['models'].append(dict(name=name,parts=len(parts),
                vertices=sum(len(p['positions']) for p in parts),
                triangles=sum(len(p['indices'])//3 for p in parts),
                noDepthWriteParts=[i for i,p in enumerate(parts) if p['flags']&1]))
        else: raise ValueError(('Unknown PAC kind',kind))
    textures=list(report['textures']); texindex={n:i for i,n in enumerate(textures)}
    report['textureNames']=textures
    report['oneSidedTextures']=[]
    report['pairedSkinQuads']=separate_paired_skins(models)
    # ENN2 retains the SMD part's depth-write mask. Source 00176070 calls
    # 0014e2b0 for bit0: GS ZBUF_2.ZMSK=1 (00148cc0), not a hidden mesh.
    with (out/report['geometry']).open('wb') as f:
        f.write(b'ENN2'); f.write(struct.pack('<I',len(models)))
        def string(s):
            b=s.encode('utf8'); f.write(struct.pack('<I',len(b))); f.write(b)
        for name,parts in models:
            string(name); f.write(struct.pack('<I',len(parts)))
            for part in parts:
                f.write(struct.pack('<4I',texindex[part['texture']],len(part['positions']),len(part['indices']),part['flags']))
                for pos,uv,col in zip(part['positions'],part['uv'],part['colors']):
                    f.write(struct.pack('<5f4B',*pos,*uv,*col))
                f.write(struct.pack('<'+str(len(part['indices']))+'I',*part['indices']))
    roads=[]
    for suffix in ('','_L','_R'):
        b=(crs/f"CRS_ROAD_{course['source']}{suffix}.BIN").read_bytes()
        n,res=struct.unpack_from('<II',b); assert res==0 and len(b)==8+n*12
        roads.append(list(struct.iter_unpack('<3f',b[8:])))
        # Only the header changes: D3's reader expects the component count.
        (out/f"{course['slug']}_path{suffix.lower()}.bin").write_bytes(struct.pack('<II',n,3)+b[8:])
    assert len(set(map(len,roads)))==1
    (out/'course.id').write_text(str(course['id'])+'\n')
    report['handling']=dict(source='Arcade Stage 3 '+course['donor'],
        forwardCondition=course['condition'], reverseCondition=course['condition']+1,
        weather='D3 dry/wet tables')
    report['timerPolicy']='D3 donor normal-difficulty allowances, scaled up per section where the new route is longer; no shortened donor allowance.'
    report['pathPoints']=len(roads[0])
    report['sourcePathLengthMeters']=sum(math.dist(a,b) for a,b in zip(roads[0],roads[0][1:]))
    cif=(crs/f"CRS_INFO_{course['source']}.BIN").read_bytes()
    assert cif[:8]==b'CIF\0\0\0\4\0' and struct.unpack_from('<I',cif,24)[0]==1
    route_base=0x240
    # CIF4's recovered night slots, including the separate wet sky/fog.
    for key,ambient,direction,color,start,end,fog,sky in (
        ('lighting',0x1c0,0x80,0x140,0x1e8,0x1f8,0x220,'sky00'),
        ('wetLighting',0x1d0,0xb0,0x170,0x1ec,0x1fc,0x230,'sky01')):
        report[key]=dict(ambient=list(struct.unpack_from('<3f',cif,ambient)),
            direction=list(struct.unpack_from('<3f',cif,direction)),directionalColor=list(struct.unpack_from('<3f',cif,color)),
            fogStart=struct.unpack_from('<i',cif,start)[0],fogEnd=struct.unpack_from('<i',cif,end)[0],
            fogColor=[x/255 for x in struct.unpack_from('<3f',cif,fog)],sky=sky,skyFollowsCameraXZ=True)
        assert report[key]['fogEnd']>report[key]['fogStart']
    chunks=struct.unpack_from('<I',cif,route_base+0x80)[0]
    table=route_base+0x84+(chunks+1)*2+chunks*28
    if course['source']=='USUI':
        # crs14 contains a deep forest backdrop that intersects a later bend.
        # Preserve its authored geometry, but only draw it in its source window.
        report['scenerySections']=scenery_sections(cif,{n for n,_ in models},len(roads[0]))
    checkpoint_gates=sorted({struct.unpack_from('<i',cif,table+i*28+20)[0] for i in range(chunks)}-{-1})
    assert len(checkpoint_gates)==3
    report['gateSets']=[dict(direction=d,models=[f'gate{x:02d}' for x in [d*2,d*2+1,*checkpoint_gates]]) for d in (0,1)]
    report['raceMarkers']=[]
    for direction in (0,1):
        start=struct.unpack_from('<h',cif,route_base+0x60+2*direction)[0]
        goal=struct.unpack_from('<h',cif,route_base+0x64+2*direction)[0]
        sections=[x for x in struct.unpack_from('<6h',cif,route_base+0x68+12*direction) if x!=-1]
        assert len(sections)==3 and start<sections[0]<sections[1]<sections[2]<goal<len(roads[0])
        source_indices=[len(roads[0])-1-x if direction else x for x in [start,*sections,goal]]
        def distance(a,z):
            a,z=sorted((a,z)); return sum(math.dist(roads[0][i],roads[0][i+1]) for i in range(a,z))
        report['raceMarkers'].append(dict(direction=direction,startIndex=start,goalIndex=goal,checkpointIndices=sections,
            sourceIndices=source_indices,timedLengthMeters=distance(source_indices[0],source_indices[-1]),
            sectionLengthsMeters=[distance(a,z) for a,z in zip(source_indices,source_indices[1:])],
            gridPositions=[list(struct.unpack_from('<3f',cif,route_base+direction*32+i*16)) for i in range(2)],
            sourceRotationRecord=list(struct.unpack_from('<4f',cif,route_base+64+direction*16))))
    (out/'race-markers.bin').write_bytes(b'ENR1'+b''.join(struct.pack('<5i',r['startIndex'],*r['checkpointIndices'],r['goalIndex']) for r in report['raceMarkers']))
    # Compare actual D3 timed sections, not the longer approach/runout path.
    import re
    table=(native/'src/original_race_rules_data.inc').read_text().split('raceRows{{',1)[1].split('}};',1)[0]
    rows=[[int(n) for n in re.findall(r'-?\d+',line)] for line in table.splitlines() if line.strip().startswith('{')]
    donor_slug={12:'n_sy',8:'s_vh',4:'h_hd'}[course['condition']]
    path=(native/f'data/courses/{donor_slug}_path.bin').read_bytes()
    n,components=struct.unpack_from('<II',path);assert components==3 and len(path)==8+n*12
    donor=list(struct.iter_unpack('<3f',path[8:]));scales=[]
    for d,markers in enumerate(report['raceMarkers']):
        row=rows[(course['condition']//2)*3+d];assert len(row)==18
        start,goal=row[:2];indices=[start,*[start+x for x in row[8:11]],start+goal]
        if d:indices=[n-1-i for i in indices]
        lengths=[]
        for a,z in zip(indices,indices[1:]):
            a,z=sorted((a,z));assert 0<=a<z<n
            lengths.append(sum(math.dist(donor[i],donor[i+1]) for i in range(a,z)))
        scale=[max(1.0,a/b) for a,b in zip(markers['sectionLengthsMeters'],lengths)]
        assert all(1<=x<=4 for x in scale)
        markers['donorSectionLengthsMeters']=lengths;markers['timerScale']=scale;scales.extend(scale)
    (out/'timer-scale.bin').write_bytes(b'TSF1'+struct.pack('<8f',*scales))
    (out/'road.bin').write_bytes(b'HKR1'+struct.pack('<I',len(roads[0]))+b''.join(struct.pack('<3f',*p) for road in roads for p in road))
    model_names={n for n,_ in models}; trees=[]
    for size in ('L','M'):
        for side in ('L','R'):
            tree_file=crs/f"TREE_{size}_{course['source']}_{side}.BIN"
            # The long Usui PAC has no medium-tree placement files.
            if course['source']=='USUI' and size=='M':
                assert not tree_file.exists()
                continue
            b=tree_file.read_bytes()
            count,=struct.unpack_from('<I',b); assert len(b)==16+64*count
            for i in range(count):
                at=16+64*i; prefix='treeLrg' if size=='L' else 'treeMid'
                name=f'{prefix}_{side}{b[at]:02d}'; assert name in model_names,name
                trees.append(dict(model=name,position=list(struct.unpack_from('<3f',b,at+16)),
                    rotation=list(struct.unpack_from('<3f',b,at+32)),scale=list(struct.unpack_from('<3f',b,at+48))))
    report['trees']=trees
    gallery=(crs/f"GALLERY_{course['source']}.BIN").read_bytes();count=struct.unpack_from('<I',gallery)[0]
    assert len(gallery)==16+count*16
    # 00165430 indexes the count table at 0x24ce40 by physical course ID.
    # IDs6,7,8 each cycle ten sprites, even where the PAC has extra gal images.
    gallery_names=[f'gal{i:02d}' for i in range(10)]
    assert all(n in textures for n in gallery_names)
    report['spectators']=[dict(texture=gallery_names[i%10],position=list(struct.unpack_from('<3f',gallery,16+i*16)),
        height=2.0,width=2.5*report['textures'][gallery_names[i%10]]['height']/report['textures'][gallery_names[i%10]]['width']) for i in range(count)]
    for direction in (0,1):
        b=(crs/f"CRS_COLI_{course['source']}_{direction}.BIN").read_bytes();h=struct.unpack_from('<12I',b)
        assert h[:2]==(0x52434c31,1) and h[8]==0
        cursor=48
        for cnt,off,stride in [(h[2],h[3],36),(h[4],h[5],32),(h[6],h[7],16),(h[10],h[11],56)]:
            assert off==cursor;cursor+=cnt*stride
        assert cursor==len(b)
        for row in struct.iter_unpack('<8h',b[h[7]:h[9]]): assert all(0<=v<h[4] for v in row[:3])
        (out/f'collision-{direction}.rcl').write_bytes(b)
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(course['title'],len(models),'models,',len(textures),'textures,',len(trees),'trees,',count,'spectators')
    return models,report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True,help='Local extracted DATA/COURSE containing COURSE and CRS_DATA')
    parser.add_argument('--output',type=Path,required=True,help='RuntimeAssets destination')
    parser.add_argument('--native-root',type=Path,default=Path(__file__).resolve().parents[1]/'Native')
    args=parser.parse_args()
    for course in COURSES: export(args.source,args.output/course['folder'],course,args.native_root)
