"""Compare native patch application on disposable, production-sized inventories.

Both helpers receive the same checked plan and contents. No real game is opened,
no downloads occur, and the generated fixture is retained for inspection.
This times native apply only (including its own verification and cleanup), not
download or the managed preparation pass. Run after regression tests finish.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import struct
import subprocess
import time
import uuid


ROOT = Path(__file__).resolve().parents[2]
REQUIRED = ["InitialDUnity.exe", "UnityPlayer.dll",
            "InitialDUnity_Data/globalgamemanagers",
            "InitialDUnity_Data/Managed/Assembly-CSharp.dll"]


def text(value):
    encoded = str(value).encode("utf-16le")
    return struct.pack("<I", len(encoded) // 2) + encoded


def digest_file(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path,
                        default=ROOT / "Native/build-update-helper/Idas3UpdateInstaller.exe")
    parser.add_argument("--files", type=int, default=18044)
    parser.add_argument("--retained-bytes", type=int, default=3972844749)
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    assert args.files > len(REQUIRED) and args.rounds > 0
    assert args.retained_bytes >= args.files - len(REQUIRED)
    args.baseline = args.baseline.resolve()
    args.candidate = args.candidate.resolve()
    assert args.baseline.is_file() and args.candidate.is_file()
    base = ROOT / "Verification" / ("updater-benchmark-" + uuid.uuid4().hex)
    game = base / "fixture game"
    game.mkdir(parents=True)
    (base / "ISOLATED_UPDATE_TEST.txt").write_text("Disposable updater benchmark")
    private = game / "userdata/card.json"
    private.parent.mkdir()
    private.write_bytes(b"personal-save-must-not-change")
    original = b"old required file\0" * 16384
    changed = b"new required file\0" * 16384
    old_digest, new_digest = hashlib.sha256(original).digest(), hashlib.sha256(changed).digest()
    retained = args.files - len(REQUIRED)
    size, extra = divmod(args.retained_bytes, retained)
    # Matching total bytes/file count with uniformly sized synthetic assets
    # isolates filesystem and verification overhead from archive compression.
    blocks = {length: b"retained scene\0" * (length // 15) + b"R" * (length % 15)
              for length in (size, size + 1)}
    hashes = {length: hashlib.sha256(block).digest() for length, block in blocks.items()}
    records = []
    timestamps = {}
    started = time.perf_counter()
    for i in range(retained):
        name = ("InitialDUnity_Data/StreamingAssets/OriginalAssets/data/cars/"
                f"car_{i // 200:03d}/textures/chunks/asset_{i:05d}.bin")
        path = game / name
        path.parent.mkdir(parents=True, exist_ok=True)
        length = size + (i < extra)
        path.write_bytes(blocks[length])
        records.append((name, length, hashes[length]))
        timestamps[name] = path.stat().st_mtime_ns
    fixture_seconds = time.perf_counter() - started
    print(f"Created {args.files:,} file / {args.retained_bytes:,} retained-byte fixture "
          f"in {fixture_seconds:.2f}s", flush=True)
    results = []
    for round_number in range(args.rounds):
        order = [("baseline", args.baseline), ("candidate", args.candidate)]
        if round_number % 2:
            order.reverse()
        for label, helper in order:
            session = base / f"session-{round_number}-{label}"
            session.mkdir()
            (session / "backup").mkdir()
            plan = bytearray(b"IDUPD002" + text(game) + struct.pack("<IQI", 0, 0, args.files))
            for name in REQUIRED:
                path = game / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(original)
                stage = session / "stage" / name
                stage.parent.mkdir(parents=True, exist_ok=True)
                stage.write_bytes(changed)
                plan += text(name) + struct.pack("<BBQ", 1, 1, len(changed)) + old_digest + new_digest
            for name, length, checksum in records:
                plan += text(name) + struct.pack("<BBQ", 0, 1, length) + checksum + checksum
            plan_path = session / "install.plan"
            plan_path.write_bytes(plan)
            started = time.perf_counter()
            wall_started = time.time()
            process = subprocess.run([str(helper), str(plan_path), "--test"],
                                     capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW,
                                     timeout=900)
            elapsed = time.perf_counter() - started
            error = (session / "error.txt").read_text() if (session / "error.txt").exists() else ""
            assert process.returncode == 0, (label, process.returncode, error)
            status = json.loads((session / "result.json").read_text())
            assert status == {"passed": True, "changedFiles": len(REQUIRED)}, status
            for name in REQUIRED:
                assert (game / name).read_bytes() == changed, name
            assert private.read_bytes() == b"personal-save-must-not-change"
            for name, _, _ in records:
                assert (game / name).stat().st_mtime_ns == timestamps[name], name
            ready_at = (session / "ready").stat().st_mtime
            committed_at = (session / "result.json").stat().st_mtime
            results.append(dict(helper=label, round=round_number + 1, seconds=elapsed,
                                preflightSeconds=max(0, ready_at - wall_started),
                                finalVerificationApplySeconds=max(0, committed_at - ready_at),
                                changedFiles=status["changedFiles"], retainedFilesUntouched=retained))
            print(f"{label} round {round_number + 1}: {elapsed:.3f}s PASS", flush=True)
    medians = {label: statistics.median(r["seconds"] for r in results if r["helper"] == label)
               for label in ("baseline", "candidate")}
    report = dict(passed=True, scope="Native verification/apply/cleanup only; synthetic assets, uncontrolled filesystem cache/machine load; no download or managed preparation timing.",
                  fixture=str(base), fileCount=args.files, retainedFileCount=retained,
                  retainedBytes=args.retained_bytes, changedPayloadBytes=len(changed) * len(REQUIRED),
                  baselineSha256=digest_file(args.baseline), candidateSha256=digest_file(args.candidate),
                  fixtureSeconds=fixture_seconds, runs=results, medianSeconds=medians,
                  medianReductionPercent=(1 - medians["candidate"] / medians["baseline"]) * 100,
                  redundantRetainedReadBytesRemoved=args.retained_bytes,
                  nativeRetainedHashPasses=dict(baseline=2, candidate=1),
                  managedRetainedIntegrityPassStillRequired=True,
                  finalNativeRetainedIntegrityPassStillRequired=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps(dict(passed=True, medianSeconds=medians,
                          medianReductionPercent=report["medianReductionPercent"])), flush=True)


if __name__ == "__main__":
    main()
