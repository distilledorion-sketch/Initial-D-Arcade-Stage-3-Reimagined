"""Real Windows process isolation checks; no game or desktop input required."""
import json
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Tools"))
from background_player import BackgroundPlayer


@unittest.skipUnless(os.name == "nt", "Windows desktop isolation")
class BackgroundPlayerTests(unittest.TestCase):
    def test_child_uses_private_desktop_and_preserves_exit_code(self):
        # Query the actual child's thread desktop, not launcher metadata.
        source = '''import ctypes,json,sys
from ctypes import wintypes as w
u=ctypes.WinDLL('user32',use_last_error=True)
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.GetCurrentThreadId.restype=w.DWORD
u.GetThreadDesktop.argtypes=[w.DWORD];u.GetThreadDesktop.restype=w.HANDLE
u.GetUserObjectInformationW.argtypes=[w.HANDLE,ctypes.c_int,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD)]
name=ctypes.create_unicode_buffer(256);needed=w.DWORD()
assert u.GetUserObjectInformationW(u.GetThreadDesktop(k.GetCurrentThreadId()),2,name,ctypes.sizeof(name),ctypes.byref(needed))
open(sys.argv[1],'w').write(json.dumps({'desktop':name.value}))
sys.exit(7)
'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            script, report = root / "probe.py", root / "report.json"
            script.write_text(source, encoding="utf-8")
            with (root / "launcher.log").open("wb") as log:
                child = BackgroundPlayer([sys.executable, str(script), str(report)], root, log)
                try:
                    self.assertEqual(child.wait(timeout=15), 7)
                    self.assertEqual(child.poll(), 7)
                    self.assertEqual(json.loads(report.read_text())["desktop"], child.desktop_name)
                    self.assertIsNone(child._process)
                    self.assertIsNone(child._desktop)
                finally:
                    if child.poll() is None:
                        child.kill(); child.wait(timeout=10)

    def test_timeout_and_termination_are_scoped_to_owned_child(self):
        with tempfile.TemporaryDirectory() as temporary:
            with (Path(temporary) / "launcher.log").open("wb") as log:
                child = BackgroundPlayer([sys.executable, "-c", "import time; time.sleep(30)"], temporary, log)
                try:
                    with self.assertRaises(subprocess.TimeoutExpired):
                        child.wait(timeout=.05)
                    self.assertIsNone(child.poll())
                finally:
                    child.kill()
                    self.assertEqual(child.wait(timeout=10), 1)

    def test_failed_launch_does_not_fall_back_to_working_desktop(self):
        with tempfile.TemporaryDirectory() as temporary:
            with (Path(temporary) / "launcher.log").open("wb") as log:
                with self.assertRaises(OSError):
                    BackgroundPlayer([str(Path(temporary) / "absent.exe")], temporary, log)

    def test_exit_between_poll_and_terminate_is_observed_before_accepting_denial(self):
        with tempfile.TemporaryDirectory() as temporary:
            with (Path(temporary) / "launcher.log").open("wb") as log:
                child = BackgroundPlayer([sys.executable, "-c", "import time; time.sleep(.2)"], temporary, log)
                def denied(*args):
                    ctypes.set_last_error(5)
                    return 0
                try:
                    self.assertIsNone(child.poll())
                    with patch.object(child.kernel, "TerminateProcess", side_effect=denied) as terminate:
                        child.kill()
                        terminate.assert_called_once()
                    self.assertEqual(child.returncode, 0)
                    self.assertIsNone(child._process)
                    self.assertIsNone(child._desktop)
                    child.kill()  # Repeated cleanup must not touch a released handle.
                finally:
                    if child.poll() is None:
                        child.kill(); child.wait(timeout=10)

    def test_denied_live_child_remains_owned_and_reports_the_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            with (Path(temporary) / "launcher.log").open("wb") as log:
                child = BackgroundPlayer([sys.executable, "-c", "import time; time.sleep(30)"], temporary, log)
                def denied(*args):
                    ctypes.set_last_error(5)
                    return 0
                try:
                    with patch.object(child.kernel, "TerminateProcess", side_effect=denied), \
                         patch.object(child, "wait", side_effect=subprocess.TimeoutExpired(child.args, 1)):
                        with self.assertRaises(OSError) as failure:
                            child.kill()
                        self.assertEqual(failure.exception.winerror, 5)
                    self.assertIsNone(child.poll())
                    self.assertIsNotNone(child._process)
                    self.assertIsNotNone(child._desktop)
                finally:
                    child.kill(); child.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
