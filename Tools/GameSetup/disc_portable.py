"""Own the Windows game's portable files and its one-time migration."""
import argparse
import base64
import contextlib
import ctypes
from ctypes import wintypes
import hashlib
import json
import msvcrt
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import uuid
import winreg

SETUP = Path(__file__).resolve().parent
ROOT = SETUP.parent.parent if SETUP.parent.name == "InitialDUnity_Data" else SETUP.parent
USER = ROOT / "UserData"
PUBLIC_LAUNCHER = "Initial D Arcade Stage 3 Reimagined.exe"
LOCAL_DISC = "rom/gds-0033.chd"
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                               wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.SetFileInformationByHandle.argtypes = [wintypes.HANDLE, ctypes.c_int,
                                             wintypes.LPVOID, wintypes.DWORD]
kernel.SetFileInformationByHandle.restype = wintypes.BOOL
kernel.SetFileAttributesW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
kernel.SetFileAttributesW.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


def plain(path):
    """Reject links and junctions anywhere between the drive and this path."""
    for part in (*reversed(path.parents), path):
        try:
            attributes = part.lstat().st_file_attributes
        except FileNotFoundError:
            continue
        if attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
            raise OSError(f"Linked path needs manual review: {part}")
    return path


def local_low():
    shell = ctypes.WinDLL("shell32")
    path = ctypes.c_wchar_p()
    guid = uuid.UUID("A520A1A4-1780-4FF6-BD18-167343C5AF16")
    if shell.SHGetKnownFolderPath(ctypes.byref(ctypes.create_string_buffer(guid.bytes_le)),
                                  0, None, ctypes.byref(path)):
        raise OSError("Windows could not locate the previous save folder.")
    try:
        return Path(path.value)
    finally:
        ctypes.windll.ole32.CoTaskMemFree(ctypes.cast(path, ctypes.c_void_p))


class InUseError(OSError):
    pass


