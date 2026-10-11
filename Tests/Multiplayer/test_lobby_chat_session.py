"""Compile the production session and exercise lobby chat over a fake admitted peer.

Unity's clock/rendering and online presence are explicit runtime stubs. The actual
session parser, send logic, room reset, chat model, and impairment boundary run.
No Unity player, native game, cloud connection, network or player files are used.
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
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/lobby-chat-20261009/session")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerSession.cs", "Idas3MultiplayerTypes.cs", "Idas3LobbyChat.cs", "Idas3OnlineActivity.cs", "Idas3OnlineUsername.cs",
        "Idas3NetworkImpairment.cs", "Idas3MultiplayerNative.cs", "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs",
        "Idas3MultiplayerRecordChecks.cs",
        "Idas3BuildCompatibility.cs", "Idas3QuickMatch.cs", "Idas3TcpTransport.cs",
        "Idas3ChallengerTestTransport.cs")]
    sources = production + [Path(__file__).with_name(name) for name in (
        "LobbyChatSessionFakes.cs", "LobbyChatSessionDriver.cs")]
    compiler = find_compiler(args.compiler)
    exe = proof / "LobbyChatSessionDriver.exe"
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/target:exe",
        "/out:" + str(exe), *map(str, sources)], capture_output=True, text=True)
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(exe)], capture_output=True, text=True)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": ran.returncode == 0, "count": len(checks), "checks": checks,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production},
        "compiler": str(compiler),
        "scope": "Production session admission, chat packet send/receive, reliable dispatch, nonce and lobby-generation isolation, state guards, invalid wire text, and room reset over a fake admitted transport. No live Relay/LAN, Unity UI, native race, or actual rematch handshake coverage."}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {len(checks)} session chat checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
