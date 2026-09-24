"""Compile pure managed cleanup code and run isolated Windows filesystem checks."""
import os
from pathlib import Path
import subprocess
import uuid

repo = Path(__file__).resolve().parents[2]
output = repo / "Verification" / "updater-cache-20260924" / uuid.uuid4().hex
output.mkdir(parents=True)
compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
exe = output / "CacheChecks.exe"
subprocess.run([str(compiler), "/nologo", "/r:System.Runtime.Serialization.dll", "/out:" + str(exe),
                str(repo / "Assets/Scripts/Idas3UpdateCache.cs"), str(repo / "Tests/Updates/CacheChecks.cs")], check=True)
subprocess.run([str(exe), str(output / "fixtures")], check=True)
print(output / "fixtures/report.json")
