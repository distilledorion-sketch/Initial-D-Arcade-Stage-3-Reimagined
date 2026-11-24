"""Build the small Windows GUI entry point with the Windows .NET Framework."""
import os
from pathlib import Path
import subprocess


LAUNCHER_NAME = "Initial D Arcade Stage 3 Reimagined.exe"


def build(output):
    compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
    source = Path(__file__).with_name("PortableLauncher.cs")
    subprocess.run([str(compiler), "/nologo", "/target:winexe", "/optimize+",
                    "/win32icon:" + str(source.with_name("Reimagined.ico")),
                    "/reference:System.Windows.Forms.dll", "/reference:System.Web.Extensions.dll",
                    "/out:" + str(output), str(source)], check=True)
