#!/usr/bin/env python3
"""Add the original forward Akina background without changing existing banks."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from extract_original_models import parse_model,write_binary,write_obj
from texture_bank import export_bank
from export_original_assembly import IMAGE_HASH,identity

def source_info(path):
    return {"path":str(path.resolve()),"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("image","hostfs","capture","project"):p.add_argument("--"+name,type=Path,required=True)
    args=p.parse_args();raw=args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=IMAGE_HASH:raise ValueError("Canonical original image mismatch")
    # Exact loader table selection for course-direction6, condition0.
    table=0x0c2eff40+6*1024
    paths=[]
    for i in range(8):paths.append(raw[table-0x0c020000+i*64:table-0x0c020000+(i+1)*64].split(b"\0",1)[0].decode("ascii"))
    if "df_etc_f_pol" not in paths[2] or "df_etc_f_tex" not in paths[3]:raise ValueError("Original Akina background path binding changed")
    capture=json.loads(args.capture.read_text())
    if capture["start_pc"]!="0C19DA74" or capture["stop_pc"]!="0C19DAAA" or len(capture["cases"])!=72:
        raise ValueError("Invalid background original-opcode capture")
    source=args.hostfs/"model/course/k_df"
    _,chunks,files,payload=parse_model(source,"df_etc_f_pol.tbl","df_etc_f_tex.tbl")
    if len(chunks)!=1 or len(chunks[0]["batches"])!=3:raise ValueError("Unexpected background bank structure")
    out=args.project/"data/original_models/courses/k_df/background"
    textures=args.project/"data/original_assets/courses/k_df/background/textures"
    out.mkdir(parents=True,exist_ok=True)
    binary=out/"df_etc_f.idasmesh";write_binary(binary,chunks)
    write_obj(out/"df_etc_f_inspection.obj",chunks)
    (out/"df_etc_f_origin.idasasm").write_bytes(b"IDAS3A1\0"+struct.pack("<2I",1,1)+struct.pack("<I16f",0,*identity()))
    bank=export_bank(source/"df_etc_f_tex.tbl",source/"df_etc_f_tex.bin.nz",textures,"twiddled")
    if len(bank["textures"])!=2:raise ValueError("Expected exact two background textures")
    vs=[v for b in chunks[0]["batches"] for v in b["vertices"]]
    meta={"schema":"idas3-original-akina-background-v1","condition":"Original course-direction6, condition0; primary forward Akina background",
        "original_image":source_info(args.image),"source_path_record_address":f"{table:08X}","source_path_record":paths,
        "source_files":{k:source_info(v) for k,v in files.items()},"texture_payload":source_info(source/"df_etc_f_tex.bin.nz"),
        "decoded_polygon_sha256":hashlib.sha256(payload).hexdigest(),"output_mesh_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
        "chunk_count":1,"vertex_count":len(vs),"triangle_count":sum(len(b["indices"])//3 for b in chunks[0]["batches"]),"batch_count":3,"texture_count":2,
        "bounds":[[min(v[k] for v in vs),max(v[k] for v in vs)] for k in (1,2,3)],
        "batches":[{"index":i,"ich_words":b["words"],"gmp_words":b["material"],"texture_index":b["material"][9],"vertex_count":len(b["vertices"])} for i,b in enumerate(chunks[0]["batches"])],
        "model":str(binary.relative_to(args.project)).replace("\\","/"),"textures":str((textures/"textures.idastex").relative_to(args.project)).replace("\\","/"),
        "draw_selection":"0C19DA9A..0C19DAAA selects bank at course object+0x158, chunk0, then submits to1D7120",
        "placement":"Original code computes inverse(view), writeszero to column-major elements12 and14, inverts again. For rigid camera views, equivalent to world translation(camera.x,0,camera.z).",
        "coordinate_conversion":"None; original authored XYZ, UV and raw vertex bytes preserved",
        "helper":"src/akina_background.h originalAkinaBackgroundInstance(cameraWorld)",
        "capture":{"cases":len(capture["cases"]),"original_instructions":capture["total_original_instructions"],"max_matrix_abs_error":capture["max_matrix_abs_error"],"hooks":capture["hooks"]},
        "integration_constraints":["Append this independent two-texture bank after existing banks and pass its actual texture-base offset.",
          "Current scene far plane650 excludes much of radius1974 background. Use a projection containing the authored dome, or a separately composed background pass; do not shrink the geometry.",
          "The origin .idasasm is for inspection only. Use the camera-dependent helper transform for gameplay.",
          "Keep background GMPb0 constant color and authored TSP fog/blend behavior; do not apply generic atmospheric fog over its original panorama."],
        "limitations":["Capture uses explicit current-view-read/final-matrix-load/model-lookup/draw hooks; both general matrix inversions and original selector execute unhooked.",
          "Helper is rigid-view mathematical equivalence with measured original F32 inversion error, not bit-for-bit matrix parity.",
          "Twiddled spatial texture layout remains explicit pending full loader TCW binding proof.",
          "This export does not establish reverse/night/snow background condition selection."]}
    (out/"background_manifest.json").write_text(json.dumps(meta,indent=2)+"\n",encoding="utf-8")
    (out/"source_capture.json").write_text(json.dumps(capture,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:meta[k] for k in ("model","textures","vertex_count","triangle_count","batch_count","texture_count","bounds","capture")},indent=2))

if __name__=="__main__":main()
