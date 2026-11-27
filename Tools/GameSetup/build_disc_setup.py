"""Build the offline installer from the same finished ZIP accepted by the game updater."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def digest(file):
    with file.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def build(core, output):
    version = (HERE / "package-version.txt").read_text().strip()
    short_version = version.replace("-community-replays.", ".")
    name = f"Initial-D-Arcade-Stage-3-Reimagined-{short_version}-Windows-x64.zip"
    if core.name != name or output.exists():
        raise ValueError("Use the versioned Windows game package and a new setup ZIP.")
    helper = ROOT / "Assets/Resources/UpdateInstaller.exe.bytes"
    with zipfile.ZipFile(core) as package:
        if package.read("InitialDUnity_Data/GameSetup/package-version.txt").decode().strip() != version:
            raise ValueError("Setup and player package versions differ.")
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        work = Path(temporary)
        identity = work / "PackageIdentity.cs"
        identity.write_text('internal static class PackageIdentity {\n' + "".join(
            f'public const string {key} = "{value}";\n'
            for key, value in (("Name", name), ("Version", version), ("Sha", digest(core)),
                               ("HelperSha", digest(helper)))) + "}\n")
        setup = work / "Install Initial D Arcade Stage 3 Reimagined.exe"
        compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
        subprocess.run([compiler, "/nologo", "/optimize+", "/out:" + str(setup),
                        "/r:System.Windows.Forms.dll", "/r:System.IO.Compression.dll",
                        "/r:System.IO.Compression.FileSystem.dll", identity, HERE / "StandaloneInstall.cs",
                        ROOT / "Assets/Scripts/Idas3UpdateStaging.cs", ROOT / "Assets/Scripts/Idas3UpdatePaths.cs"],
                       check=True)
        with zipfile.ZipFile(work / "setup.zip", "x", zipfile.ZIP_DEFLATED) as package:
            package.write(core, name, compress_type=zipfile.ZIP_STORED)
            package.write(helper, "UpdateInstaller.exe")
            package.write(setup, setup.name)
            package.writestr(setup.name + ".config", '<configuration><runtime><AppContextSwitchOverrides value="Switch.System.IO.UseLegacyPathHandling=false;Switch.System.IO.BlockLongPaths=false"/></runtime></configuration>')
            package.writestr("Install Initial D.cmd", '@echo off\r\n"%~dp0Install Initial D Arcade Stage 3 Reimagined.exe" %*\r\n')
            package.writestr("README.txt", "Extract this entire setup ZIP outside the game folder. Open Install Initial D Arcade Stage 3 Reimagined.exe and select your existing game folder, or choose an empty folder for a fresh install.\r\nKeep the full Windows game ZIP beside Setup. The game and conversion tools are included. Setup reuses verified files when updating. For disc extraction, choose either the English GDS-0033 CHD and 317-0384-COM.pic (or initdv3e.zip), or the Japanese Rev C GDS-0032C CHD and 317-0379-jpn.pic (or initdv3j.zip). One matching pair is sufficient.\r\n")
        with zipfile.ZipFile(work / "setup.zip") as package:
            if package.testzip():
                raise ValueError("Setup archive verification failed.")
        (work / "setup.zip").rename(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.core.resolve(), args.output.resolve())
