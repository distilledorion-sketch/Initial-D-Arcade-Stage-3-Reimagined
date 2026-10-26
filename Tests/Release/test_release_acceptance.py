"""Acceptance must reject missing, stale, contradictory or unrelated evidence."""
import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools"))
import release_acceptance as gate
import verify_release as verify


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def ref(self, path):
        return dict(path=str(path), sha256=verify.sha256(path))

    def test_bare_cached_pass_cannot_approve_a_release(self):
        path = self.write("cached.json", dict(passed=True, releaseReady=True))
        with patch.object(verify, "candidate_inventory", side_effect=AssertionError("should reject before reading player")):
            with self.assertRaisesRegex(ValueError, "schema"):
                gate.check(path, self.root / "player")

    def test_partial_gate_is_rejected_before_inventory(self):
        path = self.write("incomplete.json", dict(schema=gate.SCHEMA, reports={"build": {}}))
        with self.assertRaisesRegex(ValueError, "Required release reports"):
            gate.check(path, self.root / "player")

    def test_reference_requires_exact_bytes(self):
        path = self.write("proof.json", dict(passed=True))
        reference = self.ref(path)
        self.assertEqual(gate.reference(self.root, reference), path)
        self.write("proof.json", dict(passed=False))
        with self.assertRaisesRegex(ValueError, "changed"):
            gate.reference(self.root, reference)

    def test_nested_report_cannot_escape_directory(self):
        self.write("outside.json", dict(passed=True))
        folder = self.root / "inside"
        folder.mkdir()
        with self.assertRaisesRegex(ValueError, "escapes"):
            gate.inside(folder, "../outside.json")

    def test_stage_success_requires_all_cases_and_reports(self):
        report = self.write("nested.json", dict(passed=True))
        summary = dict(passed=True, checks=[dict(name="case", passed=True, exitCode=0,
                      report=report.name, reportSha256=verify.sha256(report))])
        gate.stages(self.root / "summary.json", summary, ("case",), ("case",))
        mutations = [lambda x: x["checks"].clear(), lambda x: x["checks"].append(copy.deepcopy(x["checks"][0])),
                     lambda x: x["checks"][0].update(exitCode=7), lambda x: x["checks"][0].update(error="failure"),
                     lambda x: x["checks"][0].pop("report"), lambda x: x["checks"][0].update(reportSha256="0" * 64)]
        for change in mutations:
            altered = copy.deepcopy(summary)
            change(altered)
            with self.subTest(change=change), self.assertRaises(ValueError):
                gate.stages(self.root / "summary.json", altered, ("case",), ("case",))

    def test_negative_nested_report_cannot_be_hidden_by_updated_hash(self):
        report = self.write("negative.json", dict(passed=False))
        summary = dict(passed=True, checks=[dict(name="case", passed=True, exitCode=0,
                       report=report.name, reportSha256=verify.sha256(report))])
        with self.assertRaisesRegex(ValueError, "failed"):
            gate.stages(self.root / "summary.json", summary, ("case",), ("case",))

    def test_candidate_and_build_binding_are_both_required(self):
        inventory = dict(inventorySha256="a" * 64)
        gate.same_candidate(dict(candidate=inventory, candidateUnchanged=True), inventory)
        for data in (dict(candidate=inventory), dict(candidate={"inventorySha256": "b" * 64}, candidateUnchanged=True)):
            with self.assertRaises(ValueError):
                gate.same_candidate(data, inventory)
        with self.assertRaisesRegex(ValueError, "fresh build"):
            gate.same_build(dict(buildEvidenceSha256="a" * 64), "b" * 64)

    def review_fixture(self):
        artifact = self.root / "isolated-observation.txt"
        artifact.write_text("Synthetic unit-test observation. Not real acceptance.")
        source = dict(commit="1" * 40)
        inventory = dict(inventorySha256="2" * 64)
        review = dict(schema=gate.REVIEW_SCHEMA, passed=True, reviewer="UNIT TEST ONLY",
                      reviewedAt=datetime.now(timezone.utc).isoformat(), sourceCommit=source["commit"],
                      candidateInventorySha256=inventory["inventorySha256"], applicationVersion="test-version",
                      checks=[dict(id=name, passed=True, observations="Synthetic fixture, not a real review.",
                                   evidence=[self.ref(artifact)]) for name in gate.REVIEW_CHECKS])
        return review, source, inventory

    def test_attributable_review_requires_all_rows_and_evidence(self):
        review, source, inventory = self.review_fixture()
        path = self.root / "review.json"
        gate.review_record(path, review, source, inventory, "test-version")
        mutations = [lambda x: x["checks"].pop(), lambda x: x.update(reviewer=""),
                     lambda x: x.update(reviewedAt="2026-01-01T00:00:00"), lambda x: x.update(reviewedAt="2999-01-01T00:00:00+00:00"),
                     lambda x: x.update(candidateInventorySha256="3" * 64), lambda x: x.update(applicationVersion="other"),
                     lambda x: x["checks"][0].update(passed=False), lambda x: x["checks"][0].update(observations=""),
                     lambda x: x["checks"][0].update(evidence=[])]
        for change in mutations:
            altered = copy.deepcopy(review)
            change(altered)
            with self.subTest(change=change), self.assertRaises(ValueError):
                gate.review_record(path, altered, source, inventory, "test-version")

    def test_failed_internet_cycle_cannot_be_substituted_with_local_pass(self):
        review, source, inventory = self.review_fixture()
        row = next(row for row in review["checks"] if row["id"] == "quick-match")
        row.update(passed=False, observations="Local TCP passed; two networks unavailable.")
        with self.assertRaisesRegex(ValueError, "failed"):
            gate.review_record(self.root / "review.json", review, source, inventory, "test-version")

    def test_review_supporting_artifacts_must_still_exist_and_match(self):
        review, source, inventory = self.review_fixture()
        (self.root / "isolated-observation.txt").write_text("changed after approval")
        with self.assertRaisesRegex(ValueError, "changed"):
            gate.review_record(self.root / "review.json", review, source, inventory, "test-version")

    def player_fixture(self):
        packager = verify.load_tool("acceptance_package_test", "Build-ReleasePackage.py")
        required = verify.load_tool("acceptance_patch_test", "Build-UpdatePatch.py").REQUIRED
        player = self.root / "player"
        for name in required:
            path = player / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"isolated synthetic player fixture")
        return packager, player

    def test_unapproved_packaging_creates_no_archive(self):
        packager, player = self.player_fixture()
        target = self.root / "Release.zip"
        with self.assertRaisesRegex(ValueError, "acceptance"):
            packager.build(player, target)
        self.assertFalse(target.exists())

    def test_candidate_packages_are_explicitly_unapproved(self):
        packager, player = self.player_fixture()
        with self.assertRaisesRegex(ValueError, "Candidate-"):
            packager.build(player, self.root / "Release.zip", candidate_only=True)
        result = packager.build(player, self.root / "Candidate-test.zip", candidate_only=True)
        self.assertFalse(result["releaseReady"])
        self.assertTrue(result["candidateOnly"])
        self.assertIsNone(result["acceptance"])

    def test_stale_acceptance_cannot_be_hidden_by_repackaging(self):
        packager, player = self.player_fixture()
        accepted = verify.candidate_inventory(player)["inventorySha256"]
        def changed_after_check(*args):
            (player / "InitialDUnity.exe").write_bytes(b"a different executable")
            return dict(candidateInventorySha256=accepted, releaseReady=True)
        target = self.root / "Release.zip"
        with patch.object(gate, "check", side_effect=changed_after_check):
            with self.assertRaisesRegex(ValueError, "Packaged bytes differ"):
                packager.build(player, target, acceptance=self.root / "review.json")
        self.assertFalse(target.with_suffix('.report.json').exists())

    def test_invalid_gate_prevents_packaging_before_reading_files(self):
        packager, player = self.player_fixture()
        with patch.object(gate, "check", side_effect=ValueError("missing EOS acceptance")), \
             patch.object(packager, "collect_player_files", side_effect=AssertionError("must not package")):
            with self.assertRaisesRegex(ValueError, "missing EOS"):
                packager.build(player, self.root / "Release.zip", acceptance=self.root / "review.json")
        self.assertFalse((self.root / "Release.zip").exists())

    def complete_fixture(self):
        # Synthetic files exercise the entire acceptance chain. They are never
        # treated as actual test results or copied into repository evidence.
        _, player = self.player_fixture()
        for name in ("Idas3Unity.dll", "Idas3WheelFeedback.dll"):
            path = player / "InitialDUnity_Data/Plugins/x86_64" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic native plugin")
        inventory = verify.candidate_inventory(player)
        source = dict(commit="1" * 40, files={"Tools/synthetic.py": "2" * 64}, workingTreeChanges=[])
        source["codeAndConfigSha256"] = verify.digest_json(source["files"])
        def stage(folder, name, data=None, filename=None):
            path = self.write(folder + "/" + (filename or name + ".json"), data or dict(passed=True))
            return dict(name=name, passed=True, exitCode=0, report=path.relative_to(self.root / folder).as_posix(), reportSha256=verify.sha256(path))
        unity = stage("build", "unity-build", dict(passed=True, applicationVersion="test-version",
                      cleanBuildCache=True, scriptsOnly=False), "unity-build.json")
        self.write("build/candidate.json", inventory)
        self.write("build/source.json", source)
        self.write("build/inputs.json", dict(files={"content": "synthetic input"}))
        xml = self.root / "build/native-results.xml"
        xml.write_text('<testsuites><testsuite>' + ''.join('<testcase name="' + name + '"/>' for name in gate.build.NATIVE_TESTS) + '</testsuite></testsuites>')
        proof = dict(passed=True, suite="fresh-build", source=source, sourceUnchanged=True, inputsUnchanged=True,
                     candidate=inventory, inputInventorySha256=verify.digest_json({"content": "synthetic input"}),
                     nativeTests=gate.build.require_native_results(xml),
                     nativeArtifacts={name: inventory["files"]["InitialDUnity_Data/Plugins/x86_64/" + name]["sha256"] for name in ("Idas3Unity.dll", "Idas3WheelFeedback.dll")},
                     checks=[dict(name=name, passed=True, exitCode=0) for name in ("native-configure", "native-build", "native-tests")] + [unity])
        paths = {"build": self.write("build/summary.json", proof)}
        build_hash = verify.sha256(paths["build"])
        common = dict(passed=True, buildEvidenceSha256=build_hash, candidate=inventory, candidateUnchanged=True,
                      testDistributableFilesUnchanged=True, applicationVersion="test-version")
        paths["candidate"] = self.write("candidate/summary.json", dict(**common, suite="candidate", source=source,
            sourceUnchanged=True, checks=[stage("candidate", name) for name in gate.source_checks()]))
        outer = dict(passed=True, shutdownComplete=True, applicationVersion="test-version", checks=10)
        local_rows = []
        for name in ("rom-missing", "mode-flow", "updater-session"):
            data = dict(outer)
            if name == "rom-missing":
                data.update(expectation="blocked", verified=False, nativeInitialized=False)
            elif name == "mode-flow":
                data.update(schema="idas3-mode-flow-smoke-v1")
            else:
                data.update(schema="idas3-attract-options-smoke-v1", observations=list(gate.packaged.UPDATER_OBSERVATIONS))
            local_rows.append(stage("packaged", name, data, name + "/report.json"))
        (self.root / "packaged/rom-missing/startup.png").write_bytes(b"synthetic fixture")
        for case in gate.packaged.PAIR_FLAGS:
            for role in ("host", "join"):
                name = case + "-" + role
                data = dict(outer, schema="idas3-two-client-smoke-v1", role=role, onlineRuntimeCheck=False, p2pRaceCheck=False, authorityEnabled=True)
                row = stage("packaged", name, data, name + "/report.json")
                fields = ("neutralResult", "unhideable", "saveUnchanged", "acknowledged") if case == "disconnect" else ("naturalFinish", "connectedLobby", "savesUnchanged", "secondCountdown", "secondDrive", "musicSelectionPreserved")
                detail_name = "disconnect-report.json" if case == "disconnect" else "return-lobby-report.json"
                detail = self.write("packaged/" + name + "/" + detail_name, dict(passed=True, role=role, **{key: True for key in fields}))
                row.update(detailReport=detail.relative_to(self.root / "packaged").as_posix(), detailSha256=verify.sha256(detail))
                local_rows.append(row)
        paths["packaged"] = self.write("packaged/summary.json", dict(**common, suite="packaged-all", selectedCases=list(gate.packaged.selected_cases("all")), checks=local_rows))
        earned_rows = []
        for role in ("host", "join"):
            row = stage("scored", role, dict(outer, schema="idas3-two-client-smoke-v1", role=role, onlineRuntimeCheck=False, p2pRaceCheck=False, authorityEnabled=True), role + "/report.json")
            races, balance = [], 321
            for cycle in range(2):
                won = (cycle == 0) == (role == "host")
                reward = 2000 if won else 1000
                balance += reward
                races.append(dict(raceId=cycle+1, winner=cycle, naturalFinish=True, duplicatePreserved=True,
                    tuningPoints=reward, balance=balance, displayedBalance=balance, pointsPhase=2,
                    result="YOU WIN" if won else "YOU LOSE", verifiedFrame=12001, seconds=200,
                    after=dict(battles=cycle+1, wins=int(won) if cycle==0 else 1), continued=cycle==0, hostTicks=100, guestTicks=200))
            detail = self.write("scored/" + role + "/scored-report.json", dict(passed=True, schema="idas3-scored-race-cycle-v1", role=role,
                applicationVersion="test-version", checks=10, inputSamples=24000, connectedRematch=True, persistedAfterLeaving=True, unrelatedProfilePreserved=True, races=races))
            images = {}
            for language in range(3):
                for width, height in ((1024,768), (1280,720), (1920,1080), (2560,1080)):
                    name = f"result-recovery-{language}-{width}x{height}.png"
                    path = detail.parent / name
                    path.write_bytes(b"\x89PNG\r\n\x1a\n" + struct.pack(">I",13) + b"IHDR" + struct.pack(">II",width,height))
                    images[name] = verify.sha256(path)
            cases = [dict(cycle=cycle, input="controller" if (cycle==0)==(role=="join") else "keyboard", clicks=2,
                          captures=list(images) if cycle==0 else [], **{key:True for key in ("passed","durableIntent","historyBlocked","focusGuard","heldGuard","retried")}) for cycle in range(2)]
            recovery = self.write("scored/" + role + "/result-recovery-report.json", dict(passed=True, schema="idas3-scored-result-recovery-v1", role=role, applicationVersion="test-version", cases=cases))
            row.update(detailReport=detail.relative_to(self.root / "scored").as_posix(), detailSha256=verify.sha256(detail), recoveryReportSha256=verify.sha256(recovery), recoveryCaptures=images)
            earned_rows.append(row)
        paths["scored"] = self.write("scored/summary.json", dict(**common, suite="packaged-scored", resultRecoveryCheck=True, peerResultsAgree=True, checks=earned_rows))
        startup_rows = []
        boundaries = ("after-intent", "after-tuning", "after-record", "completion-intent", "after-complete")
        for boundary in boundaries:
            for suffix in ("", "-repeat"):
                name = boundary + suffix
                log = self.write("startup/" + name + "-player.log", dict(synthetic=True))
                startup_rows.append(dict(name=name, passed=True, savedState=dict(points=2321), playerLogSha256=verify.sha256(log), recoveredBeforeNativeReady=(not suffix and boundary in boundaries[:3])))
        paths["startupRecovery"] = self.write("startup/summary.json", dict(**common, suite="packaged-result-startup", privateDistributableUnchanged=True, checks=startup_rows))
        archives = [self.root / "core.zip", self.root / "courses.zip"]
        for index, path in enumerate(archives):
            with zipfile.ZipFile(path, "x") as out:
                for number, name in enumerate(inventory["files"]):
                    if number % 2 == index:
                        out.write(player / name, name)
        paths["freshInstall"] = self.write("install/report.json", dict(passed=True, installerSha256=verify.sha256(ROOT / "Tools/Install-InitialD.ps1"),
             tests=[dict(name="full-candidate", passed=True, exitCode=0)], fullCandidate=dict(passed=True, buildEvidenceSha256=build_hash,
             inventorySha256=inventory["inventorySha256"], files=inventory["fileCount"], bytes=inventory["bytes"], version="test-version", archives=[self.ref(path) for path in archives])))
        paths["upgrade"] = self.write("upgrade/report.json", dict(passed=True, buildEvidenceSha256=build_hash,
             candidateInventorySha256=inventory["inventorySha256"], targetVersion="test-version", candidateUnchanged=True,
             privateFilesPreserved=True, payloadsCleaned=True, startupPassed=True, installedFiles=inventory["fileCount"], targetArchives=[self.ref(path) for path in archives]))
        review, _, _ = self.review_fixture()
        review.update(candidateInventorySha256=inventory["inventorySha256"])
        review_path = self.write("review.json", review)
        manifest = self.write("acceptance.json", dict(schema=gate.SCHEMA, sourceCommit=source["commit"], applicationVersion="test-version",
             candidateInventorySha256=inventory["inventorySha256"], reports={key:self.ref(value) for key,value in paths.items()}, review=self.ref(review_path)))
        return manifest, player, source, paths

    def test_complete_synthetic_chain_and_cross_layer_rejections(self):
        manifest, player, source, paths = self.complete_fixture()
        with patch.object(verify, "source_identity", return_value=source), patch.object(verify, "git", return_value=b"Tools/synthetic.py\0"):
            self.assertTrue(gate.check(manifest, player)["releaseReady"])
            for target, mutate in (("packaged", lambda d: d.update(suite="packaged-updater")),
                                   ("scored", lambda d: d.update(resultRecoveryCheck=False)),
                                   ("startupRecovery", lambda d: d["checks"].pop()),
                                   ("freshInstall", lambda d: d.update(fullCandidate={})),
                                   ("upgrade", lambda d: d.update(privateFilesPreserved=False))):
                with self.subTest(target=target):
                    original = paths[target].read_bytes()
                    original_manifest = manifest.read_bytes()
                    data = gate.load(paths[target]); mutate(data)
                    paths[target].write_text(json.dumps(data))
                    spec = gate.load(manifest); spec["reports"][target] = self.ref(paths[target]); manifest.write_text(json.dumps(spec))
                    with self.assertRaises(ValueError):
                        gate.check(manifest, player)
                    paths[target].write_bytes(original); manifest.write_bytes(original_manifest)

    def test_complete_chain_rejects_uncommitted_source_and_changed_candidate(self):
        manifest, player, source, _ = self.complete_fixture()
        with patch.object(verify, "source_identity", return_value=source), patch.object(verify, "git", return_value=b"Tools/synthetic.py\0"):
            source["workingTreeChanges"] = [" M Assets/Scripts/Game.cs"]
            with self.assertRaisesRegex(ValueError, "Commit tracked"):
                gate.check(manifest, player)
            source["workingTreeChanges"] = []
            (player / "InitialDUnity.exe").write_bytes(b"changed executable")
            with self.assertRaisesRegex(ValueError, "different player bytes"):
                gate.check(manifest, player)

    def test_archive_names_rejected_before_payload_hashing(self):
        manifest, player, source, paths = self.complete_fixture()
        archive = self.root / "core.zip"
        with zipfile.ZipFile(archive, "a") as out:
            out.writestr("rom/README.txt", "must not enter public archives")
        real_sha = verify.sha256
        def guarded_sha(path):
            self.assertNotEqual(Path(path), archive, "Unexpected archive payload was hashed")
            return real_sha(path)
        with patch.object(verify, "source_identity", return_value=source), \
             patch.object(verify, "git", return_value=b"Tools/synthetic.py\0"), patch.object(verify, "sha256", side_effect=guarded_sha):
            with self.assertRaisesRegex(ValueError, "unexpected entries"):
                gate.check(manifest, player)


if __name__ == "__main__":
    unittest.main()
