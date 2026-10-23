"""Release evidence must fail closed and must not inspect personal content."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("verify_release", ROOT / "Tools/verify_release.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def player(self):
        player = self.root / "candidate"
        required = verify.load_tool("patch_fixture", "Build-UpdatePatch.py").REQUIRED
        for name in required:
            target = player / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"isolated fixture")
        return player

    def test_accepted_node_range(self):
        for version in ("v24.22.1", "24.22.2", "v24.23.0"):
            with self.subTest(version=version):
                self.assertTrue(verify.supported_node(version, "^24.22.1"))

    def test_reject_unpatched_or_unvalidated_node(self):
        for version in ("v24.22.0", "v24.21.9", "v22.99.0", "v25.0.0", "v24.22.1-rc.1", "unexpected"):
            with self.subTest(version=version):
                self.assertFalse(verify.supported_node(version, "^24.22.1"))

    def test_unknown_engine_policy_requires_review(self):
        self.assertFalse(verify.supported_node("v24.22.1", ">=24"))

    def test_missing_and_false_reports_rejected(self):
        target = self.root / "report.json"
        with self.assertRaises(FileNotFoundError):
            verify.require_passed_report(target)
        for value in ({}, {"passed": False}, {"passed": "true"}, {"passed": 1}):
            target.write_text(json.dumps(value))
            with self.subTest(value=value), self.assertRaises(ValueError):
                verify.require_passed_report(target)

    def test_positive_report_supported(self):
        target = self.root / "report.json"
        target.write_text(json.dumps({"passed": True, "count": 3}), encoding="utf-8-sig")
        self.assertEqual(verify.require_passed_report(target)["count"], 3)

    def test_nonzero_process_preserves_failure_and_output(self):
        run = verify.Run(self.root / "evidence", "source")
        with self.assertRaises(RuntimeError) as failure:
            run.command("broken", [sys.executable, "-c", "print('specific failure'); raise SystemExit(7)"])
        run.finish(failure.exception)
        data = json.loads((run.output / "summary.json").read_text())
        self.assertEqual(data["status"], "failed")
        self.assertFalse(data["passed"])
        self.assertFalse(data["releaseReady"])
        self.assertEqual(data["checks"][0]["exitCode"], 7)
        self.assertIn("specific failure", (run.output / "broken.log").read_text())

    def test_zero_exit_does_not_override_failed_report(self):
        run = verify.Run(self.root / "evidence", "source")
        report = run.output / "report.json"
        report.write_text('{"passed":false}')
        with self.assertRaises(ValueError) as failure:
            run.command("bad-report", [sys.executable, "-c", "pass"], report=report)
        run.finish(failure.exception)
        self.assertFalse(run.summary["passed"])

    def test_timeout_cannot_pass(self):
        run = verify.Run(self.root / "evidence", "source")
        with self.assertRaises(verify.subprocess.TimeoutExpired) as failure:
            run.command("timeout", [sys.executable, "-c", "import time; time.sleep(20)"], timeout=0.1)
        run.finish(failure.exception)
        self.assertFalse(run.summary["passed"])

    def test_source_success_keeps_missing_release_gates(self):
        run = verify.Run(self.root / "evidence", "source")
        run.command("ok", [sys.executable, "-c", "pass"])
        run.finish()
        self.assertTrue(run.summary["passed"])
        self.assertEqual(run.summary["status"], "source_checks_passed")
        self.assertFalse(run.summary["releaseReady"])
        self.assertIn("Two-PC", " ".join(run.summary["pendingGates"]))

    def test_empty_run_cannot_pass(self):
        run = verify.Run(self.root / "evidence", "source")
        run.finish()
        self.assertFalse(run.summary["passed"])

    def test_existing_evidence_never_overwritten(self):
        output = self.root / "evidence"
        output.mkdir()
        sentinel = output / "summary.json"
        sentinel.write_text("preserve")
        with self.assertRaises(FileExistsError):
            verify.Run(output, "source")
        self.assertEqual(sentinel.read_text(), "preserve")

    def test_inventory_detects_binary_change(self):
        player = self.player()
        before = verify.candidate_inventory(player)
        (player / "InitialDUnity.exe").write_bytes(b"different build")
        after = verify.candidate_inventory(player)
        self.assertNotEqual(before["inventorySha256"], after["inventorySha256"])

    def test_inventory_rejects_incomplete_player(self):
        player = self.player()
        (player / "InitialDUnity.exe").unlink()
        with self.assertRaises(ValueError):
            verify.candidate_inventory(player)

    def test_rom_rejected_before_any_hashing(self):
        player = self.player()
        rom = player / "rom/gds-0033.chd"
        rom.parent.mkdir()
        rom.write_bytes(b"private fixture")
        with patch.object(verify, "sha256", side_effect=AssertionError("must not read")) as hashed:
            with self.assertRaisesRegex(AssertionError, "Original ROM"):
                verify.candidate_inventory(player)
            hashed.assert_not_called()

    def test_music_is_not_hashed(self):
        player = self.player()
        before = verify.candidate_inventory(player)
        music = player / "Custom Music/my song.mp3"
        music.parent.mkdir()
        music.write_bytes(b"private fixture")
        real_hash = verify.sha256
        def guarded(path):
            self.assertNotIn("Custom Music", Path(path).parts)
            return real_hash(path)
        with patch.object(verify, "sha256", side_effect=guarded):
            after = verify.candidate_inventory(player)
        self.assertEqual(before["inventorySha256"], after["inventorySha256"])

    def test_save_rejected_before_hashing(self):
        player = self.player()
        save = player / "userdata/card.json"
        save.parent.mkdir()
        save.write_text("private fixture")
        with patch.object(verify, "sha256") as hashed:
            with self.assertRaisesRegex(AssertionError, "Personal release input"):
                verify.candidate_inventory(player)
            hashed.assert_not_called()

    def test_source_identity_includes_native_build_rules_and_local_packages(self):
        names = ("Native/unity_plugin.cmake", "ThirdParty/EOS/package.cs", "Build Native.cmd")
        for name in names:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"first")
        def fake_git(*args):
            if args[0] == "ls-files":
                self.assertIn("Native", args)
                self.assertIn("ThirdParty", args)
                self.assertIn("Build Native.cmd", args)
                return "\0".join(names).encode()
            return b"commit" if args[0] == "rev-parse" else b""
        with patch.object(verify, "ROOT", self.root), patch.object(verify, "git", side_effect=fake_git):
            before = verify.source_identity()
            for name in names:
                (self.root / name).write_bytes(b"changed")
                after = verify.source_identity()
                self.assertNotEqual(before["codeAndConfigSha256"], after["codeAndConfigSha256"])
                before = after


if __name__ == "__main__":
    unittest.main()
