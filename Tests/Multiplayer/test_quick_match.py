"""Exercise production discovery fallback with deterministic service boundaries.

No Unity player, account, network connection or player save is used.
Unity Matchmaker and Relay lifecycle coverage belongs to their separate tests.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/unity-online-migration/quick-match")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerTypes.cs", "Idas3OnlineActivity.cs", "Idas3QuickMatch.cs")]
    sources = production + [Path(__file__).with_name("QuickMatchDriver.cs")]
    compiler = find_compiler(args.compiler)
    exe = proof / "QuickMatchDriver.exe"
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe",
        "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True)
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(exe)], capture_output=True, text=True)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": ran.returncode == 0, "count": len(checks), "checks": checks,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production},
        "compiler": str(compiler),
        "scope": "Production discovery fallback: retries, service interruption, room selection, host convergence, cancellation and handshake transition with deterministic room/service boundaries. No cloud matchmaking or Relay traffic."}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {len(checks)} discovery checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