@contextlib.contextmanager
def locked(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as lock:
        lock.seek(0)
        try:
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise InUseError("Another game process is using this file. Try again shortly.") from error
        try:
            yield
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def checksum(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def write_text(path, text):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
        staged = Path(output.name)
        try:
            output.write(text.encode("utf-8"))
            output.flush()
            os.fsync(output.fileno())
        except BaseException:
            output.close()
            staged.unlink(missing_ok=True)
            raise
    try:
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)


def transfer(source, target, data):
    # Hold the original against changes from other processes until verified.
    # DELETE access lets us remove this exact open file even across volumes.
    plain(source)
    plain(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    handle = kernel.CreateFileW(str(source), 0x80010000, 1, None, 3, 0, None)
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
    except BaseException:
        kernel.CloseHandle(handle)
        raise
    with os.fdopen(fd, "rb") as original:
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
            staged = Path(output.name)
            try:
                shutil.copyfileobj(original, output, 1024 * 1024)
                output.flush()
                os.fsync(output.fileno())
            except BaseException:
                output.close()
                staged.unlink(missing_ok=True)
                raise
        try:
            original.seek(0)
            expected = hashlib.file_digest(original, "sha256").hexdigest()
            if checksum(staged) != expected:
                raise OSError(f"Copy verification failed: {source.name}")
            conflict = target.exists() and checksum(target) != expected
            if conflict:
                target = plain(data / "Migration conflicts" / target.relative_to(data))
                target = plain(target.with_name(target.name + "." + expected))
                target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                times = os.fstat(fd)
                os.utime(staged, ns=(times.st_atime_ns, times.st_mtime_ns))
                staged.rename(target)  # Windows will never replace an existing file here.
            if checksum(target) != expected:
                raise OSError(f"Destination changed during migration: {target.name}")
            remove = wintypes.BOOL(True)
            if not kernel.SetFileInformationByHandle(handle, 4, ctypes.byref(remove), ctypes.sizeof(remove)):
                raise ctypes.WinError(ctypes.get_last_error())
            return conflict
        finally:
            staged.unlink(missing_ok=True)


def migrate_tree(source, target, data, counts):
    plain(source)
    if not source.exists():
        return
    if source.is_dir():
        plain(target).mkdir(exist_ok=True, parents=True)
        for child in list(source.iterdir()):
            migrate_tree(child, target / child.name, data, counts)
        source.rmdir()
    else:
        counts["conflicts"] += int(transfer(source, target, data))
        counts["files"] += 1


def read_name():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Chris\Initial D Unity") as key:
            value, _ = winreg.QueryValueEx(key, "idas3.online.name_h3666169665")
    except FileNotFoundError:
        return ""
    if isinstance(value, bytes):
        return value.rstrip(b"\0").decode("utf-8")
    return value if isinstance(value, str) else ""


def migrate(data, old, old_temp, name):
    marker = plain(data / "migration-v1.json")
    with locked(plain(data / "migration.lock")):
        if marker.is_file():
            return
        counts = {"files": 0, "conflicts": 0}
        for item in ("userdata", "userdata-unity-scene", "community-times", "EOS",
                     "Diagnostics", "last-error.txt"):
            migrate_tree(old / item, data / item, data, counts)
        for item in ("Player.log", "Player-prev.log"):
            migrate_tree(old / item, data / "Logs/Previous installation" / item, data, counts)
        game_temp = old_temp / "Chris/Initial D Unity"
        for item, dest in (("replay-viewer-session", "Temp/replay-viewer-session"),
                           ("Crashes", "Crashes"),
                           ("dx12_pso_cache_lib.bin", "Temp/Chris/Initial D Unity/dx12_pso_cache_lib.bin")):
            migrate_tree(game_temp / item, data / dest, data, counts)
        for log in old_temp.glob("Idas3-Disc-Play-*.log"):
            migrate_tree(log, data / "Logs/Previous installation" / log.name, data, counts)
        username = plain(data / "online-username.txt")
        if name and not username.exists():
            write_text(username, name)
        write_text(marker, json.dumps(counts) + "\n")
        print(f"Moved or verified {counts['files']} previous game files; {counts['conflicts']} conflicts preserved.")


def import_original(disc, security, root=ROOT):
    from disc_setup import CHD, check_source, read_pic
    folder = plain(root / "rom")
    folder.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=folder) as work:
        scratch = Path(work)
        if disc:
            local = plain(root / LOCAL_DISC)
            check_source(disc, CHD, "GDS-0033 disc image")
            if not local.exists():
                copied = scratch / local.name
                shutil.copyfile(disc, copied)
                check_source(copied, CHD, "copied GDS-0033 disc image")
                copied.rename(local)
            elif not local.samefile(disc):
                check_source(local, CHD, "disc in the game folder")
        if security:
            selected = read_pic(security, scratch)
            local = plain(folder / "317-0384-com.pic")
            if local.exists():
                read_pic(local, scratch)
            else:
                copied = scratch / local.name
                shutil.copyfile(selected, copied)
                read_pic(copied, scratch)
                copied.rename(local)
        source = root / "InitialDUnity_Data/StreamingAssets/IDAS3/disc-source.txt"
        if disc and source.is_file() and source.read_text(encoding="utf-8").strip() != LOCAL_DISC:
            write_text(source, LOCAL_DISC)


def prepare_runtime():
    # Released updaters stage the launcher at its original path. Materialize
    # the public name during both standalone setup and post-update startup.
    legacy = plain(ROOT / "InitialDUnity.exe")
    files = {"InitialDUnity.bin": SETUP / "runtime/InitialDUnity.bin",
             PUBLIC_LAUNCHER: legacy}
    for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        files[name] = SETUP / "tools" / name
    for name, source in files.items():
        dest = plain(ROOT / name)
        expected = checksum(plain(source))
        if dest.is_file() and checksum(dest) == expected:
            continue
        with tempfile.TemporaryDirectory(dir=ROOT) as scratch:
            pending = Path(scratch) / name
            shutil.copyfile(source, pending)
            if checksum(pending) != expected:
                raise OSError(f"Could not verify {name} in the game folder.")
            os.replace(pending, dest)
    attributes = legacy.stat().st_file_attributes
    if not attributes & stat.FILE_ATTRIBUTE_HIDDEN:
        if not kernel.SetFileAttributesW(str(legacy), attributes | stat.FILE_ATTRIBUTE_HIDDEN):
            raise ctypes.WinError(ctypes.get_last_error())


def retire_duplicate_course_packs():
    old = plain(ROOT / "setup/course-packs")
    if not old.is_dir():
        return
    for incoming in (SETUP / "course-packs").glob("*.tar.xz"):
        prior = plain(old / incoming.name)
        if prior.is_file() and prior.stat().st_size == incoming.stat().st_size and checksum(prior) == checksum(incoming):
            prior.unlink()


def install_shortcuts():
    shortcuts = {
        "Play Reimagined.cmd": ("6d6a010b0fdb52e6aeca666847fac6709481fb5d2d533f13b8d925c4567cf6db", '"%~dp0Initial D Arcade Stage 3 Reimagined.exe" %*'),
        "Verify Game Files.cmd": ("276c1ffa36ac18f46a2dfc81eeb89a2bdf86df1c691881fac2c0cb2df70243bd",
                                 r'powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0InitialDUnity_Data\GameSetup\Play.ps1" -Verify' + "\r\npause")
    }
    for name, (previous_hash, command) in shortcuts.items():
        path = plain(ROOT / name)
        known_hashes = {previous_hash}
        if name == "Verify Game Files.cmd":
            known_hashes.add("0eb22048a629cc8e30d170c443606d23b6df747da013a1a272bc2cda39ee5d96")
        if not path.exists() or checksum(path) in known_hashes or (
                name == "Play Reimagined.cmd" and checksum(path) == "5c30d39cefa357808653cc5fb8af257164246365309a66303059a3d3c05a8d06"):
            write_text(path, "@echo off\r\n" + command + "\r\n")


def retire_old_setup_name():
    # An in-place update can leave the former support directory beside the new one.
    previous = plain(SETUP.parent / "DiscEdition")
    if SETUP.name != "GameSetup" or not (previous / "native-recipe.json").is_file():
        return
    backup = plain(USER / "Previous setup files")
    if backup.exists():
        backup = plain(USER / ("Previous setup files-" + uuid.uuid4().hex))
    previous.rename(backup)


def retire_previous_installation():
    # Discard only files still matching the published .56 Windows asset inventory.
    previous = plain(USER / ".disc-cache-upgrade")
    manifest = previous / "data.manifest.json"
    from disc_setup import is_windows_cache, recorded_disc, windows_inventory
    known_manifest = manifest.is_file() and checksum(plain(manifest)) == "e4b4e41da0cc45889761a4782d200d86d66135d90f6295b8745f2e11339683a1"
    if known_manifest or (previous.is_dir() and not recorded_disc(previous) and is_windows_cache(previous)):
        entries = windows_inventory()
        if known_manifest:
            for row in json.loads(manifest.read_text(encoding="utf-8-sig"))["files"]:
                entries.setdefault(row["path"], (row["bytes"], row["sha256"]))
        old_data = previous / "data"
        for name, (size, expected) in entries.items():
            item = plain(old_data / name)
            if not item.is_relative_to(old_data) or ".." in item.parts:
                raise ValueError("Invalid previous Windows asset path.")
            if item.is_file() and item.stat().st_size == size and checksum(item) == expected:
                item.unlink()
        if known_manifest:
            manifest.unlink()
    elif previous.is_dir():
        active = ROOT / "InitialDUnity_Data/StreamingAssets/IDAS3"
        receipt = recorded_disc(active)
        old_receipt = recorded_disc(previous)
        if receipt and old_receipt:
            from disc_storage import trim_conversion_files
            trim_conversion_files(previous / "data", old_receipt["files"])
            # The new receipt came from the player's disc. Retire identical old assets only.
            for name, expected in receipt["files"].items():
                if Path(name).is_absolute() or ".." in Path(name).parts:
                    raise ValueError("Invalid disc asset path.")
                path = plain(previous / "data" / name)
                if path.is_file() and checksum(path) == expected:
                    path.unlink()
    if previous.is_dir():
        for base, dirs, files in os.walk(previous, topdown=False):
            try:
                plain(Path(base)).rmdir()
            except OSError:
                pass
        if previous.exists():
            destination = plain(USER / "Previous Windows Assets")
            if destination.exists():
                destination = plain(USER / ("Previous Windows Assets-" + uuid.uuid4().hex))
            previous.rename(destination)
    # Retire identical files left behind by the previous portable package.
    old = plain(ROOT / "setup")
    if old.is_dir():
        retired_songs = json.loads((SETUP / "previous-song-hashes.json").read_text())
        for name, expected in retired_songs.items():
            rel = Path(name)
            if rel.parts[:2] != ("original_audio", "streams") or len(rel.parts) != 4 or ".." in rel.parts:
                raise ValueError("Invalid old soundtrack path.")
            song = plain(old / "additional-data" / rel)
            if song.is_file() and checksum(song) == expected:
                song.unlink()
        for base, dirs, files in os.walk(old, topdown=False):
            folder = plain(Path(base))
            for name in files:
                path = plain(folder / name)
                replacement = SETUP / path.relative_to(old)
                if replacement.is_file() and path.stat().st_size == replacement.stat().st_size and checksum(path) == checksum(replacement):
                    path.unlink()
            try:
                folder.rmdir()
            except OSError:
                pass


def play():
    args = json.loads(base64.b64decode(os.environ.pop("IDAS3_DISC_ARGS", "W10=")).decode("utf-8"))
    if not isinstance(args, list) or not all(isinstance(s, str) for s in args):
        raise ValueError("Invalid player arguments.")
    logs = plain(USER / "Logs")
    logs.mkdir(exist_ok=True)
    crashes = plain(USER / "Crashes")
    crashes.mkdir(exist_ok=True)
    names = [arg.lower() for arg in args]
    with contextlib.ExitStack() as stack:
        if "-logfile" not in names:
            stem = "ReplayViewer" if "-idas3-replay-viewer" in names else "Player"
            try:
                stack.enter_context(locked(logs / (stem + ".lock")))
            except InUseError:
                stem += "-" + str(os.getpid())
            log = logs / (stem + ".log")
            if log.exists():
                os.replace(log, logs / (stem + "-prev.log"))
            args += ["-logFile", str(log)]
        if "-crash-report-folder" not in names:
            args += ["-crash-report-folder", str(crashes)]
        return subprocess.call([str(ROOT / "InitialDUnity.bin"), *args], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("migrate", "import", "runtime", "finish", "play"))
    parser.add_argument("--chd", type=Path)
    parser.add_argument("--security", type=Path)
    args = parser.parse_args()
    if args.action == "migrate":
        if not (USER / "migration-v1.json").is_file():
            migrate(USER, local_low() / "Chris/Initial D Unity",
                    Path(os.environ["IDAS3_LEGACY_TEMP"]), read_name())
    elif args.action == "finish":
        marker = plain(USER / "disc-upgrade-v1.json")
        if not marker.exists() or (USER / ".disc-cache-upgrade").exists():
            retire_previous_installation()
            write_text(marker, json.dumps({"version": 1}))
    elif args.action == "runtime":
        prepare_runtime()
        install_shortcuts()
        retire_old_setup_name()
        retire_duplicate_course_packs()
    elif args.action == "import":
        import_original(args.chd, args.security)
    else:
        return play()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
