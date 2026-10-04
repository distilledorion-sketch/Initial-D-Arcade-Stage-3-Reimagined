"""Run the production Steam adapter and Quick Match with a deterministic API boundary."""
import json
import subprocess
from pathlib import Path
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]
PROOF = ROOT / 'Verification/online-stability-20261004'
PROOF.mkdir(parents=True, exist_ok=True)
sources = [ROOT / 'Assets/Scripts/Multiplayer' / name for name in (
    'Idas3MultiplayerTypes.cs', 'Idas3BuildCompatibility.cs',
    'Idas3SteamTransport.cs', 'Idas3QuickMatch.cs')]
sources += [Path(__file__).with_name(name) for name in ('SteamTransportFakes.cs', 'SteamStabilityDriver.cs')]
exe = PROOF / 'SteamStabilityDriver.exe'
compile = subprocess.run([str(find_compiler(None)), '/nologo', '/warnaserror+',
                          '/out:' + str(exe), *map(str, sources)], capture_output=True, text=True)
(PROOF / 'managed-compile.log').write_text(compile.stdout + compile.stderr, encoding='utf-8')
assert compile.returncode == 0, compile.stdout + compile.stderr
run = subprocess.run([str(exe)], capture_output=True, text=True)
(PROOF / 'managed-checks.log').write_text(run.stdout + run.stderr, encoding='utf-8')
print(run.stdout + run.stderr)
assert run.returncode == 0
checks = [line.split('\t', 1)[1] for line in run.stdout.splitlines() if line.startswith('PASS\t')]
(PROOF / 'managed-checks.json').write_text(json.dumps({'passed': True, 'count': len(checks), 'checks': checks,
    'scope': 'Production adapter and matchmaking state machine with fake Steam API, no live Steam route.'}, indent=2)+'\n', encoding='utf-8')
