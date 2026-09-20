#!/usr/bin/env python3
"""Export the native race catalog and verify SPSD/ADX/MS ADPCM assets without decoding audio.

Default paths are relative to this script, so invocation does not depend on cwd.
The explicit C++ entry literals are authoritative; unsupported syntax fails closed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct
import tempfile


DECLARATION = re.compile(
    r"inline\s+constexpr\s+std::array\s*<\s*RaceMusicTrack\s*,\s*(\d+)\s*>"
    r"\s+raceMusicCatalog\s*\{\{(.*?)\}\}\s*;", re.DOTALL)
STRING = r'"(?:[^"\\]|\\.)*"'
ENTRY = re.compile(
    r"\s*\{\s*(" + STRING + r")\s*,\s*(" + STRING + r")\s*,\s*"
    r"(10|[12345678])\s*,\s*(" + STRING + r")\s*,\s*(" + STRING + r")\s*\}\s*,?")
ORIGINAL_IDS = (
    "stage3.01_gamble_rumble", "stage3.02_speedy_speed_boy", "stage3.03_remember_me",
    "stage3.04_save_me", "stage3.05_over_the_rainbow", "stage3.06_stop_your_self_control",
    "stage3.07_crazy_for_love", "stage3.08_express_love", "stage3.09_blackout",
    "stage3.10_fall_in_the_web", "stage3.11_pamela", "stage3.12_fight_for_love_tonight",
    "stage3.13_dancin_in_my_dreams",
)
PRE_STAGE4_IDS = ORIGINAL_IDS + (
    "stage1.EZ001", "stage1.MD001", "stage1.HR001", "stage1.VH002", "stage1.VH003", "stage1.EZ002",
    "stage2.EZ001", "stage2.EZ002", "stage2.NM001", "stage2.NM002", "stage2.HD001", "stage2.HD002",
    "stage2.DF001", "stage2.DF002", "stage2.VH001", "stage2.UH001", "stage2.UH002",
)
PRE_STAGE5_IDS = PRE_STAGE4_IDS + (
    "stage4.avex_02_letsgocomeon", "stage4.avex_03_gobeatcrazy", "stage4.avex_04_speedcar",
    "stage4.avex_05_flytometothemoon", "stage4.avex_06_revolution", "stage4.avex_07_wellseeheaven",
    "stage4.avex_08_allaround", "stage4.avex_09_eldorado", "stage4.avex_10_raisinhell",
    "stage4.avex_11_spacelove", "stage4.avex_12_nocontrol", "stage4.avex_13_foreveryoung",
    "stage4.avex_14_riderofthesky", "stage4.avex_15_thefiresonme",
)
PRE_STAGE6_IDS = PRE_STAGE5_IDS + (
    "stage5.avex_02_sunintherain", "stage5.avex_03_lookabomba", "stage5.avex_04_sweetsixteengirl",
    "stage5.avex_05_loveisanameoflove", "stage5.avex_06_adrenaline", "stage5.avex_07_blackufo",
    "stage5.avex_08_discofire", "stage5.avex_09_midnightlove", "stage5.avex_10_gasgasgas",
    "stage5.avex_11_chemicallove", "stage5.avex_12_rockinhardcore", "stage5.avex_13_speedman",
    "stage5.avex_14_fighting", "stage5.avex_15_rightnow",
)
PRE_STAGE7_IDS = PRE_STAGE6_IDS + (
    "stage6.avex_01_super_rider", "stage6.avex_02_rock_beaten_wild", "stage6.avex_03_once_upon_a_time",
    "stage6.avex_04_set_me_free", "stage6.avex_05_king_of_eurobeat", "stage6.avex_06_the_love_bite",
    "stage6.avex_07_euro_night", "stage6.avex_08_queen_of_meam", "stage6.avex_09_mad_desire",
    "stage6.avex_10_burn_into_the_beat", "stage6.avex_11_forever_sad", "stage6.avex_12_hurricane_man",
    "stage6.avex_13_dont_turn_it_off", "stage6.avex_14_you_are_my_wonder",
)
PRE_STAGE8_IDS = PRE_STAGE7_IDS + (
    "stage7.avex_01_disconnected", "stage7.avex_02_remember_me", "stage7.avex_03_night_of_fire",
    "stage7.avex_04_i_need_a_revolution", "stage7.avex_05_power_two", "stage7.avex_06_crazy_for_love",
    "stage7.avex_07_burning_up_the_night(total_fire)", "stage7.avex_08_freedom_ride",
    "stage7.avex_09_ministry_of_power", "stage7.avex_10_speed_of_light", "stage7.avex_11_the_top",
    "stage7.avex_12_up_and_dance_up_and_go", "stage7.avex_13_pamela", "stage7.avex_14_limousine",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_entries(header: str) -> list[dict]:
    declarations = list(DECLARATION.finditer(header))
    require(len(declarations) == 1, "Expected exactly one explicit raceMusicCatalog declaration")
    declaration = declarations[0]
    expected_count, body = int(declaration[1]), declaration[2]
    entries, at = [], 0
    while body[at:].strip():
        match = ENTRY.match(body, at)
        require(match is not None, f"Unsupported catalog entry syntax near: {body[at:at+100]!r}")
        assert match is not None
        identity, title, stage, relative, artist = match.groups()
        entries.append(dict(index=len(entries), id=json.loads(identity), title=json.loads(title),
                            sourceStage=int(stage), relativePath=json.loads(relative),
                            artist=json.loads(artist)))
        at = match.end()
    require(len(entries) == expected_count, "C++ declared catalog size differs from explicit entries")
    require(tuple(item["id"] for item in entries[:13]) == ORIGINAL_IDS,
            "Original Stage 3 indices 0..12 were changed")
    require(tuple(item["id"] for item in entries[:30]) == PRE_STAGE4_IDS,
            "Existing Stage 1/2/3 indices 0..29 were changed")
    if len(entries) >= 44 or any(item["sourceStage"] == 5 for item in entries):
        require(tuple(item["id"] for item in entries[:44]) == PRE_STAGE5_IDS,
                "Existing Stage 1/2/3/4 indices 0..43 were changed")
    if len(entries) >= 58 or any(item["sourceStage"] == 6 for item in entries):
        require(tuple(item["id"] for item in entries[:58]) == PRE_STAGE6_IDS,
                "Existing Stage 1/2/3/4/5 indices 0..57 were changed")
    if len(entries) >= 72 or any(item["sourceStage"] == 7 for item in entries):
        require(tuple(item["id"] for item in entries[:72]) == PRE_STAGE7_IDS,
                "Existing Stage 1/2/3/4/5/6 indices 0..71 were changed")
    if len(entries) >= 86 or any(item["sourceStage"] == 8 for item in entries):
        require(tuple(item["id"] for item in entries[:86]) == PRE_STAGE8_IDS,
                "Existing Stage 1/2/3/4/5/6/7 indices 0..85 were changed")
    require(len({item["id"] for item in entries}) == len(entries), "Duplicate track ID")
    require(len({item["relativePath"].casefold() for item in entries}) == len(entries),
            "Duplicate Windows asset path")
    for item in entries:
        prefix = "specialstage." if item["sourceStage"] == 10 else f"stage{item['sourceStage']}."
        require(item["id"].startswith(prefix), "ID/source stage mismatch")
        require(bool(item["title"]) and bool(item["artist"]), "Empty track title or artist")
        relative = item["relativePath"]
        path = PurePosixPath(relative)
        require(relative and "\\" not in relative and ":" not in relative
                and not path.is_absolute() and ".." not in path.parts
                and path.as_posix() == relative
                and path.suffix.lower() == (".wav" if item["sourceStage"] in (6, 7, 8) else ".adx" if item["sourceStage"] in (4, 5, 10) else ".bin"),
                f"Invalid portable asset path: {relative}")
    return entries


def inspect_spsd(data: bytes, label: str) -> dict:
    require(len(data) >= 64 and data[:4] == b"SPSD", f"Invalid SPSD header: {label}")
    require(data[4:8] in (bytes.fromhex("01010004"), bytes.fromhex("00010004")),
            f"Unsupported SPSD version: {label}")
    codec, flags, interleave, size = struct.unpack_from("<BBHI", data, 8)
    channels = 2 if flags & 3 else 1
    rate = struct.unpack_from("<H", data, 42)[0]
    require(codec in (0, 1, 3), f"Unsupported SPSD codec: {label}")
    require(8000 <= rate <= 48000 and 0 < size <= 64*1024*1024
            and size <= len(data)-64 and size % channels == 0,
            f"Invalid SPSD sample bounds: {label}")
    require(interleave in (13, 255) or (interleave == 0 and channels == 1),
            f"Unsupported SPSD interleave: {label}")
    per_channel = size // channels
    require(codec != 0 or per_channel % 2 == 0, f"Odd PCM16 sample length: {label}")
    frames = per_channel * 2 if codec == 3 else per_channel // 2 if codec == 0 else per_channel
    adjustment = 16384 if codec == 3 else 4096 if codec == 0 else 8192
    loop_start = struct.unpack_from("<I", data, 44)[0] + adjustment
    require(not flags & 128 or loop_start < frames, f"Invalid SPSD loop bounds: {label}")
    return dict(sampleRate=rate, channels=channels, frames=frames, durationSeconds=frames/rate,
                sha256=hashlib.sha256(data).hexdigest(), bytes=len(data))


def inspect_adx(data: bytes, label: str) -> dict:
    """Validate unencrypted standard ADX v3/v4 files, including Special Stage.

    Header/loop layout: vgmstream src/meta/adx.c. Frames here mean decoded
    sample frames, not the 18-byte ADPCM blocks (32 samples per channel).
    The source stream, including loop padding, history and trailer, is untouched.
    """
    require(len(data) >= 40 and data[:2] == b"\x80\x00", f"Invalid ADX header: {label}")
    start = struct.unpack_from(">H", data, 2)[0] + 4
    encoding, block, bits, channels = struct.unpack_from("4B", data, 4)
    rate, frames, cutoff, version = struct.unpack_from(">IIHH", data, 8)
    require(encoding == 3 and block == 18 and bits == 4 and channels in (1, 2)
            and version in (0x0300, 0x0400), f"Unsupported or encrypted ADX: {label}")
    require(8000 <= rate <= 48000 and 0 < frames <= rate * 3600
            and 0 < cutoff < rate // 2, f"Invalid ADX audio bounds: {label}")
    history_end = 0x14 if version == 0x0300 else 0x18 + max(8, channels * 4)
    require(history_end + 6 <= start <= len(data)
            and data[start-6:start] == b"(c)CRI", f"Invalid ADX data offset: {label}")
    encoded_bytes = ((frames + 31) // 32) * block * channels
    require(encoded_bytes <= len(data) - start, f"Truncated ADX samples: {label}")
    # This importer deliberately supports only the observed standard loop layout;
    # unknown extensions fail rather than being mistaken for loop metadata.
    require(data[history_end+4:history_end+8] != b"AINF",
            f"Unsupported ADX AINF extension: {label}")
    looping, loop_start, loop_end = False, 0, 0
    padding, loop_start_byte, loop_end_byte = 0, 0, 0
    if start - 6 >= history_end + 24:
        padding, loop_type, loop_flag, loop_start, loop_start_byte, loop_end, loop_end_byte = \
            struct.unpack_from(">HHIIIII", data, history_end)
        require(padding <= 31 and loop_type in (0, 1) and loop_flag in (0, 1),
                f"Invalid ADX loop flags: {label}")
        looping = bool(loop_flag)
        if looping:
            require(0 <= loop_start < loop_end <= frames and loop_start % 32 == 0
                    and loop_start_byte == start + (loop_start // 32) * block * channels
                    and loop_end_byte == start + ((loop_end + 31) // 32) * block * channels,
                    f"Invalid ADX loop bounds: {label}")
    return dict(sampleRate=rate, channels=channels, frames=frames,
                durationSeconds=frames/rate, sha256=hashlib.sha256(data).hexdigest(),
                bytes=len(data), format="ADX", codec="CRI ADX ADPCM", version=version >> 8,
                bitsPerSample=bits, blockBytes=block, highpassHz=cutoff,
                encodedDataOffset=start, encodedDataBytes=encoded_bytes,
                looping=looping, loopStartFrames=loop_start, loopEndFrames=loop_end,
                loopStartByteOffset=loop_start_byte, loopEndByteOffset=loop_end_byte,
                initialLoopPaddingFrames=padding)


def inspect_msadpcm_wav(data: bytes, label: str) -> dict:
    """Inspect original XWB ADPCM bytes inside a lossless RIFF container wrapper.

    fact preserves the exact frame count; smpl's inclusive endpoint becomes
    the mixer's exclusive endpoint. No block alignment is imposed on the loop.
    """
    require(len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WAVE"
            and struct.unpack_from("<I", data, 4)[0] + 8 == len(data), f"Invalid WAVE extent: {label}")
    chunks, at = {}, 12
    while at < len(data):
        require(at + 8 <= len(data), f"Truncated WAVE chunk header: {label}")
        tag, size = struct.unpack_from("<4sI", data, at)
        body = at + 8
        require(body + size + (size & 1) <= len(data), f"Truncated WAVE chunk: {label}")
        if tag in (b"fmt ", b"fact", b"smpl", b"data"):
            require(tag not in chunks, f"Duplicate WAVE chunk: {label}")
            chunks[tag] = (body, size)
        at = body + size + (size & 1)
    require(all(tag in chunks for tag in (b"fmt ", b"fact", b"data")), f"Missing WAVE chunk: {label}")
    fmt, fmt_size = chunks[b"fmt "]
    fact, fact_size = chunks[b"fact"]
    payload, encoded_bytes = chunks[b"data"]
    require(fmt_size == 50 and fact_size == 4, f"Unsupported MS ADPCM headers: {label}")
    codec, channels, rate, avg, block, bits, extra, per_block, count = struct.unpack_from("<HHIIHHHHH", data, fmt)
    coefficients = struct.unpack_from("<14h", data, fmt + 22)
    require(codec == 2 and bits == 4 and extra == 32 and count == 7
            and coefficients == (256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232),
            f"Unsupported MS ADPCM format or coefficients: {label}")
    require(channels in (1, 2) and 8000 <= rate <= 48000
            and 7 * channels <= block <= 8192 and block % channels == 0
            and per_block == (block - 7 * channels) * 2 // channels + 2
            and avg == rate * block // per_block, f"Invalid MS ADPCM format: {label}")
    frames = struct.unpack_from("<I", data, fact)[0]
    require(0 < encoded_bytes <= 64 * 1024 * 1024 and encoded_bytes % block == 0
            and 0 < frames <= rate * 3600
            and (encoded_bytes // block - 1) * per_block < frames <= encoded_bytes // block * per_block,
            f"Invalid MS ADPCM sample bounds: {label}")
    looping, loop_start, loop_end = False, 0, 0
    if b"smpl" in chunks:
        loop, size = chunks[b"smpl"]
        require(size == 60, f"Unsupported WAVE loop layout: {label}")
        count, extra = struct.unpack_from("<II", data, loop + 28)
        _, mode, loop_start, inclusive_end, fraction, repeats = struct.unpack_from("<6I", data, loop + 36)
        loop_end = inclusive_end + 1
        require(count == 1 and extra == mode == fraction == repeats == 0
                and 0 <= loop_start < loop_end <= frames, f"Invalid WAVE loop bounds: {label}")
        looping = True
    return dict(sampleRate=rate, channels=channels, frames=frames, durationSeconds=frames/rate,
                sha256=hashlib.sha256(data).hexdigest(), bytes=len(data), format="WAVE",
                codec="Microsoft ADPCM", bitsPerSample=bits, blockBytes=block, samplesPerBlock=per_block,
                encodedDataOffset=payload, encodedDataBytes=encoded_bytes,
                encodedDataSha256=hashlib.sha256(data[payload:payload+encoded_bytes]).hexdigest(),
                looping=looping, loopStartFrames=loop_start, loopEndFrames=loop_end)


def export(project: Path) -> dict:
    native = project / "Native"
    header_path = native / "src/music_catalog.h"
    header_bytes = header_path.read_bytes()
    entries = parse_entries(header_bytes.decode("utf-8-sig"))
    streams = (native / "data/original_audio/streams").resolve()
    imports_path = streams / "extra_music_manifest.json"
    imports_bytes = imports_path.read_bytes()
    imports = json.loads(imports_bytes)
    titles_bytes = (streams / "music_title_metadata.json").read_bytes()
    titles_document = json.loads(titles_bytes)
    require(titles_document.get("schema") == "idas-race-music-titles-v1", "Unsupported music title metadata")
    titles = {record["id"]: record for record in titles_document["tracks"]}
    require(len(titles) == len(titles_document["tracks"]) == len(entries)
            and set(titles) == {entry["id"] for entry in entries},
            "Title metadata and native catalog rosters differ")
    require(imports.get("schema") == "idas-extra-native-music-v1", "Unsupported extra music manifest")
    imported = {record["id"]: record for record in imports["tracks"]}
    require(len(imported) == len(imports["tracks"]) == imports["trackCount"],
            "Duplicate IDs or count mismatch in extra music manifest")
    require({item["id"] for item in entries if item["sourceStage"] != 3} == set(imported),
            "Catalog and extra music manifest rosters differ")
    original_hashes = imports["verification"]["stage3Sha256"]
    tracks = []
    for item in entries:
        path = (streams / item["relativePath"]).resolve()
        require(path.is_relative_to(streams), f"Asset escapes streams root: {item['id']}")
        metadata = (inspect_msadpcm_wav if item["sourceStage"] in (6, 7, 8) else inspect_adx if item["sourceStage"] in (4, 5, 10) else inspect_spsd)(
            path.read_bytes(), item["id"])
        title = titles[item["id"]]
        require(title["title"] == item["title"] and title["artist"] == item["artist"]
                and title["sources"] and title["fileMappingEvidence"],
                f"Missing or mismatched verified title metadata: {item['id']}")
        item = dict(item, assetPath="data/original_audio/streams/" + item["relativePath"],
                    **metadata, titleVerified=True, artist=title["artist"],
                    titleSources=title["sources"])
        if item["sourceStage"] != 3:
            source = imported[item["id"]]
            for field in ("sourceStage", "sampleRate", "channels", "frames", "sha256", "bytes"):
                require(item[field] == source[field], f"Extra import {field} mismatch: {item['id']}")
            if item["sourceStage"] in (4, 5, 6, 7, 8, 10):
                for field in metadata:
                    require(item[field] == source[field],
                            f"Encoded audio import {field} mismatch: {item['id']}")
            require(item["assetPath"] == source["path"], f"Extra import path mismatch: {item['id']}")
        else:
            require(metadata["sha256"] == original_hashes.get(item["relativePath"]),
                    f"Preserved Stage 3 hash mismatch: {item['id']}")
        tracks.append(item)
    return dict(schema="idas-race-music-catalog-v1", sourceHeader="src/music_catalog.h",
                sourceHeaderSha256=hashlib.sha256(header_bytes).hexdigest(),
                titleMetadataSha256=hashlib.sha256(titles_bytes).hexdigest(),
                extraImportManifestSha256=hashlib.sha256(imports_bytes).hexdigest(),
                trackCount=len(tracks), tracks=tracks)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check", action="store_true", help="Validate that existing JSON matches; write nothing")
    args = parser.parse_args()
    project = args.project_root.resolve()
    result = export(project)
    output = project / "Native/data/original_audio/streams/music_catalog.json"
    if args.check:
        require(json.loads(output.read_text(encoding="utf-8")) == result, "Catalog JSON is stale; rerun exporter")
    else:
        # Publish only after every header/hash/entry has passed validation.
        fd, name = tempfile.mkstemp(prefix=".music_catalog-", suffix=".tmp", dir=output.parent)
        try:
            with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
                stream.write(json.dumps(result, indent=2) + "\n")
            os.replace(name, output)
        finally:
            if os.path.exists(name):
                os.unlink(name)
    counts = {stage: sum(track["sourceStage"] == stage for track in result["tracks"]) for stage in (1, 2, 3, 4, 5, 6, 7, 8, 10)}
    print(json.dumps(dict(status="verified" if args.check else "exported", path=str(output),
                          tracks=result["trackCount"], stages=counts,
                          nativeAudioBytesChanged=0), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(f"Music catalog export failed: {error}") from error
