"""Exercise durable result intent and restart recovery on private filesystem data.

Uses real record storage and owned child-process termination. With --native,
profile recovery calls the actual Idas3Unity ABI; otherwise only that boundary
is simulated. Does not run a race, initialize a scene, or contact online services.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--native", type=Path)
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=False)
    sources = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3ResultJournal.cs", "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs",
        "Idas3MultiplayerRecordChecks.cs")]
    sources += [Path(__file__).with_name(name) for name in (
        "MultiplayerRecordStorageFakes.cs", "ResultJournalDriver.cs")]
    library = ROOT / "ThirdParty/DiscordLegacyJson/Newtonsoft.Json.dll"
    shutil.copyfile(library, proof / library.name)
    compiler = find_compiler(args.compiler)
    facade = Path(os.environ.get("WINDIR", "C:/Windows")) / "Microsoft.NET/Framework64/v4.0.30319/netstandard.dll"
    exe = proof / "ResultJournalDriver.exe"
    command = [str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/platform:x64",
        "/out:" + str(exe), "/reference:" + str(library), "/reference:" + str(facade), *map(str, sources)]
    compiled = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=120)
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    # Keep current path handling under the same modern Framework behavior as
    # the other Windows test hosts, rather than its legacy MAX_PATH defaults.
    Path(str(exe) + ".config").write_text(
        '<configuration><runtime><AppContextSwitchOverrides value="Switch.System.IO.UseLegacyPathHandling=false;Switch.System.IO.BlockLongPaths=false" /></runtime></configuration>',
        encoding="utf-8")
    native_hash = None
    if args.native:
        native = proof / "Idas3Unity.dll"
        shutil.copyfile(args.native.resolve(), native)
        native_hash = hashlib.sha256(native.read_bytes()).hexdigest()
    run = subprocess.run([str(exe), str(proof / "fixtures"), "native" if args.native else "fake"],
        capture_output=True, text=True, encoding="utf-8", timeout=180)
    (proof / "checks.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": run.returncode == 0, "count": len(checks), "checks": checks,
        "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sources + [Path(__file__)]},
        "nativeProfileRecoverySha256": native_hash, "nativeProfileRecoveryTested": bool(args.native),
        "naturalRaceTested": False, "compiler": str(compiler), "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    print(f"PASS {len(checks)} result journal checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
