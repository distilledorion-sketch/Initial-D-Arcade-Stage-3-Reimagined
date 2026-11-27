"""Restore original program data referenced by the native Windows player."""
import hashlib
import json
from pathlib import Path

def checksum(data):
    return hashlib.sha256(data).hexdigest()

def find_spans(library, program, excluded=()):
    # Match aligned, byte-exact spans in the compiled bridge to the canonical
    # arcade program. The distributed recipe contains only address/length tuples.
    width = 24
    candidates = {}
    removed = bytearray(len(library))
    for start, _, size in excluded:
        removed[start:start+size] = b"\x01"*size
    for i in range(0, len(library) - width + 1, 4):
        if excluded and removed.find(1, i, i+width) != -1:
            continue
        key = library[i:i + width]
        if len(set(key)) >= 4:
            candidates.setdefault(key, []).append(i)
    matches = {}
    for j in range(0, len(program) - width + 1, 4):
        key = program[j:j + width]
        offsets = candidates.pop(key, None)
        if offsets:
            matches.update((i, j) for i in offsets)
    spans = []
    cursor = 0
    for i, j in sorted(matches.items()):
        if i < cursor:
            continue
        size = width
        while i+size < len(library) and j+size < len(program) and not removed[i+size] and library[i+size] == program[j+size]:
            size += 1
        spans.append((i, j, size))
        cursor = i+size
    return spans

def build(library, program):
    spans = find_spans(library, program)
    if not spans:
        raise ValueError("Could not locate expected original tables in the native player.")
    cleaned = bytearray(library)
    for start, _, size in spans:
        cleaned[start:start+size] = bytes(size)
    return cleaned, {"library": checksum(library), "template": checksum(cleaned), "spans": spans}

def load_template(setup):
    recipe = json.loads((setup / "native-recipe.json").read_text())
    cleaned = bytearray((setup / "native-player.template").read_bytes())
    if checksum(cleaned) != recipe["template"]:
        raise ValueError("Damaged native player template.")
    return recipe, cleaned

def write_library(cleaned, recipe, destination):
    if checksum(cleaned) != recipe["library"]:
        raise ValueError("Disc data could not restore the native game library.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(cleaned)

def restore_from_cache(previous_file, setup, destination):
    previous = previous_file.read_bytes()
    layouts = json.loads((setup / "native-base-spans.json").read_text())
    spans = layouts.get(checksum(previous))
    if spans is None:
        return False
    # Resolve through original program offsets. The compiler can move these
    # bytes to different DLL offsets or split a span between releases.
    original = {}
    for start, source, size in spans:
        if min(start, source, size) < 0 or start + size > len(previous):
            raise ValueError("Invalid previous native player layout.")
        original.update(zip(range(source, source + size), previous[start:start + size]))
    recipe, cleaned = load_template(setup)
    for start, source, size in recipe["spans"]:
        if min(start, source, size) < 0 or start + size > len(cleaned):
            raise ValueError("Invalid native program data reference.")
        try:
            cleaned[start:start + size] = bytes(original[i] for i in range(source, source + size))
        except KeyError:
            return False  # The new version needs bytes absent from the old cache.
    write_library(cleaned, recipe, destination)
    return True

def restore(program_file, setup, destination):
    recipe, cleaned = load_template(setup)
    program = program_file.read_bytes()
    for start, source, size in recipe["spans"]:
        if min(start, source, size) < 0 or start+size > len(cleaned) or source+size > len(program):
            raise ValueError("Invalid native program data reference.")
        cleaned[start:start+size] = program[source:source+size]
    write_library(cleaned, recipe, destination)

