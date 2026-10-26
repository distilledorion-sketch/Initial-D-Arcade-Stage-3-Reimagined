"""Production race progress watchdog with fake native state, clock and transport.

No real gameplay, player saves, EOS service or native physics are used here.
--baseline demonstrates the old indefinite-stall path using the published .52 session.
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
    parser.add_argument('--compiler')
    parser.add_argument('--report-dir', type=Path, required=True)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=False)
    sources = [ROOT / 'Assets/Scripts/Multiplayer' / name for name in (
        'Idas3MultiplayerSession.cs', 'Idas3BattleRecordSettlement.cs', 'Idas3ResultJournal.cs',
        'Idas3MultiplayerTypes.cs', 'Idas3OnlineActivity.cs', 'Idas3OnlineUsername.cs',
        'Idas3LobbyChat.cs', 'Idas3NetworkImpairment.cs', 'Idas3MultiplayerRecords.cs',
        'Idas3MultiplayerRecords.Moderation.cs', 'Idas3MultiplayerRecordChecks.cs',
        'Idas3BuildCompatibility.cs', 'Idas3QuickMatch.cs', 'Idas3TcpTransport.cs', 'Idas3ChallengerTestTransport.cs')]
    if args.baseline:
        data = subprocess.check_output(['git', 'show', 'v0.3.95-community-replays.52:Assets/Scripts/Multiplayer/Idas3MultiplayerSession.cs'], cwd=ROOT)
        # Diagnostic count only: baseline has no recovery implementation.
        data = data.replace(b'public bool ExperimentalAuthority', b'internal int AuthorityRecoveries => 0;\n        public bool ExperimentalAuthority')
        baseline = proof / 'BaselineSession.cs'
        baseline.write_bytes(data)
        sources[0] = baseline
    native = ROOT / 'Assets/Scripts/Multiplayer/Idas3MultiplayerNative.cs'
    types = proof / 'ProductionMultiplayerTypes.cs'
    types.write_text(native.read_text(encoding='utf-8').split('    internal static class Idas3MultiplayerNative')[0] + '}\n', encoding='utf-8')
    sources += [types] + [Path(__file__).with_name(n) for n in ('LobbyChatSessionFakes.cs', 'OnlineSessionFakes.cs', 'RaceProgressDriver.cs')]
    compiler = find_compiler(args.compiler)
    exe = proof / 'RaceProgressDriver.exe'
    result = subprocess.run([str(compiler), '/nologo', '/warnaserror+', '/target:exe',
        '/define:IDAS3_ONLINE_SESSION_TEST;IDAS3_RACE_PROGRESS_TEST', '/out:' + str(exe), *map(str, sources)],
        capture_output=True, text=True, encoding='utf-8')
    (proof / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    run = subprocess.run([str(exe), *(['baseline'] if args.baseline else [])], capture_output=True, text=True, encoding='utf-8', timeout=60)
    (proof / 'checks.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    checks = [line.split('\t', 1)[1] for line in run.stdout.splitlines() if line.startswith('PASS\t')]
    report = dict(passed=run.returncode == 0, baseline=args.baseline, scope=__doc__, count=len(checks), checks=checks,
        source_sha256={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources + [native, Path(__file__)]})
    (proof / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    print(f'PASS {len(checks)} race progress checks; report: {proof / "report.json"}')

if __name__ == '__main__':
    main()
