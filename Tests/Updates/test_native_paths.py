"""Windows updater path regression tests using disposable files, never a game install.

Run after building Tools/Updater. On a volume with 8.3 names enabled:
    python Tests/Updates/test_native_paths.py --report path-report.json

Use --expect-short-path-rejection against an archived helper to reproduce the
original bug. Fixtures (including junctions) are deliberately kept for inspection;
this script never recursively deletes a tree containing a junction.
"""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time


REPO = Path(__file__).resolve().parents[2]
ALIAS_ERROR = "An update path resolves through a link."
OUTSIDE_ERROR = "Installer must be outside the game folder."
REQUIRED = {
    "InitialDUnity.exe": b"new game fixture",
    "UnityPlayer.dll": b"new engine fixture",
    "InitialDUnity_Data/globalgamemanagers": b"new version fixture",
    "InitialDUnity_Data/Managed/Assembly-CSharp.dll": b"new scripts fixture",
}
TRACK = "InitialDUnity_Data/StreamingAssets/Long Track Folder/track.bin"
NEW_FILE = "InitialDUnity_Data/StreamingAssets/new.bin"


def digest(data):
    return hashlib.sha256(data).digest()


def plan_string(value):
    encoded = str(value).encode("utf-16le")
    return struct.pack("<I", len(encoded) // 2) + encoded


def write_files(folder, files):
    for name, contents in files.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)


def windows_name(path, short):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    api = kernel.GetShortPathNameW if short else kernel.GetLongPathNameW
    api.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
    api.restype = wintypes.DWORD
    output = ctypes.create_unicode_buffer(32768)
    size = api(str(path), output, len(output))
    if not 0 < size < len(output):
        raise ctypes.WinError(ctypes.get_last_error())
    result = Path(output.value)
    if short and "~" not in str(result):
        raise AssertionError("Fixture volume must have DOS 8.3 names enabled: " + str(path))
    return result


