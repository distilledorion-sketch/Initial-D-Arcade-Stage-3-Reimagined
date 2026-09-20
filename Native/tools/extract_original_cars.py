#!/usr/bin/env python3
"""Extract all35 original car banks and CPU-capture their retail display presets."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import sys

from extract_original_models import parse_model, write_binary
from export_original_assembly import IMAGE_HASH
from texture_bank import export_bank

def original_catalog(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_HASH:
        raise ValueError("Original car table requires the verified retail image")
    rows = []
    for car in range(35):
        mapped = struct.unpack_from("<I", image, 0x0c2ef384-0x0c020000+car*4)[0]
        address = 0x0c2ef410 + mapped*64
        folder = image[address-0x0c020000:address-0x0c020000+64].split(b"\0")[0].decode("ascii")
        if not folder or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789_" for c in folder):
            raise ValueError("Unsafe or unsupported original car folder")
        rows.append({"car_index":car,"mapped_index":mapped,"folder":folder,
            "name_address":f"{address:08X}",
            "slot_map_address":f"{struct.unpack_from('<I',image,0x0c33b250-0x0c020000+car*4)[0]:08X}",
            "preset_address":f"{0x0c2f4758+car*12:08X}",
            "model":f"data/original_models/{folder}/{folder}.idasmesh",
            "assembly":f"data/original_models/{folder}/assembly/{folder}_default.idasasm",
            "textures":f"data/original_assets/cars/{folder}/textures/textures.idastex"})
    return rows

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--image",type=Path,required=True)
    ap.add_argument("--hostfs",type=Path,required=True)
    ap.add_argument("--capture-exe",type=Path,required=True)
    ap.add_argument("--project",type=Path,required=True)
    ap.add_argument("--catalog-only",action="store_true")
    args=ap.parse_args()
    rows=original_catalog(args.image.read_bytes())
    catalog=args.project/"data/original_models/car_catalog.json"
    catalog.parent.mkdir(parents=True,exist_ok=True)
    catalog.write_text(json.dumps({"schema":"idas3-original-car-catalog-v1",
        "source_image_sha256":IMAGE_HASH,"cars":rows},indent=2)+"\n")
    header=args.project/"src/car_catalog.h"
    header.write_text('#pragma once\n#include <array>\n#include <string_view>\nnamespace idas3 {\n'
        '// Original ID0..34 from0C2EF384/0C2EF410; no alphabetical remapping.\n'
        'inline constexpr std::array<std::string_view,35> originalCarFolders = {\n'
        + ''.join(f'    "{row["folder"]}", // {row["car_index"]}\n' for row in rows)
        + '};\n}\n')
    if args.catalog_only:return
    results=[]
    for row in rows:
        name=row["folder"]
        try:
            directory=args.hostfs/"model/car"/name
            prefix,chunks,sources,payload=parse_model(directory)
            out=args.project/"data/original_models"/name
            out.mkdir(parents=True,exist_ok=True)
            mesh=out/f"{name}.idasmesh"
            write_binary(mesh,chunks)
            catalog_chunks=[]
            for c in chunks:
                vertices=[v for batch in c["batches"] for v in batch["vertices"]]
                catalog_chunks.append({"index":c["index"],"source_offset":c["source_offset"],"source_size":c["source_size"],
                    "header_words":c["header"],"batch_count":len(c["batches"]),"material_count":c["materials"],
                    "vertex_count":len(vertices),"triangle_count":sum(len(b["indices"])//3 for b in c["batches"]),
                    "bounds":[[min(v[k] for v in vertices),max(v[k] for v in vertices)] for k in (1,2,3)] if vertices else None,
                    "texture_indices":sorted({b["material"][9] for b in c["batches"] if b["material"][9]!=0xffffffff})})
            manifest={"schema":"idas3-original-model-v1","model":prefix,"car_index":row["car_index"],
                "coordinate_transform":"none","uv_transform":"none",
                "assembly_status":"Original display preset selected in assembly subdirectory; catalog preserves all authored alternatives.",
                "source_files":{k:{"path":str(p.resolve()),"bytes":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for k,p in sources.items() if p.exists()},
                "decoded_polygon_sha256":hashlib.sha256(payload).hexdigest(),"binary_sha256":hashlib.sha256(mesh.read_bytes()).hexdigest(),
                "chunk_count":len(chunks),"batch_count":sum(c["batch_count"] for c in catalog_chunks),
                "vertex_count":sum(c["vertex_count"] for c in catalog_chunks),"triangle_count":sum(c["triangle_count"] for c in catalog_chunks),
                "chunks":catalog_chunks}
            (out/"model_manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
            assembly=out/"assembly"
            assembly.mkdir(exist_ok=True)
            capture=assembly/"source_capture.json"
            parts=args.hostfs/"parts"/f"{name}.bin"
            run=subprocess.run([str(args.capture_exe.resolve()),str(args.image.resolve()),str(parts.resolve()),
                    str(capture.resolve()),str(row["car_index"]),str(len(chunks))],capture_output=True,text=True)
            if run.returncode:raise ValueError(run.stderr.strip())
            captured=json.loads(capture.read_text())
            if any(chunks[d["chunk"]].get("nongeometry_marker") for d in captured["draws"]):
                raise ValueError("Original display selected an unresolved nongeometry marker")
            export=subprocess.run([sys.executable,str(Path(__file__).with_name("export_original_assembly.py")),
                "--capture",str(capture),"--source-image",str(args.image),"--parts",str(parts),
                "--bank-manifest",str(out/"model_manifest.json"),"--out",str(assembly)],capture_output=True,text=True)
            if export.returncode:raise ValueError(export.stderr.strip())
            texture_payload=directory/f"{name}_tex.bin.nz"
            if not texture_payload.exists():texture_payload=directory/f"{name}_tex.bin"
            textures=export_bank(directory/f"{name}_tex.tbl",texture_payload,
                    args.project/"data/original_assets/cars"/name/"textures","twiddled")
            result={"car_index":row["car_index"],"folder":name,"status":"exported",
                "chunks":len(chunks),"vertices":manifest["vertex_count"],"draws":captured["draw_count"],
                "instructions":captured["original_instructions"],"textures":len(textures["textures"])}
        except Exception as exc:
            result={"car_index":row["car_index"],"folder":name,"status":"failed","error":str(exc)}
        results.append(result)
        print(json.dumps(result),flush=True)
    (args.project/"data/original_models/car_export_report.json").write_text(json.dumps(results,indent=2)+"\n")
    if any(r["status"]!="exported" for r in results):raise SystemExit(1)

if __name__=="__main__":main()
