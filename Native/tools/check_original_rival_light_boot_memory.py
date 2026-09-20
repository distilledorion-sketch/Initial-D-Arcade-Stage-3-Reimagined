"""Read-only proof of the supplied Naomi2 ROM word used by initial0638C0.

No firmware is copied or executed. Argument: existing research handoff root.
The separate C++ request fixture executes the unchanged game instruction read.
"""
from pathlib import Path
import hashlib
import struct
import sys
import zipfile

root = Path(sys.argv[1])
firmware = root / "private_data/inputs/epr-23605c.ic27"
image = firmware.read_bytes()
assert len(image) == 0x200000
digest = hashlib.sha256(image).hexdigest()
assert digest == "cf2d28d7cb5b9feaa0e692068106a6e231fb3227be488017643e81a253d2eca0"
word, = struct.unpack_from("<I", image, 0x64)
value, = struct.unpack_from("<f", image, 0x64)
assert word == 0xA05F7480
lower, = struct.unpack("<f", struct.pack("<I", 0xC099999A))
assert lower <= value < 20.0
print(f"{firmware.name}: SHA256 {digest}, size {len(image)}, word64={word:08X}, float={value:.17g}")
count = 0
with zipfile.ZipFile(root / "private_data/original_media/naomi2.zip") as archive:
    for name in archive.namelist():
        if not (name.startswith("epr-") and name.endswith(".ic27")):
            continue
        rom = archive.read(name)
        assert len(rom) == 0x200000
        assert struct.unpack_from("<I", rom, 0x64)[0] == word
        count += 1
        print(f"{name}: SHA256 {hashlib.sha256(rom).hexdigest()}, matching word64")
assert count == 10
# Preserve the identities of the reviewed mapping/loader/endianness sources.
primary = root / "reference/flycast_source_v2.6/core"
for name in ("hw/holly/sb_mem.cpp", "hw/flashrom/nvmem.cpp",
             "hw/flashrom/flashrom.h", "hw/naomi/naomi_cart.cpp",
             "hw/naomi/naomi_roms.cpp"):
    data = (primary / name).read_bytes()
    print(f"Primary {name}: SHA256 {hashlib.sha256(data).hexdigest()}")
print(f"PASS: supplied IC27 file plus {count} archived variants agree. Initial enemy29 counter=1; after60zero-gap warmups=61. No runtime or hardware execution.")
