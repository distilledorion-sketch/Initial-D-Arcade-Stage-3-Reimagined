"""Exercise actual EOS route buffering/dispatch with an in-memory SDK and synthetic delivery costs. No live EOS, VPN, personal data or game FPS measurement."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from test_build_compatibility import find_compiler
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--report-dir',type=Path,required=True)
parser.add_argument('--observe',action='store_true')
args=parser.parse_args();out=args.report_dir.resolve();out.mkdir(parents=True,exist_ok=False)
sources=[root/'Assets/Scripts/Multiplayer'/p for p in ['Idas3EosP2P.cs','Idas3EosPackets.cs']]+[Path(__file__).with_name(p) for p in ['EosStatsP2PFakes.cs','EosP2PBurstDriver.cs']]
exe=out/'EosP2PBurstDriver.exe'
build=subprocess.run([str(find_compiler(None)),'/nologo','/warnaserror+','/target:exe','/out:'+str(exe),*map(str,sources)],capture_output=True,text=True)
(out/'compile.log').write_text(build.stdout+build.stderr)
if build.returncode:raise RuntimeError(build.stdout+build.stderr)
run=subprocess.run([str(exe)]+(['--observe'] if args.observe else []),capture_output=True,text=True,timeout=30)
(out/'checks.log').write_text(run.stdout+run.stderr)
checks=[s[5:] for s in run.stdout.splitlines() if s.startswith('PASS\t')]
observations=[s[8:] for s in run.stdout.splitlines() if s.startswith('OBSERVE\t')]
(out/'report.json').write_text(json.dumps(dict(passed=run.returncode==0,observeOnly=args.observe,scope=__doc__,checks=checks,observations=observations,sourceHashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}),indent=2))
print('\n'.join(observations));print(f'{"PASS" if run.returncode==0 else "FAIL"} {len(checks)} checks; {out}')
if run.returncode:raise RuntimeError(run.stderr)
