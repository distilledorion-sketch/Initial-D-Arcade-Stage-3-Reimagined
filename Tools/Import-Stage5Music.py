#!/usr/bin/env python3
"""Import only Stage 5 race Eurobeat AFS entries 0..13, with their original ADX bytes.

Ending entry 14 (Lady Butterfly) is deliberately excluded. No transcoding,
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

SOURCE_URL = "https://shop.mu-mo.net/mitem/AVCA-29098"
# AFS supplies truncated filenames; the unique full SONG:AVEX_* YBO cue names
# supply each remaining character. The AFS directory order is checked directly;
# YBO string order and soundtrack album order are never used as archive indices.
SONGS = (
    ("avex_02_sunintherain", "Sun in the Rain", "Manuel"),
    ("avex_03_lookabomba", "Looka Bomba", "Go 2"),
    ("avex_04_sweetsixteengirl", "Sweet Sixteen Girl", "Candy Taylor"),
    ("avex_05_loveisanameoflove", "Love Is the Name of Love", "Irene"),
    ("avex_06_adrenaline", "Adrenaline", "Manuel"),
    ("avex_07_blackufo", "Black U.F.O.", "Lupin"),
    ("avex_08_discofire", "Disco Fire", "Dave Rodgers"),
    ("avex_09_midnightlove", "Midnight Love", "Neo"),
    ("avex_10_gasgasgas", "Gas Gas Gas", "Manuel"),
    ("avex_11_chemicallove", "Chemical Love", "Kevin & Cherry"),
    ("avex_12_rockinhardcore", "Rockin' Hardcore", "Fastway"),
    ("avex_13_speedman", "Speed Man", "Dave Simon"),
    ("avex_14_fighting", "Fighting", "Cody"),
    ("avex_15_rightnow", "Right Now", "Dark Angels"),
)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def atomic_write(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".stage5-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def run(project: Path, afs: Path, source_ybo: Path, check: bool) -> dict:
    spec = importlib.util.spec_from_file_location("music_export", project / "Tools/Export-MusicCatalog.py")
    require(spec is not None and spec.loader is not None, "Cannot load catalog exporter")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    streams = project / "Native/data/original_audio/streams"
    catalog_header = project / "Native/src/music_catalog.h"
    old_header = catalog_header.read_text(encoding="utf-8-sig")
    entries = exporter.parse_entries(old_header)
    require(len(entries) in (44, 58), "Expected 44 existing or 58 already imported tracks")
    expected_ids = ["stage5." + song[0] for song in SONGS]
    require(len(entries) == 44 or [entry["id"] for entry in entries[44:]] == expected_ids,
            "Existing Stage 5 IDs/order differ")
    old_catalog = json.loads((streams / "music_catalog.json").read_bytes())
    require(len(old_catalog["tracks"]) in (44, 58), "Unexpected exported catalog count")
    prior_assets = {}
    for track in old_catalog["tracks"][:44]:
        require(track["index"] == len(prior_assets), "Existing catalog indices are not contiguous")
        data = (streams / track["relativePath"]).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        require(digest == track["sha256"], f"Existing asset hash mismatch: {track['id']}")
        prior_assets[track["relativePath"]] = digest

    source_ybo_bytes = source_ybo.read_bytes()
    require(source_ybo_bytes[:4] == b"YABX" and b"CAudio::CACueSheet" in source_ybo_bytes
            and b"inid5.afs" in source_ybo_bytes, "Unexpected Stage 5 cue-sheet container")
    cue_names = [match.group().decode("ascii") for match in
                 re.finditer(rb"SONG:AVEX_[A-Z0-9_]+", source_ybo_bytes)]
    expected_cues = {"SONG:" + code.upper() for code, _, _ in SONGS}
    expected_cues.add("SONG:AVEX_16_LADYBUTTERFLY")
    require(len(cue_names) == len(set(cue_names)) == 15 and set(cue_names) == expected_cues,
            "Unexpected Stage 5 AVEX cue roster")
    pending_assets, imports, titles = [], [], []
    archive_size = afs.stat().st_size
    with afs.open("rb") as stream:
        magic, count = struct.unpack("<4sI", stream.read(8))
        require(magic == b"AFS\0" and count == 7427, "Expected original Stage 5 AFS with 7427 entries")
        table = stream.read(count * 8 + 8)
        require(len(table) == count * 8 + 8, "Truncated AFS directory")
        names_offset, names_size = struct.unpack_from("<II", table, count * 8)
        require(names_size == count * 48 and names_offset + names_size <= archive_size,
                "Invalid AFS filename directory")
        for index, (code, title, artist) in enumerate(SONGS):
            filename = code + ".adx"
            offset, size = struct.unpack_from("<II", table, index * 8)
            require(count * 8 + 16 <= offset and 0 < size <= 64 * 1024 * 1024
                    and offset + size <= names_offset, f"Invalid AFS entry bounds: {index}")
            stream.seek(names_offset + index * 48)
            short_name = stream.read(32).split(b"\0", 1)[0].decode("ascii")
            # The AFS's 20-byte name must identify exactly one full YBO cue.
            # This is independent of the shuffled YBO cue-string order.
            candidates = [cue for cue in cue_names if
                          (cue.removeprefix("SONG:").lower() + ".adx")[:20] == short_name]
            require(candidates == ["SONG:" + code.upper()], f"AFS/YBO filename mismatch: {index}")
            stream.seek(offset)
            data = stream.read(size)
            require(len(data) == size, f"Truncated AFS payload: {index}")
            metadata = exporter.inspect_adx(data, filename)
            require(metadata["looping"] and metadata["channels"] == 2
                    and metadata["sampleRate"] == 44100, f"Unexpected Stage 5 race format: {filename}")
            relative = "stage5/" + filename
            destination = streams / relative
            require(not destination.exists() or destination.read_bytes() == data,
                    f"Refusing to overwrite differing native asset: {destination}")
            require(not check or destination.exists(), f"Imported asset missing: {destination}")
            pending_assets.append((destination, data))
            identity = "stage5." + code
            imports.append(dict(id=identity, sourceStage=5, code=code, filename=filename,
                                path="data/original_audio/streams/" + relative,
                                sourceArchive="inid5.afs", sourceArchiveEntryIndex=index,
                                sourceArchiveByteOffset=offset, sourceArchiveName=short_name,
                                sourceCue="SONG:" + code.upper(),
                                displayLabel=title, titleKnown=True, **metadata))
            titles.append(dict(id=identity, title=title, artist=artist, sources=[SOURCE_URL],
                               fileMappingEvidence=f"inid5.afs entry {index}, offset {offset}, "
                               f"has embedded filename prefix {short_name!r}, matching only "
                               f"inid5.ybo cue SONG:{code.upper()}. The full filename is reconstructed "
                               "from that cue and the ADX format. Title and artist verified against "
                               "Avex mu-mo's Stage 5 original soundtrack listing AVCA-29098."))
        stream.seek(names_offset + 14 * 48)
        require(stream.read(32).split(b"\0", 1)[0].decode("ascii") == "avex_16_ladybutterfly.adx"[:20],
                "Unexpected excluded ending filename")
        ending_offset, ending_size = struct.unpack_from("<II", table, 14 * 8)
        require(0 < ending_size <= 64 * 1024 * 1024 and ending_offset + ending_size <= names_offset,
                "Invalid excluded ending bounds")
        stream.seek(ending_offset)
        ending = exporter.inspect_adx(stream.read(ending_size), "avex_16_ladybutterfly.adx")
        require(not ending["looping"], "Expected a non-looping Lady Butterfly ending stream")
    # Hash the read-only archive and cue sheet for reproducible provenance.
    with afs.open("rb") as stream:
        archive_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    for record in imports:
        record["sourceArchiveSha256"] = archive_hash
    manifest_path = streams / "extra_music_manifest.json"
    title_path = streams / "music_title_metadata.json"
    manifest = json.loads(manifest_path.read_bytes())
    title_document = json.loads(title_path.read_bytes())
    prior_imports = [track for track in manifest["tracks"] if track["sourceStage"] != 5]
    require(len(prior_imports) == 31, "Expected 31 preserved Stage 1/2/4 imports")
    manifest["tracks"] = prior_imports + imports
    manifest["trackCount"] = len(manifest["tracks"])
    manifest["transformation"] = "none; native SPSD/ADX bytes and original filenames preserved"
    manifest["stage5Import"] = dict(sourceArchive="inid5.afs", sourceArchiveSha256=archive_hash,
        sourceCueSheet="inid5.ybo", sourceCueSheetSha256=hashlib.sha256(source_ybo_bytes).hexdigest(),
        archiveEntryIndices=list(range(14)), excludedEndingEntry=14,
        excludedEndingCode="avex_16_ladybutterfly", excludedEndingLoops=False,
        titlesSource=SOURCE_URL, originalLoopMetadataPreserved=True,
        preservedPriorCatalogSha256=prior_assets)
    prior_titles = [track for track in title_document["tracks"] if not track["id"].startswith("stage5.")]
    require(len(prior_titles) == 44, "Expected 44 preserved title records")
    title_document["tracks"] = prior_titles + titles
    new_header = old_header
    if len(entries) == 44:
        new_header = re.sub(r"(std::array\s*<\s*RaceMusicTrack\s*,\s*)44(\s*>)", r"\g<1>58\2", old_header)
        declaration = exporter.DECLARATION.search(new_header)
        require(declaration is not None, "Cannot locate catalog append point")
        at = declaration.end(2)
        lines = "".join("    {" + ", ".join((json.dumps("stage5." + code), json.dumps(title),
                      "5", json.dumps("stage5/" + code + ".adx"), json.dumps(artist))) + "},\n"
                        for code, title, artist in SONGS)
        new_header = new_header[:at] + lines + new_header[at:]
    new_entries = exporter.parse_entries(new_header)
    require(new_entries[:44] == entries[:44], "Existing native catalog entries changed")
    for entry, (code, title, artist) in zip(new_entries[44:], SONGS):
        require(entry == dict(index=entry["index"], id="stage5."+code, title=title, sourceStage=5,
                             relativePath="stage5/"+code+".adx", artist=artist),
                "Stage 5 native catalog metadata differs")
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
    require(result["tracks"][:44] == old_catalog["tracks"][:44], "Existing exported track records changed")
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
    return dict(status="verified" if check else "imported", trackCount=58, stage5Count=14,
                totalImportedBytes=sum(len(data) for _, data in pending_assets),
                catalogIndices=list(range(44, 58)), prior44CatalogRecordsUnchanged=True,
                prior49NativeStreamsUnchanged=True, sourceArchiveSha256=archive_hash,
                tracks=[dict(index=44+i, **record) for i, record in enumerate(imports)])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--afs", type=Path, required=True)
    parser.add_argument("--ybo", type=Path, help="Defaults to inid5.ybo beside the source AFS")
    parser.add_argument("--check", action="store_true", help="Verify all imports; write nothing")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = run(args.project_root.resolve(), args.afs.resolve(),
                 (args.ybo or args.afs.with_suffix(".ybo")).resolve(), args.check)
    if args.report:
        atomic_write(args.report.resolve(), json_bytes(result))
    print(json.dumps({key:value for key,value in result.items() if key != "tracks"}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        raise SystemExit(f"Stage 5 import failed: {error}") from error
