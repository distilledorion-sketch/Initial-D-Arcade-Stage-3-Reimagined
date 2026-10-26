"""Launch diagnostic players on an unshown Windows desktop, never the user's.

This is process isolation, not UI automation. The desktop is never switched to
or activated. Failure aborts launch; there is no foreground fallback.
"""
import ctypes
from ctypes import wintypes as w
import os
import subprocess
import uuid


class StartupInfo(ctypes.Structure):
    _fields_ = [("cb", w.DWORD), ("lpReserved", w.LPWSTR), ("lpDesktop", w.LPWSTR),
                ("lpTitle", w.LPWSTR), ("dwX", w.DWORD), ("dwY", w.DWORD),
                ("dwXSize", w.DWORD), ("dwYSize", w.DWORD), ("dwXCountChars", w.DWORD),
                ("dwYCountChars", w.DWORD), ("dwFillAttribute", w.DWORD), ("dwFlags", w.DWORD),
                ("wShowWindow", w.WORD), ("cbReserved2", w.WORD), ("lpReserved2", ctypes.c_void_p),
                ("hStdInput", w.HANDLE), ("hStdOutput", w.HANDLE), ("hStdError", w.HANDLE)]


class ProcessInfo(ctypes.Structure):
    _fields_ = [("hProcess", w.HANDLE), ("hThread", w.HANDLE),
                ("dwProcessId", w.DWORD), ("dwThreadId", w.DWORD)]


class BackgroundPlayer:
    """Small Popen-compatible owner for one process and its private desktop."""
    def __init__(self, command, cwd, stdout):
        if os.name != "nt":
            raise OSError("Background player isolation requires Windows")
        self.args, self.returncode, self.pid = command, None, None
        self._process = self._desktop = None
        self.desktop_name = "InitialDTest_" + uuid.uuid4().hex
        self.user = ctypes.WinDLL("user32", use_last_error=True)
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.user.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, ctypes.c_void_p, w.DWORD, w.DWORD, ctypes.c_void_p]
        self.user.CreateDesktopW.restype = w.HANDLE
        self.user.CloseDesktop.argtypes = [w.HANDLE]
        self.user.CloseDesktop.restype = w.BOOL
        self.kernel.CreateProcessW.argtypes = [w.LPCWSTR, w.LPWSTR, ctypes.c_void_p, ctypes.c_void_p,
            w.BOOL, w.DWORD, ctypes.c_void_p, w.LPCWSTR, ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInfo)]
        self.kernel.CreateProcessW.restype = w.BOOL
        self.kernel.CloseHandle.argtypes = [w.HANDLE]
        self.kernel.CloseHandle.restype = w.BOOL
        self.kernel.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
        self.kernel.WaitForSingleObject.restype = w.DWORD
        self.kernel.GetExitCodeProcess.argtypes = [w.HANDLE, ctypes.POINTER(w.DWORD)]
        self.kernel.GetExitCodeProcess.restype = w.BOOL
        self.kernel.TerminateProcess.argtypes = [w.HANDLE, w.UINT]
        self.kernel.TerminateProcess.restype = w.BOOL
        # Read/write objects, create windows/menus and enumerate. No switch,
        # journal, hook, access-control changes or inheritable desktop handle.
        self._desktop = self.user.CreateDesktopW(self.desktop_name, None, None, 0, 0xC7, None)
        if not self._desktop:
            raise ctypes.WinError(ctypes.get_last_error())
        startup = StartupInfo()
        startup.cb = ctypes.sizeof(startup)
        startup.lpDesktop = self.desktop_name
        startup.dwFlags, startup.wShowWindow = 1, 0  # STARTF_USESHOWWINDOW / SW_HIDE
        info = ProcessInfo()
        try:
            # No inherited handles. Unity writes its complete log via -logFile.
            # Not inheriting the caller's desktop is what keeps later Unity
            # SetResolution/activation calls away from the working desktop.
            stdout.write(("Isolated desktop: " + self.desktop_name +
                "\nUnity output is in the corresponding -player.log.\n").encode())
            stdout.flush()
            if not self.kernel.CreateProcessW(command[0], ctypes.create_unicode_buffer(subprocess.list2cmdline(command)),
                    None, None, False, 0x08000000, None, str(cwd), ctypes.byref(startup), ctypes.byref(info)):
                raise ctypes.WinError(ctypes.get_last_error())
            self._process, self.pid = info.hProcess, info.dwProcessId
            self.kernel.CloseHandle(info.hThread)
        except BaseException:
            self.user.CloseDesktop(self._desktop)
            self._desktop = None
            raise

    def poll(self):
        if self.returncode is not None:
            return self.returncode
        result = self.kernel.WaitForSingleObject(self._process, 0)
        if result == 258:
            return None
        if result != 0:
            raise ctypes.WinError(ctypes.get_last_error())
        code = w.DWORD()
        if not self.kernel.GetExitCodeProcess(self._process, ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        self.returncode = code.value
        self.kernel.CloseHandle(self._process)
        self._process = None
        self.user.CloseDesktop(self._desktop)
        self._desktop = None
        return self.returncode

    def wait(self, timeout=None):
        if self.returncode is not None:
            return self.returncode
        result = self.kernel.WaitForSingleObject(self._process, 0xFFFFFFFF if timeout is None else int(timeout * 1000))
        if result == 258:
            raise subprocess.TimeoutExpired(self.args, timeout)
        if result != 0:
            raise ctypes.WinError(ctypes.get_last_error())
        return self.poll()

    def kill(self):
        if self.poll() is None and not self.kernel.TerminateProcess(self._process, 1):
            error = ctypes.WinError(ctypes.get_last_error())
            # taskkill or a normal exit can win after poll(). Windows reports
            # ACCESS_DENIED for a process whose termination is already underway.
            # Accept only an observed exit, never a permission failure on a live
            # child. Keep ownership/handles intact when it is still running.
            if error.winerror == 5:
                try:
                    self.wait(timeout=1)
                    return
                except subprocess.TimeoutExpired:
                    pass
            raise error
