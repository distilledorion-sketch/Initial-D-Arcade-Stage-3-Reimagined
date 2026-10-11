"""Offline check of actual player DiscordRPC and shared Unity Newtonsoft binaries.

Only pure descriptions, JSON serialization/deserialization and response merging
run. No Discord client is initialized, native game loaded, or presence sent.
"""
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
    parser.add_argument("--player", type=Path, default=ROOT / "Builds/Current")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/unity-online-20261011/discord-json")
    args = parser.parse_args()
    managed = args.player.resolve() / "InitialDUnity_Data/Managed"
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    driver = Path(__file__).with_name("DiscordJsonCompatibilityDriver.cs")
    exe = proof / "DiscordJsonCompatibilityDriver.exe"
    compiler = find_compiler(None)
    result = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), str(driver)], capture_output=True, text=True)
    (proof / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    result = subprocess.run([str(exe), str(managed)], capture_output=True, text=True, encoding="utf-8", timeout=60)
    (proof / "checks.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in result.stdout.splitlines() if line.startswith("PASS\t")]
    files = [managed / name for name in ("Assembly-CSharp.dll", "DiscordRPC.dll", "Newtonsoft.Json.dll")]
    packages = list((ROOT / "Library/PackageCache").glob("com.unity.nuget.newtonsoft-json@*/Runtime/**/Newtonsoft.Json.dll"))
    current_json = hashlib.sha256(files[-1].read_bytes()).hexdigest()
    matched_package = any(hashlib.sha256(path.read_bytes()).hexdigest() == current_json for path in packages)
    report = {"passed": result.returncode == 0 and matched_package, "count": len(checks), "checks": checks,
              "player_json_matches_installed_unity_package": matched_package,
              "binaries_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in files},
              "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError(result.stdout + result.stderr + "\nUnity JSON package binary match: " + str(matched_package))
    print(f"PASS {len(checks)} offline Discord/shared JSON checks; {proof / 'report.json'}")


if __name__ == "__main__":
    main()
