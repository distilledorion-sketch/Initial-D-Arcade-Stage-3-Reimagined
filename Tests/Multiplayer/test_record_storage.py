"""Exercise production online record resets with genuine JSON serialization.

All fixtures are fresh private output. No game saves, native runtime, EOS
account, cloud requests or real player identity is used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/record-storage")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs", "Idas3MultiplayerRecordChecks.cs")]
    sources = production + [Path(__file__).with_name(name) for name in (
        "MultiplayerRecordStorageFakes.cs", "MultiplayerRecordStorageDriver.cs")]
    library = ROOT / "ThirdParty/DiscordLegacyJson/Newtonsoft.Json.dll"
    if not library.is_file():
        raise RuntimeError("The repository's Newtonsoft.Json fixture dependency is unavailable")
    shutil.copyfile(library, proof / library.name)
    compiler = find_compiler(args.compiler)
    facade = Path(os.environ.get("WINDIR", "C:/Windows")) / "Microsoft.NET/Framework64/v4.0.30319/netstandard.dll"
    if not facade.is_file():
        raise RuntimeError("The .NET Framework netstandard facade is unavailable")
    exe = proof / "MultiplayerRecordStorageDriver.exe"
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe",
        "/out:" + str(exe), "/reference:" + str(library), "/reference:" + str(facade), *map(str, sources)],
        capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    # The framework test host retains legacy MAX_PATH handling. Keep its live
    # fixtures independent of the checkout/report path, then retain a copy with
    # the evidence. This does not alter the Unity game's storage behavior.
    fixture_root = Path(tempfile.mkdtemp(prefix="idas3-records-"))
    run = subprocess.run([str(exe), str(fixture_root / "run")],
        capture_output=True, text=True, encoding="utf-8")
    (proof / "checks.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    fixture_copy = proof / ("fixtures-" + uuid.uuid4().hex)
    shutil.copytree(fixture_root, fixture_copy)
    if fixture_root.resolve().parent != Path(tempfile.gettempdir()).resolve() or not fixture_root.name.startswith("idas3-records-"):
        raise RuntimeError("Refusing to clean an unexpected fixture path")
    shutil.rmtree(fixture_root)
    checks = [line.split("\t", 1)[1] for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": run.returncode == 0, "count": len(checks), "checks": checks,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},
        "compiler": str(compiler), "serializer": str(library), "fixtures": str(fixture_copy),
        "scope": "Production storage with real filesystem failures/recovery, real Newtonsoft public-field JSON and independent v1 checksum fixture. Deterministic injected progression. No Unity player, native progression, live EOS or real player saves."}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    print(f"PASS {len(checks)} record storage checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
