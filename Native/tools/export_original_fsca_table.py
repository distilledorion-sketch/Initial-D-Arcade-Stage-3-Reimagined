"""Extract the verified local hardware-math lookup data; no interpreter code."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

EXPECTED_SOURCE = "f7f2912cf098efd40c7507b8a91dbd8adba478449dd08bd0fdc883c5dafc3c04"
EXPECTED_PAYLOAD = "da5ed930c0102e1aa7c7f77a79697f16c52e52de68996b80d55b9192bb4bb26a"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_header", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source_header.read_bytes()
    if hashlib.sha256(source).hexdigest() != EXPECTED_SOURCE:
        raise ValueError("FSCA source header identity mismatch")
    words = [int(x, 16) for x in re.findall(rb"0x([0-9A-Fa-f]{8})", source)]
    if len(words) != 32768:
        raise ValueError("Expected exactly 32768 half-wave lookup words")
    payload = struct.pack("<32768I", *words)
    if hashlib.sha256(payload).hexdigest() != EXPECTED_PAYLOAD:
        raise ValueError("FSCA numerical data identity mismatch")
    output = b"ID3FSCA1" + struct.pack("<II", 32768, 0x18553798) + payload
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "fsca_table.bin").write_bytes(output)
    manifest = dict(schema="idas3-original-fsca-data-v1", source=str(args.source_header),
                    source_sha256=EXPECTED_SOURCE, payload_sha256=EXPECTED_PAYLOAD,
                    file="fsca_table.bin", bytes=len(output), file_sha256=hashlib.sha256(output).hexdigest(),
                    provenance="Local primary Flycast core/hw/sh4/fsca-table.h numerical lookup data, expanded by the documented sh4_rom.cpp phase/sign policy",
                    contract="32768 float32 words, phase0..32767; upper half XORs the sign bit; cosine adds0x4000 to the16-bit phase. Finite normal-FPSCR FSCA reference contract; not independent physical-silicon certification.",
                    runtime="Native matrix arithmetic uses this numerical table only; no interpreter or guest memory is loaded")
    (args.output / "fsca_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(words):,} verified FSCA words ({len(output):,} bytes)")
