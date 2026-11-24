"""Compile spectator quads using the disc's XYZ placement lists and course sectors."""
from pathlib import Path
import hashlib
import json
import struct

def layout_for_packaging(native, host):
    placements = {}
    for f in sorted((host/"path").glob("*.bin")):
        if not any(t in f.name for t in ("_gal","_hito","_human")):
            continue
        data = f.read_bytes()
        if len(data)<8 or struct.unpack_from("<II",data)!=( (len(data)-8)//12,3) or (len(data)-8)%12:
            continue
        for i in range(8,len(data),12):
            xyz=data[i:i+12]
            placements.setdefault(xyz,[]).append((f.name,(i-8)//12,False))
            # Some spectators use the arcade's three-centimetre vertical adjustment.
            y=struct.unpack_from("<f",xyz,4)[0]
            lowered=xyz[:4]+struct.pack("<f",y-0.03)+xyz[8:]
            placements.setdefault(lowered,[]).append((f.name,(i-8)//12,True))
    recipes = {}
    cases = {}
    for src in sorted((native/"data/original_models/courses").glob("*/scene_*.idasbillboards")):
        name = src.stem.removeprefix("scene_")
        raw = src.read_bytes()
        version, path_count, choices, changes = struct.unpack_from("<4I",raw,8)
        if raw[:8]!=b"ID3BLB1\0" or version!=1:
            raise ValueError("Unrecognized billboard format.")
        capture = json.loads((src.parent/(name+"_capture.json")).read_text())
        if path_count != capture["path_count"]:
            raise ValueError("Unexpected course length.")
        cursor = 24
        groups = []
        for _ in range(choices):
            count, = struct.unpack_from("<I",raw,cursor)
            cursor += 4
            group = []
            for _ in range(count):
                before,chunk = struct.unpack_from("<II",raw,cursor)
                matches = placements[raw[cursor+8:cursor+20]]
                # Equal source positions sometimes occur in multiple course files.
                course_prefix = src.parent.name.replace("k_df3","k_df") + "_"
                file,index,lowered = next((m for m in matches if m[0].startswith(course_prefix)), matches[0])
                cursor += 20
                if group:
                    prev = group[-1]
                    if prev[0] == file and prev[1]+prev[2] == index and prev[3]==before and prev[4]+(prev[2]%3)==chunk and prev[5]==lowered:
                        prev[2]+=1
                        continue
                group.append([file,index,1,before,chunk,lowered])
            groups.append(group)
        selectors = {v:i for i,v in enumerate(capture["boundaries"])}
        transitions = []
        for _ in range(changes):
            first,choice=struct.unpack_from("<II",raw,cursor)
            cursor+=8
            if first==0:
                sector=-1
            elif first in selectors:
                sector=selectors[first]
            else:
                raise ValueError(f"Unknown sector boundary {first} in {src.name}")
            transitions.append([sector,choice])
        if cursor != len(raw):
            raise ValueError("Unexpected trailing billboard data.")
        recipe={"groups":groups,"transitions":transitions}
        key=hashlib.sha256(json.dumps(recipe).encode()).hexdigest()[:20]
        recipes[key]=recipe
        cases[src.parent.name+"/"+name]=key
    return {"recipes":recipes,"cases":cases}

def generate(host, project, layout):
    data = {}
    for case,key in layout["cases"].items():
        course,name=case.split("/")
        base=project/"data/original_models/courses"/course
        capture=json.loads((base/(name+"_capture.json")).read_text())
        recipe=layout["recipes"][key]
        result=bytearray(b"ID3BLB1\0"+struct.pack("<4I",1,capture["path_count"],len(recipe["groups"]),len(recipe["transitions"])))
        for group in recipe["groups"]:
            result.extend(struct.pack("<I",sum(row[2] for row in group)))
            for file,first,count,before,chunk,lowered in group:
                if file not in data:
                    data[file]=(host/"path"/file).read_bytes()
                source=data[file]
                for j in range(count):
                    xyz=source[8+12*(first+j):20+12*(first+j)]
                    if len(xyz)!=12:
                        raise ValueError("Truncated disc spectator list.")
                    if lowered:
                        xyz=xyz[:4]+struct.pack("<f",struct.unpack_from("<f",xyz,4)[0]-0.03)+xyz[8:]
                    result.extend(struct.pack("<II",before,chunk+j%3)+xyz)
        for sector,choice in recipe["transitions"]:
            first=0 if sector<0 else capture["boundaries"][sector]
            result.extend(struct.pack("<II",first,choice))
        (base/("scene_"+name+".idasbillboards")).write_bytes(result)

