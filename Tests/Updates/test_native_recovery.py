"""Exercise native recovery with disposable files and actual process interruption.

No installed game, real ROM, network service or desktop UI is used. The optional
seed helper proves recovery of plans written by the previously bundled updater.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
CORE = ["InitialDUnity.exe", "UnityPlayer.dll", "InitialDUnity_Data/globalgamemanagers",
        "InitialDUnity_Data/Managed/Assembly-CSharp.dll", "READ ME.txt"]
PRIVATE = ["userdata/card.json", "Custom Music/song.mp3", "rom/gds-0033.chd", "replays/example.idreplay"]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def text(value):
    data = str(value).encode("utf-16le")
    return struct.pack("<I", len(data) // 2) + data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--child", type=Path, required=True)
    parser.add_argument("--seed-helper", type=Path, default=ROOT / "Assets/Resources/UpdateInstaller.exe.bytes")
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--skip-interruption", action="store_true", help="Fast validation only; real process termination remains untested")
    args = parser.parse_args()
    if sys.flags.optimize:
        parser.error("Assertions must stay enabled")
    proof = (args.report_dir or ROOT / "Verification" / ("native-recovery-" + uuid.uuid4().hex)).resolve()
    proof.mkdir(parents=True, exist_ok=False)
    helper, seed, child = [proof / name for name in ("recovery.exe", "previous-installer.exe", "child.exe")]
    for source, dest in ((args.helper, helper), (args.seed_helper, seed), (args.child, child)):
        shutil.copyfile(source.resolve(strict=True), dest)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    invalid = ctypes.c_void_p(-1).value
    results = []

    def lock(path):
        handle = kernel.CreateFileW(str(path), 0x80000000, 1, None, 3, 0, None)
        assert handle not in (None, invalid), "Could not lock fixture " + str(path)
        return handle

    def run(plan, **kwargs):
        return subprocess.run([str(helper), str(plan), "--recover", "--test"], capture_output=True,
                              timeout=120, creationflags=subprocess.CREATE_NO_WINDOW, **kwargs)

    def fixture(name, *, large_backup=False, ready=True, executable=False):
        folder = proof / name
        game, session = folder / "Game 日本 & spaces !", folder / "session"
        game.mkdir(parents=True); session.mkdir()
        (folder / "ISOLATED_UPDATE_TEST.txt").write_text("Disposable updater fixture only.")
        old, new, entries = {}, {}, []
        for relative in CORE:
            dest, backup, stage = game / relative, session / "backup" / relative, session / "stage" / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            backup.parent.mkdir(parents=True, exist_ok=True)
            stage.parent.mkdir(parents=True, exist_ok=True)
            existed = relative != "READ ME.txt"
            changed = relative != "InitialDUnity_Data/globalgamemanagers"
            before = b"original " + relative.encode()
            after = b"replacement " + relative.encode()
            if executable and relative == CORE[0]:
                before = after = child.read_bytes()
            if existed:
                if large_backup and relative == CORE[1]:
                    with dest.open("wb") as stream:
                        for _ in range(512):
                            stream.write(b"old recovery data".ljust(1024 * 1024, b"!"))
                else:
                    dest.write_bytes(before)
                old[relative] = digest(dest)
                if changed:
                    shutil.copyfile(dest, backup)
            stage.write_bytes(after if changed else before)
            new[relative] = digest(stage)
            entries.append((relative, changed, existed, stage.stat().st_size,
                            bytes.fromhex(old[relative]) if existed else bytes(32), bytes.fromhex(new[relative])))
            # Simulate both complete and torn replacements, plus an added file.
            if changed:
                dest.write_bytes(after if relative != CORE[1] else b"torn replacement")
        for name in PRIVATE:
            path = game / name; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"private fixture, not actual player content")
            old[name] = digest(path)
        if ready:
            (session / "ready").write_text("Prepared")
        def save_plan(rows=None, pid=0, stamp=0):
            chosen = entries if rows is None else rows
            data = b"IDUPD002" + text(game) + struct.pack("<IQI", pid, stamp, len(chosen))
            for name, changed, existed, size, previous, following in chosen:
                data += text(name) + struct.pack("<BBQ", changed, existed, size) + previous + following
            (session / "install.plan").write_bytes(data)
        save_plan()
        return folder, game, session, old, entries, save_plan

    def inventory(game):
        return {p.relative_to(game).as_posix(): digest(p) for p in game.rglob("*") if p.is_file()}

    def assert_recovered(game, session, old):
        assert inventory(game) == old, "Restored game differs from the complete original inventory"
        status = json.loads((session / "result.json").read_text())
        assert status["recovered"] and status["cleanupSafe"] and not status["passed"] and not status["rollbackNeeded"], status
        assert (session / "backup").exists(), "Recovery discarded backups before normal cleanup"
        repeated = run(session / "install.plan")
        assert repeated.returncode == 0 and inventory(game) == old, repeated.stderr.decode(errors="replace")

    def scenario(name, action):
        row = {"name": name, "passed": False}; results.append(row)
        try:
            action()
            row["passed"] = True
            print("PASS " + name, flush=True)
        except Exception as error:
            row["error"] = str(error)
            print("FAIL " + name + ": " + str(error), flush=True)

    def basic(name, mutate=lambda *x: None, expected=True, retry=False):
        folder, game, session, old, entries, save_plan = fixture(name)
        handle = mutate(folder, game, session, entries, save_plan)
        before, backups = inventory(game), inventory(session / "backup")
        private_times = {n: (game / n).stat().st_mtime_ns for n in PRIVATE}
        try:
            result = run(session / "install.plan")
        finally:
            if handle is not None:
                kernel.CloseHandle(handle)
        (folder / "output.txt").write_bytes(result.stdout + result.stderr)
        assert (result.returncode == 0) == expected, result.stderr.decode(errors="replace")
        assert inventory(session / "backup") == backups, "Recovery modified its backup set"
        assert all((game / n).stat().st_mtime_ns == t for n, t in private_times.items()), "Recovery touched private files"
        if expected:
            assert_recovered(game, session, old)
        elif retry:
            assert not (session / "result.json").exists(), "Failed recovery published a cleanup-safe status"
            again = run(session / "install.plan")
            assert again.returncode == 0, again.stderr.decode(errors="replace")
            assert_recovered(game, session, old)
        else:
            assert inventory(game) == before, "Invalid recovery changed installed files"

    cases = [
        ("mixed-files", lambda *x: None, True, False),
        ("missing-executable", lambda f,g,s,e,p: (g/CORE[0]).unlink(), True, False),
        ("rollback-needed", lambda f,g,s,e,p: (s/"result.json").write_text('{"schema":1,"passed":false,"changedFiles":2,"cleanupSafe":false,"rollbackNeeded":true}') and None, True, False),
        ("partial-result", lambda f,g,s,e,p: (s/"result.json.tmp").write_text('{"schema":') and None, True, False),
        ("missing-last-backup", lambda f,g,s,e,p: (s/"backup"/CORE[3]).unlink(), False, False),
        ("damaged-last-backup", lambda f,g,s,e,p: (s/"backup"/CORE[3]).write_bytes(b"bad") and None, False, False),
        ("changed-retained", lambda f,g,s,e,p: (g/CORE[2]).write_bytes(b"changed outside update") and None, False, False),
        ("missing-ready", lambda f,g,s,e,p: (s/"ready").unlink(), False, False),
        ("partial-ready", lambda f,g,s,e,p: (s/"ready").write_text("Pre") and None, False, False),
        ("malformed-status", lambda f,g,s,e,p: (s/"result.json").write_text('{"schema":1') and None, False, False),
        ("completed-update", lambda f,g,s,e,p: (s/"result.json").write_text('{"schema":1,"passed":true,"changedFiles":4,"cleanupSafe":true,"rollbackNeeded":false}') and None, False, False),
        ("duplicate-plan", lambda f,g,s,e,p: p(e+[e[0]]), False, False),
        ("traversal-plan", lambda f,g,s,e,p: p(e+[("../outside", *e[0][1:])]), False, False),
        ("private-plan", lambda f,g,s,e,p: p(e+[("userdata/card.json", *e[0][1:])]), False, False),
        ("truncated-plan", lambda f,g,s,e,p: (s/"install.plan").write_bytes((s/"install.plan").read_bytes()[:-5]) and None, False, False),
        ("locked-destination", lambda f,g,s,e,p: lock(g/CORE[1]), False, True),
        ("locked-publication", lambda f,g,s,e,p: ((s/"result.json.tmp").write_text("unfinished"), lock(s/"result.json.tmp"))[1], False, True),
    ]
    for name, mutate, expected, retry in cases:
        scenario(name, lambda n=name,m=mutate,e=expected,r=retry: basic(n,m,e,r))

    def session_locked():
        folder, game, session, old, entries, save_plan = fixture("session-locked")
        path = session / ".install-lock"; path.touch()
        handle = lock(path); before = inventory(game)
        try:
            result = run(session / "install.plan")
            assert result.returncode != 0 and inventory(game) == before
            assert not (session / "result.json").exists() and not (session / "recovery-error.txt").exists()
        finally:
            kernel.CloseHandle(handle)
    scenario("session-locked", session_locked)

    def running_game():
        folder, game, session, old, entries, save_plan = fixture("running-game", executable=True)
        process = subprocess.Popen([str(game/CORE[0]), "-wait"], cwd=game, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic()+15
            while not (game/"parent.txt").exists() and time.monotonic()<deadline:
                time.sleep(.05)
            pid, stamp = map(int,(game/"parent.txt").read_text().split())
            for use_pid in (False, True):
                save_plan(pid=pid if use_pid else 0, stamp=stamp if use_pid else 0)
                before = inventory(game)
                result = run(session/"install.plan")
                assert result.returncode != 0 and inventory(game)==before, "Recovery touched a running game"
        finally:
            (game/"release-parent").write_text("exit")
            process.wait(timeout=10)
    scenario("running-game", running_game)

    def interrupted(name, recovery):
        folder, game, session, old, entries, save_plan = fixture(name, large_backup=recovery)
        if recovery:
            command = [str(helper), str(session/"install.plan"), "--recover", "--test"]
            wanted = old[CORE[0]]
        else:
            # Start from a clean installation and let the supplied seed updater
            # produce its actual backups/ready marker before killing it.
            for name in CORE:
                if name in old:
                    if name != CORE[2]:
                        shutil.copyfile(session/"backup"/name, game/name)
                else:
                    (game/name).unlink()
            for path in (session/"backup").rglob("*"):
                if path.is_file():
                    path.unlink()  # Exact files in this newly created fixture.
            (session/"ready").unlink()
            with (session/"stage"/CORE[1]).open("wb") as stream:
                for _ in range(512):
                    stream.write(b"new payload data".ljust(1024*1024,b"!"))
            row = entries[1]; entries[1] = (*row[:3], (session/"stage"/CORE[1]).stat().st_size, row[4], bytes.fromhex(digest(session/"stage"/CORE[1])))
            save_plan()
            command = [str(seed), str(session/"install.plan"), "--test"]
            wanted = entries[0][5].hex()
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic()+120
            while process.poll() is None and time.monotonic()<deadline:
                try:
                    changed = digest(game/CORE[0])==wanted
                except OSError:
                    changed = False
                if changed:
                    process.kill(); process.wait(timeout=15); break
                time.sleep(.001)
            else:
                raise AssertionError("Could not interrupt the owned helper after the first replacement")
            assert (session/"ready").exists() and not (session/"result.json").exists()
            assert inventory(game)!=old, "Fixture did not leave an incomplete operation"
            result = run(session/"install.plan")
            assert result.returncode==0, result.stderr.decode(errors="replace")
            assert_recovered(game,session,old)
        finally:
            if process.poll() is None:
                process.kill();process.wait(timeout=15)
            process.communicate()
    if not args.skip_interruption:
        scenario("interrupted-seed-updater", lambda: interrupted("interrupted-seed-updater",False))
        scenario("interrupted-recovery", lambda: interrupted("interrupted-recovery",True))
    summary = {"passed":all(row["passed"] for row in results),"count":len(results),"tests":results,
               "helperSha256":digest(helper),"seedHelperSha256":digest(seed),"realInterruptionIncluded":not args.skip_interruption,
               "scope":"Disposable native files, whole-inventory restore, private-file preservation, failed/repeated recovery and process termination. No power-loss durability or actual game launch claim."}
    (proof/"report.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
    print("Evidence: "+str(proof/"report.json"))
    return 0 if summary["passed"] else 1


if __name__=="__main__":
    raise SystemExit(main())
