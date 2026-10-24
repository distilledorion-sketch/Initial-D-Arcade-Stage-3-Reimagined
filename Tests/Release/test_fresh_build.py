"""Reject incomplete native evidence and unsafe or changing build inputs."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Tools"))
import build_release_candidate as build


class FreshBuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_native_report_requires_every_expected_test(self):
        report = self.root / "results.xml"
        report.write_text('<testsuite><testcase name="physics" status="run"/></testsuite>')
        with self.assertRaisesRegex(ValueError, "exactly"):
            build.require_native_results(report, ("physics", "race"))

    def test_native_report_rejects_skipped_failed_and_disabled(self):
        report = self.root / "results.xml"
        for case in ('<testcase name="physics"><skipped/></testcase>',
                     '<testcase name="physics"><failure/></testcase>',
                     '<testcase name="physics"><error/></testcase>',
                     '<testcase name="physics" status="notrun"/>'):
            with self.subTest(case=case):
                report.write_text('<testsuite>' + case + '</testsuite>')
                with self.assertRaisesRegex(ValueError, "did not pass"):
                    build.require_native_results(report, ("physics",))

    def test_native_report_rejects_duplicate_names(self):
        report = self.root / "results.xml"
        report.write_text('<testsuite><testcase name="physics"/><testcase name="physics"/></testsuite>')
        with self.assertRaises(ValueError):
            build.require_native_results(report, ("physics", "race"))

    def test_native_report_accepts_complete_success(self):
        report = self.root / "results.xml"
        report.write_text('<testsuite><testcase name="physics" status="run"/><testcase name="race" status="run"/></testsuite>')
        self.assertEqual(build.require_native_results(report, ("physics", "race"))["count"], 2)

    def test_clean_build_rejects_cached_missing_or_ambiguous_flags(self):
        for report in ({}, {"cleanBuildCache": True}, {"cleanBuildCache": False, "scriptsOnly": False},
                       {"cleanBuildCache": True, "scriptsOnly": True},
                       {"cleanBuildCache": 1, "scriptsOnly": False},
                       {"cleanBuildCache": True, "scriptsOnly": 0}):
            with self.subTest(report=report), self.assertRaisesRegex(ValueError, "clean"):
                build.require_clean_unity_build(report)

    def test_clean_build_accepts_reported_full_rebuild(self):
        build.require_clean_unity_build({"cleanBuildCache": True, "scriptsOnly": False})

    def test_input_manifest_detects_content_and_local_config_changes(self):
        assets = self.root / "Assets"
        assets.mkdir()
        config = assets / "local-client.json"
        config.write_text('private fixture')
        with patch.object(build, "ROOT", self.root), patch.object(build, "INPUT_TREES", ("Assets",)):
            before = build.build_inputs()
            self.assertNotIn("private fixture", str(before))
            config.write_text('changed private fixture')
            self.assertNotEqual(before["inventorySha256"], build.build_inputs()["inventorySha256"])
            config.unlink()
            self.assertNotEqual(before["inventorySha256"], build.build_inputs()["inventorySha256"])

    def test_input_manifest_does_not_read_unrelated_user_data(self):
        (self.root / "Assets").mkdir()
        (self.root / "userdata").mkdir()
        (self.root / "userdata/save.json").write_text('private fixture')
        with patch.object(build, "ROOT", self.root), patch.object(build, "INPUT_TREES", ("Assets",)), \
                patch.object(build.verify, "sha256", side_effect=AssertionError("must not read")):
            self.assertEqual(build.build_inputs()["fileCount"], 0)


if __name__ == "__main__":
    unittest.main()
