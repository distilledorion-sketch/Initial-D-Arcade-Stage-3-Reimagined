"""Exercise the shipped setup extraction boundary with disposable archives.

Default fixtures use inert data. The optional full-build check uses clean
distributable assets, never original ROMs or existing player profiles. Nothing
is downloaded. Damaged compressed data fails after metadata validation.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import shutil
import time
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools"))
import verify_release as verify
import verify_packaged_player as packaged

REQUIRED = {
    "InitialDUnity.exe": b"executable fixture",
    "UnityPlayer.dll": b"engine fixture" * 1024,
    "InitialDUnity_Data/globalgamemanagers": b"version fixture",
    "InitialDUnity_Data/Managed/Assembly-CSharp.dll": b"managed fixture",
    "InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex": b"Gunsai fixture",
    "InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex": b"Odawara fixture",
}


def archive(path, entries):
    with zipfile.ZipFile(path, "x", zipfile.ZIP_DEFLATED) as output:
        for name, data in entries.items():
            output.writestr(name, data)


def corrupt_payload(path, name):
    with zipfile.ZipFile(path) as source:
        offset = source.getinfo(name).header_offset
    with path.open("r+b") as output:
        output.seek(offset)
        header = output.read(30)
        filename, extra = struct.unpack_from("<HH", header, 26)
        output.seek(offset + 30 + filename + extra)
        output.write(b"\x07")  # DEFLATE BTYPE=3 is reserved and cannot be decoded.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--build-evidence", type=Path, help="Also package/extract the exact clean player from a successful fresh build")
    parser.add_argument("--archives", type=Path, nargs=2, metavar=("CORE", "COURSES"), help="Reuse two already-built test archives; requires --build-evidence")
    args = parser.parse_args()
    if sys.flags.optimize:
        parser.error("Assertions must stay enabled")
    if args.archives and not args.build_evidence:
        parser.error("--archives requires --build-evidence for the expected candidate inventory")
    proof = (args.report_dir or ROOT / "Verification" / ("setup-extraction-" + uuid.uuid4().hex)).resolve()
    proof.mkdir(parents=True, exist_ok=False)
    installer = proof / "Install-InitialD.ps1"
    shutil.copyfile(ROOT / "Tools/Install-InitialD.ps1", installer)
    driver = proof / "Extract.ps1"
    driver.write_text("""param([string]$Config)
