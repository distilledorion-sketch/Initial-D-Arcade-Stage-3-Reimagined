#!/usr/bin/env python3
"""Extract original model chunks, vertices, UVs and material indices directly.

This is a structural asset conversion, not a guessed car assembly. Every
authored chunk remains separately identified, including alternatives and LODs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import struct
import zlib
from pathlib import Path

VERTEX_SIZE = {0x002: 16, 0x00A: 24, 0x00E: 40, 0x042: 24, 0x04A: 32, 0x10A: 40}

def read_payload(path: Path) -> bytes:
    data = path.read_bytes()
    if data.startswith(b"NMZIP"):
        if len(data) < 32: raise ValueError("Truncated NMZIP header")
        expected = struct.unpack_from("<I", data, 28)[0]
        if expected > 512 * 1024 * 1024: raise ValueError("NMZIP size exceeds import bound")
        data = zlib.decompress(data[32:])
        if len(data) != expected: raise ValueError("NMZIP size mismatch")
    return data

def packed_normal(header):
    def component(shift):
        n = (header >> shift) & 255
        return (n if n < 128 else n - 256) / 127.0
    return tuple(component(shift) for shift in (0, 8, 16))

def triangles(vertices):
    """ELAN header bits29/30=2 select fan; bit31 terminates the primitive."""
    result, sequence = [], []
    for i, vertex in enumerate(vertices):
        sequence.append(i)
        if len(sequence) >= 3:
            if (vertex[0] >> 29) & 3 == 2:
                face = (sequence[0], sequence[-2], i)
            elif len(sequence) % 2 == 1:
                face = (sequence[-3], sequence[-2], i)
            else:
                face = (sequence[-2], sequence[-3], i)
            result.extend(face)
        if vertex[0] & 0x80000000: sequence = []
    return result

def parse_model(directory: Path, polygon_table: str | None = None,
                texture_table_name: str | None = None):
    tables = [directory/polygon_table] if polygon_table else list(directory.glob("*_pol.tbl"))
    if len(tables) != 1: raise ValueError("Expected one *_pol.tbl in asset directory")
    table = tables[0]
    prefix = table.name.removesuffix("_pol.tbl") if table.name.endswith("_pol.tbl") else table.stem.replace("_pol_", "_")
    polygon = table.with_suffix(".bin.nz")
    if not polygon.exists(): polygon = table.with_suffix(".bin")
    payload = read_payload(polygon)
    tbl = table.read_bytes()
    if not tbl or len(tbl) % 4: raise ValueError("Invalid polygon offset table")
    offsets = list(struct.unpack(f"<{len(tbl)//4}I", tbl))
    if offsets[0] != 0 or offsets != sorted(set(offsets)):
        raise ValueError("Polygon offset table must start at zero and increase")
    texture_table = directory / (texture_table_name or f"{prefix}_tex.tbl")
    texture_count = texture_table.stat().st_size // 16 if texture_table.exists() else None
    if texture_table.exists() and texture_table.stat().st_size % 16: raise ValueError("Invalid texture table")
    chunks = []
    for index, start in enumerate(offsets):
        end = offsets[index + 1] if index + 1 < len(offsets) else len(payload)
        if not 0 <= start < end <= len(payload):
            raise ValueError(f"Chunk {index}: invalid bounds")
        # NB8C index53 is an authored eight-byte nongeometry marker. Preserve
        # its words and index rather than inventing a geometry header or
        # renumbering every later piece. Its rendering semantics are unresolved.
        if payload[start:end] == struct.pack("<2I", 0xffffffff, 3):
            chunks.append({"index":index,"source_offset":start,"source_size":8,
                           "header":(0xffffffff,3)+(0,)*22,"materials":0,"batches":[],
                           "nongeometry_marker":True})
            continue
        if end - start < 96:
            raise ValueError(f"Chunk {index}: invalid bounds")
        header = struct.unpack_from("<24I", payload, start)
        if header[0] != 0x100 or header[6] != end-start:
            raise ValueError(f"Chunk {index}: unrecognized header/declared size")
        position, material, batches, material_count = start + 96, None, [], 0
        while position < end:
            if position + 32 > end: raise ValueError(f"Chunk {index}: truncated command")
            words = struct.unpack_from("<8I", payload, position)
            command = (words[0] >> 8) & 15 if words[0] & 0x08000000 else -1
            if command == 5:
                if position + 64 > end: raise ValueError("Truncated material")
                material = struct.unpack_from("<16I", payload, position)
                # These embedded texture-reference records are observed intact
                # in all106 AE86 chunks, not inferred from texture colors.
                if any(material[n] != 0x08000000 for n in (8,10,12,14)):
                    raise ValueError("Unknown material texture-reference records")
                if texture_count is not None:
                    for n in (9,11,13,15):
                        if material[n] != 0xffffffff and material[n] >= texture_count:
                            raise ValueError("Material references outside texture bank")
                material_count += 1
                position += 64
                continue
            if command != 7 or material is None: raise ValueError(f"Chunk {index}: unsupported command at {position:#x}")
            flags, count = words[6:8]
            if flags not in VERTEX_SIZE or count > 1_000_000: raise ValueError("Unsupported vertex layout/count")
            stride = VERTEX_SIZE[flags]
            size = 32 + stride*count
            if position + size > end: raise ValueError("Vertex array exceeds chunk")
            vertices = []
            for i in range(count):
                off = position + 32 + i*stride
                vh = struct.unpack_from("<I", payload, off)[0]
                xyz = struct.unpack_from("<3f", payload, off+4)
                normal, uv = packed_normal(vh), (0.0,0.0)
                colors = (0xffffffff,0xffffffff)
                if flags == 0x00E:
                    normal = struct.unpack_from("<3f", payload, off+16)
                    uv = struct.unpack_from("<2f", payload, off+32)
                elif flags in (0x00A,0x04A,0x10A): uv = struct.unpack_from("<2f", payload, off+16)
                if flags == 0x042: colors = struct.unpack_from("<2I", payload, off+16)
                elif flags == 0x04A: colors = struct.unpack_from("<2I", payload, off+24)
                # VUB carries additional source attributes; preserve its full
                # payload below, and do not assign unproven skinning semantics.
                if not all(math.isfinite(v) for v in (*xyz,*normal,*uv)):
                    raise ValueError("Non-finite model coordinate/attribute")
                vertices.append((vh,*xyz,*normal,*uv,*colors))
            batches.append({"source_offset": position, "words": words, "material": material,
                            "vertices": vertices,"indices": triangles(vertices),
                            "source_vertex_bytes":payload[position+32:position+size]})
            position += size
        chunks.append({"index":index,"source_offset":start,"source_size":end-start,
                       "header":header,"materials":material_count,"batches":batches})
    return prefix, chunks, {"polygon":polygon,"table":table,"texture_table":texture_table}, payload

def write_binary(path: Path, chunks):
    with path.open("wb") as f:
        f.write(b"IDAS3M1\0")
        f.write(struct.pack("<II",1,len(chunks)))
        for chunk in chunks:
            f.write(struct.pack("<4I",chunk["index"],chunk["source_offset"],chunk["source_size"],len(chunk["batches"])))
            f.write(struct.pack("<24I",*chunk["header"]))
            for batch in chunk["batches"]:
                f.write(struct.pack("<4I",batch["source_offset"],len(batch["vertices"]),len(batch["indices"]),len(batch["source_vertex_bytes"])))
                f.write(struct.pack("<8I",*batch["words"]))
                f.write(struct.pack("<16I",*batch["material"]))
                for vertex in batch["vertices"]: f.write(struct.pack("<I8f2I",*vertex))
                f.write(struct.pack(f"<{len(batch['indices'])}I",*batch["indices"]))
                f.write(batch["source_vertex_bytes"])

def write_obj(path: Path,chunks):
    # OBJ is for inspection/interchange. The binary preserves original headers,
    # material words and source vertex bytes absent from OBJ.
    with path.open("w",encoding="utf-8",newline="\n") as f:
        f.write("# Original authored chunks. Alternatives are NOT a selected car assembly.\n")
        f.write("# XYZ and UV unchanged; texture origin must be matched by the consuming renderer.\n")
        vertex_base = 1
        for chunk in chunks:
            f.write(f"o original_chunk_{chunk['index']:03d}\n")
            for number,batch in enumerate(chunk["batches"]):
                tex=batch["material"][9]
                f.write(f"g chunk_{chunk['index']:03d}_batch_{number:03d}\nusemtl original_texture_{tex if tex!=0xffffffff else 'none'}\n")
                for v in batch["vertices"]:f.write("v "+" ".join(format(x,".9g") for x in v[1:4])+"\n")
                for v in batch["vertices"]:f.write("vt "+" ".join(format(x,".9g") for x in v[7:9])+"\n")
                for v in batch["vertices"]:f.write("vn "+" ".join(format(x,".9g") for x in v[4:7])+"\n")
                indices=batch["indices"]
                for i in range(0,len(indices),3):
                    face=[j+vertex_base for j in indices[i:i+3]]
                    f.write("f "+" ".join(f"{n}/{n}/{n}" for n in face)+"\n")
                vertex_base+=len(batch["vertices"])

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--asset-directory",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    args=ap.parse_args()
    prefix,chunks,sources,payload=parse_model(args.asset_directory)
    args.out.mkdir(parents=True,exist_ok=True)
    binary=args.out/f"{prefix}.idasmesh"
    write_binary(binary,chunks)
    write_obj(args.out/f"{prefix}_all_authored_chunks.obj",chunks)
    catalog=[]
    for c in chunks:
        all_vertices=[v for b in c["batches"] for v in b["vertices"]]
        catalog.append({"index":c["index"],"source_offset":c["source_offset"],"source_size":c["source_size"],
            "header_words":c["header"],"batch_count":len(c["batches"]),"material_count":c["materials"],
            "vertex_count":len(all_vertices),"triangle_count":sum(len(b["indices"])//3 for b in c["batches"]),
            "bounds":[[min(v[k] for v in all_vertices),max(v[k] for v in all_vertices)] for k in (1,2,3)] if all_vertices else None,
            "texture_indices":sorted({b["material"][9] for b in c["batches"] if b["material"][9]!=0xffffffff})})
    manifest={"schema":"idas3-original-model-v1","model":prefix,"coordinate_transform":"none",
        "uv_transform":"none","assembly_status":"All authored alternatives/LODs remain separate; original selected car assembly unresolved.",
        "source_files":{k:{"path":str(p.resolve()),"bytes":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for k,p in sources.items() if p.exists()},
        "decoded_polygon_sha256":hashlib.sha256(payload).hexdigest(),"binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
        "chunk_count":len(chunks),"batch_count":sum(c["batch_count"] for c in catalog),"vertex_count":sum(c["vertex_count"] for c in catalog),
        "triangle_count":sum(c["triangle_count"] for c in catalog),"chunks":catalog}
    (args.out/"model_manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:manifest[k] for k in ("model","chunk_count","batch_count","vertex_count","triangle_count","assembly_status")},indent=2))

if __name__=="__main__":main()
