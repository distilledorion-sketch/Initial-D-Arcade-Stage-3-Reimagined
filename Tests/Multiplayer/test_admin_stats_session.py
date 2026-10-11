"""Verify administrative reset integration in actual session and record storage.

Use real filesystem/JSON and production session, record, moderation, packet
parser and impairment code. Only online and native boundaries are fake. Race
states are injected to exercise reset boundaries without starting a simulator.
All saves are isolated under the report; no actual userdata or EOS is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import uuid
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/stats-session")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerSession.cs", "Idas3MultiplayerTypes.cs", "Idas3OnlineActivity.cs", "Idas3OnlineUsername.cs",
        "Idas3LobbyChat.cs", "Idas3NetworkImpairment.cs", "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs",
        "Idas3MultiplayerRecordChecks.cs", "Idas3BuildCompatibility.cs", "Idas3QuickMatch.cs", "Idas3TcpTransport.cs", "Idas3ChallengerTestTransport.cs")]
    native = ROOT / "Assets/Scripts/Multiplayer/Idas3MultiplayerNative.cs"
    native_text = native.read_text(encoding="utf-8")
    marker = "    internal static class Idas3MultiplayerNative"
    if native_text.count(marker) != 1:
        raise RuntimeError("Native boundary changed; inspect fixture")
    types = proof / "ProductionMultiplayerTypes.cs"
    types.write_text(native_text.split(marker)[0] + "}\n", encoding="utf-8")
    sources = production + [types] + [Path(__file__).with_name(name) for name in (
        "LobbyChatSessionFakes.cs", "OnlineSessionFakes.cs", "MultiplayerRecordStorageFakes.cs", "AdminStatsSessionDriver.cs")]
    library = ROOT / "ThirdParty/DiscordLegacyJson/Newtonsoft.Json.dll"
    shutil.copy2(library, proof / library.name)
    executable = proof / "AdminStatsSessionDriver.exe"
    command = [str(find_compiler(args.compiler)), "/nologo", "/warnaserror+", "/target:exe", "/define:IDAS3_ONLINE_SESSION_TEST;IDAS3_RECORD_STORAGE_INTEGRATION",
               "/reference:" + str(library), "/reference:C:/Windows/Microsoft.NET/Framework64/v4.0.30319/netstandard.dll", "/out:" + str(executable), *map(str, sources)]
    compiled = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    saves = proof / ("isolated-saves-" + uuid.uuid4().hex)
    ran = subprocess.run([str(executable), str(saves)], capture_output=True, text=True, encoding="utf-8", timeout=180)
    (proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
    checks = [line.split("\t", 1)[1] for line in ran.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": ran.returncode == 0, "count": len(checks), "checks": checks, "isolated_saves": str(saves),
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production + [native]}, "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    print(f"PASS {len(checks)} actual session/storage checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
