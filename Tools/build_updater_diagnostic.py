"""Build the isolated updater scene into new private evidence.

This does not replace a shipping player. verify_updater_flow.py separately
requires its managed assembly to match a successful fresh release candidate.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

import build_release_candidate as build
import verify_release as verify

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or not __debug__:
        parser.error("Windows with assertions enabled is required")
    output = args.output.resolve()
    build.require_plain_path(output.parent)
    output.mkdir(parents=True, exist_ok=False)
    temporary = output / "temp"
    temporary.mkdir()
    before = verify.source_identity()
    report = {"passed": False, "scope": __doc__.strip(), "source": before}
    try:
        version_file = (ROOT / "ProjectSettings/ProjectVersion.txt").read_text()
        version = next(line.split(":", 1)[1].strip() for line in version_file.splitlines() if line.startswith("m_EditorVersion:"))
        unity = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Unity/Hub/Editor" / version / "Editor/Unity.exe"
        if not unity.is_file():
            raise FileNotFoundError("Matching Unity Editor is not installed: " + str(unity))
        command = [str(unity), "-batchmode", "-quit", "-force-d3d11", "-projectPath", str(ROOT), "-buildTarget", "Win64",
                   "-executeMethod", "Idas3MenuFontBuild.BuildDiagnostic", "-logFile", str(output / "editor.log")]
        report.update(command=command, unityVersion=version, editorSha256=verify.sha256(unity))
        verify.write_json(output / "report.json", report)
        with (output / "stdout.log").open("wb") as log:
            result = subprocess.run(command, cwd=ROOT, env=dict(os.environ, TEMP=str(temporary), TMP=str(temporary),
                                    IDAS3_DIAGNOSTIC_OUTPUT=str(output / "player")), stdout=log, stderr=subprocess.STDOUT,
                                    timeout=1200, **verify.hidden_process_options())
        report["exitCode"] = result.returncode
        assert result.returncode == 0, "Diagnostic build failed; see editor.log"
        report["assemblySha256"] = verify.sha256(output / "player/InitialDUnity_Data/Managed/Assembly-CSharp.dll")
        report["sourceAfter"] = verify.source_identity()
        assert report["sourceAfter"] == before, "Diagnostic build changed source/configuration"
        assert not (ROOT / "Assets/Scenes/MenuFontDiagnostic.unity").exists(), "Temporary scene was not removed"
        report.update(passed=True, sourceUnchanged=True)
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        verify.write_json(output / "report.json", report)


if __name__ == "__main__":
    main()
