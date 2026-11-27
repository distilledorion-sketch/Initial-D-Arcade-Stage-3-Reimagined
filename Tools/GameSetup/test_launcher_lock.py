import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

if os.name == "nt":
    import _winapi


@unittest.skipUnless(os.name == "nt", "Windows launcher")
class LauncherLockTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.setup = self.root / "InitialDUnity_Data/GameSetup"
        self.user = self.root / "UserData"
        self.user.mkdir()
        (self.setup / "python").mkdir(parents=True)
        self.script = self.setup / "Play.ps1"
        shutil.copyfile(Path(__file__).with_name("Play.ps1"), self.script)
        shutil.copyfile(sys.executable, self.setup / "python/python.exe")
        (self.setup / "disc_setup.py").write_text("")
        (self.setup / "disc_portable.py").write_text(
            "from pathlib import Path\nimport sys\n"
            "with Path('calls.txt').open('a') as log: print(sys.argv[1], file=log)\n"
        )
        (self.setup / "Recover-PlayerData.ps1").write_text(
            "function Invoke-PlayerRecovery([switch]$Manual, $Source, [switch]$Keep) {}"
        )
        (self.root / "rom").mkdir()
        cache = self.root / "InitialDUnity_Data/StreamingAssets/IDAS3"
        cache.mkdir(parents=True)
        (cache / "disc-source.txt").write_text("rom/gds-0033.chd")
        (cache / "disc-receipt.json").write_text("{}")
        for name in ("rom/gds-0033.chd", "rom/317-0384-com.pic",
                     "UserData/migration-v1.json", "UserData/disc-upgrade-v1.json"):
            (self.root / name).touch()
        self.env = dict(os.environ, PYTHONHOME=sys.base_prefix,
                        PATH=sys.base_prefix + os.pathsep + os.environ["PATH"])
        for key in ("IDAS3_DISC_ARGS", "IDAS3_LAUNCHER_PID"):
            self.env.pop(key, None)

    def launch(self, *options, player_args=None):
        env = self.env.copy()
        if player_args is not None:
            env["IDAS3_DISC_ARGS"] = base64.b64encode(json.dumps(player_args).encode()).decode()
        process = subprocess.Popen(
            ["powershell.exe", "-NoProfile", "-STA", "-ExecutionPolicy", "Bypass",
             "-File", str(self.script), *options],
            cwd=self.root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        def clean():
            if process.poll() is None:
                process.kill()
            process.communicate()
        self.addCleanup(clean)
        return process

    def assert_success(self, process):
        output = process.communicate(timeout=20)[0]
        self.assertEqual(process.returncode, 0, output)

    def test_click_during_setup_waits_and_second_click_does_not_start_two_games(self):
        lock = _winapi.CreateFile(str(self.user / "setup.lock"), 0xC0000000, 0, 0, 4, 0, 0)
        try:
            first = self.launch(player_args=[])
            deadline = time.monotonic() + 8
            while not (self.user / "launch.lock").exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue((self.user / "launch.lock").exists())
            self.assert_success(self.launch(player_args=[]))
            self.assertIsNone(first.poll())
            self.assertFalse((self.root / "calls.txt").exists())
            self.assertFalse((self.user / "Logs/Launcher-error.txt").exists())
        finally:
            _winapi.CloseHandle(lock)
        self.assert_success(first)
        self.assertEqual((self.root / "calls.txt").read_text().splitlines(), ["runtime", "play"])
        self.assert_success(self.launch("-InstallOnly"))
        self.assertEqual((self.root / "calls.txt").read_text().splitlines(), ["runtime", "play", "runtime"])

    def test_explicit_replay_launch_keeps_its_own_request(self):
        lock = _winapi.CreateFile(str(self.user / "launch.lock"), 0xC0000000, 0, 0, 4, 0, 0)
        try:
            self.assert_success(self.launch(player_args=["-idas3-replay-viewer"]))
        finally:
            _winapi.CloseHandle(lock)
        self.assertEqual((self.root / "calls.txt").read_text().splitlines(), ["runtime", "play"])

    def test_fresh_install_uses_japanese_disc_for_both_conversion_and_activation(self):
        cache = self.root / "InitialDUnity_Data/StreamingAssets/IDAS3"
        for name in ("disc-source.txt", "disc-receipt.json"):
            (cache / name).unlink()
        rom = self.root / "rom"
        (rom / "gds-0033.chd").unlink()
        (rom / "317-0384-com.pic").unlink()
        disc = rom / "gds-0032c.chd"
        pic = rom / "317-0379-jpn.pic"
        disc.touch()
        pic.touch()
        (self.setup / "disc_setup.py").write_text(
            "import json, sys\n"
            "with open('setup-calls.jsonl', 'a') as f: print(json.dumps(sys.argv[1:]), file=f)\n"
        )
        self.assert_success(self.launch("-InstallOnly"))
        calls = [json.loads(row) for row in (self.root / "setup-calls.jsonl").read_text().splitlines()]
        self.assertEqual(calls, [
            ["--chd", str(disc), "--security", str(pic), "--output", str(cache)],
            ["--activate", "--chd", str(disc), "--output", str(cache)],
        ])
        self.assertEqual((self.root / "calls.txt").read_text().splitlines(), ["runtime", "find-security"])

        # Reinstalling with both discs present must honor the player's restored preference.
        (rom / "gds-0033.chd").touch()
        (rom / "317-0384-com.pic").touch()
        settings = self.user / "userdata-unity-scene/game-options.json"
        settings.parent.mkdir()
        for language, expected in ((1, disc), (0, rom / "gds-0033.chd")):
            with self.subTest(romLanguage=language):
                settings.write_text(json.dumps({"version": 1, "romLanguage": language, "customMenuLanguage": 2}))
                self.assert_success(self.launch("-InstallOnly"))
                rows = (self.root / "setup-calls.jsonl").read_text().splitlines()
                extraction, activation = map(json.loads, rows[-2:])
                self.assertEqual(extraction[:2], ["--chd", str(expected)])
                self.assertEqual(activation[:3], ["--activate", "--chd", str(expected)])

    def test_menu_language_does_not_switch_rom_and_missing_second_disc_keeps_base(self):
        settings = self.user / "userdata-unity-scene/game-options.json"
        settings.parent.mkdir()
        (self.setup / "disc_setup.py").write_text(
            "import sys\n"
            "if '--base-region' in sys.argv: print('gds-0033')\n"
            "elif '--activate' in sys.argv: assert 'gds-0033.chd' in sys.argv[3]\n"
            "else: raise ValueError('unexpected regional switch')\n"
        )
        for values in ({"customMenuLanguage": 1}, {"customMenuLanguage": 2, "romLanguage": 1}):
            settings.write_text(json.dumps({"version": 1, **values}))
            self.assert_success(self.launch("-InstallOnly"))
            self.assertEqual(json.loads(settings.read_text())["customMenuLanguage"], values["customMenuLanguage"])

    def test_fresh_and_upgrade_failures_keep_the_converter_error_after_ranking(self):
        cache = self.root / "InitialDUnity_Data/StreamingAssets/IDAS3"
        receipt = cache / "disc-receipt.json"
        receipt.unlink()
        (self.setup / "disc_setup.py").write_text(
            "import sys\n"
            "if '--base-region' in sys.argv: print('gds-0033'); sys.exit(0)\n"
            "if '--activate' in sys.argv: sys.exit(4)\n"
            "print('Captured original ranking records', flush=True)\n"
            "raise PermissionError('Access denied writing \\u66f2.dat')\n"
        )
        logs = self.user / "Logs"
        reports = {}
        for stage in ("Disc extraction did not complete", "Could not upgrade"):
            with self.subTest(stage=stage):
                process = self.launch("-InstallOnly")
                process.communicate(timeout=20)
                self.assertEqual(process.returncode, 1)
                message = (logs / "Launcher-error.txt").read_text(encoding="utf-8-sig")
                report = Path(message.splitlines()[-1])
                self.assertEqual(report.parent, logs)
                self.assertNotIn(report, reports)
                self.assertIn("PermissionError: Access denied writing 曲.dat", message)
                details = report.read_text(encoding="utf-8-sig")
                for fragment in (stage, "disc_setup.py --chd", "exit code 1", "Captured original ranking records",
                                 "Traceback (most recent call last)", "PermissionError: Access denied writing 曲.dat"):
                    self.assertIn(fragment, details)
                reports[report] = details
            receipt.write_text("{}")
        for report, contents in reports.items():
            self.assertEqual(report.read_text(encoding="utf-8-sig"), contents)

    def test_unusable_lock_path_still_reports_a_real_error(self):
        (self.user / "setup.lock").mkdir()
        process = self.launch()
        process.communicate(timeout=10)
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("setup.lock", (self.user / "Logs/Launcher-error.txt").read_text(encoding="utf-8-sig"))

    def test_runtime_reports_missing_and_locked_files(self):
        shutil.copyfile(Path(__file__).with_name("disc_portable.py"), self.setup / "disc_portable.py")
        (self.setup / "runtime").mkdir()
        (self.setup / "runtime/InitialDUnity.bin").write_bytes(b"runtime fixture")
        error_log = self.user / "Logs/Launcher-error.txt"

        def expect_failure(path):
            process = self.launch("-InstallOnly")
            process.communicate(timeout=20)
            self.assertNotEqual(process.returncode, 0)
            message = error_log.read_text(encoding="utf-8-sig")
            self.assertIn(path.name, message)
            return message

        legacy = self.root / "InitialDUnity.exe"
        missing = expect_failure(legacy)
        legacy.write_bytes(b"launcher fixture")
        runtime = self.root / "InitialDUnity.bin"
        lock = _winapi.CreateFile(str(runtime), 0x80000000, 0, 0, 3, 0, 0)
        try:
            locked = expect_failure(runtime)
        finally:
            _winapi.CloseHandle(lock)
        self.assertNotEqual(locked, missing)
        self.assertEqual(runtime.read_bytes(), b"runtime fixture")
