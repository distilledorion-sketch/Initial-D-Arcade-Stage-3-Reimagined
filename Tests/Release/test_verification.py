"""Release evidence must fail closed and must not inspect personal content."""
import importlib.util
import base64
import hashlib
import struct
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


sys.path.insert(0, str(ROOT / "Tools"))
import verify_result_startup as startup
import verify_scored_player as scored


class ResultEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    @staticmethod
    def envelope(state):
        payload=json.dumps(state,ensure_ascii=False,separators=(",", ":")).encode("utf-8")
        return json.dumps({"state":state,"sha256":base64.b64encode(hashlib.sha256(payload).digest()).decode("ascii")})

    def fixture(self):
        root=self.root/"save"
        startup.seed_profile(root)
        profile=root/"driver_profiles_v1/car_08.profile"
        data=bytearray(profile.read_bytes());struct.pack_into("<I",data,24+18*4,2321)
        checksum=2166136261
        for i,value in enumerate(data):
            if i<20 or i>=24:checksum=((checksum^value)*16777619)&0xffffffff
        struct.pack_into("<I",data,20,checksum);profile.write_bytes(data)
        record={"car":8,"record":{"battles":1,"wins":1},"receipts":[{"raceId":"123","won":True}]}
        folder=root/"online_records_v1";folder.mkdir()
        (folder/"car_08.json").write_text(self.envelope(record))
        journal={"version":1,"generation":2,"entry":{"complete":True,"raceId":"123","car":8,"winner":0,"won":True,
                 "reward":{"selection":13,"points":2000,"after":list(struct.unpack_from("<307I",data,24))}}}
        for name in ("online_result_v1.json","online_result_v1.json.previous"):
            (root/name).write_text(self.envelope(journal))
        return root

    def test_startup_evidence_accepts_complete_single_reward(self):
        self.assertEqual(startup.profile_state(self.fixture())["points"],2321)

    def test_startup_evidence_rejects_bad_checksum(self):
        root=self.fixture();path=root/"driver_profiles_v1/car_08.profile"
        data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
        with self.assertRaises(ValueError):startup.profile_state(root)

    def test_startup_evidence_rejects_valid_checksum_wrong_history(self):
        root=self.fixture();path=root/"online_records_v1/car_08.json"
        state=json.loads(path.read_text())["state"];state["record"]["battles"]=2
        path.write_text(self.envelope(state))
        with self.assertRaises(ValueError):startup.profile_state(root)

    def test_startup_evidence_rejects_unsettled_copies(self):
        root=self.fixture();(root/"online_result_v1.json.pending").write_text("pending")
        with self.assertRaises(ValueError):startup.profile_state(root)

    def test_recovery_reports_cannot_omit_input_guards(self):
        cases=[dict(cycle=i,input=("keyboard" if i==0 else "controller"),clicks=2,captures=[],
                    **{key:True for key in ("passed","durableIntent","historyBlocked","focusGuard","heldGuard","retried")})
               for i in range(2)]
        images={f"result-recovery-{lang}-{w}x{h}.png":(w,h) for lang in range(3)
                for w,h in ((1024,768),(1280,720),(1920,1080),(2560,1080))}
        cases[0]["captures"]=list(images)
        for name,(w,h) in images.items():
            (self.root/name).write_bytes(b"\x89PNG\r\n\x1a\n"+struct.pack(">I",13)+b"IHDR"+struct.pack(">II",w,h))
        report=dict(passed=True,schema="idas3-scored-result-recovery-v1",applicationVersion="test",role="host",cases=cases)
        path=self.root/"result-recovery-report.json";path.write_text(json.dumps(report))
        self.assertEqual(len(scored.require_recovery_report(self.root,"test","host")),12)
        for key in ("durableIntent","historyBlocked","focusGuard","heldGuard","retried"):
            with self.subTest(key=key):
                cases[0][key]=False;path.write_text(json.dumps(report))
                with self.assertRaises(ValueError):scored.require_recovery_report(self.root,"test","host")
                cases[0][key]=True
        cases[0]["captures"].pop();path.write_text(json.dumps(report))
        with self.assertRaises(ValueError):scored.require_recovery_report(self.root,"test","host")


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
