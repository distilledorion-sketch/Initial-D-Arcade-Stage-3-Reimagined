import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

if os.name == "nt":
    import disc_portable


@unittest.skipUnless(os.name == "nt", "Windows game")
class PortableStorageTests(unittest.TestCase):
    def test_setup_rename_preserves_previous_files_on_upgrade(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            setup = root / "InitialDUnity_Data/GameSetup"
            previous = setup.with_name("DiscEdition")
            user = root / "UserData"
            setup.mkdir(parents=True)
            previous.mkdir()
            user.mkdir()
            (previous / "native-recipe.json").write_text("{}")
            (previous / "custom.txt").write_text("keep")
            with patch.object(disc_portable, "SETUP", setup), patch.object(disc_portable, "USER", user):
                disc_portable.retire_old_setup_name()
                disc_portable.retire_old_setup_name()
            self.assertFalse(previous.exists())
            self.assertEqual((user / "Previous setup files/custom.txt").read_text(), "keep")

    def test_verified_move_retry_and_conflict(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            old, data = root / "previous", root / "game/UserData"
            before, after = old / "userdata/saves", data / "userdata/saves"
            before.mkdir(parents=True)
            after.mkdir(parents=True)
            (before / "card").write_bytes(b"previous card")
            (after / "card").write_bytes(b"portable card")
            (before / "replay").write_bytes(b"identical")
            (after / "replay").write_bytes(b"identical")
            (old / "other").write_bytes(b"leave alone")
            # A locked input must stay recoverable even if an earlier file moved.
            with (before / "replay").open("rb"):
                with self.assertRaises(OSError):
                    disc_portable.migrate(data, old, root / "temp", "Driver")
                self.assertFalse((data / "migration-v1.json").exists())
                self.assertTrue((before / "replay").is_file())
            disc_portable.migrate(data, old, root / "temp", "Driver")
            self.assertFalse(before.exists())
            self.assertEqual((after / "card").read_bytes(), b"portable card")
            self.assertEqual((after / "replay").read_bytes(), b"identical")
            recovered, = (data / "Migration conflicts/userdata/saves").glob("card.*")
            self.assertEqual(recovered.read_bytes(), b"previous card")
            self.assertEqual((old / "other").read_bytes(), b"leave alone")
            self.assertEqual((data / "online-username.txt").read_text(), "Driver")
            before.mkdir(parents=True)
            (before / "new card").touch()
            disc_portable.migrate(data, old, root / "temp", "New driver")
            self.assertTrue((before / "new card").exists())

    def test_updates_refresh_public_launcher_and_preserve_custom_shortcuts(self):
        import stat
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            setup = root / "InitialDUnity_Data/GameSetup"
            (setup / "runtime").mkdir(parents=True)
            (setup / "runtime/InitialDUnity.bin").write_bytes(b"engine")
            (setup / "tools").mkdir()
            for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
                (setup / "tools" / name).write_bytes(name.encode())
            legacy = root / "InitialDUnity.exe"
            legacy.write_bytes(b"updated launcher")
            public = root / disc_portable.PUBLIC_LAUNCHER
            public.write_bytes(b"previous launcher")
            cmd = root / "Play Reimagined.cmd"
            cmd.write_bytes(b'@echo off\r\n"%~dp0InitialDUnity.exe" %*\r\n')
            with patch.object(disc_portable, "ROOT", root), patch.object(disc_portable, "SETUP", setup):
                disc_portable.prepare_runtime()
                self.assertEqual(public.read_bytes(), legacy.read_bytes())
                self.assertTrue(legacy.stat().st_file_attributes & stat.FILE_ATTRIBUTE_HIDDEN)
                self.assertFalse(public.stat().st_file_attributes & stat.FILE_ATTRIBUTE_HIDDEN)
                stamp = public.stat().st_mtime_ns
                disc_portable.prepare_runtime()
                self.assertEqual(public.stat().st_mtime_ns, stamp)
                disc_portable.install_shortcuts()
                self.assertIn(disc_portable.PUBLIC_LAUNCHER.encode(), cmd.read_bytes())
                cmd.write_text("custom command")
                disc_portable.install_shortcuts()
                self.assertEqual(cmd.read_text(), "custom command")

    def test_game_and_viewer_have_independent_logs(self):
        import base64
        import json
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            data = root / "UserData"
            data.mkdir()
            encoded = base64.b64encode(json.dumps(["-idas3-replay-viewer", "file with spaces"]).encode()).decode()
            with patch.object(disc_portable, "ROOT", root), patch.object(disc_portable, "USER", data), \
                    patch.dict(os.environ, {"IDAS3_DISC_ARGS": encoded}), \
                    patch.object(disc_portable.subprocess, "call", return_value=0) as player, \
                    disc_portable.locked(data / "Logs/Player.lock"):
                self.assertEqual(disc_portable.play(), 0)
                args = player.call_args.args[0]
                self.assertEqual(args[:3], [str(root / "InitialDUnity.bin"), "-idas3-replay-viewer", "file with spaces"])
                self.assertEqual(args[args.index("-logFile") + 1], str(data / "Logs/ReplayViewer.log"))
                self.assertEqual(args[args.index("-crash-report-folder") + 1], str(data / "Crashes"))


if __name__ == "__main__":
    unittest.main()
