"""Convert the extracted Tsubaki road strips to the existing D3 course/contact data."""
import json, math, struct
from pathlib import Path
import argparse
from tsubaki_guardrails import align_guardrails
from tsubaki_fork import partition_rows
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
root=parser.parse_args().output
b=(root/'road.bin').read_bytes(); assert b[:4]==b'HKR1'
n=struct.unpack_from('<I',b,4)[0]
roads=[[struct.unpack_from('<3f',b,8+(side*n+i)*12) for i in range(n)] for side in range(3)]
roads,collision_roads,rail_report=align_guardrails(root,roads)
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a):
    d=math.sqrt(sum(x*x for x in a)); assert d>1e-8
    return tuple(x/d for x in a)
# OriginalRacePath expects clockwise XZ cells (left to right along the course).
if cross(sub(roads[2][0],roads[1][0]),sub(roads[0][1],roads[0][0]))[1]<0:
    roads[1],roads[2]=roads[2],roads[1]
    collision_roads[1],collision_roads[2]=collision_roads[2],collision_roads[1]
for suffix,points in zip(['','_l','_r'],roads):
    (root/f'tsubaki_path{suffix}.bin').write_bytes(struct.pack('<II',n,3)+b''.join(struct.pack('<3f',*p) for p in points))
# Preserve every road-edge point when the signed 16-bit contact index permits.
# Coarsening Tsubaki's tight bends can put the wall behind its visible edge.
vertices=[];rows=[]
for points in partition_rows(root,collision_roads):
    row=[];unique={}
    for point in points:
        if point not in unique:
            unique[point]=len(vertices);vertices.append(point)
        row.append(unique[point])
    rows.append(row)
tri=[]
for previous,current in zip(rows,rows[1:]):
    for strip in range(5):
        a,c=previous[strip:strip+2];e,d=current[strip:strip+2]
        for ids in [(a,c,d),(a,d,e)]:
            if len(set(ids))<3:continue
            p,q,r=(vertices[j] for j in ids)
            area=cross(sub(q,p),sub(r,p))[1]
            assert abs(area)>1e-8,'Degenerate fork contact triangle'
            if area<0:ids=(ids[0],ids[2],ids[1])
            tri.append(list(ids)+[-1,-1,-1,0,0 if strip in (1,3) else -32768])
assert len(vertices)<32768 and len(tri)<32768
edges={}; normals=[[0.,0.,0.] for p in vertices]
for i,t in enumerate(tri):
    a,c,d=(vertices[j] for j in t[:3]); normal=norm(cross(sub(c,a),sub(d,a)))
    for j in t[:3]: normals[j]=[x+y for x,y in zip(normals[j],normal)]
    for edge in range(3):
        key=tuple(sorted((t[(edge-1)%3],t[edge])))
        if key in edges:
            other,e=edges.pop(key); tri[other][3+e]=i;t[3+edge]=other
        else: edges[key]=(i,edge)
# Each coarse cell points into its matching triangle. Boundary neighbors remain
# -1 so the original swept-contact solver supplies roadside wall response.
coarse=[]
for i,t in enumerate(tri):
    points=[vertices[j] for j in t[:3]]
    # Rank overlapping cells by the actual road plane. A constant plane makes
    # the first XZ overlap win at Tsubaki's overpass, selecting the wrong deck.
    plane=norm(cross(sub(points[1],points[0]),sub(points[2],points[0])))
    # Wheel probes extend below the body; retain a two-metre contact allowance
    # while keeping the separate road deck above the overpass out of this cell.
    offset=-sum(x*y for x,y in zip(plane,points[0]))+2.0
    coarse.append(struct.pack('<13fI',*plane,offset,*sum((list(p) for p in points),[]),i))
material=struct.pack('<9I',*([0]*9))
verts=b''.join(struct.pack('<6fII',*p,*norm(v),0,0) for p,v in zip(vertices,normals))
triangles=b''.join(struct.pack('<8h',*t) for t in tri)
offset1=48;offset2=offset1+len(material);offset3=offset2+len(verts);offset4=offset3+len(triangles)
header=struct.pack('<12I',0x52434c31,1,1,offset1,len(vertices),offset2,len(tri),offset3,0,offset4,len(coarse),offset4)
(root/'tsubaki.rcl').write_bytes(header+material+verts+triangles+b''.join(coarse))
manifest=json.loads((root/'scene.json').read_text())
(root/'race.bin').write_bytes(b'HKD3'+struct.pack('<9i',*manifest['checkpoints'],*manifest['times']))
print(f'D3 road exported: {n} path points, {len(tri)} contact triangles, {len(edges)} boundary edges')
print(f'Lower guardrail alignment: {len(rail_report)} mesh samples, offsets {min(r["offset"] for r in rail_report):.3f}..{max(r["offset"] for r in rail_report):.3f} m')
