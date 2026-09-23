"""Compile and run the production ROM validator against isolated invalid fixtures.

Optionally pass --game-root to verify an existing installation's real ROM too.
No ROM is downloaded or copied; production startup has no test bypass.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid


REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "Assets/Scripts/Idas3RomValidation.cs"
DRIVER = Path(__file__).with_name("RomValidationDriver.cs")


def compiler_path(explicit):
    if explicit:
        return Path(explicit).resolve(strict=True)
    visual_studio = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio"
    candidates = sorted(visual_studio.glob("*/*/MSBuild/Current/Bin/Roslyn/csc.exe"))
    if not candidates:
        raise RuntimeError("Pass --compiler with a C# 7.3+ compiler.")
    return candidates[-1]


def cue_variants(executable, source_root, proof):
    """Use real read-only track links and newly written cue copies, never edit input links."""
    source = source_root / "rom"
    original = (source / "gds-0033.cue").read_text(encoding="utf-8-sig")
    variants = {
        "formatting": ("\nREM private formatting test\n" + "\n".join("  \t" + line.lower() for line in original.splitlines()), True),
        "traversal": (original.replace("gds-0033-track1.bin", "../gds-0033-track1.bin"), False),
        "wrong-layout": (original.replace("TRACK 02 AUDIO", "TRACK 02 MODE1/2352"), False),
        "missing-track": ("\n".join(original.splitlines()[:-3]), False),
        "extra-file": (original + '\nFILE "extra.bin" BINARY\nTRACK 04 AUDIO\nINDEX 01 00:00:00\n', False),
        "oversized": (original + "\nREM " + "x" * 16384, False),
        "locked-chd": (original, True),
    }
    directory = proof / ("cue-variants-" + uuid.uuid4().hex)
    results = []
    for name, (cue, expected) in variants.items():
        game = directory / name
        target = game / "rom"
        target.mkdir(parents=True)
        (target / "gds-0033.cue").write_text(cue, encoding="utf-8")
        for track in range(1, 4):
            filename = f"gds-0033-track{track}.bin"
            os.link(source / filename, target / filename)
        mode = "--validate-locked-chd" if name == "locked-chd" else "--validate-path"
        completed = subprocess.run([str(executable), mode, str(game), str(expected)],
                                   capture_output=True, text=True, timeout=90)
        (game / "validation.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
        if completed.returncode:
            raise RuntimeError(name + ": " + completed.stdout + completed.stderr)
        count = next(int(line.split("\t", 1)[1]) for line in completed.stdout.splitlines() if line.startswith("TOTAL\t"))
        results.append({"name": name, "passed": True, "expected_verified": expected, "checks": count})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--game-root", type=Path, help="Read-only validation of an actual installation's rom folder")
    parser.add_argument("--report-dir", type=Path, default=REPO / "Verification/rom-validation-20260923")
    args = parser.parse_args()
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=True)
    executable = proof / "RomValidationDriver.exe"
    compiler = compiler_path(args.compiler)
    compiled = subprocess.run([str(compiler), "/nologo", "/warnaserror+", "/langversion:latest",
                               "/target:exe", "/out:" + str(executable), str(SOURCE), str(DRIVER)],
                              capture_output=True, text=True)
    (proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    fixtures = proof / ("fixtures-" + uuid.uuid4().hex)
    command = [str(executable), str(fixtures)]
    if args.game_root:
        command.append(str(args.game_root.resolve(strict=True)))
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    (proof / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    rows = [line.split("\t", 1) for line in result.stdout.splitlines() if "\t" in line]
    checks = [value for kind, value in rows if kind == "PASS"]
    total = next((int(value) for kind, value in rows if kind == "TOTAL"), 0)
    report = {"passed": result.returncode == 0 and total == len(checks), "total_checks": total,
              "checks": checks, "compiler": str(compiler), "source": str(SOURCE),
              "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(), "fixtures": str(fixtures),
              "actual_game_root": str(args.game_root.resolve()) if args.game_root else None,
              "actual_format": next((value for kind, value in rows if kind == "FORMAT"), None),
              "scope": "Production pure-C# validator and digest helper. Synthetic invalid files only; actual ROM read only when --game-root is supplied. Runtime startup ordering requires a separate Unity check."}
    if report["passed"] and report["actual_format"] == "CUE/BIN":
        report["cue_variants"] = cue_variants(executable, args.game_root.resolve(), proof)
        report["total_checks"] += sum(item["checks"] for item in report["cue_variants"])
    (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "checks": report["total_checks"], "report": str(proof / "report.json")}))
    if not report["passed"]:
        raise RuntimeError(result.stdout + result.stderr)


if __name__ == "__main__":
    main()
