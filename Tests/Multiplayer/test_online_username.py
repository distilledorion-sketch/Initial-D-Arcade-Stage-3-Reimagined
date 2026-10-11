"""Exercise production username validation and preference persistence with an isolated store.

No Unity player, local player preference, online identity or network is used.
Controller and IME rendering are covered separately by the Unity menu diagnostic.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/online-username-20261011/helper")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3OnlineUsername.cs", "Idas3OnlineUsernameChecks.cs")]
    sources = production + [Path(__file__).with_name("OnlineUsernameDriver.cs")]
    exe = proof / "OnlineUsernameDriver.exe"
    compiler = find_compiler(args.compiler)
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), *map(str, sources)],
                              capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(exe), str(proof)], capture_output=True, text=True, encoding="utf-8", timeout=30)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    validation = sum(int(line.split("\t", 1)[1]) for line in ran.stdout.splitlines() if line.startswith("VALIDATION\t"))
    report = {"passed": ran.returncode == 0, "count": len(checks) + validation, "validation_count": validation,
              "persistence_count": len(checks), "persistence_checks": checks,
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production},
              "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {report['count']} online username checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
