"""Compile and exercise the production lobby-chat model without Unity or networking.

    python Tests/Multiplayer/test_lobby_chat.py --report-dir Verification/lobby-chat-20261009

Only the chosen report directory is written. No player, saves, Steam account or
network connection is used. This checks model behavior, not Unity text-field input
or session packet admission; those require the session/player checks separately.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from test_build_compatibility import find_compiler


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Assets/Scripts/Multiplayer/Idas3LobbyChat.cs"
DRIVER = Path(__file__).with_name("LobbyChatDriver.cs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", help="C# 6+ Roslyn compiler path")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/lobby-chat-20261009")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    compiler = find_compiler(args.compiler)
    executable = proof / "LobbyChatDriver.exe"
    compiled = subprocess.run(
        [str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(executable), str(SOURCE), str(DRIVER)],
        capture_output=True, text=True,
    )
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(executable)], capture_output=True, text=True)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    report = {
        "passed": ran.returncode == 0,
        "count": len(checks),
        "checks": checks,
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "compiler": str(compiler),
        "scope": "Production lobby-chat model: Unicode/input bounds, local/remote rate limits, replay rejection, attribution, history and room reset. No Unity UI, session packet admission, Steam or live network coverage.",
    }
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {len(checks)} lobby-chat model checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
