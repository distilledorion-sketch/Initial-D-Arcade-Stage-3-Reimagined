"""Reject unsafe combined release inputs before the full upgrade fixture extracts them."""
import argparse
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools"))
import verify_release_upgrade as upgrade


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    if not __debug__:
        parser.error("Assertions must stay enabled")
    output = args.report_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    core = {"InitialDUnity.exe": b"exe", "UnityPlayer.dll": b"engine", "InitialDUnity_Data/globalgamemanagers": b"metadata",
            "InitialDUnity_Data/Managed/Assembly-CSharp.dll": b"scripts"}
    course = "InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex"
    tests = []
    for name in ("valid-combined", "duplicate", "case-collision", "private", "rom", "traversal", "symlink", "missing-core"):
        folder = output / name
        folder.mkdir()
        entries = dict(core)
        if name == "missing-core":
            entries.pop("UnityPlayer.dll")
        with zipfile.ZipFile(folder / "core.zip", "x") as archive:
            for key, data in entries.items():
                archive.writestr(key, data)
        key = {"duplicate": "InitialDUnity.exe", "case-collision": "INITIALDUNITY.EXE", "private": "InitialDUnity_Data/userdata/card.json",
               "rom": "rom/gds-0033.chd", "traversal": "../escape.bin"}.get(name, course)
        with zipfile.ZipFile(folder / "courses.zip", "x") as archive:
            if name == "symlink":
                key = zipfile.ZipInfo(course)
                key.create_system = 3
                key.external_attr = 0o120777 << 16
            archive.writestr(key, b"course payload")
        rejected = False
        try:
            inventory = upgrade.archive_inventory([folder / "core.zip", folder / "courses.zip"])
        except (AssertionError, ValueError):
            rejected = True
        assert rejected == (name != "valid-combined"), name
        if not rejected:
            assert set(inventory) == set(core) | {course}
            assert inventory[course]["size"] == len(b"course payload")
        tests.append({"name": name, "passed": True})
        print("PASS " + name, flush=True)
    (output / "report.json").write_text(json.dumps({"passed": True, "count": len(tests), "tests": tests}, indent=2) + "\n")


if __name__ == "__main__":
    main()
