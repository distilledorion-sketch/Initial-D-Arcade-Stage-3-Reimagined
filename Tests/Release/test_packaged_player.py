"""Exercise failure, isolation and evidence boundaries without launching a game."""
import json
import io
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Tools"))
import verify_packaged_player as player
import verify_scored_player as scored


class PackagedPlayerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.folder = self.root / "case"
        self.folder.mkdir()
        self.report = dict(passed=True, shutdownComplete=True, applicationVersion="test-version", checks=12,
                           schema="idas3-two-client-smoke-v1", role="host", onlineRuntimeCheck=False,
                           p2pRaceCheck=False, authorityEnabled=True)
        self.detail = dict(passed=True, role="host", naturalFinish=True, connectedLobby=True,
                           savesUnchanged=True, secondCountdown=True, secondDrive=True, musicSelectionPreserved=True)

    def write_reports(self):
        player.verify.write_json(self.folder / "report.json", self.report)
        player.verify.write_json(self.folder / "return-lobby-report.json", self.detail)

    def validate(self, exit_code=0):
        return player.require_runtime_report(self.folder, "test-version", exit_code, "return-rematch", "host")

    def test_rejects_negative_missing_and_empty_reports(self):
        for change in ({"passed": False}, {"passed": None}, {"checks": 0}, {"checks": True}, {"checks": None}):
            with self.subTest(change=change):
                previous = self.report.copy()
                self.report.update(change)
                self.write_reports()
                with self.assertRaises(ValueError):
                    self.validate()
                self.report = previous
        (self.folder / "report.json").unlink()
        with self.assertRaises(FileNotFoundError):
            self.validate()

    def test_rejects_wrong_binary_or_failed_shutdown(self):
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "cleanly"):
            self.validate(1)
        self.report["shutdownComplete"] = False
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "cleanly"):
            self.validate()
        self.report.update(shutdownComplete=True, applicationVersion="old-version")
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "version"):
            self.validate()

    def test_rejects_wrong_role_schema_transport_or_authority(self):
        for change in ({"role": "join"}, {"schema": "wrong"}, {"onlineRuntimeCheck": True},
                       {"p2pRaceCheck": True}, {"onlineRuntimeCheck": None}, {"authorityEnabled": False}):
            with self.subTest(change=change):
                previous = self.report.copy()
                self.report.update(change)
                self.write_reports()
                with self.assertRaises(ValueError):
                    self.validate()
                self.report = previous

    def test_requires_complete_detail_report(self):
        self.write_reports()
        self.assertEqual(self.validate()[1], "return-lobby-report.json")
        for field in ("passed", "connectedLobby", "savesUnchanged", "secondCountdown", "secondDrive"):
            with self.subTest(field=field):
                self.detail[field] = False
                self.write_reports()
                with self.assertRaises(ValueError):
                    self.validate()
                self.detail[field] = True

    def test_startup_state_alone_cannot_pass_visual_check(self):
        self.report.update(expectation="blocked", verified=False, nativeInitialized=False)
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "capture"):
            player.require_runtime_report(self.folder, "test-version", 0, "startup")
        (self.folder / "startup.png").write_bytes(b"fixture")
        player.require_runtime_report(self.folder, "test-version", 0, "startup")
        self.report["verified"] = True
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "blocked"):
            player.require_runtime_report(self.folder, "test-version", 0, "startup")

    def test_pair_is_loopback_and_has_distinct_isolated_profiles(self):
        host, join = self.root / "host", self.root / "join"
        for case in player.PAIR_FLAGS:
            h = player.pair_arguments(case, "host", host, join, 28475)
            j = player.pair_arguments(case, "join", join, host, 28475)
            self.assertEqual(j[j.index("-idas3-multiplayer-address") + 1], "127.0.0.1:28475")
            self.assertEqual(h[h.index("-idas3-multiplayer-port") + 1], "28475")
            self.assertEqual(h[1], str(host))
            self.assertEqual(j[1], str(join))
            self.assertNotIn("-idas3-multiplayer-address", h)
            self.assertIn("-idas3-multiplayer-world-only", j)

    def test_updater_is_in_default_flows_and_can_run_without_race_cases(self):
        for suite in ("all", "flows", "updater"):
            self.assertIn("updater-session", player.selected_cases(suite))
        self.assertEqual(player.selected_cases("updater"), ("updater-session",))
        self.assertEqual(player.selected_cases("startup"), ("rom-missing",))
        self.assertNotIn("rom-missing", player.selected_cases("flows"))
        with self.assertRaises(ValueError):
            player.selected_cases("unknown")

    def test_updater_requires_connection_guard_and_restored_install_evidence(self):
        self.report.update(schema="idas3-attract-options-smoke-v1", observations=list(player.UPDATER_OBSERVATIONS))
        self.write_reports()
        validate = lambda: player.require_runtime_report(self.folder, "test-version", 0, "updater-session")
        self.assertIsNone(validate()[1])
        for missing in player.UPDATER_OBSERVATIONS:
            with self.subTest(missing=missing):
                self.report["observations"] = [item for item in player.UPDATER_OBSERVATIONS if item != missing]
                self.write_reports()
                with self.assertRaisesRegex(ValueError, "Incomplete"):
                    validate()
        invalid = [None, "generic pass", [True],
                   [item.replace("open=True", "open=False") for item in player.UPDATER_OBSERVATIONS],
                   [item.replace("in room=0", "in room=3") for item in player.UPDATER_OBSERVATIONS],
                   [item.replace("installs=2", "installs=1") for item in player.UPDATER_OBSERVATIONS],
                   [*player.UPDATER_OBSERVATIONS, player.UPDATER_OBSERVATIONS[1].replace("open=True", "open=False")]]
        for observations in invalid:
            with self.subTest(observations=observations):
                self.report["observations"] = observations
                self.write_reports()
                with self.assertRaises(ValueError):
                    validate()
        self.report.update(schema="idas3-mode-flow-smoke-v1", observations=list(player.UPDATER_OBSERVATIONS))
        self.write_reports()
        with self.assertRaisesRegex(ValueError, "Unexpected updater-session"):
            validate()

    def test_copy_reads_only_inventoried_distributable_files(self):
        source = self.root / "source"
        source.mkdir()
        executable = source / "InitialDUnity.exe"
        executable.write_bytes(b"executable fixture")
        (source / "Custom Music").mkdir()
        (source / "Custom Music/private.mp3").write_bytes(b"private fixture")
        inventory = {"files": {"InitialDUnity.exe": {"bytes": executable.stat().st_size, "sha256": player.verify.sha256(executable)}}}
        destination = self.root / "private-player"
        player.copy_player(source, destination, inventory)
        self.assertEqual({p.name for p in destination.iterdir()}, {"InitialDUnity.exe", "PRIVATE_TEST_ONLY.txt"})
        self.assertEqual((destination / "InitialDUnity.exe").read_bytes(), b"executable fixture")
        with self.assertRaises(FileExistsError):
            player.copy_player(source, destination, inventory)
        inventory["files"]["InitialDUnity.exe"]["sha256"] = "changed"
        with self.assertRaisesRegex(ValueError, "differs"):
            player.copy_player(source, self.root / "changed-copy", inventory)

    def updater_capture_fixture(self):
        captures, dimensions = [], []
        for language in range(3):
            for width, height in player.UPDATER_SIZES:
                name = f"updater-{language}-{width}x{height}.png"
                captures.append(name)
                dimensions.append(dict(file=name, requestedWidth=width, requestedHeight=height,
                                       actualWidth=width, actualHeight=height, requestedSizeSupported=True))
                # Header-only fixture tests metadata rejection, not image decoding.
                (self.folder / name).write_bytes(b"\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR" + player.struct.pack(">II", width, height))
        return dict(captures=captures, captureDimensions=dimensions)

    def test_updater_captures_require_every_language_and_exact_viewport(self):
        report = self.updater_capture_fixture()
        hashes = player.require_updater_captures(self.folder, report)
        self.assertEqual(len(hashes), 12)
        for change in (lambda r: r["captures"].pop(), lambda r: r["captureDimensions"].pop(),
                       lambda r: r["captureDimensions"][0].update(actualWidth=600),
                       lambda r: r["captureDimensions"][0].update(requestedSizeSupported=False)):
            modified = copy.deepcopy(report)
            change(modified)
            with self.assertRaises(ValueError):
                player.require_updater_captures(self.folder, modified)

    def test_updater_captures_reject_missing_truncated_and_wrong_size_files(self):
        for content in (None, b"broken", b"\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR" + player.struct.pack(">II", 600, 480)):
            with self.subTest(content=content):
                report = self.updater_capture_fixture()
                path = self.folder / report["captures"][0]
                if content is None:
                    path.unlink()
                else:
                    path.write_bytes(content)
                with self.assertRaises((FileNotFoundError, ValueError)):
                    player.require_updater_captures(self.folder, report)

    def test_independent_success_does_not_erase_prior_failure(self):
        run = player.verify.Run(self.root / "output", "packaged-all")
        players = player.Players(run, self.root / "unused", "test-version")
        with patch.object(players, "close") as close:
            players.case("startup", lambda: (_ for _ in ()).throw(TimeoutError("capture timed out")))
            players.case("flows", lambda: run.summary["checks"].append({"name": "flows", "passed": True}))
            run.finish()
        self.assertEqual(close.call_count, 2)
        self.assertFalse(run.summary["passed"])
        self.assertFalse(run.summary["releaseReady"])
        self.assertTrue(run.summary["checks"][1]["passed"])
        self.assertIn("capture timed out", run.summary["checks"][0]["error"])

    def test_cleanup_waits_for_taskkill_and_is_idempotent(self):
        run = player.verify.Run(self.root / "cleanup", "packaged-all")
        players = player.Players(run, self.root / "unused", "test-version")
        child = Mock(pid=123, **{"poll.return_value": None, "wait.return_value": 0})
        log = io.BytesIO()
        players.owned.append((child, log))
        with patch.object(player.subprocess, "run") as taskkill:
            players.close()
            players.close()
        taskkill.assert_called_once()
        child.wait.assert_called_once_with(timeout=1)
        child.kill.assert_not_called()
        self.assertTrue(log.closed)
        self.assertEqual(players.owned, [])

    def test_cleanup_attempts_every_child_and_retains_only_unfinished_ownership(self):
        run = player.verify.Run(self.root / "cleanup", "packaged-all")
        players = player.Players(run, self.root / "unused", "test-version")
        unfinished = Mock(pid=123, **{"poll.return_value": None})
        unfinished.wait.side_effect = player.subprocess.TimeoutExpired("fixture", 1)
        unfinished.kill.side_effect = PermissionError("still running")
        finished = Mock(pid=124, **{"poll.return_value": 0})
        pending_log, finished_log = io.BytesIO(), io.BytesIO()
        players.owned.extend([(unfinished, pending_log), (finished, finished_log)])
        with patch.object(player.subprocess, "run") as taskkill:
            with self.assertRaisesRegex(RuntimeError, "1 owned"):
                players.close()
            self.assertEqual(players.owned, [(unfinished, pending_log)])
            self.assertFalse(pending_log.closed)
            self.assertTrue(finished_log.closed)
            finished.poll.assert_called_once()
            unfinished.wait.side_effect = None
            unfinished.wait.return_value = 0
            players.close()
            players.close()
        self.assertEqual(taskkill.call_count, 2)
        self.assertTrue(pending_log.closed)
        self.assertEqual(players.owned, [])

    def prepare_evidence(self):
        inventory = {"files": {"InitialDUnity.exe": {"bytes": 1, "sha256": "fixture"}}}
        inventory["inventorySha256"] = player.verify.digest_json(inventory["files"])
        player.verify.write_json(self.folder / "candidate.json", inventory)
        player.verify.write_json(self.folder / "unity-build.json", {"passed": True, "applicationVersion": "test-version",
                                                                    "cleanBuildCache": True, "scriptsOnly": False})
        proof = {"passed": True, "suite": "fresh-build", "sourceUnchanged": True, "inputsUnchanged": True,
                 "candidate": {"inventorySha256": inventory["inventorySha256"]},
                 "checks": [{"name": "unity-build", "passed": True, "reportSha256": player.verify.sha256(self.folder / "unity-build.json")}]}
        player.verify.write_json(self.folder / "summary.json", proof)
        return inventory, proof

    def test_rejects_other_candidate_or_modified_evidence(self):
        inventory, proof = self.prepare_evidence()
        self.assertEqual(player.require_build_evidence(self.folder, inventory)[1], "test-version")
        with self.assertRaisesRegex(ValueError, "differs"):
            player.require_build_evidence(self.folder, {"inventorySha256": "another-player"})
        (self.folder / "unity-build.json").write_text(json.dumps({"passed": True, "applicationVersion": "different"}))
        with self.assertRaisesRegex(ValueError, "does not match"):
            player.require_build_evidence(self.folder, inventory)

    def test_rejects_failed_or_incomplete_fresh_build(self):
        for field in ("passed", "sourceUnchanged", "inputsUnchanged"):
            with self.subTest(field=field):
                inventory, proof = self.prepare_evidence()
                proof[field] = False
                player.verify.write_json(self.folder / "summary.json", proof)
                with self.assertRaises(ValueError):
                    player.require_build_evidence(self.folder, inventory)
        inventory, proof = self.prepare_evidence()
        proof["checks"] = []
        player.verify.write_json(self.folder / "summary.json", proof)
        with self.assertRaises(ValueError):
            player.require_build_evidence(self.folder, inventory)

    def test_rejects_cached_player_even_with_matching_report_hash(self):
        inventory, proof = self.prepare_evidence()
        player.verify.write_json(self.folder / "unity-build.json", {"passed": True, "applicationVersion": "test-version"})
        proof["checks"][0]["reportSha256"] = player.verify.sha256(self.folder / "unity-build.json")
        player.verify.write_json(self.folder / "summary.json", proof)
        with self.assertRaisesRegex(ValueError, "clean"):
            player.require_build_evidence(self.folder, inventory)


class ScoredPlayerEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.report = dict(passed=True, shutdownComplete=True, applicationVersion="test-version", checks=12,
                           schema="idas3-two-client-smoke-v1", role="host", onlineRuntimeCheck=False,
                           p2pRaceCheck=False, authorityEnabled=True)
        self.detail = dict(passed=True, schema="idas3-scored-race-cycle-v1", applicationVersion="test-version",
                           role="host", checks=100, inputSamples=12000, connectedRematch=True,
                           persistedAfterLeaving=True, unrelatedProfilePreserved=True, races=[])
        for winner in range(2):
            self.detail["races"].append(dict(winner=winner, raceId=winner + 100, naturalFinish=True,
                duplicatePreserved=True, tuningPoints=2000 if winner == 0 else 1000, balance=2321 if winner == 0 else 3321,
                displayedBalance=2321 if winner == 0 else 3321, pointsPhase=2,
                result="YOU WIN" if winner == 0 else "YOU LOSE", verifiedFrame=12000, seconds=204,
                after=dict(battles=winner + 1, wins=1), continued=winner == 0))

    def validate(self):
        player.verify.write_json(self.folder / "report.json", self.report)
        player.verify.write_json(self.folder / "scored-report.json", self.detail)
        return scored.require_scored_report(self.folder, "test-version", 0, "host")

    def test_accepts_both_winners_and_persisted_awards(self):
        self.assertEqual(len(self.validate()[1]["races"]), 2)
        self.report["role"] = self.detail["role"] = "join"
        for winner, race in enumerate(self.detail["races"]):
            race.update(tuningPoints=1000 if winner == 0 else 2000, balance=1321 if winner == 0 else 3321,
                        displayedBalance=1321 if winner == 0 else 3321,
                        result="YOU LOSE" if winner == 0 else "YOU WIN", after=dict(battles=winner + 1, wins=winner))
        player.verify.write_json(self.folder / "report.json", self.report)
        player.verify.write_json(self.folder / "scored-report.json", self.detail)
        scored.require_scored_report(self.folder, "test-version", 0, "join")

    def test_rejects_neutral_duplicate_forced_or_wrong_rewards(self):
        before = copy.deepcopy(self.detail)
        for change in ({"winner": -1}, {"naturalFinish": False}, {"duplicatePreserved": False},
                       {"tuningPoints": 0}, {"balance": 4321}, {"seconds": 2}, {"verifiedFrame": 0},
                       {"after": {"battles": 2, "wins": 2}}, {"continued": False}, {"raceId": 0}):
            with self.subTest(change=change):
                self.detail = copy.deepcopy(before)
                self.detail["races"][0].update(change)
                with self.assertRaises(ValueError):
                    self.validate()
        self.detail = before
        self.detail["races"][1]["raceId"] = self.detail["races"][0]["raceId"]
        with self.assertRaises(ValueError):
            self.validate()

    def test_requires_reload_preservation_and_actual_input(self):
        before = copy.deepcopy(self.detail)
        for field, value in (("persistedAfterLeaving", False), ("unrelatedProfilePreserved", False),
                             ("connectedRematch", False), ("passed", False), ("inputSamples", 0), ("checks", True)):
            self.detail = copy.deepcopy(before)
            self.detail[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.validate()


if __name__ == "__main__":
    unittest.main()
