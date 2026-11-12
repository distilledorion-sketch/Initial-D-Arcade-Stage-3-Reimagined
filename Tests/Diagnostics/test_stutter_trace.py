"""Validate production trace retention, chronology, timing ownership and safe output with fake Unity/host boundaries. No live EOS, GPU benchmark or VPN reproduction."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'Tests/Multiplayer'))
from test_build_compatibility import find_compiler
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--report-dir',type=Path,required=True);args=parser.parse_args()
out=args.report_dir.resolve();out.mkdir(parents=True,exist_ok=False)
sources=[root/'Assets/Scripts'/p for p in ['Idas3StutterTrace.cs','Idas3StutterMeasurements.cs']]+[Path(__file__).with_name('StutterTraceDriver.cs')]
exe=out/'StutterTraceDriver.exe'
build=subprocess.run([str(find_compiler(None)),'/nologo','/warnaserror+','/target:exe','/out:'+str(exe),*map(str,sources)],capture_output=True,text=True)
(out/'compile.log').write_text(build.stdout+build.stderr)
if build.returncode:raise RuntimeError(build.stdout+build.stderr)
run=subprocess.run([str(exe),str(out/'output')],capture_output=True,text=True,timeout=30)
(out/'checks.log').write_text(run.stdout+run.stderr)
checks=[s[5:] for s in run.stdout.splitlines() if s.startswith('PASS\t')]
(out/'report.json').write_text(json.dumps(dict(passed=run.returncode==0,checks=checks,scope=__doc__,sourceHashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}),indent=2))
print(f'{"PASS" if run.returncode==0 else "FAIL"} {len(checks)} checks; {out}')
if run.returncode:raise RuntimeError(run.stderr)