$ErrorActionPreference='Stop'
$settings=Get-Content -LiteralPath $Config -Raw -Encoding UTF8|ConvertFrom-Json
. ([string]$settings.installer)
try {
    if($settings.action -eq 'cleanup') { Remove-SetupStaging -Stage ([string]$settings.stage) -Parent ([string]$settings.parent) }
    else { Expand-GameArchives -Archives ([string[]]$settings.archives) -Target ([string]$settings.target) }
}
catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
""", encoding="utf-8-sig")
    shell = Path(os.environ["SystemRoot"]) / "System32/WindowsPowerShell/v1.0/powershell.exe"
    environment = {k: v for k, v in os.environ.items() if k.lower() != "psmodulepath"}
    tests = []
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]

    def invoke(config):
        return subprocess.run([str(shell), "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", str(driver), str(config)],
                              env=environment, capture_output=True, text=True, errors="replace", timeout=60,
                              creationflags=subprocess.CREATE_NO_WINDOW)

    def case(name, *, empty=False, damaged=False, unexpected=None, duplicate=False, incomplete=False, existing=False, locked=False, collision=False, trailing=False):
        folder = proof / name
        folder.mkdir()
        target = folder / "Game 日本語 & spaces !"
        core = {key: data for key, data in REQUIRED.items() if "/StreamingAssets/" not in key}
        courses = {key: data for key, data in REQUIRED.items() if "/StreamingAssets/" in key}
        if incomplete:
            courses.pop("InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex")
        if unexpected:
            courses[unexpected] = b"must be rejected before installation"
        if duplicate:
            courses["initialdunity.exe"] = b"duplicate fixture"
        if collision:
            courses["InitialDUnity_Data/Managed"] = b"file collides with an extracted directory"
        archive(folder / "core.zip", core)
        archive(folder / "courses.zip", courses)
        if damaged:
            corrupt_payload(folder / "core.zip", "UnityPlayer.dll")
        if empty or existing or locked:
            target.mkdir()
        if existing:
            (target / "player-save.txt").write_bytes(b"existing data")
        before = {p.relative_to(target).as_posix(): p.read_bytes() for p in target.rglob("*") if p.is_file()} if target.exists() else {}
        settings = {"installer": str(installer), "archives": [str(folder / n) for n in ("core.zip", "courses.zip")], "target": str(target) + ("\\" if trailing else "")}
        config = folder / "case.json"
        config.write_text(json.dumps(settings), encoding="utf-8")
        handle = None
        try:
            if locked:
                # Allow reads/writes but deny directory deletion, so publication
                # fails only after extraction has completed in the sibling.
                handle = kernel.CreateFileW(str(target), 0x80000000, 3, None, 3, 0x02000000, None)
                assert handle not in (None, ctypes.c_void_p(-1).value), "Could not lock disposable destination"
            process = invoke(config)
        finally:
            if handle not in (None, ctypes.c_void_p(-1).value):
                kernel.CloseHandle(handle)
        (folder / "output.txt").write_text(process.stdout + process.stderr, encoding="utf-8")
        good = not (damaged or unexpected or duplicate or incomplete or existing or locked or collision)
        check = {"name": name, "passed": False, "exitCode": process.returncode}
        tests.append(check)
        try:
            assert (process.returncode == 0) == good, process.stderr
            after = {p.relative_to(target).as_posix(): p.read_bytes() for p in target.rglob("*") if p.is_file()} if target.exists() else {}
            assert after == (REQUIRED if good else before), "Failed extraction left a partial installation or changed existing files"
            if not good:
                assert target.exists() == (empty or existing or locked), "Failed extraction changed whether the destination exists"
            assert not list(folder.glob(".initial-d-extract-*")), "Extraction left temporary payloads behind"
            if damaged or locked:
                if damaged:
                    (folder / "core.zip").unlink()
                    archive(folder / "core.zip", core)
                retry = invoke(config)
                (folder / "retry-output.txt").write_text(retry.stdout + retry.stderr, encoding="utf-8")
                assert retry.returncode == 0, "Retry after failure did not install: " + retry.stderr
                after_retry = {p.relative_to(target).as_posix(): p.read_bytes() for p in target.rglob("*") if p.is_file()}
                assert after_retry == REQUIRED, "Retry did not publish the complete game"
                assert not list(folder.glob(".initial-d-extract-*")), "Retry left temporary payloads"
                check["retryPassed"] = True
            check["passed"] = True
            print("PASS " + name, flush=True)
        except Exception as error:
            check["error"] = str(error)
            print("FAIL " + name + ": " + str(error), flush=True)

    case("new-folder")
    case("existing-empty-folder", empty=True)
    case("trailing-separator", trailing=True)
    case("trailing-separator-empty-folder", empty=True, trailing=True)
    case("damaged-payload", damaged=True)
    case("damaged-payload-empty-folder", empty=True, damaged=True)
    case("existing-player-data", existing=True)
    case("incomplete-course-download", incomplete=True)
    case("duplicate-across-downloads", duplicate=True)
    case("path-traversal", unexpected="../escape.txt")
    case("private-save-entry", unexpected="InitialDUnity_Data/userdata/save.json")
    case("rom-entry", unexpected="rom/gds-0033.chd")
    case("late-directory-collision", collision=True)
    case("locked-publication", locked=True)

    def cleanup_case(name, wrong_parent=False, wrong_name=False, link=False):
        folder = proof / name
        folder.mkdir()
        outside = folder / "outside"
        outside.mkdir()
        sentinel = outside / "preserve.txt"
        sentinel.write_bytes(b"outside staging")
        stage = folder / ("ordinary-folder" if wrong_name else ".initial-d-extract-" + uuid.uuid4().hex)
        stage.mkdir()
        (stage / "first.txt").write_bytes(b"staged fixture")
        (stage / "nested").mkdir()
        (stage / "nested/second.txt").write_bytes(b"nested staged fixture")
        if link:
            env = dict(environment, IDAS3_TEST_LINK=str(stage / "linked"), IDAS3_TEST_TARGET=str(outside))
            made = subprocess.run([str(shell), "-NoProfile", "-NonInteractive", "-Command",
                                   "$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path $env:IDAS3_TEST_LINK -Target $env:IDAS3_TEST_TARGET|Out-Null"],
                                  env=env, capture_output=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            assert made.returncode == 0, made.stderr
        config = folder / "case.json"
        config.write_text(json.dumps({"installer": str(installer), "action": "cleanup", "stage": str(stage),
                                      "parent": str(outside if wrong_parent else folder)}), encoding="utf-8")
        process = invoke(config)
        (folder / "output.txt").write_text(process.stdout + process.stderr, encoding="utf-8")
        good = not (wrong_parent or wrong_name or link)
        check = {"name": name, "passed": False, "exitCode": process.returncode}
        tests.append(check)
        try:
            assert (process.returncode == 0) == good, process.stderr
            assert sentinel.read_bytes() == b"outside staging", "Cleanup changed data outside its staging folder"
            assert stage.exists() != good, "Cleanup removed an unapproved folder or left its own staging folder"
            if not good:
                assert (stage / "first.txt").read_bytes() == b"staged fixture", "Cleanup started before validating every descendant"
            check["passed"] = True
            print("PASS " + name, flush=True)
        except Exception as error:
            check["error"] = str(error)
            print("FAIL " + name + ": " + str(error), flush=True)

    cleanup_case("cleanup-owned-staging")
    cleanup_case("cleanup-reject-wrong-parent", wrong_parent=True)
    cleanup_case("cleanup-reject-ordinary-folder", wrong_name=True)
    cleanup_case("cleanup-reject-linked-descendant", link=True)
    candidate_result = None
    if args.build_evidence:
        folder = proof / "full-candidate"
        folder.mkdir()
        check = {"name": "full-candidate", "passed": False}
        tests.append(check)
        try:
            evidence = args.build_evidence.resolve(strict=True)
            source = packaged.build.require_plain_path(evidence / "player")
            assert not proof.is_relative_to(evidence), "Test output must be outside the fresh build"
            print("Hashing clean candidate before packaging...", flush=True)
            before = verify.candidate_inventory(source)
            build_proof, version = packaged.require_build_evidence(evidence, before)
            courses = {"HAKONE", "SADAMINE", "ENNA", "MYOGI_SPECIAL", "USUI_SPECIAL", "MOMIJI", "TSUBAKI", "GUNSAI", "ODAWARA"}
            archives = ([path.resolve(strict=True) for path in args.archives] if args.archives else [folder / "core.zip", folder / "courses.zip"])
            if not args.archives:
                print("Creating two local test downloads from the shipping allowlist...", flush=True)
                with zipfile.ZipFile(archives[0], "x", zipfile.ZIP_DEFLATED, compresslevel=6) as core, \
                        zipfile.ZipFile(archives[1], "x", zipfile.ZIP_DEFLATED, compresslevel=6) as extra:
                    for name in before["files"]:
                        parts = Path(name).parts
                        is_course = len(parts) > 3 and parts[:2] == ("InitialDUnity_Data", "StreamingAssets") and parts[2] in courses
                        (extra if is_course else core).write(source / name, name)
            names = []
            for path in archives:
                with zipfile.ZipFile(path) as output:
                    names.extend(output.namelist())
            # Reject unexpected/private archive paths before reading payloads,
            # including when reusing files from a previous diagnostic run.
            assert len(names) == len(set(names)) and set(names) == set(before["files"]), "Test downloads do not contain exactly the candidate's distributable files"
            archive_rows = []
            for path in archives:
                with zipfile.ZipFile(path) as output:
                    assert output.testzip() is None, "Test download CRC failed"
                assert path.stat().st_size < 2 * 1024**3, "Test download exceeds the public asset size limit"
                archive_rows.append({"name": path.name, "path": str(path), "bytes": path.stat().st_size, "sha256": verify.sha256(path)})
            target = folder / "Installed game 日本語 & spaces !"
            config = folder / "case.json"
            config.write_text(json.dumps({"installer": str(installer), "archives": [str(p) for p in archives], "target": str(target)}), encoding="utf-8")
            print("Extracting the full candidate through the setup script...", flush=True)
            extraction_started = time.monotonic()
            process = subprocess.run([str(shell), "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", str(driver), str(config)],
                                     env=environment, capture_output=True, text=True, errors="replace", timeout=1800,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            extraction_seconds = round(time.monotonic() - extraction_started, 3)
            (folder / "output.txt").write_text(process.stdout + process.stderr, encoding="utf-8")
            check["exitCode"] = process.returncode
            assert process.returncode == 0, process.stderr
            print("Checking every installed byte against the build...", flush=True)
            installed = verify.candidate_inventory(target)
            after = verify.candidate_inventory(source)
            assert before["inventorySha256"] == installed["inventorySha256"] == after["inventorySha256"], "Installed files differ or the source candidate changed"
            assert not list(folder.glob(".initial-d-extract-*")), "Full extraction left temporary payloads"
            candidate_result = {"passed": True, "version": version, "sourceCommit": build_proof["source"]["commit"],
                                "buildEvidenceSha256": verify.sha256(evidence / "summary.json"),
                                "inventorySha256": before["inventorySha256"], "files": before["fileCount"],
                                "bytes": before["bytes"], "archives": archive_rows, "extractionSeconds": extraction_seconds,
                                "scope": "Full candidate packaging and actual setup extraction with byte-for-byte comparison. No network download, existing-install update or game launch."}
            check["passed"] = True
            print("PASS full-candidate", flush=True)
        except Exception as error:
            check["error"] = str(error)
            print("FAIL full-candidate: " + str(error), flush=True)
    summary = {"passed": all(test["passed"] for test in tests), "count": len(tests), "tests": tests,
               "installerSha256": hashlib.sha256(installer.read_bytes()).hexdigest(), "scope": __doc__.strip(),
               "fullCandidate": candidate_result}
    (proof / "report.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print("Evidence: " + str(proof / "report.json"))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
