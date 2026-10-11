"""Exercise production EOS platform authentication and callback boundaries.

Only SDK/Unity boundary types are fake. Production async authentication,
callback queuing, identity checks, renewal and shutdown execute unchanged.
Each scenario runs in a separate process. No native SDK, network, actual EOS
credentials, existing identity, Unity player or saved game is used.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/platform")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = ROOT / "Assets/Scripts/Multiplayer/Idas3EosPlatform.cs"
    sources = [production, Path(__file__).with_name("EosPlatformFakes.cs"), Path(__file__).with_name("EosPlatformDriver.cs")]
    compiler = find_compiler(args.compiler)
    exe = proof / "EosPlatformDriver.exe"
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    runs = {name: subprocess.run([str(exe), name], capture_output=True, text=True, encoding="utf-8", timeout=15)
            for name in ("existing", "new", "load", "retry", "missing-token", "invalid-identity", "renew", "changed-identity", "shutdown")}
    (proof / "checks.log").write_text("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items()), encoding="utf-8")
    checks = [line.split("\t", 1)[1] for run in runs.values() for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": all(run.returncode == 0 for run in runs.values()), "count": len(checks), "checks": checks,
              "production_sha256": hashlib.sha256(production.read_bytes()).hexdigest(), "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items() if run.returncode))
    print(f"PASS {len(checks)} production EOS platform checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
