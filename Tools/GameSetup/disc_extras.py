"""Package Reimagined's additional courses and music separately from the GDS-0033 cache."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tempfile

STREAMING = "InitialDUnity_Data/StreamingAssets/"
CACHE = "IDAS3/data/"
SOURCES = "setup/additional-data/"


def ranked_files(repository):
    inventory = repository / "Assets/Resources/RankedMaps.txt"
    rows = {}
    for line in inventory.read_text(encoding="utf-8").splitlines():
        name, size, checksum = line.split("\t")
        rows[name] = [int(size), checksum]
    return rows


def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def member(root, name):
    relative = PurePosixPath(name)
    if not relative.parts or relative.is_absolute() or ".." in relative.parts or "\\" in name or ":" in name:
        raise ValueError(f"Invalid content path: {name}")
    return root.joinpath(*relative.parts)


def map_packs(files):
    return {name.split("/")[0] for name in files if re.fullmatch(r"[A-Z][A-Z0-9_]*/menu\.idastex", name)}


def maps_folder(streaming):
    if streaming.parts[-2:] != ("InitialDUnity_Data", "StreamingAssets"):
        raise ValueError(f"Expected the game's StreamingAssets folder: {streaming}")
    return streaming.parents[1] / "Mods/Maps"


def installed_file(streaming, name, packs):
    root = maps_folder(streaming) if name.partition("/")[0] in packs else streaming
    return member(root, name)


def song_paths(repository):
    native = (repository / "Native/src/music_catalog.h").read_text(encoding="utf-8")
    return {key: path for key, stage, path in re.findall(
        r'\{"([^"]+)",\s*"[^"]+",\s*(\d+),\s*"([^"]+)"', native) if stage != "3"}


def write_content(archive, streaming, repository, music):
    """Select by shipped catalogs; never traverse the base game's IDAS3 tree."""
    files = {}

    def add(name, source, content=None):
        if name in files:
            return
        dest = SOURCES + name[len(CACHE):] if name.startswith(CACHE) else STREAMING + name
        if content is None:
            if not source.is_file():
                raise ValueError(f"Missing additional game content: {source}")
            archive.write(source, dest)
            files[name] = [source.stat().st_size, digest(source)]
        else:
            archive.writestr(dest, content)
            files[name] = [len(content), hashlib.sha256(content).hexdigest()]

    for name in ("Mesh.json", "Shadow.png", "Body.png"):
        filename = name if name.endswith(".json") else name + ".bytes"
        add("TimeAttackGhost/" + name, repository / "Assets/Resources/TimeAttackGhost" / filename)

    files.update(ranked_files(repository))
    titles = CACHE + "original_assets/hud/imported_start_names/titles.idastex"
    add(titles, streaming / titles)

    catalog = json.loads((streaming / "SoundRoom/catalog.json").read_text(encoding="utf-8-sig"))
    entries = [track for track in catalog["tracks"] if track["stage"] != 3]
    songs = song_paths(repository)
    encoded = json.loads((music / "encoding.json").read_text())
    if set(encoded) != set(songs.values()):
        raise ValueError("Incomplete encoded song set")
    retired = {}
    if not songs or {t["key"] for t in entries if t["id"] < 2000} != songs.keys():
        raise ValueError("Sound Room and native song catalogs disagree")
    for rel in songs.values():
        if len(PurePosixPath(rel).parts) != 2:
            raise ValueError(f"Expected an additional race soundtrack: {rel}")
        name = CACHE + "original_audio/streams/" + rel
        source = member(streaming, name)
        compressed = member(music, rel).with_suffix(".mp3")
        if digest(source) != encoded[rel]["source"] or digest(compressed) != encoded[rel]["mp3"]:
            raise ValueError(f"Re-encode the changed song {rel}")
        add(str(PurePosixPath(name).with_suffix(".mp3")), compressed)
        retired[name[len(CACHE):]] = encoded[rel]["source"]
    archive.writestr("setup/previous-song-hashes.json", json.dumps(retired, sort_keys=True))
    for track in entries:
        if not track.get("preview") or (track["id"] >= 2000 and not track.get("audio")):
            raise ValueError(f"Incomplete Sound Room track {track['key']}")
        for field, suffix in (("preview", ".mp3"), ("audio", ".ogg"), ("cover", ".png")):
            rel = track.get(field)
            if rel:
                if PurePosixPath(rel).suffix.lower() != suffix:
                    raise ValueError(f"Unexpected {field} for {track['key']}")
                add("SoundRoom/" + rel, member(streaming / "SoundRoom", rel))
    catalog["tracks"] = entries
    add("SoundRoom/catalog.json", None, json.dumps(catalog, ensure_ascii=False, separators=(",", ":")).encode())
    archive.writestr("setup/additional-content.json", json.dumps(files, sort_keys=True))
    return {STREAMING + name for name in files if not name.startswith(CACHE)}


def manifest(setup):
    return json.loads((setup / "additional-content.json").read_text(encoding="utf-8"))


def install_cache_files(project, setup, files, receipt):
    """Add just the supplemental files consumed by the native asset loader."""
    updated = False
    for name, (size, expected) in files.items():
        if not name.startswith(CACHE):
            continue
        rel = name[len(CACHE):]
        dest = member(project / "data", rel)
        if receipt.get(rel) == expected and dest.is_file() and dest.stat().st_size == size:
            continue
        source = member(setup / "additional-data", rel)
        if source.stat().st_size != size or digest(source) != expected:
            raise ValueError(f"Re-extract the game ZIP to restore {name}.")
        dest.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dest.parent) as temp:
            stage = Path(temp) / "content"
            try:
                os.link(source, stage)
            except OSError:
                shutil.copyfile(source, stage)
            os.replace(stage, dest)
        receipt[rel] = expected
        updated = True
    # Remove only byte-identical originals that this edition previously installed.
    # Writing the MP3s first also makes a partial upgrade safe to retry.
    previous = json.loads((setup / "previous-song-hashes.json").read_text(encoding="utf-8"))
    for rel, old_hash in previous.items():
        old_rel = PurePosixPath(rel)
        replacement = old_rel.with_suffix(".mp3").as_posix()
        if (old_rel.parts[:2] != ("original_audio", "streams") or len(old_rel.parts) != 4 or
                replacement == rel or CACHE + replacement not in files):
            raise ValueError(f"Invalid previous song entry: {rel}")
        for root in (project / "data", setup / "additional-data"):
            path = member(root, rel)
            if path.is_file() and digest(path) == old_hash:
                path.unlink()
        if receipt.get(rel) == old_hash and not member(project / "data", rel).exists():
            del receipt[rel]
            updated = True
    return updated


def verify_external(streaming, files):
    packs = map_packs(files)
    missing = {pack for pack in packs if not (maps_folder(streaming) / pack).is_dir()}
    for name, (size, expected) in files.items():
        if name.startswith(CACHE) or name.partition("/")[0] in missing:
            continue
        path = installed_file(streaming, name, packs)
        if not path.is_file() or path.stat().st_size != size or digest(path) != expected:
            raise ValueError(f"Additional game file failed verification: {name}")

