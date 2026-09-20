#!/usr/bin/env python3
"""Export the CPU-captured original AE86 draw selection as an instance assembly.

The capture executes the original retail selection opcodes. Matrix helpers are
explicitly hooked: their exact inputs survive in JSON; derived matrices use
host sin/cos and do not claim SH-4 FSCA/FTRV bit parity.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

IMAGE_HASH = "efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335"

def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]

def identity():
    return [float(i % 5 == 0) for i in range(16)]

def multiply(a, b):
    return [f32(sum(a[r*4+k]*b[k*4+c] for k in range(4)))
            for r in range(4) for c in range(4)]

def operations_matrix(operations):
    result = identity()
    for op in operations:
        kind, values = op["type"], op["values"]
        if not all(math.isfinite(v) for v in values):
            raise ValueError("Non-finite captured matrix input")
        m = identity()
        if kind == "translate" and len(values) == 3:
            m[3], m[7], m[11] = values
        elif kind == "scale" and len(values) == 3:
            m[0], m[5], m[10] = values
        elif kind == "load_matrix" and len(values) == 16:
            # SH-4 XMTRX memory is column-major; the export is row-major.
            result = [values[c*4+r] for r in range(4) for c in range(4)]
            continue
        elif kind.startswith("rotate_") and len(values) == 1:
            axis = kind.split("_")[1]
            if kind.endswith("_float"):
                # The captured default has only neutral float rotations.
                # Refuse to silently guess nonzero unit conversion semantics.
                if values[0] != 0:
                    raise ValueError("Nonzero float-angle capture requires original conversion helper")
                angle = 0.0
            else:
                if not kind.endswith("_u16") or not 0 <= values[0] < 65536:
                    raise ValueError("Invalid original integer angle")
                angle = values[0] * (2.0 * math.pi / 65536.0)
            c, s = f32(math.cos(angle)), f32(math.sin(angle))
            if axis == "x": m[5], m[6], m[9], m[10] = c, -s, s, c
            elif axis == "y": m[0], m[2], m[8], m[10] = c, s, -s, c
            elif axis == "z": m[0], m[1], m[4], m[5] = c, -s, s, c
            else: raise ValueError("Unknown original rotation axis")
        else:
            raise ValueError(f"Unsupported captured operation {kind}")
        result = multiply(result, m)
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--capture", type=Path, required=True)
    ap.add_argument("--source-image", type=Path, required=True)
    ap.add_argument("--parts", type=Path, required=True)
    ap.add_argument("--bank-manifest", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    image = args.source_image.read_bytes()
    if hashlib.sha256(image).hexdigest() != IMAGE_HASH:
        raise ValueError("Assembly capture requires verified GDS-0033 retail image")
    capture = json.loads(args.capture.read_text())
    bank = json.loads(args.bank_manifest.read_text())
    car_index = capture.get("car_index", 0)
    if capture["final_matrix_depth"] != 0 or not 0 <= car_index < 35:
        raise ValueError("Unexpected capture state or unbalanced original matrix stack")
    slot_map = struct.unpack_from("<I", image, 0x0c33b250-0x0c020000+car_index*4)[0]
    if len(capture["draws"]) != capture["draw_count"] or capture["draw_count"] > 4096:
        raise ValueError("Invalid capture draw count")
    for draw in capture["draws"]:
        if not 0 <= draw["chunk"] < bank["chunk_count"]:
            raise ValueError("Assembly chunk outside original bank")
        lookup = struct.unpack_from("<i", image, slot_map-0x0c020000+draw["semantic"]*4)[0]
        if lookup != draw["chunk"]:
            raise ValueError("Draw disagrees with original live slot mapping")
        draw["matrix"] = operations_matrix(draw["operations"])
    args.out.mkdir(parents=True, exist_ok=True)
    model = bank["model"]
    binary = args.out / f"{model}_default.idasasm"
    with binary.open("wb") as f:
        f.write(b"IDAS3A1\0")
        f.write(struct.pack("<2I", 1, len(capture["draws"])))
        for draw in capture["draws"]:
            f.write(struct.pack("<I16f", draw["chunk"], *draw["matrix"]))
    manifest = {
        "schema": "idas3-original-assembly-v1",
        "appearance": f"Original retail car{car_index} display preset; neutral steering/suspension, lights off",
        "model_bank": f"../{model}.idasmesh" if args.out.name == "assembly" else f"{model}.idasmesh",
        "model_bank_sha256": bank["binary_sha256"],
        "capture": capture,
        "matrix_format": "row-major, column vectors, translation elements3/7/11; original postmultiplication order",
        "matrix_accuracy": "Exact captured float inputs/integer angles; host sin/cos-derived matrices, hardware FSCA/FTRV bit parity untested",
        "model_coordinate_transform": "none; +Z front, +Y up; no metric rescale applied",
        "sources": {key: {"path": str(path.resolve()), "bytes": path.stat().st_size,
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                    for key, path in {"image": args.source_image, "parts": args.parts,
                                      "capture": args.capture, "bank_manifest": args.bank_manifest}.items()},
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "evidence": {
            "renderer": "0C026D80..0C028364",
            "display_preset_bytes": f"{0x0c2f4758+car_index*12:08X}, 12 bytes; applied by original routine 0C035F00",
            "car_table_root": f"{0x0c33b250+car_index*4:08X} -> {slot_map:08X}",
            "slot_lookup": "0C026100; object+354 semantic map, object+358 render remap",
            "part_transforms": "0C0271CC onward; 23 records of 9 float32 values scaleXYZ/rotationXYZ/translationXYZ",
            "matrix_helpers": "1F6610 push,1F65C0 pop,1FD060/1F6AC0 translate,1F6950/1F68A0/1F67E0 rotateZ/Y/X,1F69D0 scale"
        },
        "limitations": [
            "Original selection branches run on a bounded CPU oracle; graphics/matrix callees are explicit hooks.",
            "Separately loaded number plate model helper 0C026160 is omitted.",
            "Wheel blur helper 0C029EE0 is omitted at neutral zero intensity.",
            "Paint/material initialization overrides and dynamic lighting are not captured.",
            "This is one original display preset, not every card/tuning/color/LOD state."
        ]
    }
    (args.out / "assembly_manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
    print(f"Exported {len(capture['draws'])} original draw instances to {binary}")

if __name__ == "__main__":
    main()
