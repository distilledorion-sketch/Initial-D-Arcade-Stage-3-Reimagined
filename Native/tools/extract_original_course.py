#!/usr/bin/env python3
"""Export original Akina daytime course banks and CPU-captured sector choices."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

from extract_original_models import parse_model, write_binary
from texture_bank import export_bank
from export_original_assembly import IMAGE_HASH, identity

def provenance(path):
    return {"path":str(path.resolve()),"bytes":path.stat().st_size,
            "sha256":hashlib.sha256(path.read_bytes()).hexdigest()}

def validate_capture(capture):
    sectors=capture["sectors"]
    if [r["sector"] for r in sectors]!=list(range(30)):
        raise ValueError("Expected all 30 unique original course sectors")
    for row in sectors:
        sector=row["sector"]
        near=list(range(max(0,sector-2),min(30,sector+3)))
        far=[60+i for i in range(30) if i not in near]
        if row["primary"]["entry"]!=0x0c19d860 or row["primary"]["chunks"]!=near+far+[100]:
            raise ValueError("Primary captured original sector selection disagrees with reviewed routine")
        if row["alternate"]["entry"]!=0x0c19db20 or row["alternate"]["chunks"]!=[i+30 for i in near]+far+[101]:
            raise ValueError("Alternate captured original sector selection disagrees with reviewed routine")
        if row["static"]["entry"]!=0x0c19dc80 or row["static"]["chunks"]!=[134,128,130,131,132]:
            raise ValueError("Static captured original selection disagrees with reviewed routine")
    return sectors

def write_assembly(path, chunks):
    if len(set(chunks))!=len(chunks) or not all(0<=c<135 for c in chunks):
        raise ValueError("Invalid or duplicate course selection")
    with path.open("wb") as out:
        out.write(b"IDAS3A1\0"+struct.pack("<II",1,len(chunks)))
        for chunk in chunks:out.write(struct.pack("<I16f",chunk,*identity()))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--image",type=Path,required=True)
    ap.add_argument("--hostfs",type=Path,required=True)
    ap.add_argument("--capture",type=Path,required=True)
    ap.add_argument("--project",type=Path,required=True)
    args=ap.parse_args()
    image=args.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=IMAGE_HASH:raise ValueError("Original image hash mismatch")
    boundaries=list(struct.unpack_from("<30i",image,0x0c29ee58-0x0c020000))
    if boundaries[-1]!=-1 or boundaries[:-1]!=sorted(set(boundaries[:-1])):
        raise ValueError("Invalid original sector boundary table")
    capture=json.loads(args.capture.read_text())
    sectors=validate_capture(capture)
    source=args.hostfs/"model/course/k_df"
    destination=args.project/"data/original_models/courses/k_df"
    assembly=destination/"assembly"
    assembly.mkdir(parents=True,exist_ok=True)
    chunks=[];catalog=[];sources={}
    for bank,expected in zip("abc",[20,25,90]):
        _,items,files,payload=parse_model(source,f"df_pol_{bank}.tbl","df_tex.tbl")
        if len(items)!=expected:raise ValueError("Original bank count changed")
        sources[bank]={"files":{k:provenance(v) for k,v in files.items()},
                       "decoded_polygon_sha256":hashlib.sha256(payload).hexdigest(),
                       "first_global_chunk":len(chunks),"chunk_count":len(items)}
        for item in items:
            local=item["index"];item["index"]=len(chunks)
            vertices=[v for b in item["batches"] for v in b["vertices"]]
            catalog.append({"index":item["index"],"source_bank":bank,"source_local_index":local,
                "source_offset":item["source_offset"],"source_size":item["source_size"],
                "header_words":item["header"],"batch_count":len(item["batches"]),
                "vertex_count":len(vertices),"triangle_count":sum(len(b["indices"])//3 for b in item["batches"]),
                "bounds":[[min(v[k] for v in vertices),max(v[k] for v in vertices)] for k in (1,2,3)],
                "texture_indices":sorted({b["material"][9] for b in item["batches"] if b["material"][9]!=0xffffffff})})
            chunks.append(item)
    mesh=destination/"k_df.idasmesh"
    write_binary(mesh,chunks)
    texture_dir=args.project/"data/original_assets/courses/k_df/textures"
    textures=export_bank(source/"df_tex.tbl",source/"df_tex.bin.nz",texture_dir,"twiddled")
    if len(textures["textures"])!=211:raise ValueError("Original Akina texture count changed")
    instances=[]
    for row in sectors:
        selected=row["primary"]["chunks"]+row["static"]["chunks"]
        path=assembly/f"sector_{row['sector']:02d}.idasasm"
        write_assembly(path,selected)
        instances.append({"sector":row["sector"],"assembly":str(path.relative_to(args.project)).replace("\\","/"),
                          "chunks":selected,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
    (destination/"source_capture.json").write_text(json.dumps(capture,indent=2)+"\n")
    meta={"schema":"idas3-original-course-v1","course":"k_df","name":"Akina",
        "condition":"Primary scene callback with object+48=0; original normal forward bank",
        "coordinate_transform":"none; authored XYZ and UV preserved",
        "original_image":provenance(args.image),"source_banks":sources,
        "model":str(mesh.relative_to(args.project)).replace("\\","/"),
        "textures":str((texture_dir/"textures.idastex").relative_to(args.project)).replace("\\","/"),
        "binary_sha256":hashlib.sha256(mesh.read_bytes()).hexdigest(),
        "global_chunk_dispatch":"Original 0C19D000 concatenates banks A=20, B=25, C=90 through 05A8E0",
        "selection_evidence":"Original opcodes at 0C19D860, 0C19DB20, 0C19DC80 executed for all30 sectors with explicit graphics hooks",
        "boundary_table_address":"0C29EE58","path_point_count":4089,
        "boundaries":boundaries[:-1],"boundary_semantics":"Sector advances when original forward k_df_path index >= boundary; not physics PATH_dfi_0 indices",
        "chunk_count":len(chunks),"vertex_count":sum(c["vertex_count"] for c in catalog),
        "batch_count":sum(c["batch_count"] for c in catalog),"triangle_count":sum(c["triangle_count"] for c in catalog),
        "texture_count":len(textures["textures"]),"chunks":catalog,"sectors":instances,
        "limitations":["Original dynamic crowd/tree objects are not instantiated by this bounded static scene capture.",
          "The separate df_etc_f background model draw is recorded as an excluded external draw; its independent texture bank/transform is not exported here.",
          "Alternate callback near meshes 30..59 and landscape101 remain separate in the source bank and are not stacked with primary choices.",
          "Original fog/lighting/weather/material state and texture TCW scan-order are not fully captured; twiddled texture layout is explicit.",
          "Identity assembly matrices preserve world-authored course XYZ; hooks do not claim original GPU matrix/lighting execution parity."]}
    (destination/"scene_manifest.json").write_text(json.dumps(meta,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:meta[k] for k in ("model","textures","chunk_count","vertex_count","batch_count","triangle_count","texture_count")},indent=2))

if __name__=="__main__":main()
