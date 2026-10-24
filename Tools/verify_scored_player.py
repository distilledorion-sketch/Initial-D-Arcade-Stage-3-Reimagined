"""Drive two real scored LAN races using a verified, isolated Windows candidate.

The winner changes on the connected rematch. The opponent is stationary and
times out. This does not certify EOS/NAT, two moving finishers or crash recovery.
"""
import argparse
import os
from pathlib import Path
import shutil
import socket
import struct
import sys
import time

import build_release_candidate as build
import verify_packaged_player as packaged
import verify_release as verify


def arguments(role, folder, peer, port, recovery=False):
    args = ["-idas3-multiplayer-smoke", str(folder), "-idas3-multiplayer-role", role,
            "-idas3-multiplayer-peer-output", str(peer), "-idas3-multiplayer-port", str(port),
            "-idas3-multiplayer-course", "0", "-idas3-multiplayer-world-only",
            "-idas3-multiplayer-scored-check"]
    if role == "join":
        args += ["-idas3-multiplayer-address", "127.0.0.1:" + str(port)]
    if recovery:
        args.append("-idas3-multiplayer-result-recovery-check")
    return args


def require_scored_report(folder, version, exit_code, role):
    report = verify.require_passed_report(folder / "report.json")
    detail = verify.require_passed_report(folder / "scored-report.json")
    if (exit_code != 0 or report.get("shutdownComplete") is not True
            or report.get("schema") != "idas3-two-client-smoke-v1"
            or report.get("applicationVersion") != version or report.get("role") != role
            or report.get("authorityEnabled") is not True or report.get("p2pRaceCheck") is not False
            or report.get("onlineRuntimeCheck") is not False or type(report.get("checks")) is not int or report["checks"] <= 0):
        raise ValueError("Scored player did not complete under the expected local authority")
    if (detail.get("schema") != "idas3-scored-race-cycle-v1" or detail.get("role") != role
            or detail.get("applicationVersion") != version or type(detail.get("checks")) is not int or detail["checks"] <= 0
            or detail.get("inputSamples", 0) < 10000
            or any(detail.get(key) is not True for key in (
                "connectedRematch", "persistedAfterLeaving", "unrelatedProfilePreserved"))):
        raise ValueError("Scored cycle evidence is incomplete")
    races = detail.get("races", [])
    if len(races) != 2 or any(not race.get("raceId") for race in races) or races[0].get("raceId") == races[1].get("raceId"):
        raise ValueError("Two distinct completed races are required")
    balance = 321
    for winner, race in enumerate(races):
        won = winner == (0 if role == "host" else 1)
        amount = 2000 if won else 1000
        balance += amount
        if (race.get("winner") != winner or race.get("naturalFinish") is not True
                or race.get("duplicatePreserved") is not True or race.get("tuningPoints") != amount
                or race.get("balance") != balance or race.get("result") != ("YOU WIN" if won else "YOU LOSE")
                or race.get("verifiedFrame", 0) <= 10000 or race.get("seconds", 0) <= 150
                or race.get("after", {}).get("battles") != winner + 1
                or race.get("after", {}).get("wins") != (int(won) if winner == 0 else 1)):
            raise ValueError("Scored race result failed independent report validation")
    if races[0].get("continued") is not True:
        raise ValueError("The first scored race did not continue into the same room")
    return report, detail


def require_recovery_report(folder, version, role):
    report = verify.require_passed_report(folder / "result-recovery-report.json")
    if (report.get("schema") != "idas3-scored-result-recovery-v1" or report.get("role") != role
            or report.get("applicationVersion") != version or len(report.get("cases", [])) != 2):
        raise ValueError("Unexpected result-recovery report")
    images = {}
    for cycle, case in enumerate(report["cases"]):
        expected_input = "controller" if (cycle == 0) == (role == "join") else "keyboard"
        if (case.get("cycle") != cycle or case.get("input") != expected_input or case.get("clicks") != 2
                or any(case.get(key) is not True for key in
                       ("passed", "durableIntent", "historyBlocked", "focusGuard", "heldGuard", "retried"))):
            raise ValueError("Result recovery lacks input, persistence or focus-guard evidence")
        expected = {f"result-recovery-{lang}-{width}x{height}.png": (width, height)
                    for lang in range(3) for width, height in
                    ((1024, 768), (1280, 720), (1920, 1080), (2560, 1080))} if cycle == 0 else {}
        if len(case.get("captures", [])) != len(expected) or set(case["captures"]) != set(expected):
            raise ValueError("Recovery language/resolution captures are incomplete")
        for name, dimensions in expected.items():
            image = folder / name
            with image.open("rb") as stream:
                header = stream.read(24)
            if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR" or struct.unpack(">II", header[16:24]) != dimensions:
                raise ValueError("Recovery capture has the wrong format or dimensions")
            images[name] = verify.sha256(image)
    return images


