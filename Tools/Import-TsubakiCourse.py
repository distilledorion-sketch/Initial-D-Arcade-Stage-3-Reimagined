from pathlib import Path
import struct,json,math,hashlib,re,sys
from stage8_yabx import load,value,string
from tsubaki_fork import export_island
import argparse,subprocess,mmap
parser=argparse.ArgumentParser(description="Import Tsubaki Line scenery and authored road from a local Arcade Stage 8 installation")
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--work',type=Path,required=True)
parser.add_argument('--decoder',type=Path,required=True)
parser.add_argument('--variant',choices=('day_dry','day_wet','night_dry','night_wet'),default='day_dry')
args=parser.parse_args()
variant=args.variant
DEST=args.output if variant=='day_dry' else args.output/variant
DEST.mkdir(parents=True,exist_ok=True)
ROOT=args.source/'data/COURSE/tsubaki'
extracted=args.work/'extracted'/variant
extracted.mkdir(parents=True,exist_ok=True)

def extract(names,dest):
    report=[]
    with (args.source/'data/COURSE.xaf').open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as bank:
        assert bank[:4]==b'xaf0'
        entries={}
        for i in range(struct.unpack_from('<I',bank,12)[0]):
            offset=256+i*176
            name=bank[offset:offset+128].split(b'\0')[0].decode('ascii')
            if name in names:entries.setdefault(name,[]).append(struct.unpack_from('<12I',bank,offset+128))
        for name in names:
            assert Path(name).name==name and len(entries.get(name,[]))==1,name
            flag,parent,sibling,child,_,size,packed,_,sector,*_=entries[name][0]
            assert flag in (1,257) and sector*2048+packed<=len(bank)
            data=bank[sector*2048:sector*2048+packed];target=dest/name
            if flag==257:
                temporary=dest/(name+'.packed');temporary.write_bytes(data)
                subprocess.run([str(args.decoder),str(args.source),str(temporary),str(target),'0',str(size)],check=True)
            else:target.write_bytes(data)
            assert target.stat().st_size==size
            report.append(dict(name=name,size=size,packed=packed,sector=sector,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    (args.work/('extraction-'+variant+'.json')).write_text(json.dumps(report,indent=2))

def section(path,name):
    text=path.read_text(encoding='cp932')
    match=re.search(r'^\[\s*'+re.escape(name)+r'\s*\]\s*$(.*?)(?=^\[|\Z)',text,re.M|re.S)
    assert match, (path,name)
    return dict(re.findall(r'^\s*(\w+)\s*=\s*([^;\r\n]+)',match.group(1),re.M))
def refs(b):
    n=struct.unpack_from('<I',b)[0];assert len(b)==4+n*2
    return [v-10001 for v in struct.unpack_from('<'+'H'*n,b,4)]
def ref(o,k):return value(o,k,'H')-10001
def arr(b,fmt):
    n=struct.unpack_from('<I',b)[0];assert len(b)==4+n*struct.calcsize(fmt)
    return list(struct.iter_unpack('<'+fmt,b[4:]))
course=section(ROOT.parent/'COURSE_DATA.ini','Tsubaki')
condition=section(ROOT.parent/'COURSE_DATA.ini','Tsubaki_'+variant.replace('day','Day').replace('night','Night').replace('dry','Dry').replace('wet','Rain'))
assetKeys=dict(crs_a='ModelData',crs_m='MountainModelData',sky_a='DetailedSkyModelData',tree='TreeModelData',gallery='GalleryModel')
files={part:Path(condition[key].strip()).name for part,key in assetKeys.items()}
extract(list(dict.fromkeys(files.values())),extracted)
materials=[];shapes=[];textures={};templates={};instances=[]
for part in ['crs_a','crs_m','sky_a','tree','gallery']:
    filename=files[part]
    objects=load(extracted/filename);verts={};states={}
    for i,o in enumerate(objects):
        if o['type']=='_sState':
            tx=[]
            for index in refs(o['fields']['texture']):
                t=objects[index];im=objects[ref(t,'textureImage')]['fields']['file']
                assert im[:4]==b'DDS '
                file=hashlib.sha256(im).hexdigest()[:20]+'.dds'
                if file not in textures:(DEST/file).write_bytes(im);textures[file]=len(im)
                tx.append(dict(file=file,uv=value(t,'uvSetIndex','B'),type=value(t,'textureType','B')))
            states[i]=len(materials);materials.append(dict(name=string(o),textures=tx,cutoff=value(o,'alphaRef','B')/255,
                shadow='shadow' in string(o).lower(),sky=part=='sky_a',diffuse=value(o,'diffuse'),
                blendSrc=value(o,'blendSrc','B'),blendDst=value(o,'blendDst','B'),kind=part))
        if o['type']=='_sGeometry':
            count=value(o,'vertexNumber');stride=value(o,'strideSize');a=objects[ref(o,'vertexArray')]
            data=a['fields']['array'];assert struct.unpack_from('<I',data)[0]==count and len(data)==4+count*stride
            layout=a['type'].replace('_sVertexArray','');out=[]
            for j in range(count):
                v=data[4+j*stride:4+(j+1)*stride];pos=struct.unpack_from('<3f',v);cursor=12
                assert all(math.isfinite(x) and abs(x)<100000 for x in pos)
                normal=struct.unpack_from('<3f',v,cursor) if 'N' in layout else (0,0,0)
                if 'N' in layout:cursor+=12
                color=v[cursor:cursor+4] if 'C' in layout else b'\xff'*4
                if 'C' in layout:cursor+=4
                uv=struct.unpack_from('<2f',v,cursor) if 'T' in layout else (0,0);cursor+=8 if 'T' in layout else 0
                uv2=struct.unpack_from('<2f',v,cursor) if '2' in layout else uv
                out.append(struct.pack('<3f3f2f2f4B',*pos,*normal,*uv,*uv2,*color))
            verts[i]=out
    transforms={ref(o,'shapeHeader'):struct.unpack('<12f',o['fields']['mtxLocal']) for o in objects if o['type']=='_sBone' and ref(o,'shapeHeader')>=0}
    for hi,h in enumerate(objects):
        if h['type']!='_sShapeHeader':continue
        matrix=transforms.get(hi,(1,0,0,0,0,1,0,0,0,0,1,0))
        if part in ['tree','gallery']:templates[(part,string(h))]=[]
        for si in refs(h['fields']['shape']):
            sh=objects[si];dl=objects[ref(sh,'displayList')];geometry=ref(dl,'geometry');source=verts[geometry]
            owner=dl
            while ref(owner,'displayListRef')>=0:owner=objects[ref(owner,'displayListRef')]
            indexbytes=owner['fields']['index'];indexcount=struct.unpack_from('<I',indexbytes)[0];tri=[]
            assert len(indexbytes)==4+indexcount*2
            for pi in refs(dl['fields']['primitiveList']):
                p=objects[pi];start=value(p,'indexStart');n=value(p,'indexNumber');mode=value(p,'primitiveType')
                assert start+n<=indexcount
                seq=struct.unpack_from('<'+'H'*n,indexbytes,4+start*2)
                assert len(seq)==n and max(seq,default=0)<len(source),(part,si,start,n,indexcount,len(source))
                if mode==4:
                    for j in range(2,len(seq)):
                        t=[seq[j-2],seq[j-1],seq[j]]
                        if j%2:t[0],t[1]=t[1],t[0]
                        if len(set(t))==3:tri.extend(t)
                elif mode==3:tri.extend(seq)
                else:raise ValueError(('primitive',mode))
            used=sorted(set(tri));remap={v:i for i,v in enumerate(used)}
            if part in ['tree','gallery']:templates[(part,string(h))].append(len(shapes))
            shapes.append((states[ref(sh,'state')],matrix,[source[v] for v in used],[remap[v] for v in tri]))
    print(part,'shapes',len(shapes),'materials',len(materials),flush=True)
for kind in ['tree','gallery']:
    records=[]
    for key,relative in course.items():
        if re.fullmatch(('TreePath' if kind=='tree' else 'GalleryPath')+r'\d+',key):
            b=(args.source/relative.strip()).read_bytes();count=struct.unpack_from('<I',b,12)[0]
            assert b[:4]==b'Path' and len(b)==32+48*count
            records.extend(struct.iter_unpack('<12f',b[32:]))
    count=len(records)
    for record in records:
        assert all(math.isfinite(v) for v in record)
        instanceVariant=int(record[3]);assert record[3]==instanceVariant
        names=[]
        for lod in ('abc' if kind=='tree' else 'a'):
            matches=[name for part,name in templates if part==kind and re.match(r'^'+lod+f'{instanceVariant:02d}'+r'(?:_|$)',name)]
            assert len(matches)==1,(kind,instanceVariant,lod,matches)
            names.append(matches[0])
        lods=[templates[(kind,name)] for name in names]
        assert all(len(lod)==1 for lod in lods), 'Multi-material instances need grouped LODs'
        instances.append(dict(kind=kind,meshes=[lod[0] for lod in lods],position=record[:3],forward=record[4:7],angles=record[8:11],scale=record[11]))
    print(kind,'placements',count)
with (DEST/'scene.bin').open('wb') as f:
    f.write(b'HKN2');f.write(struct.pack('<I',len(shapes)))
    for mat,matrix,v,t in shapes:
        f.write(struct.pack('<III12f',mat,len(v),len(t),*matrix));f.write(b''.join(v));f.write(struct.pack('<'+'I'*len(t),*t))
paths=[]
for side in ['c','l','r']:
    b=(ROOT/'path'/f'tsubaki_path_road_{side}.pa4').read_bytes();count=struct.unpack_from('<I',b,12)[0]
    assert b[:4]==b'Path' and len(b)==32+count*16
    paths.append([x[:3] for x in struct.iter_unpack('<4f',b[32:])])
assert len(set(map(len,paths)))==1
# The original files prepend ten disconnected control points, over 2 km
# away from the actual approach. Keep the entire connected race/approach
# and remap its source indices together instead of bridging that gap.
pathOffset=10
assert math.dist(paths[0][9],paths[0][10])>1000
assert int(course['Checkpoint0'])>pathOffset
assert all(math.dist(road[i-1],road[i])<1000 for road in paths for i in range(pathOffset+1,len(road)))
paths=[road[pathOffset:] for road in paths]
with (DEST/'road.bin').open('wb') as f:
    f.write(b'HKR1');f.write(struct.pack('<I',len(paths[0])))
    for path in paths:
        for p in path:f.write(struct.pack('<3f',*p))
course=section(ROOT.parent/'COURSE_DATA.ini','Tsubaki')
weather='Rain' if variant.endswith('wet') else 'Dry'
forward=section(ROOT.parents[1]/'GAME/GAME_SETTING.ini',f'Tsubaki_{weather}_Forward')
backward=section(ROOT.parents[1]/'GAME/GAME_SETTING.ini',f'Tsubaki_{weather}_Backward')
times=[int(x) for x in forward['TimeTableTA'].split(',')]
assert times==[int(x) for x in backward['TimeTableTA'].split(',')], 'Separate direction timer tables required'
checkpoints=[int(float(course['Checkpoint'+str(i)].strip().rstrip('f')))-pathOffset for i in range(5)]
env=load(ROOT/'env'/Path(condition['ShaderConfig'].strip()).name)
profiles=[];events=[]
for obj in env:
    if obj['type']=='cShaderConfig':
        profiles.append(dict(name=string(obj,'sName'),sunDirection=struct.unpack('<3f',obj['fields']['v3CourseSunDirection']),
            fogColor=struct.unpack('<3f',obj['fields']['v3CourseMieColor']),distanceScale=value(obj,'fCourseDistanceScale','f')))
    elif obj['type']=='cPathEvent':
        events.append(dict(points=[dict(point=max(0,p-pathOffset),profile=i) for p,i in arr(obj['fields']['_vEvent'],'fI')]))
assert len(events)==2 and all(0<=p['profile']<len(profiles) for e in events for p in e['points'])
metadata=dict(materials=materials,checkpoints=checkpoints,times=times,instances=instances,lighting=profiles,lightingEvents=events,sourcePathOffset=pathOffset,
    source='Initial D Arcade Stage 8 / Tsubaki '+variant,assets=files,pathPoints=len(paths[0]),shapes=len(shapes),triangles=sum(len(t)//3 for _,_,_,t in shapes))
(DEST/'scene.json').write_text(json.dumps(metadata,indent=2));print(metadata['triangles'],'triangles',len(textures),'textures')
if variant=='day_dry':
    export_island(ROOT/'collision/tsubaki_collision.bin', DEST/'fork_island.json')
    lamp=(ROOT/'path/tsubaki_path_light.pa8').read_bytes();n=struct.unpack_from('<I',lamp,12)[0]
    assert lamp[:4]==b'Path' and len(lamp)==32+n*48 and n<=1024
    positions=[row[:3] for row in struct.iter_unpack('<12f',lamp[32:])]
    (DEST/'lamps.bin').write_bytes(b'HKL1'+struct.pack('<I',n)+b''.join(struct.pack('<3f',*p) for p in positions))
    (DEST/'course.id').write_text('15\n')

