"""Verify Saved Data steering telemetry without Unity or physical wheel output."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tests/Multiplayer"))
from test_build_compatibility import find_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    output = args.report_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sources = [ROOT / "Assets/Scripts/Idas3WheelFeedback.cs"]
    driver = Path(__file__).with_name("SavedSteeringDriver.cs")
    exe = output / "SavedSteeringDriver.exe"
    compiler = find_compiler(args.compiler)
    flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if sys.platform == "win32" else {}
    build = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe),
                            *map(str, sources), str(driver)], capture_output=True, text=True, encoding="utf-8", timeout=120, **flags)
    (output / "compile.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, encoding="utf-8", timeout=60, **flags)
    (output / "checks.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = dict(passed=run.returncode == 0 and len(checks) >= 30, count=len(checks), checks=checks,
                  source_sha256={str(source.relative_to(ROOT)): hashlib.sha256(source.read_bytes()).hexdigest() for source in sources},
                  compiler=str(compiler), scope="Production wheel telemetry packing and managed ABI layout. No physical hardware calls.")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError(run.stdout + run.stderr)
    print(f"PASS {len(checks)} saved steering checks; report: {output / 'report.json'}")


if __name__ == "__main__":
    main()
