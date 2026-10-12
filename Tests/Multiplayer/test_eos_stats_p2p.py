"""Exercise production EOS P2P retries, reset handshakes and admission buffering.

Compile the real P2P and packet framing implementations. Only the SDK, platform,
and owner boundary are fake; no live service, player identity, or save is used.
The owner callback tests disposal before delivery; full transport policy refresh
and sanctions enforcement are covered separately by test_eos_transport.py.
SDK callback tests reproduce early rejection, bounded retry and cancellation;
they do not establish the cause of a particular user's network failure.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/stats-p2p")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in ("Idas3EosP2P.cs", "Idas3EosPackets.cs")]
    sources = production + [Path(__file__).with_name(name) for name in ("EosStatsP2PFakes.cs", "EosStatsP2PDriver.cs")]
    executable = proof / "EosStatsP2PDriver.exe"
    compiled = subprocess.run([str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(executable), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(executable)], capture_output=True, text=True, encoding="utf-8", timeout=30)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": ran.returncode == 0, "count": len(checks), "checks": checks,
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production}, "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {len(checks)} actual production P2P checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
