"""Check release evidence against the exact candidate; never publish or launch it.

Automated reports are revalidated, not accepted merely because passed=true.
The remaining real-world acceptance requires an attributable manual review with
hashed supporting artifacts. This verifies the review record, not the truth of
an arbitrary person's claims. Do not manufacture a review to clear this gate.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys
import zipfile

import build_release_candidate as build
import verify_packaged_player as packaged
import verify_release as verify
import verify_scored_player as scored
import verify_release_upgrade as upgrade

SCHEMA = "idas3-release-acceptance-v1"
REVIEW_SCHEMA = "idas3-release-review-v1"
REQUIRED_REPORTS = ("build", "candidate", "packaged", "scored", "startupRecovery", "freshInstall", "upgrade")
# These IDs describe real player journeys, not substitutes such as local TCP,
# a driver helper, an injected catalog, or a compilation-only result.
REVIEW_CHECKS = {
    "native-reference": "All registered native tests, including any privilege-dependent cases, with the candidate's native inputs.",
    "quick-match": "Ten consecutive complete EOS Quick Match cycles on two Windows PCs on different Internet connections, with correlated logs from both peers.",
    "room-code": "Ten consecutive complete EOS room-code cycles on two Windows PCs on different Internet connections, covering public and private rooms.",
    "online-recovery": "Join/cancel/retry, host disappearance, unreachable peer, service failure, both finish orders, Yes/Yes, Yes/No and immediate rematch.",
    "moving-finishers": "Two moving drivers in the packaged game: the leader's finish ends both races, with matching results and exactly-once rewards.",
    "input-ownership": "Physical controller reconnect, keyboard/controller transitions, focus loss and recovery through menus, race, results and rematch.",
    "save-interruption": "Natural race-process interruption and disk-write failures; recover before gameplay loads, preserve unrelated saves and administrative resets, and document power-loss limits.",
    "published-updater": "Update from each supported previously shipped updater, using its actual executable, and verify game startup, user-data preservation and recovery after interruption.",
    "public-install": "Fresh install and Full Repair using actual downloadable packages; verify successful game startup and preserved user data.",
    "presentation": "English/Japanese/Chinese pause, online, music, results and Continue at 640x480, 1280x720, 1920x1080 and 2560x1080; inspect glyphs, bounds, focus, background and audio restoration.",
    "performance": "Declared minimum hardware and development PC: full routes and ten race cycles, CPU/GPU frame times including p99, memory and loading, against documented settings and budgets.",
    "arcade-behavior": "Controlled Arcade Stage 3 reference comparison for FR/FF/AWD/MR on original courses: timing, distance, acceleration, corner speed, steering and wall losses; imported handling documented separately.",
    "identity-moderation": "Verified online/leaderboard identity relationship, rename/reinstall recovery, bans and stat resets across supported recovery paths, with a migration plan.",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def plain_file(path):
    path = build.require_plain_path(path)
    require(path.is_file(), "Missing evidence file: " + str(path))
    return path


def load(path):
    data = json.loads(plain_file(path).read_text(encoding="utf-8-sig"))
    require(isinstance(data, dict), "Evidence must be a JSON object: " + str(path))
    return data


def reference(base, value):
    require(isinstance(value, dict) and isinstance(value.get("path"), str) and value["path"], "Missing evidence path")
    require(isinstance(value.get("sha256"), str) and re.fullmatch(r"[0-9a-f]{64}", value["sha256"]), "Missing evidence SHA-256")
    path = plain_file(base / value["path"])
    require(verify.sha256(path) == value["sha256"], "Evidence bytes changed: " + str(path))
    return path


def inside(base, name):
    require(isinstance(name, str) and name, "Missing report path")
    path = plain_file(base / name)
    require(path.is_relative_to(base.resolve()), "Nested report escapes its evidence directory")
    return path


def positive(data):
    require(data.get("passed") is True and not data.get("error"), "Report failed or lacks a positive result")


def stages(path, data, expected, required_reports=()):
    positive(data)
    rows = data.get("checks", [])
    names = [row.get("name") for row in rows]
    require(len(names) == len(set(names)) and set(names) == set(expected), "Missing, duplicate or unexpected verification stages")
    for row in rows:
        positive(row)
        require(type(row.get("exitCode")) is int and row["exitCode"] == 0, "Stage did not exit successfully: " + row["name"])
        require(row["name"] not in required_reports or "report" in row, "Required stage report omitted: " + row["name"])
        for key, hash_key in (("report", "reportSha256"), ("detailReport", "detailSha256")):
            if key in row:
                target = inside(path.parent, row[key])
                require(verify.sha256(target) == row.get(hash_key), "Nested report hash mismatch: " + row[key])
                positive(load(target))
    return {row["name"]: row for row in rows}


def source_checks():
    return ("verification-tool-tests", "fresh-build-tool-tests", "packaged-player-tool-tests", "setup-package-tool-tests",
            "release-acceptance-tool-tests", "publication-tool-tests", "backend", *verify.MANAGED_TESTS, *verify.PACKAGE_TESTS,
            "setup-launcher", "setup-extraction", "upgrade-archive-contract", "updater-configure", "updater-build",
            *("updater-" + kind + "-" + case for kind in ("compiled", "bundled")
              for case in ("installation", "split", "long-paths", "recovery")),
            "updater-cache", "recovery-launcher", "setup-package", "localization", "candidate-compatibility")


def same_candidate(data, inventory):
    require(data.get("candidate", {}).get("inventorySha256") == inventory["inventorySha256"], "Report is for a different candidate")
    require(data.get("candidateUnchanged") is True, "Candidate changed during verification")


def same_build(data, digest):
    require(data.get("buildEvidenceSha256") == digest, "Report is not bound to this fresh build")


def review_record(path, data, source, inventory, version):
    positive(data)
    require(data.get("schema") == REVIEW_SCHEMA, "Unsupported manual review schema")
    require(data.get("sourceCommit") == source["commit"] and data.get("candidateInventorySha256") == inventory["inventorySha256"]
            and data.get("applicationVersion") == version, "Manual review targets a different candidate")
    require(isinstance(data.get("reviewer"), str) and data["reviewer"].strip(), "Manual review must identify its reviewer")
    timestamp = datetime.fromisoformat(data.get("reviewedAt", ""))
    require(timestamp.tzinfo is not None and timestamp <= datetime.now(timezone.utc), "Manual review needs a completed, timezone-qualified timestamp")
    rows = data.get("checks", [])
    names = [row.get("id") for row in rows]
    require(len(names) == len(set(names)) and set(names) == set(REVIEW_CHECKS), "Manual acceptance matrix is incomplete")
    for row in rows:
        positive(row)
        require(isinstance(row.get("observations"), str) and row["observations"].strip(), "Missing review observations: " + row["id"])
        require(isinstance(row.get("evidence"), list) and row["evidence"], "Missing supporting artifacts: " + row["id"])
        for artifact in row["evidence"]:
            reference(path.parent, artifact)


def check(manifest_path, player):
    """Recheck the complete local acceptance chain. No cached pass is trusted."""
    require(not sys.flags.optimize, "Assertions must remain enabled for the existing shipping validators")
    manifest_path = plain_file(manifest_path)
    manifest_digest = verify.sha256(manifest_path)
    manifest = load(manifest_path)
    require(manifest.get("schema") == SCHEMA, "Unsupported release acceptance schema")
    reports = manifest.get("reports", {})
    require(set(reports) == set(REQUIRED_REPORTS), "Required release reports are missing or unexpected")
    paths = {name: reference(manifest_path.parent, reports[name]) for name in REQUIRED_REPORTS}
    review_path = reference(manifest_path.parent, manifest.get("review"))
    source = verify.source_identity()
    require(not source["workingTreeChanges"], "Commit tracked changes before release acceptance")
    tracked = set(verify.git("ls-files", "-z", "--cached").decode("utf-8").split("\0"))
    require(set(source["files"]) <= tracked, "Commit relevant untracked source before release acceptance")
    require(manifest.get("sourceCommit") == source["commit"], "Acceptance does not target the current source commit")
    inventory = verify.candidate_inventory(build.require_plain_path(player))
    require(manifest.get("candidateInventorySha256") == inventory["inventorySha256"], "Acceptance targets different player bytes")
    proof, version = packaged.require_build_evidence(paths["build"].parent, inventory)
    require(paths["build"].name == "summary.json", "Fresh-build reference must be summary.json")
    require(proof["source"]["commit"] == source["commit"] and
            proof["source"]["codeAndConfigSha256"] == source["codeAndConfigSha256"], "Build source differs from the current committed code/config")
    require(manifest.get("applicationVersion") == version, "Acceptance version differs from the built player")
    stages(paths["build"], proof, ("native-configure", "native-build", "native-tests", "unity-build"), ("unity-build",))
    build_source = load(paths["build"].parent / "source.json")
    require(verify.digest_json(build_source.get("files", {})) == source["codeAndConfigSha256"], "Build source inventory does not match")
    inputs = load(paths["build"].parent / "inputs.json")
    require(verify.digest_json(inputs.get("files", {})) == proof.get("inputInventorySha256"), "Build-input inventory does not match")
    require(build.require_native_results(paths["build"].parent / "native-results.xml") == proof.get("nativeTests"), "Native test results changed")
    for name in ("Idas3Unity.dll", "Idas3WheelFeedback.dll"):
        expected = proof.get("nativeArtifacts", {}).get(name)
        require(isinstance(expected, str) and re.fullmatch(r"[0-9a-f]{64}", expected)
                and inventory["files"].get("InitialDUnity_Data/Plugins/x86_64/" + name, {}).get("sha256") == expected,
                "Candidate native plugin differs from build: " + name)
    build_digest = verify.sha256(paths["build"])
    candidate = load(paths["candidate"])
    require(candidate.get("suite") == "candidate" and candidate.get("sourceUnchanged") is True, "Candidate verification did not cover unchanged source")
    require(candidate.get("source", {}).get("codeAndConfigSha256") == source["codeAndConfigSha256"]
            and candidate.get("source", {}).get("commit") == source["commit"], "Candidate tests used different code/config")
    same_candidate(candidate, inventory)
    no_report = {"verification-tool-tests", "fresh-build-tool-tests", "packaged-player-tool-tests", "setup-package-tool-tests",
                 "release-acceptance-tool-tests", "publication-tool-tests", "backend", "rom_package", "custom_music_package", "updater-configure", "updater-build"}
    stages(paths["candidate"], candidate, source_checks(), set(source_checks()) - no_report)

    local = load(paths["packaged"])
    require(local.get("suite") == "packaged-all" and local.get("selectedCases") == list(packaged.selected_cases("all")), "Full packaged suite is required")
    same_build(local, build_digest)
    same_candidate(local, inventory)
    require(local.get("testDistributableFilesUnchanged") is True, "Private packaged player changed")
    kinds = {"rom-missing": ("startup", None), "mode-flow": ("mode-flow", None), "updater-session": ("updater-session", None)}
    kinds.update({case + "-" + role: (case, role) for case in packaged.PAIR_FLAGS for role in ("host", "join")})
    rows = stages(paths["packaged"], local, kinds, kinds)
    for name, (kind, role) in kinds.items():
        require(role is None or "detailReport" in rows[name], "Paired runtime detail report omitted")
        folder = inside(paths["packaged"].parent, rows[name]["report"]).parent
        packaged.require_runtime_report(folder, version, rows[name]["exitCode"], kind, role)

    earned = load(paths["scored"])
    require(earned.get("suite") == "packaged-scored" and earned.get("resultRecoveryCheck") is True
            and earned.get("peerResultsAgree") is True and earned.get("testDistributableFilesUnchanged") is True,
            "Scored tests must include failed-save recovery and matching peers")
    same_build(earned, build_digest)
    same_candidate(earned, inventory)
    rows = stages(paths["scored"], earned, ("host", "join"), ("host", "join"))
    details = []
    for role, row in rows.items():
        require("detailReport" in row, "Scored runtime detail report omitted")
        folder = inside(paths["scored"].parent, row["report"]).parent
        _, detail = scored.require_scored_report(folder, version, row["exitCode"], role)
        details.append(detail)
        require(verify.sha256(folder / "result-recovery-report.json") == row.get("recoveryReportSha256"), "Recovery report changed")
        require(scored.require_recovery_report(folder, version, role) == row.get("recoveryCaptures"), "Recovery screenshots changed")
    for left, right in zip(details[0]["races"], details[1]["races"]):
        require(all(left[key] == right[key] for key in ("raceId", "winner", "hostTicks", "guestTicks")), "Scored peers disagree")

    startup = load(paths["startupRecovery"])
    positive(startup)
    same_build(startup, build_digest)
    require(startup.get("suite") == "packaged-result-startup" and startup.get("candidateUnchanged") is True
            and startup.get("privateDistributableUnchanged") is True, "Startup recovery is incomplete")
    boundaries = ("after-intent", "after-tuning", "after-record", "completion-intent", "after-complete")
    rows = startup.get("checks", [])
    by_name = {row.get("name"): row for row in rows}
    require(len(rows) == 10 and set(by_name) == {name + suffix for name in boundaries for suffix in ("", "-repeat")}, "Startup recovery boundaries are incomplete")
    for name, row in by_name.items():
        positive(row)
        log = inside(paths["startupRecovery"].parent, name + "-player.log")
        require(verify.sha256(log) == row.get("playerLogSha256"), "Startup log changed")
        require(row.get("recoveredBeforeNativeReady") is (name in boundaries[:3]), "Startup recovery ordering mismatch")
    for name in boundaries:
        require(by_name[name].get("savedState") and by_name[name]["savedState"] == by_name[name + "-repeat"].get("savedState"), "Repeated startup changed saved state")

    install = load(paths["freshInstall"])
    positive(install)
    full = install.get("fullCandidate", {})
    positive(full)
    same_build(full, build_digest)
    require(full.get("inventorySha256") == inventory["inventorySha256"] and full.get("version") == version,
            "Fresh installation did not verify this complete candidate")
    require(full.get("files") == inventory["fileCount"] and full.get("bytes") == inventory["bytes"], "Fresh installation omitted candidate data")
    require(install.get("installerSha256") == verify.sha256(verify.ROOT / "Tools/Install-InitialD.ps1"), "Fresh installation tested a different setup script")
    require(install.get("tests") and all(row.get("passed") is True and not row.get("error") for row in install["tests"]), "Fresh-install cases failed")
    full_cases = [row for row in install["tests"] if row.get("name") == "full-candidate"]
    require(len(full_cases) == 1 and type(full_cases[0].get("exitCode")) is int and full_cases[0]["exitCode"] == 0,
            "Missing successful full-candidate extraction process")
    updated = load(paths["upgrade"])
    positive(updated)
    same_build(updated, build_digest)
    require(updated.get("candidateInventorySha256") == inventory["inventorySha256"] and updated.get("targetVersion") == version,
            "Upgrade did not verify this candidate")
    require(all(updated.get(key) is True for key in ("candidateUnchanged", "privateFilesPreserved", "payloadsCleaned", "startupPassed")), "Upgrade acceptance is incomplete")
    require(updated.get("installedFiles") == inventory["fileCount"], "Upgrade omitted candidate files")
    archives = updated.get("targetArchives", [])
    require(len(archives) == 2, "Upgrade requires both full downloads")
    installed_archives = full.get("archives", [])
    require(len(installed_archives) == 2 and sorted(row.get("sha256", "") for row in installed_archives)
            == sorted(row.get("sha256", "") for row in archives), "Fresh install and upgrade must test the same downloads")
    archive_paths = [plain_file(paths["upgrade"].parent / row["path"]) for row in archives]
    # Read names before hashing payloads. Even a ROM README or empty directory
    # would break older updaters and must not be silently omitted by inventory.
    names = []
    for archive in archive_paths:
        require(0 < archive.stat().st_size < 2 * 1024**3, "Download exceeds the asset size limit")
        with zipfile.ZipFile(archive) as contents:
            names.extend(contents.namelist())
    require(len(names) == len(set(names)) and set(names) == set(inventory["files"]), "Downloads contain omitted or unexpected entries")
    for row in archives:
        reference(paths["upgrade"].parent, row)
    contents = upgrade.archive_inventory(archive_paths)
    require({name: {"bytes": row["size"], "sha256": row["sha256"]} for name, row in contents.items()} == inventory["files"], "Tested downloads differ from the candidate")
    review_record(review_path, load(review_path), source, inventory, version)
    # Re-read the named proof files: a stale approval must not survive edits
    # made while a large candidate/archive inventory was being checked.
    for name in REQUIRED_REPORTS:
        reference(manifest_path.parent, reports[name])
    reference(manifest_path.parent, manifest["review"])
    require(verify.sha256(manifest_path) == manifest_digest, "Acceptance manifest changed during verification")
    require(verify.candidate_inventory(player)["inventorySha256"] == inventory["inventorySha256"], "Candidate changed during acceptance")
    after = verify.source_identity()
    require(after["codeAndConfigSha256"] == source["codeAndConfigSha256"] and after["commit"] == source["commit"]
            and not after["workingTreeChanges"], "Source changed during acceptance")
    return {"schema": SCHEMA, "passed": True, "releaseReady": True, "sourceCommit": source["commit"],
            "applicationVersion": version, "candidateInventorySha256": inventory["inventorySha256"],
            "manifestSha256": manifest_digest, "manualReviewSha256": verify.sha256(review_path),
            "scope": "Automated evidence revalidated and complete attributable review record checked. Manual real-world observations remain the named reviewer's responsibility."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--player", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="NEW local report file; never upload private evidence")
    args = parser.parse_args()
    output = build.require_plain_path(args.output)
    require(not output.exists(), "Do not overwrite acceptance evidence")
    report = {"schema": SCHEMA, "passed": False, "releaseReady": False}
    try:
        report = check(args.manifest, args.player)
    except Exception as error:
        report["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps(report))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
