"""Check source catalog structure and built production localization without UI.

Checks all English/Japanese/Chinese labels, placeholder preservation, current
online labels and opaque driver-name formatting. The text boundary is injected;
Unity resource packaging, glyphs, font sizing and screenshots are not covered.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tests/Multiplayer"))
from test_build_compatibility import find_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, default=ROOT / "Builds/Current")
    parser.add_argument("--report-dir", type=Path, default=ROOT / "Verification/eos-online-20261011/localization")
    parser.add_argument("--mono", type=Path, help="Unity Mono runtime matching the project")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    managed = args.player.resolve() / "InitialDUnity_Data/Managed"
    catalog = ROOT / "Assets/Resources/Localization/CustomMenus.txt"
    source = ROOT / "Assets/Scripts/Idas3MenuLocalization.cs"
    status_sources = [ROOT / "Assets/Scripts/Multiplayer" / name for name in ("Idas3EosTransport.cs", "Idas3EosSanctions.cs", "Idas3EosP2P.cs", "Idas3EosPlatform.cs", "Idas3EosRoomRules.cs", "Idas3EosOnlineConfig.cs")]
    exe = proof / "LocalizationCatalogDriver.exe"
    result = subprocess.run([str(find_compiler(None)), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(exe), str(Path(__file__).with_name("LocalizationCatalogDriver.cs"))], capture_output=True, text=True)
    (proof / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    editor_version = re.search(r"m_EditorVersion: (\S+)", (ROOT / "ProjectSettings/ProjectVersion.txt").read_text()).group(1)
    mono = args.mono or Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Unity/Hub/Editor" / editor_version / "Editor/Data/MonoBleedingEdge/bin/mono.exe"
    result = subprocess.run([str(mono), str(exe), str(managed), str(catalog), *map(str, status_sources)], capture_output=True, text=True, encoding="utf-8", timeout=60)
    result.stdout = result.stdout.lstrip("\ufeff")
    (proof / "checks.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    report = {"passed": result.returncode == 0, "output": result.stdout.strip(), "scope": __doc__.strip(), "runtime": str(mono),
              "player": str(args.player.resolve()),
              "sha256": {("player/InitialDUnity_Data/Managed/Assembly-CSharp.dll" if path == managed / "Assembly-CSharp.dll" else path.relative_to(ROOT).as_posix()): hashlib.sha256(path.read_bytes()).hexdigest() for path in (catalog, source, managed / "Assembly-CSharp.dll", *status_sources)}}
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout.strip() + "; " + str(proof / "report.json"))


if __name__ == "__main__":
    main()
