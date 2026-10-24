"""Verify packaged startup recovery after terminating an isolated result writer.

An owned helper is terminated at a durable result boundary. Its save fixture is
copied by the game's existing isolated diagnostic setup, then the real startup
path must recover it before announcing the native scene ready. The game is
stopped deliberately at that point; no full multiplayer-smoke pass is claimed.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import verify_release as verify
import verify_packaged_player as packaged
import build_release_candidate as build


def seed_profile(root):
    root.mkdir(parents=True, exist_ok=False)
    words = [0] * 307
    words[4], words[18], words[150] = 8, 321, 0x87123456
    data = bytearray(b"ID3PRF1\0" + struct.pack("<4I", 1, 8, 307, 0) + struct.pack("<307I", *words))
    checksum = 2166136261
    for i, value in enumerate(data):
        if i < 20 or i >= 24:
            checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    struct.pack_into("<I", data, 20, checksum)
    path = root / "driver_profiles_v1/car_08.profile"
    path.parent.mkdir()
    path.write_bytes(data)


def profile_state(root):
    data = (root / "driver_profiles_v1/car_08.profile").read_bytes()
    if len(data) != 1252 or data[:8] != b"ID3PRF1\0" or struct.unpack_from("<3I", data, 8) != (1, 8, 307):
        raise ValueError("Wrong native profile format")
    checksum = 2166136261
    for i, value in enumerate(data):
        if i < 20 or i >= 24:
            checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    if struct.unpack_from("<I", data, 20)[0] != checksum:
        raise ValueError("Native profile checksum failed")
    words = struct.unpack_from("<307I", data, 24)
    if words[4] != 8 or words[18] != 2321 or words[150] != 0x87123456:
        raise ValueError("Wrong reward or lost opaque profile field")
    def checked_envelope(path):
        envelope = packaged.read_json(path)
        payload = json.dumps(envelope["state"], ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if base64.b64encode(hashlib.sha256(payload).digest()).decode("ascii") != envelope["sha256"]:
            raise ValueError("Serialized state checksum differs: " + str(path))
        return envelope["state"]
    record = checked_envelope(root / "online_records_v1/car_08.json")
    if record["car"] != 8 or record["record"]["battles"] != 1 or record["record"]["wins"] != 1:
        raise ValueError("History was lost or credited twice")
    receipt = record["receipts"]
    if len(receipt) != 1 or receipt[0]["raceId"] != "123" or receipt[0]["won"] is not True:
        raise ValueError("Recovered result receipt is missing")
    journal = root / "online_result_v1.json"
    state = checked_envelope(journal)
    entry = state["entry"]
    if (state["version"] != 1 or entry["complete"] is not True or entry["raceId"] != "123"
            or entry["car"] != 8 or entry["winner"] != 0 or entry["won"] is not True
            or entry["reward"]["selection"] != 13 or entry["reward"]["points"] != 2000
            or entry["reward"]["after"] != list(words)):
        raise ValueError("Journal did not reach completion")
    if journal.read_bytes() != Path(str(journal) + ".previous").read_bytes() or Path(str(journal) + ".pending").exists():
        raise ValueError("Completed journal copies are not settled")
    names = ("driver_profiles_v1/car_08.profile", "online_records_v1/car_08.json", "online_result_v1.json", "online_result_v1.json.previous")
    return {"points": words[18], "record": record["record"], "generation": state["generation"],
            "sha256": {name: verify.sha256(root / name) for name in names}}


def hidden_start(command, cwd, stdout):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    return subprocess.Popen(list(map(str, command)), cwd=cwd, stdout=stdout, stderr=subprocess.STDOUT,
                            startupinfo=startup, **verify.hidden_process_options())


def stop_owned(process):
    if process.poll() is None:
        process.kill()
    return process.wait(timeout=15)


def prepare_fixture(run, helper, name):
    fixture = run.output / (name + "-interrupted-save")
    seed_profile(fixture)
    with (run.output / (name + "-helper.log")).open("wb") as log:
        process = hidden_start([helper, fixture, "native", "child", name], helper.parent, log)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline and process.poll() is None:
                if (fixture / "ready").is_file():
                    break
                time.sleep(.05)
            else:
                raise RuntimeError("Helper did not reach interruption boundary: " + name)
        finally:
            exit_code = stop_owned(process)
    if name == "after-record":
        obstruction = fixture / "online_result_v1.json.pending"
        if obstruction.parent.resolve() != fixture.resolve() or obstruction.is_symlink():
            raise ValueError("Unexpected owned obstruction path")
        obstruction.rmdir()  # Only the owned empty test directory; never recursive.
    return fixture, {"boundary": name, "helperPid": process.pid, "intentionalHelperExitCode": exit_code,
                     "fixtureJournalSha256": verify.sha256(fixture / "online_result_v1.json")}


def startup(run, private, fixture, name, expected_recovery):
    game_output = run.output / (name + "-game")  # The game requires a NEW path.
    player_log = run.output / (name + "-player.log")
    with socket.socket() as reserved:
        reserved.bind(("127.0.0.1", 0))
        port = reserved.getsockname()[1]
    command = [private / "InitialDUnity.exe", "-force-d3d11", "-screen-fullscreen", "0",
               "-idas3-skip-update-once", "-logFile", player_log,
               "-idas3-multiplayer-smoke", game_output, "-idas3-multiplayer-role", "host",
               "-idas3-multiplayer-port", str(port), "-idas3-multiplayer-world-only",
               "-idas3-multiplayer-saved-cars-check", "-idas3-multiplayer-saved-cars-fixture", fixture]
    entry = {"name": name, "passed": False, "scope": "Real scene startup only; no completed race or complete smoke-suite claim"}
    run.summary["checks"].append(entry)
    run.save()
    print("RUN " + name, flush=True)
    with (run.output / (name + "-stdout.log")).open("wb") as log:
        process = hidden_start(command, private, log)
        entry["pid"] = process.pid
        try:
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                text = player_log.read_text(encoding="utf-8", errors="replace") if player_log.exists() else ""
                if "IDAS3 Unity scene ready:" in text:
                    recovered = "IDAS3 recovered a previously interrupted online result." in text
                    if recovered != expected_recovery:
                        raise ValueError("Startup recovery status did not match the durable boundary")
                    if recovered and text.index("IDAS3 recovered") > text.index("IDAS3 Unity scene ready:"):
                        raise ValueError("Recovery occurred after native initialization")
                    entry["savedState"] = profile_state(game_output / "userdata")
                    entry["recoveredBeforeNativeReady"] = recovered
                    break
                if process.poll() is not None:
                    raise RuntimeError("Game exited before scene-ready confirmation")
                time.sleep(.1)
            else:
                raise TimeoutError("Packaged startup never confirmed native scene readiness")
        finally:
            entry["intentionalStartupStopExitCode"] = stop_owned(process)
            entry["stopReason"] = "Owned startup-only probe stopped after observation; not a clean full-smoke exit"
    entry["playerLogSha256"] = verify.sha256(player_log)
    entry["passed"] = True
    run.save()
    print("PASS " + name, flush=True)
    return game_output / "userdata", entry["savedState"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-evidence", required=True, type=Path)
    parser.add_argument("--helper-evidence", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if os.name != "nt" or sys.flags.optimize:
        parser.error("Windows Python with assertions enabled is required")
    evidence, helpers, rom, output = (build.require_plain_path(p) for p in
        (args.build_evidence, args.helper_evidence, args.rom, args.output))
    if output.is_relative_to(evidence) or output.is_relative_to(helpers) or not rom.is_file() or rom.suffix.lower() != ".chd":
        parser.error("Use an existing local CHD and a NEW evidence directory outside the build/helper evidence")
    run = verify.Run(output, "packaged-result-startup")
    try:
        candidate = evidence / "player"
        print("Verifying and copying candidate...", flush=True)
        inventory = verify.candidate_inventory(candidate)
        proof, version = packaged.require_build_evidence(evidence, inventory)
        helper_report = verify.require_passed_report(helpers / "report.json")
        for name, digest in helper_report["source_sha256"].items():
            if verify.sha256(ROOT / name) != digest:
                raise ValueError("Helper source changed: " + name)
        run.summary.update(builtSource=proof["source"], applicationVersion=version,
                           buildEvidence=str(evidence), buildEvidenceSha256=verify.sha256(evidence / "summary.json"),
                           scope=__doc__.strip(), driverSha256=verify.sha256(Path(__file__)),
                           nativeProfileRecoverySha256=proof["nativeArtifacts"]["Idas3Unity.dll"])
        helper_root = run.output / "helper"
        helper_root.mkdir()
        for name in ("ResultJournalDriver.exe", "ResultJournalDriver.exe.config", "Newtonsoft.Json.dll"):
            shutil.copyfile(helpers / name, helper_root / name)
        dll = candidate / "InitialDUnity_Data/Plugins/x86_64/Idas3Unity.dll"
        shutil.copyfile(dll, helper_root / "Idas3Unity.dll")
        private = run.output / "private-player"
        packaged.copy_player(candidate, private, inventory)
        (private / "rom").mkdir(exist_ok=True)
        target_rom = private / "rom/gds-0033.chd"
        if target_rom.exists():
            raise ValueError("Private ROM path unexpectedly already exists")
        shutil.copyfile(rom, target_rom)
        if verify.sha256(rom) != verify.sha256(target_rom):
            raise ValueError("Private ROM copy mismatch")
        run.summary["privatePlayerContainsRomDoNotUpload"] = True
        for boundary in ("after-intent", "after-tuning", "after-record", "completion-intent", "after-complete"):
            fixture, details = prepare_fixture(run, helper_root / "ResultJournalDriver.exe", boundary)
            run.summary.setdefault("interruptedFixtures", []).append(details)
            first_root, first = startup(run, private, fixture, boundary, boundary in ("after-intent", "after-tuning", "after-record"))
            _, repeated = startup(run, private, first_root, boundary + "-repeat", False)
            if first != repeated:
                raise ValueError("Repeated startup changed a completed reward or history")
        run.summary["candidateUnchanged"] = verify.candidate_inventory(candidate)["inventorySha256"] == inventory["inventorySha256"]
        # The marked private copy intentionally contains the user's ROM, which
        # the shipping allowlist must reject. Check only the originally verified
        # distributable paths; never weaken that allowlist to accept private data.
        run.summary["privateDistributableUnchanged"] = all(
            (private / name).stat().st_size == row["bytes"] and verify.sha256(private / name) == row["sha256"]
            for name, row in inventory["files"].items())
        if not run.summary["candidateUnchanged"] or not run.summary["privateDistributableUnchanged"]:
            raise ValueError("Distributable changed during startup probes")
        run.summary.update(passed=True, status="packaged_result_startup_checks_passed")
    except Exception as error:
        run.summary.update(passed=False, status="failed", error=repr(error))
        raise
    finally:
        run.save()
        print("Evidence: " + str(output / "summary.json"), flush=True)


if __name__ == "__main__":
    main()
