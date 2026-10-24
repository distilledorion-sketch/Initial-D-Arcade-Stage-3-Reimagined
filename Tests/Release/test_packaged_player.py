"""Exercise failure, isolation and evidence boundaries without launching a game."""
import json
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

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

    def prepare_evidence(self):
        inventory = {"files": {"InitialDUnity.exe": {"bytes": 1, "sha256": "fixture"}}}
        inventory["inventorySha256"] = player.verify.digest_json(inventory["files"])
        player.verify.write_json(self.folder / "candidate.json", inventory)
        player.verify.write_json(self.folder / "unity-build.json", {"passed": True, "applicationVersion": "test-version"})
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
