"""Export original rival numerical tables and complete raw PATH capacity."""
import argparse,hashlib,json,struct
from pathlib import Path
from export_original_physics_data import SOURCE_SHA256,BASE,course_paths,fnv1a

def export(image_path,hostfs,output):
    image=image_path.read_bytes()
    if len(image)!=4194304 or hashlib.sha256(image).hexdigest()!=SOURCE_SHA256:
        raise ValueError("Canonical original program required")
    output.mkdir(parents=True,exist_ok=True)
    start,end=0x0C271618,0x0C283DE4
    raw=image[start-BASE:end-BASE]
    pack=b"ID3RIV01"+struct.pack("<IIII",1,start,len(raw),fnv1a(raw))+bytes.fromhex(SOURCE_SHA256)+raw
    (output/"tables.bin").write_bytes(pack)
    paths=[];alternates=[]
    for condition in range(18):
        stem=Path(course_paths(image,condition)[7]).name[5:]+("o" if condition&1 else "i")
        source=hostfs/"binary"/f"PATH_{stem}_0.bin"
        data=source.read_bytes();last=struct.unpack_from("<I",raw,condition*8)[0]
        if len(data)//12<last+11:raise ValueError("Missing original rival lookahead capacity")
        filename=f"path_{condition:02d}.bin"
        payload=b"ID3RVP01"+struct.pack("<IIII",condition,last,len(data),fnv1a(data))+data
        (output/filename).write_bytes(payload)
        paths.append(dict(condition=condition,file=filename,source=str(source.relative_to(hostfs)),
            source_bytes=len(data),source_sha256=hashlib.sha256(data).hexdigest(),inclusive_last_index=last,
            note="Complete bytes preserved, including lookahead/padding beyond the player valid prefix"))
        # Source046F40 puts i_1/o_1 beside the same direction's i_0/o_0.
        # 0625E0 passes these as159720.r7; profile26 selects r7 in15AE00.
        source=hostfs/"binary"/f"PATH_{stem}_1.bin";data=source.read_bytes()
        entry=dict(condition=condition,source=str(source.relative_to(hostfs)),source_bytes=len(data),
            source_sha256=hashlib.sha256(data).hexdigest(),available=bool(data))
        if data:
            if len(data)//12<last+11:raise ValueError("Incomplete original alternate rival path")
            filename=f"path_{condition:02d}_alternate.bin"
            (output/filename).write_bytes(b"ID3RVA01"+struct.pack("<IIII",condition,last,len(data),fnv1a(data))+data)
            entry.update(file=filename,inclusive_last_index=last)
        alternates.append(entry)
    manifest=dict(schema="idas3-original-rival-v1",program_sha256=SOURCE_SHA256,table_original_start=hex(start),
        table_original_end_exclusive=hex(end),table_bytes=len(raw),table_sha256=hashlib.sha256(raw).hexdigest(),paths=paths,alternate_paths=alternates,
        scope="Numerical lookup data only; no original opcode interpreter/runtime")
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(f"Exported {len(raw)} original rival table bytes,18 primary and {sum(x['available'] for x in alternates)} alternate full-capacity paths")

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("image",type=Path);p.add_argument("hostfs",type=Path);p.add_argument("output",type=Path)
    a=p.parse_args();export(a.image,a.hostfs,a.output)