def junction(link, target):
    # Pass paths as environment values; no shell-built path interpolation.
    env = dict(os.environ, IDAS3_TEST_LINK=str(link), IDAS3_TEST_TARGET=str(target))
    shell = Path(os.environ["SystemRoot"]) / "System32/WindowsPowerShell/v1.0/powershell.exe"
    result = subprocess.run(
        [str(shell), "-NoProfile", "-NonInteractive", "-Command",
         "$ErrorActionPreference = 'Stop'; "
         "New-Item -ItemType Junction -Path $env:IDAS3_TEST_LINK "
         "-Target $env:IDAS3_TEST_TARGET | Out-Null"],
        env=env, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert link.lstat().st_file_attributes & 0x400, "Fixture is not a reparse point"


def run_case(base, helper, name, baseline):
    folder = base / name
    game = folder / "Game Install Folder"
    session = folder / "Update Session Folder"
    game.mkdir(parents=True)
    session.mkdir()
    (folder / "ISOLATED_UPDATE_TEST.txt").write_text("Disposable native updater path fixture")
    originals = {path: b"old " + data for path, data in REQUIRED.items()}
    originals[TRACK] = b"old track fixture"
    targets = {**REQUIRED, TRACK: b"new track fixture", NEW_FILE: b"new content fixture"}
    personal = {"userdata/card.json": b"save must survive", "replays/run.idreplay": b"personal replay"}
    write_files(game, {**originals, **personal})

    game_arg, session_arg = game, session
    if name == "inside-game-root-short":
        session = game / "Temporary Update Session"
        session.mkdir()
        session_arg = session
    stage = session / "stage"
    if name == "junction-target":
        game_arg = folder / "Linked Game"
        junction(game_arg, game)
    elif name == "junction-session":
        session_arg = folder / "Linked Session"
        junction(session_arg, session)
    elif name == "junction-stage":
        stage = folder / "External Stage"
        stage.mkdir()
        junction(session / "stage", stage)
    write_files(stage, targets)
    (session / "backup").mkdir()

    aliases = name in {"short-session", "short-game", "short-both", "short-file-ancestor"}
    if name in {"short-game", "short-both", "inside-game-root-short"}:
        game_arg = windows_name(game, short=True)
    if name in {"short-session", "short-both"}:
        session_arg = windows_name(session, short=True)

    plan_names = {path: path for path in targets}
    if name == "short-file-ancestor":
        long_parent = game / TRACK
        game_alias = windows_name(long_parent.parent, short=True).name
        stage_alias = windows_name((stage / TRACK).parent, short=True).name
        assert game_alias == stage_alias and "~" in game_alias, "Expected matching short ancestor aliases"
        plan_names[TRACK] = "InitialDUnity_Data/StreamingAssets/" + game_alias + "/track.bin"

    plan = b"IDUPD002" + plan_string(game_arg) + struct.pack("<IQI", 0, 0, len(targets))
    for path, contents in targets.items():
        existed = path in originals
        plan += plan_string(plan_names[path]) + struct.pack("<BBQ", 1, existed, len(contents))
        plan += digest(originals[path]) if existed else bytes(32)
        plan += digest(contents)
    (session / "install.plan").write_bytes(plan)

    # Also check the exact staged bytes on rejection, including the external
    # target of the stage junction. No update should consume or alter them.
    result = subprocess.run(
        [str(helper), str(session_arg / "install.plan"), "--test"],
        capture_output=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW,
    )
    error_path = session / "error.txt"
    error = error_path.read_text(encoding="utf-8") if error_path.exists() else ""
    expected_success = name == "long-path" or (aliases and not baseline)
    assert (result.returncode == 0) == expected_success, (name, result.returncode, error)
    if expected_success:
        assert (session / "ready").read_text() == "Prepared"
        status = json.loads((session / "result.json").read_text())
        assert status == {"passed": True, "changedFiles": len(targets)}, status
        expected = targets
    else:
        assert not (session / "ready").exists(), "Rejected update reached ready state"
        assert not (session / "result.json").exists(), "Rejected update recorded success"
        assert not (game / NEW_FILE).exists(), "Rejected update installed a new file"
        expected = originals
        for path, contents in targets.items():
            assert (stage / path).read_bytes() == contents, (name, "changed staged data", path)
        if aliases or (name == "inside-game-root-short" and baseline):
            assert error == ALIAS_ERROR, (name, error)
        elif name == "inside-game-root-short":
            assert error == OUTSIDE_ERROR, (name, error)
        else:
            assert "links" in error.lower() or "through a link" in error.lower(), (name, error)
    for path, contents in {**expected, **personal}.items():
        assert (game / path).read_bytes() == contents, (name, "changed original/personal data", path)
    assert not (game / ".update-lock").exists(), "Installer left its lock file"
    return {"name": name, "passed": True, "installed": expected_success,
            "exitCode": result.returncode, "error": error, "fixture": str(folder)}


def run_running_game_case(base, helper, child, baseline):
    """An 8.3 root must not hide a second process running the same game file."""
    name = "second-running-game-short-root"
    folder = base / name
    game = folder / "Game Install Folder"
    session = folder / "Update Session Folder"
    game.mkdir(parents=True)
    session.mkdir()
    original = {path: b"old " + data for path, data in REQUIRED.items()}
    original["InitialDUnity.exe"] = child.read_bytes()
    original["userdata/card.json"] = b"personal save"
    targets = {**REQUIRED, NEW_FILE: b"new content fixture"}
    targets["InitialDUnity.exe"] = original["InitialDUnity.exe"]
    write_files(game, original)
    write_files(session / "stage", targets)
    (session / "backup").mkdir()
    processes = []
    try:
        for role in ("parent", "extra"):
            working = folder / role
            working.mkdir()
            process = subprocess.Popen([str(game / "InitialDUnity.exe"), "-wait"],
                                       cwd=working, creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append((process, working))
            deadline = time.monotonic() + 10
            marker = working / "parent.txt"
            while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.05)
            assert marker.exists() and process.poll() is None, "Disposable game process did not start"
        parent, parent_working = processes[0]
        pid, stamp = map(int, (parent_working / "parent.txt").read_text().split())
        assert pid == parent.pid
        plan = b"IDUPD002" + plan_string(windows_name(game, short=True))
        plan += struct.pack("<IQI", pid, stamp, len(targets))
        for path, contents in targets.items():
            existed = path in original
            plan += plan_string(path) + struct.pack("<BBQ", 1, existed, len(contents))
            plan += digest(original[path]) if existed else bytes(32)
            plan += digest(contents)
        (session / "install.plan").write_bytes(plan)
        # Real mode exercises process identity; these executables are tiny,
        # disposable test children. Neither process is the user's game.
        result = subprocess.run([str(helper), str(session / "install.plan")],
                                capture_output=True, timeout=20,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        error = (session / "error.txt").read_text(encoding="utf-8")
        assert result.returncode != 0
        assert error == (ALIAS_ERROR if baseline else "Another copy of this game is running."), error
        assert not (session / "ready").exists(), "Installer ignored the additional running game"
        assert not (session / "result.json").exists()
        assert not (game / NEW_FILE).exists()
        for path, contents in original.items():
            assert (game / path).read_bytes() == contents, (name, path)
        for path, contents in targets.items():
            assert (session / "stage" / path).read_bytes() == contents, (name, path)
        assert all(process.poll() is None for process, _ in processes), "Installer stopped a game"
        assert not (game / ".update-lock").exists()
        return {"name": name, "passed": True, "installed": False,
                "exitCode": result.returncode, "error": error, "fixture": str(folder)}
    finally:
        for process, working in processes:
            (working / "release-parent").write_text("exit disposable test child")
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                # Only terminate the exact disposable child this test started.
                process.terminate()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, default=REPO / "Native/build-update-helper/Idas3UpdateInstaller.exe")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--child", type=Path, default=REPO / "Native/build-update-helper/UpdateTestChild.exe")
    parser.add_argument("--fixture-root", type=Path, default=Path(tempfile.gettempdir()))
    parser.add_argument("--expect-short-path-rejection", action="store_true")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("These regression tests require Windows and a volume with DOS 8.3 names enabled.")
    helper = args.helper.resolve(strict=True)
    base = windows_name(Path(tempfile.mkdtemp(prefix="Idas3 Updater Path Tests ", dir=args.fixture_root)), short=False)
    report = args.report or base / "report.json"
    cases = ["long-path", "short-session", "short-game", "short-both", "short-file-ancestor",
             "junction-target", "junction-session", "junction-stage", "inside-game-root-short",
             "second-running-game-short-root"]
    results = []
    for name in cases:
        try:
            if name == "second-running-game-short-root":
                item = run_running_game_case(base, helper, args.child, args.expect_short_path_rejection)
            else:
                item = run_case(base, helper, name, args.expect_short_path_rejection)
        except Exception as error:
            item = {"name": name, "passed": False, "error": str(error)}
        results.append(item)
        print(name + ": " + ("PASS" if item["passed"] else "FAIL " + item["error"]), flush=True)
    passed = all(item["passed"] for item in results)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"passed": passed, "helper": str(helper),
                                 "helperSha256": hashlib.sha256(helper.read_bytes()).hexdigest(),
                                 "baseline": args.expect_short_path_rejection,
                                 "fixture": str(base), "tests": results}, indent=2), encoding="utf-8")
    print("Report: " + str(report))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
