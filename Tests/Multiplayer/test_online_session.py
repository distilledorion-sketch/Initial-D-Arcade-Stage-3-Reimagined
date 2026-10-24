"""Exercise production session lifecycle with fake online and native boundaries.

No Unity player/build, cloud traffic, sockets, real player profile or native
simulation is used. Production car/snapshot types are extracted unchanged from
Idas3MultiplayerNative.cs; its external entry points are replaced by test fakes.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/session")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerSession.cs", "Idas3BattleRecordSettlement.cs", "Idas3MultiplayerTypes.cs", "Idas3OnlineActivity.cs", "Idas3OnlineUsername.cs",
        "Idas3LobbyChat.cs", "Idas3NetworkImpairment.cs", "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs",
        "Idas3MultiplayerRecordChecks.cs", "Idas3BuildCompatibility.cs",
        "Idas3QuickMatch.cs", "Idas3TcpTransport.cs", "Idas3ChallengerTestTransport.cs")]
    native = ROOT / "Assets/Scripts/Multiplayer/Idas3MultiplayerNative.cs"
    native_text = native.read_text(encoding="utf-8")
    marker = "    internal static class Idas3MultiplayerNative"
    if native_text.count(marker) != 1:
        raise RuntimeError("Native entry-point boundary changed; inspect the fixture before continuing")
    types = proof / "ProductionMultiplayerTypes.cs"
    types.write_text(native_text.split(marker)[0] + "}\n", encoding="utf-8")
    sources = production + [types] + [Path(__file__).with_name(name) for name in (
        "LobbyChatSessionFakes.cs", "OnlineSessionFakes.cs", "OnlineSessionDriver.cs")]
    compiler = find_compiler(args.compiler)
    exe = proof / "OnlineSessionDriver.exe"
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe",
        "/define:IDAS3_ONLINE_SESSION_TEST", "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    runs = [subprocess.run([str(exe), *flags], capture_output=True, text=True, encoding="utf-8")
            for flags in ([], ["-idas3-multiplayer-p2p-check"])]
    (proof / "checks.log").write_text("\n".join(run.stdout + run.stderr for run in runs), encoding="utf-8")
    checks = [line.split("\t", 1)[1] for run in runs for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": all(run.returncode == 0 for run in runs), "count": len(checks), "checks": checks,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production + [native]},
        "compiler": str(compiler),
        "scope": "Production session async initialization, queue status/cancel/error/retry, callback reentrancy, actual Hello compatibility parsing, challenger/Ready guards, idle LAN switching and gated shared P2P diagnostic identity with fake online/native boundaries. No Unity player, native race, cloud deployment, live EOS matchmaking or P2P coverage."}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["passed"]:
        raise RuntimeError("\n".join(run.stdout + run.stderr for run in runs if run.returncode))
    print(f"PASS {len(checks)} online session lifecycle checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
