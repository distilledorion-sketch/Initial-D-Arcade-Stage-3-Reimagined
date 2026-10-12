"""Exercise production EOS transport async cancellation and membership rules.

Only the external Lobby SDK and platform/P2P boundaries are fake. Production
transport initialization, SDK callback deferral, cancellation generations,
cleanup barriers, failure propagation and membership-before-delivery decisions
run unchanged. Each scenario uses a fresh process and simulated remote state.
No actual EOS identity, network request, race, game save or live relay is used.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/transport")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3EosTransport.cs", "Idas3EosSanctions.cs", "Idas3EosRoomRules.cs", "Idas3EosPackets.cs", "Idas3MultiplayerTypes.cs", "Idas3OnlineActivity.cs")]
    sources = production + [Path(__file__).with_name(name) for name in ("EosLobbyFakes.cs", "EosSanctionsFakes.cs", "EosTransportDriver.cs")]
    exe = proof / "EosTransportDriver.exe"
    compiled = subprocess.run([str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    runs = {name: subprocess.run([str(exe), name], capture_output=True, text=True, encoding="utf-8", timeout=30)
            for name in ("cancel-create", "cancel-join", "cleanup-failure", "request-error", "dispose-create", "stale-left", "removed-peer", "replaced-peer", "owner-changed", "peer-departure", "public-search", "sanctions-local", "sanctions-unavailable", "sanctions-peer", "sanctions-cancel-peer", "sanctions-cancel-init", "sanctions-active-ban", "sanctions-outage", "sanctions-new-room", "sanctions-late-admission", "sanctions-admission-error", "sanctions-admission-peer-ban", "sanctions-admission-local-ban", "sanctions-admission-reset", "sanctions-admission-cancel", "sanctions-admission-timeout", "sanctions-admission-depart", "sanctions-departed-peer", "sanctions-native-load", "reset-initial", "reset-write-failure", "reset-live-local", "reset-live-peer", "reset-no-owner", "queue-long", "queue-stalled-search", "queue-stalled-create", "queue-peer-deadline", "queue-peer-missing-attributes", "early-listener", "early-listener-failure", "queue-shared-bucket", "queue-public-host", "queue-prefers-public", "queue-waiting-public", "queue-peer-before-public", "queue-public-exclusions")}
    (proof / "checks.log").write_text("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items()), encoding="utf-8")
    checks = [line.split("\t", 1)[1] for run in runs.values() for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": all(run.returncode == 0 for run in runs.values()), "count": len(checks), "checks": checks,
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production}, "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError("\n".join(name + "\n" + run.stdout + run.stderr for name, run in runs.items() if run.returncode))
    print(f"PASS {len(checks)} production EOS transport checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
