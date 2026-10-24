"""Stage both release downloads and exercise native installation/rollback.

Uses inert disposable files and the production staging code. This is not a
full-sized player upgrade, network download, or game-startup acceptance test.
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
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
CORE = {
    "InitialDUnity.exe": b"new executable fixture",
    "UnityPlayer.dll": b"new engine fixture",
    "InitialDUnity_Data/globalgamemanagers": b"new version fixture",
    "InitialDUnity_Data/Managed/Assembly-CSharp.dll": b"new managed fixture",
}
COURSES = {
    "InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex": b"new Gunsai fixture" * 1024,
    "InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex": b"new Odawara fixture",
}
PRIVATE = {"userdata/card.json": b"save", "rom/gds-0033.chd": b"inert ROM sentinel",
           "Custom Music/song.mp3": b"music", "replays/local.idreplay": b"replay"}


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def archive(path, entries):
    with zipfile.ZipFile(path, "x", zipfile.ZIP_DEFLATED) as output:
        for name, data in entries.items():
            output.writestr(name, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path)
    args = parser.parse_args()
    if os.name != "nt" or not __debug__:
        parser.error("Windows with assertions enabled is required")
    proof = (args.report_dir or ROOT / "Verification" / ("split-update-" + uuid.uuid4().hex)).resolve()
    proof.mkdir(parents=True, exist_ok=False)
    helper = args.helper.resolve(strict=True)
    report = {"passed": False, "scope": __doc__.strip(), "helperSha256": sha(helper),
              "stagingSha256": sha(ROOT / "Assets/Scripts/Idas3UpdateStaging.cs"), "tests": []}
    driver = proof / "StageDriver.exe"
    compiler = Path(os.environ["SystemRoot"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                                  ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]

    def execute(command):
        return subprocess.run(list(map(str, command)), capture_output=True, text=True, errors="replace",
                              timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)

    try:
        compiled = execute([compiler, "/nologo", "/out:" + str(driver), "/r:System.IO.Compression.dll",
                            "/r:System.IO.Compression.FileSystem.dll", "/r:System.Web.Extensions.dll",
                            ROOT / "Assets/Scripts/Idas3UpdateStaging.cs", ROOT / "Tests/Updates/StageDriver.cs"])
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        for name in ("split-full", "missing-old-courses", "bad-content-hash", "invalid-content-zip",
                     "damaged-content-payload", "duplicate-across-archives", "private-content",
                     "traversal-content", "missing-core", "file-directory-collision", "patch-with-content",
                     "locked-course-rolls-back-core"):
            row = {"name": name, "passed": False}
            report["tests"].append(row)
            folder = proof / name
            game, session = folder / "Game 日本語 & spaces !", folder / "session"
            game.mkdir(parents=True)
            session.mkdir()
            (folder / "ISOLATED_UPDATE_TEST.txt").write_text("Owned inert updater fixture", encoding="utf-8")
            old = {key: b"old " + value for key, value in {**CORE, **COURSES}.items()}
            if name == "missing-old-courses":
                old = {key: value for key, value in old.items() if key not in COURSES}
            for relative, data in {**old, **PRIVATE}.items():
                path = game / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            private_stamps = {key: (game / key).stat().st_mtime_ns for key in PRIVATE}
            core, courses = dict(CORE), dict(COURSES)
            if name == "duplicate-across-archives":
                courses["initialdunity.exe"] = b"case-insensitive collision with core"
            if name == "private-content":
                courses["InitialDUnity_Data/userdata/card.json"] = b"must reject"
            if name == "traversal-content":
                courses["../escaped.txt"] = b"must reject"
            if name == "missing-core":
                core.pop("UnityPlayer.dll")
            if name == "file-directory-collision":
                courses["InitialDUnity_Data/Managed"] = b"conflicts with scripts directory"
            archive(session / "game.zip", core)
            content = session / "content.zip"
            archive(content, courses)
            if name == "invalid-content-zip":
                content.write_bytes(b"not a ZIP")
            if name == "damaged-content-payload":
                with zipfile.ZipFile(content) as package:
                    offset = package.getinfo(next(iter(COURSES))).header_offset
                with content.open("r+b") as stream:
                    stream.seek(offset)
                    header = stream.read(30)
                    filename, extra = struct.unpack_from("<HH", header, 26)
                    stream.seek(offset + 30 + filename + extra)
                    stream.write(b"\x07")  # Invalid DEFLATE block, despite valid archive hash.
            staged = execute([driver, game, session, sha(session / "game.zip"),
                              "patch" if name == "patch-with-content" else "full", "1.0.0", "1.0.1", "0", "0",
                              content, "0" * 64 if name == "bad-content-hash" else sha(content)])
            (folder / "staging.log").write_text(staged.stdout + staged.stderr, encoding="utf-8")
            good = name in ("split-full", "missing-old-courses")
            rollback = name == "locked-course-rolls-back-core"
            assert (staged.returncode == 0) == (good or rollback), staged.stdout + staged.stderr
            if staged.returncode == 0:
                handle = None
                try:
                    if rollback:
                        handle = kernel.CreateFileW(str(game / next(iter(COURSES))), 0x80000000, 1, None, 3, 0, None)
                        assert handle not in (None, ctypes.c_void_p(-1).value), "Could not lock disposable course"
                    installed = execute([helper, session / "install.plan", "--test"])
                    (folder / "installer.log").write_text(installed.stdout + installed.stderr, encoding="utf-8")
                    assert (installed.returncode == 0) == good, installed.stderr
                finally:
                    if handle not in (None, ctypes.c_void_p(-1).value):
                        kernel.CloseHandle(handle)
                status = json.loads((session / "result.json").read_text())
                assert status["passed"] == good and status["cleanupSafe"] and not status["rollbackNeeded"], status
                assert all(not (session / entry).exists() for entry in ("stage", "backup", "game.zip", "content.zip")), "Downloads or rollback files leaked after safe completion"
                row.update(nativeInstallationAttempted=True, payloadsCleaned=True, rollbackVerified=rollback)
            else:
                assert not (session / "install.plan").exists() and not (session / "ready").exists(), "Rejected content reached the native helper"
                row["rejectedBeforeHandoff"] = True
            expected = {**old, **CORE, **COURSES} if good else old
            for relative, data in {**expected, **PRIVATE}.items():
                assert (game / relative).read_bytes() == data, "Wrong installed bytes: " + relative
            assert all((game / relative).stat().st_mtime_ns == stamp for relative, stamp in private_stamps.items())
            assert not (folder / "escaped.txt").exists() and not (session / "escaped.txt").exists()
            row.update(passed=True, privateFilesPreserved=True)
            print("PASS " + name, flush=True)
        report.update(passed=True, count=len(report["tests"]))
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
