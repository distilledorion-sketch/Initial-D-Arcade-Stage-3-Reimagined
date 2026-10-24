"""Check earned-result persistence through real filesystem failures and retries.

Production session and record storage; fake transport, native award/presentation,
and clock. Optional native progression exercises only the battle-record formula,
not a natural race or native tuning-save transaction. No player saves or services.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--native", type=Path)
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=False)
    production = [ROOT / "Assets/Scripts/Multiplayer" / name for name in (
        "Idas3MultiplayerSession.cs", "Idas3BattleRecordSettlement.cs", "Idas3MultiplayerTypes.cs",
        "Idas3OnlineActivity.cs", "Idas3OnlineUsername.cs", "Idas3LobbyChat.cs",
        "Idas3NetworkImpairment.cs", "Idas3MultiplayerRecords.cs", "Idas3MultiplayerRecords.Moderation.cs",
        "Idas3MultiplayerRecordChecks.cs", "Idas3BuildCompatibility.cs", "Idas3QuickMatch.cs",
        "Idas3TcpTransport.cs", "Idas3ChallengerTestTransport.cs")]
    native_types = ROOT / "Assets/Scripts/Multiplayer/Idas3MultiplayerNative.cs"
    text = native_types.read_text(encoding="utf-8")
    marker = "    internal static class Idas3MultiplayerNative"
    if text.count(marker) != 1:
        raise RuntimeError("Native boundary changed; inspect the fixture")
    extracted = proof / "ProductionMultiplayerTypes.cs"
    extracted.write_text(text.split(marker)[0] + "}\n", encoding="utf-8")
    fixtures = [Path(__file__).with_name(name) for name in (
        "LobbyChatSessionFakes.cs", "OnlineSessionFakes.cs", "MultiplayerRecordStorageFakes.cs",
        "ResultSettlementDriver.cs")]
    library = ROOT / "ThirdParty/DiscordLegacyJson/Newtonsoft.Json.dll"
    shutil.copyfile(library, proof / library.name)
    compiler = find_compiler(args.compiler)
    facade = Path(os.environ.get("WINDIR", "C:/Windows")) / "Microsoft.NET/Framework64/v4.0.30319/netstandard.dll"
    exe = proof / "ResultSettlementDriver.exe"
    command = [str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/platform:x64",
        "/define:IDAS3_ONLINE_SESSION_TEST;IDAS3_RECORD_STORAGE_INTEGRATION;IDAS3_RESULT_SETTLEMENT_TEST",
        "/out:" + str(exe), "/reference:" + str(library), "/reference:" + str(facade),
        *map(str, production + [extracted] + fixtures)]
    compiled = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=120)
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    native_hash = None
    if args.native:
        library_copy = proof / "Idas3Unity.dll"
        shutil.copyfile(args.native.resolve(), library_copy)
        native_hash = hashlib.sha256(library_copy.read_bytes()).hexdigest()
    # Short private live fixtures avoid imposing the legacy Framework host's
    # MAX_PATH behavior on a deep checkout. Preserve them with the evidence.
    temporary = Path(tempfile.mkdtemp(prefix="idas3-result-"))
    run = subprocess.run([str(exe), str(temporary / "run"), *( ["native"] if args.native else [])],
        capture_output=True, text=True, encoding="utf-8", timeout=180)
    (proof / "checks.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    shutil.copytree(temporary, proof / "fixtures")
    if temporary.resolve().parent != Path(tempfile.gettempdir()).resolve() or not temporary.name.startswith("idas3-result-"):
        raise RuntimeError("Refusing to clean an unexpected fixture path")
    shutil.rmtree(temporary)
    checks = [line.split("\t", 1)[1] for line in run.stdout.splitlines() if line.startswith("PASS\t")]
    report = {"passed": run.returncode == 0, "count": len(checks), "checks": checks,
        "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in production + [native_types] + fixtures + [Path(__file__)]},
        "nativeProgressionSha256": native_hash, "nativeTuningPersistenceTested": False,
        "naturalRaceTested": False, "compiler": str(compiler), "scope": __doc__.strip()}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    print(f"PASS {len(checks)} result settlement checks; report: {proof / 'report.json'}")


if __name__ == "__main__":
    main()
