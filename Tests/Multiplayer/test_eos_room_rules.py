"""Execute production EOS queue convergence and lobby-membership rules.

The actual pure production rules are compiled unchanged. These tests do not
mock successful live EOS requests and do not establish cancellation, packet
delivery, public service configuration or multi-PC behavior.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/room-rules")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = ROOT / "Assets/Scripts/Multiplayer/Idas3EosRoomRules.cs"
    exe = proof / "EosRoomRulesDriver.exe"
    compiled = subprocess.run([str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), str(production), str(Path(__file__).with_name("EosRoomRulesDriver.cs"))], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, encoding="utf-8", timeout=15)
    (proof / "checks.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": run.returncode == 0, "count": len(checks), "checks": checks,
              "production_sha256": hashlib.sha256(production.read_bytes()).hexdigest(), "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    print(f"PASS {len(checks)} production EOS room rules checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