def pair(players, recovery=False):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    host, join = (players.run.output / role for role in ("host", "join"))
    hp, he = players.launch("host", arguments("host", host, join, port, recovery))
    deadline = time.monotonic() + 90
    while not (host / "room.json").exists():
        if hp.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError("Scored host did not open its private test room")
        time.sleep(.2)
    jp, je = players.launch("join", arguments("join", join, host, port, recovery))
    deadline = time.monotonic() + 930
    while hp.poll() is None or jp.poll() is None:
        for process, entry in ((hp, he), (jp, je)):
            if process.poll() not in (None, 0):
                raise RuntimeError(entry["name"] + " failed with exit " + str(process.returncode))
        if time.monotonic() > deadline:
            raise TimeoutError("Scored pair did not finish within 930 seconds")
        time.sleep(.5)
    for process, entry, folder, role in ((hp, he, host, "host"), (jp, je, join, "join")):
        report, detail = require_scored_report(folder, players.version, process.returncode, role)
        if recovery:
            entry["recoveryCaptures"] = require_recovery_report(folder, players.version, role)
            entry["recoveryReportSha256"] = verify.sha256(folder / "result-recovery-report.json")
        entry.update(passed=True, exitCode=process.returncode, reportedCount=report["checks"],
                     report=str((folder / "report.json").relative_to(players.run.output)),
                     reportSha256=verify.sha256(folder / "report.json"),
                     detailReport=str((folder / "scored-report.json").relative_to(players.run.output)),
                     detailSha256=verify.sha256(folder / "scored-report.json"))
        players.run.save()
        print("PASS scored " + role, flush=True)
    left, right = (packaged.read_json(folder / "scored-report.json") for folder in (host, join))
    for h, g in zip(left["races"], right["races"]):
        if any(h[key] != g[key] for key in ("raceId", "winner", "hostTicks", "guestTicks")):
            raise ValueError("Scored peers disagree on confirmed race identity or finish times")
    players.run.summary["peerResultsAgree"] = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-evidence", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--result-recovery", action="store_true",
                        help="Block private history writes and check the real retry modal, input and translated layouts")
    args = parser.parse_args()
    if os.name != "nt" or sys.flags.optimize:
        parser.error("Windows Python with assertions enabled is required")
    evidence, output, rom = (build.require_plain_path(path) for path in (args.build_evidence, args.output, args.rom))
    if output.is_relative_to(evidence) or not rom.is_file() or rom.suffix.lower() != ".chd":
        parser.error("Use an existing local CHD and a NEW evidence directory outside the build")
    run = verify.Run(output, "packaged-scored")
    players = None
    try:
        player = evidence / "player"
        print("Verifying and copying candidate...", flush=True)
        inventory = verify.candidate_inventory(player)
        proof, version = packaged.require_build_evidence(evidence, inventory)
        verify.write_json(output / "candidate.json", inventory)
        run.summary.update(builtSource=proof["source"], buildEvidence=str(evidence),
                           buildEvidenceSha256=verify.sha256(evidence / "summary.json"), applicationVersion=version,
                           candidate={k: v for k, v in inventory.items() if k != "files"},
                           tools={"python": sys.version, "driverSha256": verify.sha256(Path(__file__)),
                                  "sharedDriverSha256": verify.sha256(Path(packaged.__file__))}, scope=__doc__)
        run.summary["resultRecoveryCheck"] = args.result_recovery
        if args.result_recovery:
            run.summary["scope"] += " Includes real scored-result save failures, keyboard/controller Retry and actual modal Repaints. Capture hashes do not substitute for visual review."
        run.summary["pendingGates"] = ["Two moving finishers and remaining full-cycle cases",
            "Process restart, disk failure and cross-store reward recovery",
            "Two-PC EOS matchmaking and room-code cycles on different networks",
            "Remaining install, identity, physics, performance, input and presentation acceptance"]
        run.save()
        private = output / "private-player"
        packaged.copy_player(player, private, inventory)
        (private / "rom").mkdir(exist_ok=True)
        shutil.copyfile(rom, private / "rom/gds-0033.chd")
        players = packaged.Players(run, private, version)
        players.case("scored-cycle", lambda: pair(players, args.result_recovery))
        for name, row in inventory["files"].items():
            if verify.sha256(private / name) != row["sha256"]:
                raise RuntimeError("Diagnostic changed distributable file " + name)
        if verify.candidate_inventory(player)["inventorySha256"] != inventory["inventorySha256"]:
            raise RuntimeError("Original candidate changed during scored test")
        run.summary.update(candidateUnchanged=True, testDistributableFilesUnchanged=True)
        run.finish()
    except Exception as error:
        run.finish(error)
        print("FAIL: " + str(error), file=sys.stderr)
    finally:
        if players:
            players.close()
    print("Evidence: " + str(output / "summary.json"))
    print("Private test copy contains ROM data. Never distribute it. Release acceptance remains incomplete.")
    return 0 if run.summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
