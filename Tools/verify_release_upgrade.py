"""Upgrade exact public-release files with a freshly built candidate's updater.

The diagnostic runs CURRENT managed code with an explicitly simulated installed
version. Downloads use loopback, not GitHub. The target files, native helper,
restart, and subsequent blocked-startup check are real. This is not a claim
that the old shipped player's updater or an Internet download was exercised.
"""
import argparse
import contextlib
import ctypes
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import verify_release as verify
import verify_packaged_player as packaged

REPO = "distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined"
COURSES = {"HAKONE", "SADAMINE", "ENNA", "MYOGI_SPECIAL", "USUI_SPECIAL", "MOMIJI", "TSUBAKI", "GUNSAI", "ODAWARA"}
PRIVATE = {"userdata/card.json": b"save sentinel", "rom/keep.bin": b"inert original-data sentinel",
           "Custom Music/keep.mp3": b"music sentinel", "replays/keep.idreplay": b"replay sentinel"}


class CombinedZip:
    def __init__(self, archives):
        self.archives = archives
        self.by_name = {}
        self.by_id = {}
        for archive in archives:
            for info in archive.infolist():
                if info.filename in self.by_name:
                    raise ValueError("Duplicate file across downloads: " + info.filename)
                self.by_name[info.filename] = archive
                self.by_id[id(info)] = archive

    def infolist(self):
        return [info for archive in self.archives for info in archive.infolist()]

    def open(self, name):
        archive = self.by_name[name] if isinstance(name, str) else self.by_id[id(name)]
        return archive.open(name)


def archive_inventory(paths):
    patch = verify.load_tool("upgrade_patch", "Build-UpdatePatch.py")
    with contextlib.ExitStack() as stack:
        combined = CombinedZip([stack.enter_context(zipfile.ZipFile(path)) for path in paths])
        return patch.inventory(combined)


