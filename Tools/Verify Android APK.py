"""Verify packaged runtime data and ARM64 libraries without installing the APK.

Usage: python "Tools/Verify Android APK.py" Builds/Android/InitialDUnity.apk
Android apksigner/zipalign must also be run; this script does not verify signing.
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path


def digest(stream):
    h = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        h.update(chunk)
    return h.hexdigest()


def elf_load_alignment(data):
    if data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Expected little-endian ELF64")
    if struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError("Expected ARM64 e_machine")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    result = []
    for i in range(count):
        header = struct.unpack_from("<IIQQQQQQ", data, offset + size * i)
        if header[0] == 1:
            result.append(header[7])
    if not result:
        raise ValueError("ELF has no LOAD segments")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    args = parser.parse_args()
    apk = args.apk.resolve()
    report = {"apk": str(apk), "bytes": apk.stat().st_size, "libraries": {}}
    errors = []
    if report["bytes"] >= 0xFFFFFFFF:
        errors.append("APK exceeds ZIP32 file size")
    with apk.open("rb") as stream:
        report["sha256"] = digest(stream)
    with zipfile.ZipFile(apk) as z:
        names = z.namelist()
        if len(names) != len(set(names)):
            errors.append("Duplicate ZIP entries")
        if len(names) >= 65535:
            errors.append("ZIP64 entry count")
        for name in ("AndroidManifest.xml", "classes.dex", "lib/arm64-v8a/libIdas3Unity.so", "lib/arm64-v8a/libil2cpp.so"):
            if name not in names:
                errors.append("Missing " + name)
        manifest = json.loads(z.read("assets/IDAS3/data.manifest.json"))
        if manifest.get("schema") != "idas3-unity-runtime-data-v1":
            errors.append("Unexpected manifest schema")
        entries = manifest["files"]
        for i, entry in enumerate(entries):
            name = "assets/IDAS3/data/" + entry["path"]
            try:
                info = z.getinfo(name)
                if info.file_size != entry["bytes"]:
                    errors.append("Size mismatch: " + name)
                with z.open(info) as stream:
                    if digest(stream).lower() != entry["sha256"].lower():
                        errors.append("SHA256 mismatch: " + name)
            except (KeyError, zipfile.BadZipFile) as error:
                errors.append(str(error))
            if (i + 1) % 2000 == 0:
                print(f"Verified {i + 1}/{len(entries)} runtime files", flush=True)
        report["runtime_files_verified"] = len(entries)
        # A bundled ROM is optional (private builds only); public APKs ask the
        # player to import their own copy, so verify it only when present.
        report["rom_sha256"] = None
        if "assets/rom/gds-0033.chd" in names:
            with z.open("assets/rom/gds-0033.chd") as stream:
                report["rom_sha256"] = digest(stream)
            if report["rom_sha256"] != "9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e":
                errors.append("ROM SHA256 mismatch")
        for name in names:
            if name.startswith("lib/") and name.endswith(".so"):
                data = z.read(name)
                alignment = elf_load_alignment(data)
                report["libraries"][name] = {
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "load_alignment": alignment,
                    "supports_16kb_alignment": all(n >= 16384 for n in alignment),
                }
                if any(n < 16384 for n in alignment):
                    errors.append("ELF below 16 KB alignment: " + name)
        # Also read the non-manifest entries so zipfile checks their CRCs.
        covered = {"assets/IDAS3/data/" + e["path"] for e in entries}
        covered.add("assets/rom/gds-0033.chd")
        for info in z.infolist():
            if not info.is_dir() and info.filename not in covered:
                with z.open(info) as stream:
                    digest(stream)
        report["zip_entries"] = len(names)
        report["uncompressed_bytes"] = sum(i.file_size for i in z.infolist())
    report["errors"] = errors
    report["passed"] = not errors
    output = apk.with_suffix(".verification.json")
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
