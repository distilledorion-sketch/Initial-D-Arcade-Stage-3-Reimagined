"""Real local TCP delivery and adapter-boundary LAN address checks.

No Unity player, cloud service, adapter changes or personal saves.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report-dir', type=Path, required=True)
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=False)
    sources = [ROOT/'Assets/Scripts/Multiplayer'/name for name in
               ('Idas3TcpTransport.cs', 'Idas3MultiplayerTypes.cs', 'Idas3OnlineActivity.cs')]
    sources += [Path(__file__).with_name(name) for name in ('TcpTransportDriver.cs','LanAddressChecks.cs')]
    exe = proof/'TcpTransportDriver.exe'
    compile = subprocess.run([str(find_compiler(None)), '/nologo', '/warnaserror+', '/target:exe',
                              '/out:'+str(exe), *map(str,sources)], capture_output=True, text=True)
    (proof/'compile.log').write_text(compile.stdout+compile.stderr)
    if compile.returncode:
        raise RuntimeError(compile.stdout+compile.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    (proof/'checks.log').write_text(run.stdout+run.stderr)
    checks = [line.split('\t',1)[1] for line in run.stdout.splitlines() if line.startswith('PASS\t')]
    report = dict(passed=run.returncode==0, checks=checks, count=len(checks), scope=__doc__,
                  source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources})
    (proof/'report.json').write_text(json.dumps(report,indent=2))
    if run.returncode:
        raise RuntimeError(run.stdout+run.stderr)
    print('PASS',len(checks),'TCP delivery and LAN address checks')

if __name__=='__main__':
    main()
