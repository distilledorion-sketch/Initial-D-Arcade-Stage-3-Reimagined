"""Compile and exercise the production matchmaking key helper without Steam.

    python Tests/Multiplayer/test_build_compatibility.py --build Builds/Current

The deterministic fixture runs without a player build. With --build, also hash
the actual native/managed binaries and four Special Stage simulation packs.
The script only reads game files; all generated files go in the report folder.
"""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess


REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "Assets/Scripts/Multiplayer/Idas3BuildCompatibility.cs"
DRIVER = Path(__file__).with_name("BuildCompatibilityDriver.cs")
PACKS = (("ENNA", "enna"), ("MYOGI_SPECIAL", "myogi_special"),
         ("USUI_SPECIAL", "usui_special"), ("MOMIJI", "momiji"))


def encoded_hash(data):
    return base64.b64encode(hashlib.sha256(data).digest()).decode("ascii")


def file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").digest()


def fixture(parts):
    full_identity = "idas3-mp9-" + parts[0] + "-" + parts[1]
    full_identity += "".join(f"-{index + 11}-{value}" for index, value in enumerate(parts[2:]))
    full_identity += "-authority1"
    return parts + ["idas3-build1-" + encoded_hash(full_identity.encode("utf-8"))]


def build_fixture(build):
    data = build / "InitialDUnity_Data"
    parts = [base64.b64encode(file_hash(path)).decode("ascii") for path in (
        data / "Plugins/x86_64/Idas3Unity.dll", data / "Managed/Assembly-CSharp.dll")]
    for index, (pack, slug) in enumerate(PACKS):
        folder = data / "StreamingAssets" / pack
        assert (folder / "menu.idastex").is_file(), f"Course not installed: {pack}"
        names = ["course.id", f"{slug}_path.bin", f"{slug}_path_l.bin", f"{slug}_path_r.bin",
                 "race-markers.bin", "collision-0.rcl", "collision-1.rcl"]
        if index > 0:
            names.append("timer-scale.bin")
        parts.append(encoded_hash(b"".join(file_hash(folder / name) for name in names)))
    return fixture(parts)


def find_compiler(explicit):
    if explicit:
        return Path(explicit).resolve(strict=True)
    visual_studio = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio"
    candidates = sorted(visual_studio.glob("*/*/MSBuild/Current/Bin/Roslyn/csc.exe"))
    if not candidates:
        raise RuntimeError("A C# 6+ compiler is required; pass --compiler path/to/csc.exe")
    return candidates[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, help="Optionally test hashes of this actual Windows player")
    parser.add_argument("--compiler", help="Path to C# 6+ csc.exe (defaults to Visual Studio Roslyn)")
    parser.add_argument("--report-dir", type=Path, default=REPO / "Verification/steam-build-key-20260922")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    executable = proof / "BuildCompatibilityDriver.exe"
    compiler = find_compiler(args.compiler)
    compile_result = subprocess.run(
        [str(compiler), "/nologo", "/warnaserror+", "/target:exe", "/out:" + str(executable),
         str(SOURCE), str(DRIVER)], capture_output=True, text=True,
    )
    (proof / "compile.log").write_text(compile_result.stdout + compile_result.stderr, encoding="utf-8")
    assert compile_result.returncode == 0, compile_result.stdout + compile_result.stderr
    fixtures = {"deterministic": fixture([encoded_hash(value.encode("ascii")) for value in
                ("native", "managed", "enna", "myogi_special", "usui_special", "momiji")])}
    if args.build:
        fixtures["installed-player"] = build_fixture(args.build.resolve(strict=True))
    results = []
    for name, values in fixtures.items():
        fixture_path = proof / (name + ".txt")
        fixture_path.write_text("\n".join(values) + "\n", encoding="utf-8")
        completed = subprocess.run([str(executable), str(fixture_path)], capture_output=True, text=True)
        (proof / (name + ".log")).write_text(completed.stdout + completed.stderr, encoding="utf-8")
        assert completed.returncode == 0, completed.stdout + completed.stderr
        lines = [line.split("\t", 1) for line in completed.stdout.splitlines()]
        passed = [detail for kind, detail in lines if kind == "PASS"]
        stats = {kind: int(detail) for kind, detail in lines if kind != "PASS"}
        assert len(passed) == stats["TOTAL"]
        results.append({"fixture": name, "passed": True, "checks": passed, **stats})
        print(f"{name}: {len(passed)} checks passed; full identity {stats['IDENTITY_LENGTH']} chars, key {stats['KEY_LENGTH']} chars")
    report = {"passed": True, "source": str(SOURCE), "source_sha256": file_hash(SOURCE).hex(),
              "compiler": str(compiler), "build": str(args.build.resolve()) if args.build else None,
              "total_checks": sum(result["TOTAL"] for result in results), "results": results}
    (proof / "unit-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Report:", proof / "unit-report.json")


if __name__ == "__main__":
    main()
