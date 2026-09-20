#!/usr/bin/env python3
"""Import only the user's extracted course centerlines and edges, with hashes.

No geometry is approximated. The original three float32 path streams per course
are copied byte-for-byte. This script does not need or launch the legacy game.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import shutil
import struct
from pathlib import Path

COURSES = {"k_ez": "Myogi", "s_nm": "Usui", "h_hd": "Akagi", "k_df": "Akina",
           "s_vh": "Happogahara", "s_uh": "Irohazaka", "n_sy": "Shomaru", "k_tu": "Tsuchisaka"}

def read_path(path: Path):
    data = path.read_bytes()
    if len(data) < 32:
        raise ValueError(f"Truncated file: {path}")
    count, components = struct.unpack_from("<II", data)
    if not 2 <= count <= 1_000_000 or components != 3 or len(data) != 8 + count * 12:
        raise ValueError(f"Invalid path header: {path}")
    points = list(struct.iter_unpack("<3f", data[8:]))
    if any(not math.isfinite(v) or abs(v) > 1e7 for p in points for v in p):
        raise ValueError(f"Invalid coordinates: {path}")
    return data, points

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Existing HOSTFS/path, HOSTFS, or driveA folder")
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1] / "data" / "courses")
    args = parser.parse_args()
    source = args.source.resolve()
    if (source / "HOSTFS").is_dir(): source /= "HOSTFS"
    if (source / "path").is_dir(): source /= "path"
    records = []
    pending = []
    for course_id, name in COURSES.items():
        streams = [read_path(source / f"{course_id}_path{suffix}.bin") for suffix in ("", "_l", "_r")]
        center, left, right = [p for _, p in streams]
        if len(center) != len(left) or len(center) != len(right):
            raise ValueError(f"Mismatched edge counts: {course_id}")
        steps = [math.dist(a, b) for a, b in zip(center, center[1:])]
        widths = [math.dist(a, b) for a, b in zip(left, right)]
        if min(steps) < 1e-6 or max(steps) > 1000 or min(widths) < .05 or max(widths) > 200:
            raise ValueError(f"Invalid spacing/boundaries: {course_id}")
        hashes = {f"{course_id}_path{suffix}.bin": hashlib.sha256(data).hexdigest()
                  for suffix, (data, _) in zip(("", "_l", "_r"), streams)}
        records.append({"id": course_id, "name": name, "points": len(center),
            "length_native_units": sum(steps), "height_min": min(p[1] for p in center),
            "height_max": max(p[1] for p in center), "width_min": min(widths), "width_max": max(widths),
            "closed": math.dist(center[0], center[-1]) < .05, "start": center[0], "finish": center[-1],
            "source_files_sha256": hashes,
            "combined_sha256": hashlib.sha256(b"".join(data for data, _ in streams)).hexdigest()})
        pending.extend(hashes)
    args.out.mkdir(parents=True, exist_ok=True)
    for name in pending:
        dest = args.out / name
        if dest.resolve() != (source / name).resolve(): shutil.copyfile(source / name, dest)
    manifest = {"schema": "idas3-course-paths-v1", "source": str(source),
        "format": "little-endian uint32 count, uint32 components=3, count packed XYZ float32 records",
        "coordinates": "Original native XYZ coordinates retained without resampling, smoothing, or scaling.",
        "unit_scale": 1, "metre_calibration": "Native guest QA labels distances metres; independent real-world calibration is unverified.",
        "race_endpoint_status": "Authoring-path endpoints are imported; exact game timing gates and race start offsets are not yet decoded.",
        "course_name_evidence": "Legacy race_condition_matrix_manifest.csv ExpectedCourseAssets mapping",
        "private_assets": True, "courses": records}
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"imported_courses": len(records), "files": len(pending), "out": str(args.out.resolve()),
        "total_points": sum(c["points"] for c in records)}, indent=2))

if __name__ == "__main__": main()
