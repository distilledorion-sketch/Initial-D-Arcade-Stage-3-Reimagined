"""Exercise a fresh Windows player in a private copy with isolated test saves.

Local TCP tests do not certify EOS matchmaking, NAT traversal, scored rewards,
or visual layout. The startup suite includes real GUI capture and may fail in
a hidden window. Failures remain failures while independent cases continue.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
import uuid

import build_release_candidate as build
import verify_release as verify

PAIR_FLAGS = {
    "return-rematch": ("-idas3-multiplayer-return-check",),
    "waiting-no": ("-idas3-multiplayer-return-check", "-idas3-multiplayer-waiting-no-check"),
    "disconnect": ("-idas3-multiplayer-disconnect-check",),
}


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def require_build_evidence(evidence, inventory):
    proof = verify.require_passed_report(evidence / "summary.json")
    if (proof.get("suite") != "fresh-build" or proof.get("sourceUnchanged") is not True
            or proof.get("inputsUnchanged") is not True or not proof.get("checks")
            or any(check.get("passed") is not True for check in proof["checks"])):
        raise ValueError("Fresh build lacks successful unchanged-input evidence")
    recorded = read_json(evidence / "candidate.json")
    identity = inventory["inventorySha256"]
    if (identity != proof.get("candidate", {}).get("inventorySha256")
            or identity != recorded.get("inventorySha256")
            or identity != verify.digest_json(recorded.get("files", {}))):
        raise ValueError("Candidate differs from the fresh build evidence")
    unity = verify.require_passed_report(evidence / "unity-build.json")
    stages = [check for check in proof["checks"] if check.get("name") == "unity-build"]
    if len(stages) != 1 or stages[0].get("reportSha256") != verify.sha256(evidence / "unity-build.json"):
        raise ValueError("Unity build report does not match recorded evidence")
    if not unity.get("applicationVersion"):
        raise ValueError("Unity build report lacks the application version")
    return proof, unity["applicationVersion"]


def copy_player(source, destination, inventory):
    destination.mkdir(exist_ok=False)
    for name, row in inventory["files"].items():
        # inventory was independently collected from the shipping allowlist,
        # never from paths supplied by an evidence file.
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)
        if target.stat().st_size != row["bytes"] or verify.sha256(target) != row["sha256"]:
            raise ValueError("Private test copy differs: " + name)
    (destination / "PRIVATE_TEST_ONLY.txt").write_text(
        "Private diagnostic copy. May contain an original ROM. Never distribute or upload this folder.\n",
        encoding="utf-8")


def require_runtime_report(folder, version, exit_code, kind, role=None):
    report = verify.require_passed_report(folder / "report.json")
    if exit_code != 0 or report.get("shutdownComplete") is not True:
        raise ValueError("Player did not exit cleanly")
    if report.get("applicationVersion") != version:
        raise ValueError("Player report has a different application version")
    if type(report.get("checks")) is not int or report["checks"] <= 0:
        raise ValueError("Player report contains no checks")
    if kind == "startup":
        if report.get("expectation") != "blocked" or report.get("verified") is not False or report.get("nativeInitialized") is not False:
            raise ValueError("Missing-ROM startup did not remain blocked")
        if not (folder / "startup.png").is_file():
            raise ValueError("Missing-ROM startup has no visual capture")
    elif kind == "mode-flow":
        if report.get("schema") != "idas3-mode-flow-smoke-v1":
            raise ValueError("Unexpected mode-flow report")
    else:
        if report.get("schema") != "idas3-two-client-smoke-v1" or report.get("role") != role:
            raise ValueError("Unexpected two-client report or role")
        if (report.get("onlineRuntimeCheck") is not False or report.get("p2pRaceCheck") is not False
                or report.get("authorityEnabled") is not True):
            raise ValueError("Expected local TCP with native authority, not a live service test")
        detail_name = "disconnect-report.json" if kind == "disconnect" else "return-lobby-report.json"
        detail = verify.require_passed_report(folder / detail_name)
        fields = ("neutralResult", "unhideable", "saveUnchanged", "acknowledged") if kind == "disconnect" else (
            "naturalFinish", "connectedLobby", "savesUnchanged", "secondCountdown", "secondDrive", "musicSelectionPreserved")
        if detail.get("role") != role or any(detail.get(field) is not True for field in fields):
            raise ValueError("Incomplete two-client result evidence")
        return report, detail_name
    return report, None


def pair_arguments(case, role, folder, peer, port):
    args = ["-idas3-multiplayer-smoke", str(folder), "-idas3-multiplayer-role", role,
            "-idas3-multiplayer-peer-output", str(peer), "-idas3-multiplayer-port", str(port),
            "-idas3-multiplayer-course", "0", "-idas3-multiplayer-world-only", *PAIR_FLAGS[case]]
    if role == "join":
        args += ["-idas3-multiplayer-address", "127.0.0.1:" + str(port)]
    return args


class Players:
    def __init__(self, run, player, version):
        self.run, self.player, self.version = run, player, version
        self.owned = []

    def launch(self, name, arguments):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        log = (self.run.output / (name + "-stdout.log")).open("wb")
        command = [str(self.player / "InitialDUnity.exe"), "-force-d3d11", "-screen-fullscreen", "0",
                   "-idas3-skip-update-once", "-logFile", str(self.run.output / (name + "-player.log")), *arguments]
        entry = {"name": name, "command": command, "passed": False}
        self.run.summary["checks"].append(entry)
        self.run.save()
        try:
            process = subprocess.Popen(command, cwd=self.player, stdout=log, stderr=subprocess.STDOUT,
                                       startupinfo=startup, **verify.hidden_process_options())
        except Exception:
            log.close()
            raise
        self.owned.append((process, log))
        entry["pid"] = process.pid
        self.run.save()
        print("RUN " + name, flush=True)
        return process, entry

    def collect(self, process, entry, folder, kind, role=None):
        entry["exitCode"] = process.returncode
        report, detail = require_runtime_report(folder, self.version, process.returncode, kind, role)
        entry.update(passed=True, report=str((folder / "report.json").relative_to(self.run.output)),
                     reportSha256=verify.sha256(folder / "report.json"), reportedCount=report["checks"])
        if detail:
            entry.update(detailReport=str((folder / detail).relative_to(self.run.output)),
                         detailSha256=verify.sha256(folder / detail))
        self.run.save()
        print("PASS " + entry["name"], flush=True)

    def single(self, name, flag, kind, extra=()):
        folder = self.run.output / name
        process, entry = self.launch(name, [flag, str(folder), *extra])
        process.wait(timeout=360)
        self.collect(process, entry, folder, kind)

    def pair(self, case):
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        host, join = (self.run.output / (case + "-" + role) for role in ("host", "join"))
        hp, he = self.launch(case + "-host", pair_arguments(case, "host", host, join, port))
        deadline = time.monotonic() + 90
        while not (host / "room.json").exists():
            if hp.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError("Host did not publish its private local test room")
            time.sleep(.2)
        jp, je = self.launch(case + "-join", pair_arguments(case, "join", join, host, port))
        deadline = time.monotonic() + 420
        while hp.poll() is None or jp.poll() is None:
            for process, entry in ((hp, he), (jp, je)):
                if process.poll() not in (None, 0):
                    raise RuntimeError(entry["name"] + " exited " + str(process.returncode))
            if time.monotonic() > deadline:
                raise TimeoutError("Local pair did not complete")
            time.sleep(.5)
        self.collect(hp, he, host, case, "host")
        self.collect(jp, je, join, case, "join")

    def close(self):
        for process, log in self.owned:
            try:
                if process.poll() is None:
                    subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                   stdout=log, stderr=subprocess.STDOUT, **verify.hidden_process_options())
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=15)
            finally:
                log.close()
        self.owned.clear()

    def case(self, name, action):
        first = len(self.run.summary["checks"])
        try:
            action()
        except Exception as error:
            pending = [check for check in self.run.summary["checks"][first:] if not check["passed"]]
            if not pending:
                pending = [{"name": name, "passed": False}]
                self.run.summary["checks"].extend(pending)
            for check in pending:
                check["error"] = str(error)
            print("FAIL " + name + ": " + str(error), flush=True)
        finally:
            self.close()
            self.run.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-evidence", type=Path, required=True, help="Successful fresh-build evidence directory")
    parser.add_argument("--player", type=Path, help="Clean candidate; defaults to the evidence directory's player subdirectory")
    parser.add_argument("--rom", type=Path, help="Explicit local GDS-0033 CHD; copied only into a marked private test player")
    parser.add_argument("--suite", choices=("all", "startup", "flows"), default="all")
    parser.add_argument("--output", type=Path, help="NEW evidence directory, outside the build and player")
    args = parser.parse_args()
    if os.name != "nt" or sys.flags.optimize:
        parser.error("Requires Windows with Python assertions enabled")
    if args.suite != "startup" and args.rom is None:
        parser.error("Gameplay diagnostics require an explicit --rom CHD")
    evidence = build.require_plain_path(args.build_evidence)
    player = build.require_plain_path(args.player or evidence / "player")
    output = build.require_plain_path(args.output or verify.ROOT / "Verification" / ("player-check-" + uuid.uuid4().hex))
    if output.is_relative_to(player) or output.is_relative_to(evidence):
        parser.error("Evidence must be outside the candidate and fresh-build directories")
    rom = build.require_plain_path(args.rom) if args.rom else None
    if rom and (not rom.is_file() or rom.suffix.lower() != ".chd"):
        parser.error("--rom must name an existing CHD; validation is performed by the game")
    run = verify.Run(output, "packaged-" + args.suite)
    players = None
    try:
        print("Checking candidate against fresh-build evidence...", flush=True)
        inventory = verify.candidate_inventory(player)
        proof, version = require_build_evidence(evidence, inventory)
        verify.write_json(output / "candidate.json", inventory)
        run.summary.update(candidate={k: v for k, v in inventory.items() if k != "files"},
                           buildEvidence=str(evidence), buildEvidenceSha256=verify.sha256(evidence / "summary.json"),
                           builtSource=proof["source"], applicationVersion=version,
                           tools={"python": sys.version, "driverSha256": verify.sha256(Path(__file__))},
                           scope="Private saves, two packaged processes using loopback TCP and shared native authority. Natural timeouts, native points/Continue, rematch and disconnect recovery. Not Internet/EOS, scored reward or human-driven finish acceptance.")
        run.summary["pendingGates"] = [
            "Remaining native reference coverage",
            "Nonzero win/loss rewards exactly once and both driver finish orders",
            "Two-PC Quick Match and room-code cycles on different networks",
            "Supported install/update journeys and user-data preservation",
            "Hardware performance, physical controllers, language/resolution and actual window-pixel acceptance",
        ]
        if args.suite == "flows":
            run.summary["pendingGates"].append("ROM startup state and visual capture (not selected)")
        if args.suite == "startup":
            run.summary["pendingGates"].append("Packaged race-flow diagnostics (not selected)")
        run.save()
        private = output / "private-player"
        print("Copying distributable files into a marked private test player...", flush=True)
        copy_player(player, private, inventory)
        players = Players(run, private, version)
        if args.suite != "flows":
            players.case("rom-missing", lambda: players.single("rom-missing", "-idas3-rom-smoke", "startup", ("-idas3-rom-expect", "blocked")))
        if args.suite != "startup":
            (private / "rom").mkdir(exist_ok=True)
            shutil.copyfile(rom, private / "rom/gds-0033.chd")
            players.case("mode-flow", lambda: players.single("mode-flow", "-idas3-mode-flow-smoke", "mode-flow"))
            for case in PAIR_FLAGS:
                players.case(case, lambda case=case: players.pair(case))
        print("Rechecking distributable bytes in the test copy and original candidate...", flush=True)
        for name, row in inventory["files"].items():
            if verify.sha256(private / name) != row["sha256"]:
                raise RuntimeError("Diagnostic changed a distributable file: " + name)
        if verify.candidate_inventory(player)["inventorySha256"] != inventory["inventorySha256"]:
            raise RuntimeError("Candidate changed during diagnostics")
        run.summary.update(candidateUnchanged=True, testDistributableFilesUnchanged=True)
        run.finish()
    except Exception as error:
        run.finish(error)
        print("FAIL: " + str(error), file=sys.stderr)
    finally:
        if players:
            players.close()
    print("Evidence: " + str(output / "summary.json"))
    print("Release readiness remains incomplete. Private test player must never be uploaded.")
    return 0 if run.summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
