"""Compile pure managed cleanup code and run isolated Windows filesystem checks."""
import os
import argparse
import json
from pathlib import Path
from framework_runtime import configure_managed_test_driver
import subprocess
import uuid

repo = Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--report-dir',type=Path)
args=parser.parse_args()
output=(args.report_dir or repo/'Verification'/('updater-cache-'+uuid.uuid4().hex)).resolve()
output.mkdir(parents=True,exist_ok=False)
compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
exe = output / "CacheChecks.exe"
configure_managed_test_driver(exe)
subprocess.run([str(compiler), "/nologo", "/r:System.Runtime.Serialization.dll", "/out:" + str(exe),
                str(repo / "Assets/Scripts/Idas3UpdatePaths.cs"), str(repo / "Assets/Scripts/Idas3UpdateCache.cs"), str(repo / "Tests/Updates/CacheChecks.cs")], check=True)
subprocess.run([str(exe), str(output / "fixtures")], check=True)
report=json.loads((output/'fixtures/report.json').read_text())
(output/'report.json').write_text(json.dumps(report,indent=2))
print(output / "fixtures/report.json")
