#!/usr/bin/env python3
"""Import only Stage 4 race Eurobeat AFS entries 1..14, with their original ADX bytes.

Opening entry 0 and ending entry 15 are deliberately excluded. No transcoding,
normalization, decoding, or modification of the source installation is performed.
Safe to rerun: existing assets must match byte-for-byte or the import fails.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import tempfile

SOURCE_URL = "https://shop.mu-mo.net/mitem/AVCA-26336?jsiteid=mumo"
# Source AFS entry order, verified with the accompanying inid4.h and embedded AFS
# filename table. The full native filename (including native typos) is retained.
SONGS = (
    ("avex_02_letsgocomeon", "Let's Go, Come On", "Manuel"),
    ("avex_03_gobeatcrazy", "Go Beat Crazy", "Fastway"),
    ("avex_04_speedcar", "Speed Car", "D-Team"),
    ("avex_05_flytometothemoon", "Fly to Me to the Moon & Back", "The Spiders from Mars"),
    ("avex_06_revolution", "Revolution", "Fastway"),
    ("avex_07_wellseeheaven", "We'll See Heaven", "Digital Planet"),
    ("avex_08_allaround", "All Around", "Lia"),
    ("avex_09_eldorado", "Eldorado", "Dave Rodgers"),
    ("avex_10_raisinhell", "Raising Hell", "Fastway"),
    ("avex_11_spacelove", "Space Love", "Fastway"),
    ("avex_12_nocontrol", "No Control", "Manuel"),
    ("avex_13_foreveryoung", "Forever Young", "Symbol"),
    ("avex_14_riderofthesky", "Rider of the Sky", "Ace"),
    ("avex_15_thefiresonme", "The Fire's on Me", "Spock"),
)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def atomic_write(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".stage4-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def run(project: Path, afs: Path, source_header: Path, check: bool) -> dict:
    spec = importlib.util.spec_from_file_location("music_export", project / "Tools/Export-MusicCatalog.py")
    require(spec is not None and spec.loader is not None, "Cannot load catalog exporter")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    streams = project / "Native/data/original_audio/streams"
    catalog_header = project / "Native/src/music_catalog.h"
    old_header = catalog_header.read_text(encoding="utf-8-sig")
    entries = exporter.parse_entries(old_header)
    require(len(entries) in (30, 44), "Expected 30 existing or 44 already imported tracks")
    expected_ids = ["stage4." + song[0] for song in SONGS]
    require(len(entries) == 30 or [entry["id"] for entry in entries[30:]] == expected_ids,
            "Existing Stage 4 IDs/order differ")
    old_catalog = json.loads((streams / "music_catalog.json").read_bytes())
    require(len(old_catalog["tracks"]) in (30, 44), "Unexpected exported catalog count")
    prior_assets = {}
    for track in old_catalog["tracks"][:30]:
        require(track["index"] == len(prior_assets), "Existing catalog indices are not contiguous")
        data = (streams / track["relativePath"]).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        require(digest == track["sha256"], f"Existing asset hash mismatch: {track['id']}")
        prior_assets[track["relativePath"]] = digest

    source_header_bytes = source_header.read_bytes()
    mappings = dict((int(index), filename) for index, filename in re.findall(
        r"#define\s+SONG:AVEX_\S+\s+\((\d+)\)\s*/\*\s*\.\./adx_song/(\S+)\s*\*/",
        source_header_bytes.decode("ascii")))
    require(mappings.get(0) == "avex_01_raimei.adx"
            and mappings.get(15) == "avex_16_namida3000.adx", "Unexpected opening/ending entries")
    pending_assets, imports, titles = [], [], []
    archive_size = afs.stat().st_size
    with afs.open("rb") as stream:
        magic, count = struct.unpack("<4sI", stream.read(8))
        require(magic == b"AFS\0" and count == 6130, "Expected original Stage 4 AFS with 6130 entries")
        table = stream.read(count * 8 + 8)
        require(len(table) == count * 8 + 8, "Truncated AFS directory")
        names_offset, names_size = struct.unpack_from("<II", table, count * 8)
        require(names_size == count * 48 and names_offset + names_size <= archive_size,
                "Invalid AFS filename directory")
        for index, (code, title, artist) in enumerate(SONGS, 1):
            filename = code + ".adx"
            require(mappings.get(index) == filename, f"Source header mapping mismatch at {index}")
            offset, size = struct.unpack_from("<II", table, index * 8)
            require(count * 8 + 16 <= offset and 0 < size <= 64 * 1024 * 1024
                    and offset + size <= names_offset, f"Invalid AFS entry bounds: {index}")
            stream.seek(names_offset + index * 48)
            short_name = stream.read(32).split(b"\0", 1)[0].decode("ascii")
            # This bank's internal names are truncated to 20 bytes; the header
            # above supplies full filenames for the first sixteen exact entries.
            require(short_name == filename[:20], f"AFS filename/header mismatch: {index}")
            stream.seek(offset)
            data = stream.read(size)
            require(len(data) == size, f"Truncated AFS payload: {index}")
            metadata = exporter.inspect_adx(data, filename)
            require(metadata["looping"] and metadata["channels"] == 2
                    and metadata["sampleRate"] == 44100, f"Unexpected Stage 4 race format: {filename}")
            relative = "stage4/" + filename
            destination = streams / relative
            require(not destination.exists() or destination.read_bytes() == data,
                    f"Refusing to overwrite differing native asset: {destination}")
            require(not check or destination.exists(), f"Imported asset missing: {destination}")
            pending_assets.append((destination, data))
            identity = "stage4." + code
            imports.append(dict(id=identity, sourceStage=4, code=code, filename=filename,
                                path="data/original_audio/streams/" + relative,
                                sourceArchive="inid4.afs", sourceArchiveEntryIndex=index,
                                sourceArchiveByteOffset=offset, sourceArchiveName=short_name,
                                sourceRelativePath="../adx_song/" + filename,
                                displayLabel=title, titleKnown=True, **metadata))
            titles.append(dict(id=identity, title=title, artist=artist, sources=[SOURCE_URL],
                               fileMappingEvidence=f"inid4.afs entry {index}, offset {offset}, "
                               f"has embedded filename prefix {short_name!r}; inid4.h maps the same "
                               f"entry to ../adx_song/{filename}. Title and artist verified against "
                               "Avex mu-mo's Stage 4 original soundtrack listing AVCA-26336."))
    # Hash the read-only archive and source header for reproducible provenance.
    with afs.open("rb") as stream:
        archive_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    for record in imports:
        record["sourceArchiveSha256"] = archive_hash
    manifest_path = streams / "extra_music_manifest.json"
    title_path = streams / "music_title_metadata.json"
    manifest = json.loads(manifest_path.read_bytes())
    title_document = json.loads(title_path.read_bytes())
    prior_imports = [track for track in manifest["tracks"] if track["sourceStage"] != 4]
    require(len(prior_imports) == 17, "Expected 17 preserved Stage 1/2 imports")
    manifest["tracks"] = prior_imports + imports
    manifest["trackCount"] = len(manifest["tracks"])
    manifest["transformation"] = "none; native SPSD/ADX bytes and original filenames preserved"
    manifest["stage4Import"] = dict(sourceArchive="inid4.afs", sourceArchiveSha256=archive_hash,
        sourceHeader="inid4.h", sourceHeaderSha256=hashlib.sha256(source_header_bytes).hexdigest(),
        archiveEntryIndices=list(range(1, 15)), excludedOpeningEntry=0, excludedEndingEntry=15,
        titlesSource=SOURCE_URL, originalLoopMetadataPreserved=True,
        preservedPriorCatalogSha256=prior_assets)
    prior_titles = [track for track in title_document["tracks"] if not track["id"].startswith("stage4.")]
    require(len(prior_titles) == 30, "Expected 30 preserved title records")
    title_document["tracks"] = prior_titles + titles
    new_header = old_header
    if len(entries) == 30:
        new_header = re.sub(r"(std::array\s*<\s*RaceMusicTrack\s*,\s*)30(\s*>)", r"\g<1>44\2", old_header)
        declaration = exporter.DECLARATION.search(new_header)
        require(declaration is not None, "Cannot locate catalog append point")
        at = declaration.end(2)
        lines = "".join("    {" + ", ".join((json.dumps("stage4." + code), json.dumps(title),
                      "4", json.dumps("stage4/" + code + ".adx"), json.dumps(artist))) + "},\n"
                        for code, title, artist in SONGS)
        new_header = new_header[:at] + lines + new_header[at:]
    new_entries = exporter.parse_entries(new_header)
    require(new_entries[:30] == entries[:30], "Existing native catalog entries changed")
    for entry, (code, title, artist) in zip(new_entries[30:], SONGS):
        require(entry == dict(index=entry["index"], id="stage4."+code, title=title, sourceStage=4,
                             relativePath="stage4/"+code+".adx", artist=artist),
                "Stage 4 native catalog metadata differs")
    pending_metadata = [(catalog_header, new_header.encode("utf-8")),
                        (manifest_path, json_bytes(manifest)), (title_path, json_bytes(title_document))]
    if check:
        for path, data in pending_metadata:
            require(path.read_text(encoding="utf-8-sig") == data.decode("utf-8"),
                    f"Imported metadata is stale: {path}")
    else:
        for path, data in pending_assets + pending_metadata:
            atomic_write(path, data)
    result = exporter.export(project)
    require(result["tracks"][:30] == old_catalog["tracks"][:30], "Existing exported track records changed")
    output = streams / "music_catalog.json"
    if check:
        require(json.loads(output.read_bytes()) == result, "Exported catalog is stale")
    else:
        atomic_write(output, json_bytes(result))
    for relative, digest in prior_assets.items():
        require(hashlib.sha256((streams / relative).read_bytes()).hexdigest() == digest,
                f"Prior asset changed: {relative}")
    # All original Stage 3 non-race streams must remain unchanged as well.
    for relative, digest in manifest["verification"]["stage3Sha256"].items():
        require(hashlib.sha256((streams / relative).read_bytes()).hexdigest() == digest,
                f"Original Stage 3 stream changed: {relative}")
    return dict(status="verified" if check else "imported", trackCount=44, stage4Count=14,
                totalImportedBytes=sum(len(data) for _, data in pending_assets),
                catalogIndices=list(range(30, 44)), prior30CatalogRecordsUnchanged=True,
                prior35NativeStreamsUnchanged=True, sourceArchiveSha256=archive_hash,
                tracks=[dict(index=29+i, **record) for i, record in enumerate(imports, 1)])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--afs", type=Path, required=True)
    parser.add_argument("--header", type=Path, help="Defaults to inid4.h beside the source AFS")
    parser.add_argument("--check", action="store_true", help="Verify all imports; write nothing")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = run(args.project_root.resolve(), args.afs.resolve(),
                 (args.header or args.afs.with_suffix(".h")).resolve(), args.check)
    if args.report:
        atomic_write(args.report.resolve(), json_bytes(result))
    print(json.dumps({key:value for key,value in result.items() if key != "tracks"}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        raise SystemExit(f"Stage 4 import failed: {error}") from error