def game_processes(executable, terminate=False):
    """Only touch a process whose queried image is this newly created fixture."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.QueryFullProcessImageNameW.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint32)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    processes = (ctypes.c_uint32 * 32768)()
    used = ctypes.c_uint32()
    assert psapi.EnumProcesses(processes, ctypes.sizeof(processes), ctypes.byref(used)), "Could not enumerate owned restart"
    result = []
    for pid in processes[:used.value // 4]:
        handle = kernel.OpenProcess(0x1000 | (0x100001 if terminate else 0), False, pid)
        if not handle:
            continue
        try:
            buffer = ctypes.create_unicode_buffer(32768)
            size = ctypes.c_uint32(len(buffer))
            if kernel.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)) and os.path.normcase(buffer.value) == os.path.normcase(str(executable)):
                result.append(pid)
                if terminate:
                    assert kernel.TerminateProcess(handle, 0), "Could not stop owned fixture restart"
                    assert kernel.WaitForSingleObject(handle, 15000) == 0, "Owned restart did not stop"
        finally:
            kernel.CloseHandle(handle)
    return result


class DownloadHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.downloads.append(self.path)
        path = self.server.archives.get(self.path)
        if path is None:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(path.stat().st_size))
        self.end_headers()
        with path.open("rb") as stream:
            shutil.copyfileobj(stream, self.wfile, 1024 * 1024)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-evidence", type=Path, required=True)
    parser.add_argument("--diagnostic-player", type=Path, required=True)
    parser.add_argument("--baseline-archives", nargs=2, type=Path, required=True, metavar=("CORE", "COURSES"))
    parser.add_argument("--target-archives", nargs=2, type=Path, metavar=("CORE", "COURSES"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or not __debug__:
        parser.error("Windows with assertions enabled is required")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"passed": False, "releaseReady": False, "scope": __doc__.strip()}
    server = None
    game = output / "Game 日本語 & spaces !"
    try:
        evidence, diagnostic = args.build_evidence.resolve(strict=True), args.diagnostic_player.resolve(strict=True)
        candidate = evidence / "player"
        assert not output.is_relative_to(evidence) and not output.is_relative_to(diagnostic), "Fixture output must be separate from its input players"
        print("Checking candidate and diagnostic identity...", flush=True)
        inventory = verify.candidate_inventory(candidate)
        proof, target_version = packaged.require_build_evidence(evidence, inventory)
        assembly = "InitialDUnity_Data/Managed/Assembly-CSharp.dll"
        assert verify.sha256(diagnostic / assembly) == inventory["files"][assembly]["sha256"]
        assert verify.sha256(diagnostic / "UnityPlayer.dll") == inventory["files"]["UnityPlayer.dll"]["sha256"]
        release = json.loads(subprocess.check_output(["gh", "api", "repos/" + REPO + "/releases/latest"], **verify.hidden_process_options()))
        verify.write_json(output / "public-release.json", release)
        assert not release["draft"] and not release["prerelease"] and release["html_url"] == "https://github.com/" + REPO + "/releases/tag/" + release["tag_name"]
        baseline_version = release["tag_name"].removeprefix("v")
        assert baseline_version != target_version, "Candidate must have a new version; same-version repair is not this gate"
        baseline = [path.resolve(strict=True) for path in args.baseline_archives]
        metadata = {asset["name"]: asset for asset in release["assets"]}
        baseline_rows = []
        print("Verifying baseline ZIPs against the latest public release...", flush=True)
        for path in baseline:
            asset = metadata[path.name]
            digest = verify.sha256(path)
            assert asset["state"] == "uploaded" and asset["size"] == path.stat().st_size and asset["digest"] == "sha256:" + digest
            baseline_rows.append({"name": path.name, "bytes": path.stat().st_size, "sha256": digest, "assetId": asset["id"]})
        before = archive_inventory(baseline)
        verify.write_json(output / "baseline-inventory.json", before)
        target = [path.resolve(strict=True) for path in args.target_archives] if args.target_archives else [output / "core.zip", output / "courses.zip"]
        if not args.target_archives:
            print("Packaging exact candidate into two downloads...", flush=True)
            with zipfile.ZipFile(target[0], "x", zipfile.ZIP_DEFLATED, compresslevel=6) as core, zipfile.ZipFile(target[1], "x", zipfile.ZIP_DEFLATED, compresslevel=6) as courses:
                for name in inventory["files"]:
                    parts = Path(name).parts
                    extra = len(parts) > 3 and parts[:2] == ("InitialDUnity_Data", "StreamingAssets") and parts[2] in COURSES
                    (courses if extra else core).write(candidate / name, name)
        target_inventory = archive_inventory(target)
        assert {name: {"bytes": row["size"], "sha256": row["sha256"]} for name, row in target_inventory.items()} == inventory["files"], "Target downloads differ from candidate"
        assert all(0 < path.stat().st_size < 2 * 1024**3 for path in target), "Download exceeds asset size limit"
        target_rows = [{"path": str(path), "bytes": path.stat().st_size, "sha256": verify.sha256(path)} for path in target]
        report.update(baselineVersion=baseline_version, targetVersion=target_version, baselineArchives=baseline_rows,
                      targetArchives=target_rows, candidateInventorySha256=inventory["inventorySha256"],
                      buildEvidenceSha256=verify.sha256(evidence / "summary.json"), runnerSha256=verify.sha256(Path(__file__)))
        verify.write_json(output / "report.json", report)
        print("Extracting verified previous release into private installation...", flush=True)
        game.mkdir()
        (output / "ISOLATED_UPDATE_TEST.txt").write_text("Owned private full-release upgrade", encoding="utf-8")
        for path in baseline:
            with zipfile.ZipFile(path) as archive:
                for name in archive.namelist():
                    if name not in before:
                        continue
                    destination = game / name
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(name) as stream, destination.open("xb") as installed:
                        shutil.copyfileobj(stream, installed)
                    assert verify.sha256(destination) == before[name]["sha256"]
        for name, data in PRIVATE.items():
            destination = game / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        private_stamps = {name: (game / name).stat().st_mtime_ns for name in PRIVATE}
        (game / "PRIVATE_TEST_ONLY.txt").write_text("Never distribute this test installation", encoding="utf-8")
        temporary = output / "temp"
        temporary.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), DownloadHandler)
        server.downloads = []
        server.archives = {"/core.zip": target[0], "/courses.zip": target[1]}
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        config = dict(gameRoot=str(game), installedVersion=baseline_version, targetVersion=target_version, timeoutSeconds=3600,
                      url=f"http://127.0.0.1:{server.server_port}/core.zip", sha256=target_rows[0]["sha256"], bytes=target_rows[0]["bytes"],
                      contentUrl=f"http://127.0.0.1:{server.server_port}/courses.zip", contentSha256=target_rows[1]["sha256"], contentBytes=target_rows[1]["bytes"])
        verify.write_json(output / "config.json", config)
        print("Running current Unity updater against full previous-release installation...", flush=True)
        started = time.monotonic()
        with (output / "stdout.log").open("wb") as log:
            process = subprocess.run([str(diagnostic / "InitialDUnity.exe"), "-batchmode", "-nographics", "-idas3-updater-flow-smoke",
                                      str(output / "config.json"), "-logFile", str(output / "player.log")], cwd=diagnostic,
                                     env=dict(os.environ, TEMP=str(temporary), TMP=str(temporary)), stdout=log, stderr=subprocess.STDOUT,
                                     timeout=3660, **verify.hidden_process_options())
        flow = packaged.read_json(output / "unity-flow.json")
        assert process.returncode == 0 and flow["reachedShutdown"] and not flow.get("error"), flow
        assert (flow["applicationVersion"], flow["installedVersion"], flow["targetVersion"]) == (target_version, baseline_version, target_version)
        assert Path(flow["tempPath"]).resolve() == temporary and Path(flow["cacheRoot"]).resolve() == temporary / "InitialDUpdates"
        sessions = {Path(row["session"]).resolve() for row in flow["observations"] if row.get("session")}
        assert len(sessions) == 1 and all(path.parent == temporary / "InitialDUpdates" for path in sessions)
        session = next(iter(sessions))
        deadline = time.monotonic() + 900
        while not (session / "result.json").exists() and time.monotonic() < deadline:
            time.sleep(.5)
        status = packaged.read_json(session / "result.json")
        assert status["passed"] and status["cleanupSafe"] and not status["rollbackNeeded"], status
        deadline = time.monotonic() + 60
        owned = []
        while not owned and time.monotonic() < deadline:
            owned = game_processes(game / "InitialDUnity.exe")
            time.sleep(.5)
        assert len(owned) == 1, "Actual upgraded game did not restart"
        report.update(actualRestartPid=owned[0], updateSeconds=round(time.monotonic() - started, 3))
        game_processes(game / "InitialDUnity.exe", terminate=True)
        assert server.downloads == ["/core.zip", "/courses.zip"], server.downloads
        assert verify.sha256(session / "install.exe") == verify.sha256(ROOT / "Assets/Resources/UpdateInstaller.exe.bytes")
        assert all(not (session / item).exists() for item in ("stage", "backup", "game.zip", "content.zip"))
        print("Checking all installed files and preserved private data...", flush=True)
        for name, row in inventory["files"].items():
            assert (game / name).stat().st_size == row["bytes"] and verify.sha256(game / name) == row["sha256"], name
        for name, data in PRIVATE.items():
            assert (game / name).read_bytes() == data and (game / name).stat().st_mtime_ns == private_stamps[name], name
        # Preserve unlisted legacy files; the native updater does not silently
        # delete them. The migration/removal policy is a separate gate.
        retained = sorted(set(before) - inventory["files"].keys())
        for name in retained:
            assert verify.sha256(game / name) == before[name]["sha256"], name
        startup = output / "upgraded-startup"
        with (output / "startup-stdout.log").open("wb") as log:
            result = subprocess.run([str(game / "InitialDUnity.exe"), "-force-d3d11", "-screen-fullscreen", "0",
                "-idas3-skip-update-once", "-idas3-rom-smoke", str(startup), "-idas3-rom-expect", "blocked",
                "-logFile", str(output / "startup-player.log")], cwd=game, stdout=log, stderr=subprocess.STDOUT,
                timeout=360, **verify.hidden_process_options())
        packaged.require_runtime_report(startup, target_version, result.returncode, "startup")
        for name, data in PRIVATE.items():
            assert (game / name).read_bytes() == data and (game / name).stat().st_mtime_ns == private_stamps[name], name
        for name, row in inventory["files"].items():
            assert verify.sha256(game / name) == row["sha256"], "Startup changed an installed file: " + name
        assert verify.candidate_inventory(candidate)["inventorySha256"] == inventory["inventorySha256"]
        report.update(passed=True, downloads=server.downloads, installedFiles=len(inventory["files"]),
                      privateFilesPreserved=True, payloadsCleaned=True, startupPassed=True, candidateUnchanged=True,
                      retainedLegacyFiles=retained, helperSha256=verify.sha256(session / "install.exe"))
        print("PASS full previous-release upgrade with current updater", flush=True)
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        if server:
            server.shutdown()
            server.server_close()
        if (game / "InitialDUnity.exe").is_file():
            game_processes(game / "InitialDUnity.exe", terminate=True)
        verify.write_json(output / "report.json", report)


if __name__ == "__main__":
    main()
