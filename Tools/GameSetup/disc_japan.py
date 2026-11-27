"""Read the original Japanese Rev C disc using the player's matching PIC."""
import ctypes as ct
import hashlib
from pathlib import Path
import struct
import zlib


PAYLOAD_BYTES = 241831936
PAYLOAD_SHA256 = "5aaa5833179d787ea3b77bbe1d8b08d4dc226d076b30c77b1b9a7d2c4facfaff"


def reverse_words(block):
    result = bytearray(len(block))
    for lane in range(8):
        result[lane::8] = block[7-lane::8]
    return result


def read_volume(track, pic):
    # Rev C's primary volume puts INID.BIN at LBA 430911 of a Mode 1 GD-ROM.
    # Verify the entire decoded payload before using any offsets or filenames.
    security = pic.read_bytes()
    key = (security[0x780:0x78e:2] + security[0x7a0:0x7a1])[::-1]
    crypto = ct.WinDLL("bcrypt")
    algorithm, session = ct.c_void_p(), ct.c_void_p()
    def check(status):
        if status != 0:
            raise OSError(f"Windows could not read the disc security format (0x{status & 0xffffffff:08x}).")
    check(crypto.BCryptOpenAlgorithmProvider(ct.byref(algorithm), "DES", None, 0))
    try:
        ecb = ct.create_unicode_buffer("ChainingModeECB")
        check(crypto.BCryptSetProperty(algorithm, "ChainingMode", ecb, ct.sizeof(ecb), 0))
        check(crypto.BCryptGenerateSymmetricKey(algorithm, ct.byref(session), None, 0, key, len(key), 0))
        payload = bytearray()
        with track.open("rb") as raw:
            raw.seek((430911 - 45000) * 2352)
            while len(payload) < PAYLOAD_BYTES:
                sectors = min(1024, (PAYLOAD_BYTES - len(payload)) // 2048)
                batch = raw.read(sectors * 2352)
                if len(batch) != sectors * 2352:
                    raise ValueError("Japanese disc data track is incomplete.")
                sector_data = b"".join(batch[i+16:i+2064] for i in range(0, len(batch), 2352))
                block = reverse_words(sector_data)
                buffer = (ct.c_ubyte * len(block)).from_buffer(block)
                count = ct.c_ulong()
                check(crypto.BCryptDecrypt(session, buffer, len(block), None, None, 0, buffer,
                                           len(block), ct.byref(count), 0))
                if count.value != len(block):
                    raise ValueError("Windows returned an incomplete disc block.")
                payload.extend(reverse_words(block))
    finally:
        if session:
            crypto.BCryptDestroyKey(session)
        crypto.BCryptCloseAlgorithmProvider(algorithm, 0)
    if hashlib.sha256(payload).hexdigest() != PAYLOAD_SHA256:
        raise ValueError("Japanese Rev C disc data failed verification.")
    return payload


def extract(track, pic, output, *, expand_assets=False):
    payload = memoryview(read_volume(track, pic))
    volume_start = 0x800000
    def extent(block, size):
        offset = volume_start + block * 2048
        if offset + size > len(payload):
            raise ValueError("Invalid extent in Japanese disc filesystem.")
        return payload[offset:offset+size]
    descriptor = extent(17, 2048)
    if bytes(descriptor[:7]) != b"\x02CD001\x01" or bytes(descriptor[88:91]) not in (b"%/@", b"%/C", b"%/E"):
        raise ValueError("Japanese disc filesystem is missing.")
    output.mkdir()
    code, address, length = struct.unpack_from("<3I", payload, 0x360)
    if (code, address, length) != (0x500, 0x0c020000, 0x400000):
        raise ValueError("Unexpected Japanese game program layout.")
    (output / "idas3_main_0C020000.bin").write_bytes(payload[code:code+length])
    root = output / "driveA/HOSTFS"
    root.mkdir(parents=True)
    pending = [(struct.unpack_from("<I", descriptor, 158)[0], struct.unpack_from("<I", descriptor, 166)[0], root)]
    seen = set()
    while pending:
        block, size, parent = pending.pop()
        if block in seen:
            raise ValueError("Duplicate directory in Japanese disc filesystem.")
        seen.add(block)
        folder = extent(block, size)
        at = 0
        while at < len(folder):
            length = folder[at]
            if not length:
                at = (at // 2048 + 1) * 2048
                continue
            entry = folder[at:at+length]
            at += length
            if len(entry) < 34 or 33 + entry[32] > len(entry):
                raise ValueError("Invalid directory record in Japanese disc.")
            raw_name = bytes(entry[33:33+entry[32]])
            if raw_name in (b"\0", b"\1"):
                continue
            name = raw_name.decode("utf-16-be").split(";", 1)[0]
            if not name or name in (".", "..") or any(c in name for c in "/\\:"):
                raise ValueError("Invalid filename in Japanese disc.")
            child = parent / name
            start, count = struct.unpack_from("<I", entry, 2)[0], struct.unpack_from("<I", entry, 10)[0]
            if entry[25] & 2:
                child.mkdir()
                pending.append((start, count, child))
            else:
                content = extent(start, count)
                if expand_assets and content[:5] == b"NMZIP":
                    expected_size, = struct.unpack_from("<I", content, 28)
                    content = zlib.decompress(content[32:])
                    if len(content) != expected_size:
                        raise ValueError(f"Invalid packed asset length: {name}")
                with child.open("xb") as stream:
                    stream.write(content)
    return output
