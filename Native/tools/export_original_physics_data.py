"""Export only identified vehicle tables and bounded path data; no opcodes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

SOURCE_SHA256 = "efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335"
BASE = 0x0C020000
SECTIONS = [
    ("mode_masks", 0x0C283E08, 32 * 4),
    ("path_bounds", 0x0C283E88, 18 * 8),
    ("car_geometry", 0x0C283F18, 35 * 44),
    ("frame_coefficients", 0x0C28451C, 18 * 35 * 4),
    ("vehicle_types", 0x0C284EF4, 35 * 4),
    ("loss_parameters", 0x0C284F80, 35 * 8),
    ("throttle_history_counts", 0x0C285098, 35 * 4),
    *[(f"angular_memory_{address:08x}", address, 9 * 35 * 4) for address in
      (0x0C285124, 0x0C285610, 0x0C285AFC, 0x0C285FE8, 0x0C2864D4,
       0x0C2869C0, 0x0C286EAC, 0x0C287398, 0x0C287884, 0x0C287D70)],
    ("transmission_profiles", 0x0C28825C, 2 * 88),
    ("transmission_rows_and_override", 0x0C28830C, 36 * 24),
    ("upgrade_coefficients", 0x0C28866C, 76 * 4),
]
def course_paths(image, condition, weather=0):
    """Exact042700 setup record, eight64-byte source paths."""
    address = 0x0C2EFF40 + condition * 1024 + weather * 512
    return [image[address - BASE + i * 64:address - BASE + (i + 1) * 64]
            .split(b"\0", 1)[0].decode("ascii") for i in range(8)]

def fnv1a(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value

def export(image_path, hostfs, output):
    image = image_path.read_bytes()
    if len(image) != 4194304 or hashlib.sha256(image).hexdigest() != SOURCE_SHA256:
        raise ValueError("Physics export requires the verified canonical GDS-0033 program")
    output.mkdir(parents=True, exist_ok=True)
    header = b"ID3TBL01" + struct.pack("<II", 1, len(SECTIONS)) + bytes.fromhex(SOURCE_SHA256)
    cursor = len(header) + len(SECTIONS) * 16
    directory, payload, manifest_sections = bytearray(), bytearray(), []
    for name, address, size in SECTIONS:
        data = image[address - BASE:address - BASE + size]
        directory += struct.pack("<IIII", address, size, cursor, fnv1a(data))
        payload += data
        manifest_sections.append(dict(name=name, original_address=f"0x{address:08X}",
                                      bytes=size, pack_offset=cursor, sha256=hashlib.sha256(data).hexdigest()))
        cursor += size
    tables = header + directory + payload
    (output / "tables.bin").write_bytes(tables)
    paths = []
    for condition in range(18):
        # Source scene order places VH/Happogahara before IR/Irohazaka.
        # Derive the stem from the original table rather than a UI enum.
        name = Path(course_paths(image, condition)[7]).name[5:] + ("o" if condition & 1 else "i")
        source = hostfs / "binary" / f"PATH_{name}_0.bin"
        raw = source.read_bytes()
        last_index = struct.unpack_from("<I", image, 0x0C283E88 - BASE + condition * 8)[0]
        count = last_index + 1
        size = count * 12
        if len(raw) < size:
            raise ValueError(f"Original physics path too short: {source}")
        exported = b"ID3PATH1" + struct.pack("<II", condition, count) + raw[:size]
        filename = f"path_{condition:02d}.bin"
        (output / filename).write_bytes(exported)
        paths.append(dict(condition_code=condition, source=source.relative_to(hostfs).as_posix(),
                          source_bytes=len(raw), source_sha256=hashlib.sha256(raw).hexdigest(),
                          file=filename, sha256=hashlib.sha256(exported).hexdigest(),
                          inclusive_last_index=last_index, exported_points=count,
                          policy="Exact original valid prefix; native XYZ float32 bits, no scale/reversal/resampling"))
    collision = []
    collision_sources = sorted({Path(record).name for condition in range(18)
                                for weather in range(2)
                                for record in course_paths(image, condition, weather)[5:7]})
    for stem in collision_sources:
        source = hostfs / "binary" / f"{stem}.bin.nz"
        raw = source.read_bytes()
        if struct.unpack_from("<II", raw) != (0x52434C31, 1):
            raise ValueError(f"Unsupported original collision format: {source}")
        filename = "collision_" + stem.replace("_colli", "") + ".rcl"
        (output / filename).write_bytes(raw)
        collision.append(dict(source=source.relative_to(hostfs).as_posix(), file=filename,
                              bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                              policy="Byte-for-byte RCL1 source, already uncompressed; no geometry changes"))
    manifest = dict(schema="idas3-original-physics-data-v1", source_program=image_path.name,
                    source_program_sha256=SOURCE_SHA256, source_program_base=f"0x{BASE:08X}",
                    tables_sha256=hashlib.sha256(tables).hexdigest(), tables_bytes=len(tables),
                    sections=manifest_sections, paths=paths, collision=collision,
                    limitations=["The upgrade table exports 76 identified coefficient cells. Values outside indices0..75 are not accepted; valid profile upgrade progression is not yet lifted.",
                                 "All14 unchanged RCL1 collision datasets referenced by the36 original course/weather records are included. Collision geometry is shared across weather records; weather-dependent vehicle state is separate.",
                                 "Car35 is an original special powertrain row, not a selectable model in the35-car frontend."])
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(SECTIONS)} exact data sections ({len(tables):,} bytes), {len(paths)} original physics paths, {len(collision)} unchanged collision datasets")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("hostfs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    export(args.image, args.hostfs, args.output)
