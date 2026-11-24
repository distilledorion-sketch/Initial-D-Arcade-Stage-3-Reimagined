"""Restore original program data referenced by the native Windows player."""
import hashlib
import json
from pathlib import Path

def checksum(data):
    return hashlib.sha256(data).hexdigest()

def build(library, program):
    # Match aligned, byte-exact spans in the compiled bridge to the canonical
    # arcade program. The distributed recipe contains only address/length tuples.
    width = 24
    candidates = {}
    for i in range(0, len(library) - width + 1, 4):
        key = library[i:i + width]
        if len(set(key)) >= 4:
            candidates.setdefault(key, []).append(i)
    matches = {}
    for j in range(0, len(program) - width + 1, 4):
        key = program[j:j + width]
        offsets = candidates.pop(key, None)
        if offsets:
            matches.update((i, j) for i in offsets)
    cleaned = bytearray(library)
    spans = []
    cursor = 0
    for i, j in sorted(matches.items()):
        if i < cursor:
            continue
        size = width
        while i+size < len(library) and j+size < len(program) and library[i+size] == program[j+size]:
            size += 1
        spans.append((i, j, size))
        cleaned[i:i+size] = bytes(size)
        cursor = i+size
    if not spans:
        raise ValueError("Could not locate expected original tables in the native player.")
    return cleaned, {"library": checksum(library), "template": checksum(cleaned), "spans": spans}

def restore(program_file, setup, destination):
    recipe = json.loads((setup / "native-recipe.json").read_text())
    cleaned = bytearray((setup / "native-player.template").read_bytes())
    if checksum(cleaned) != recipe["template"]:
        raise ValueError("Damaged native player template.")
    program = program_file.read_bytes()
    for start, source, size in recipe["spans"]:
        if min(start, source, size) < 0 or start+size > len(cleaned) or source+size > len(program):
            raise ValueError("Invalid native program data reference.")
        cleaned[start:start+size] = program[source:source+size]
    if checksum(cleaned) != recipe["library"]:
        raise ValueError("Disc data could not restore the native game library.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(cleaned)

