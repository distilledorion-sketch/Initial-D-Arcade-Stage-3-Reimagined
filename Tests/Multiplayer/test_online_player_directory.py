"""Exercise the production offline-player directory cache and combined EOS policy.

Uses injected request, identity, username and monotonic-clock delegates with the
actual directory and sanctions classes. Checks rename invalidation, explicit
status/identity validation, independent policy combination, outages preserving
bans, retry/grace bounds, timeout/disposal and late/out-of-order responses. SDK
calls are faked; no HTTP, credentials, real player identity or sanction changes.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/online-player-directory/policy")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3OnlinePlayerDirectory.cs", "Idas3EosSanctions.cs", "Idas3EosTransport.cs",
        "Idas3EosRoomRules.cs", "Idas3EosPackets.cs", "Idas3MultiplayerTypes.cs", "Idas3OnlineActivity.cs")]
    sources = production + [Path(__file__).with_name(name) for name in (
        "EosLobbyFakes.cs", "EosSanctionsFakes.cs", "OnlinePlayerDirectoryDriver.cs")]
    exe = proof / "OnlinePlayerDirectoryDriver.exe"
    compiled = subprocess.run([str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/define:PRODUCTION_DIRECTORY",
                               "/target:exe", "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    scenarios = ("cache-rename", "ban-outage", "invalid", "timeout-dispose", "reply-order", "combined")
    runs = {name: subprocess.run([str(exe), name], capture_output=True, text=True, encoding="utf-8", timeout=30) for name in scenarios}
    (proof / "checks.log").write_text("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items()), encoding="utf-8")
    checks = [line.split("\t", 1)[1] for run in runs.values() for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": all(run.returncode == 0 for run in runs.values()), "count": len(checks), "checks": checks,
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production}, "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items() if run.returncode))
    print(f"PASS {len(checks)} production directory/policy checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
