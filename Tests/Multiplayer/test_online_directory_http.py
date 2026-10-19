"""Test the production directory HTTP adapter against fake Unity/Connect boundaries.

No HTTP is sent. Focuses on bearer/body encoding, token/response validation,
request disposal, application/editor shutdown cancellation, synchronous native
completion and cancellation when EOS shutdown discards deferred callbacks.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/online-player-directory/http")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    source = ROOT / "Assets/Scripts/Multiplayer/Idas3OnlinePlayerDirectoryHttp.cs"
    sources = [source, Path(__file__).with_name("OnlineDirectoryHttpFakes.cs"), Path(__file__).with_name("OnlineDirectoryHttpDriver.cs")]
    exe = proof / "OnlineDirectoryHttpDriver.exe"
    compiled = subprocess.run([str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/define:UNITY_EDITOR",
                               "/reference:System.Web.Extensions.dll", "/target:exe", "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    runs = {name: subprocess.run([str(exe), name], capture_output=True, text=True, encoding="utf-8", timeout=30)
            for name in ("encoding", "failures", "lifetime", "shutdown")}
    (proof / "checks.log").write_text("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items()), encoding="utf-8")
    checks = [line.split("\t", 1)[1] for run in runs.values() for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": all(run.returncode == 0 for run in runs.values()), "count": len(checks), "checks": checks,
              "source_sha256": {str(source.relative_to(ROOT)): hashlib.sha256(source.read_bytes()).hexdigest()}, "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items() if run.returncode))
    print(f"PASS {len(checks)} production directory HTTP checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
