"""Losslessly compress large read-only installed assets on Windows when supported."""
import ctypes
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile

from disc_extras import digest, installed_file, map_packs, member

SKIP = {".mp3", ".ogg", ".adx", ".zip", ".xz", ".dll", ".png", ".jpg"}


def compiled_asset(name):
    """Only converter outputs with a known, separately consumed runtime asset."""
    if not name.startswith("original_assets/"):
        return None
    folder, _, filename = name.rpartition("/")
    if filename == "original_texture_decoded.bin":
        return folder + "/textures/textures.idastex"
    if filename in ("original_pol.bin", "original_pol.tbl"):
        return folder + "/" + PurePosixPath(folder).name + ".idasmesh"
    if folder.endswith("/textures") and re.fullmatch(r"texture_[0-9]{3,}\.png", filename):
        return folder + "/textures.idastex"
    return None


def trim_conversion_files(data, files):
    """Remove verified converter scratch files after all consumers have finished.

    The caller publishes the shortened receipt atomically. If interrupted, a
    retry also retires already-removed entries after verifying their runtime bank.
    """
    banks = {}
    for name in files:
        bank = compiled_asset(name)
        if bank in files:
            banks.setdefault(bank, []).append(name)
    changed = False
    for bank, names in banks.items():
        packed = member(data, bank)
        if packed.resolve() != packed.absolute() or not packed.is_file() or digest(packed) != files[bank]:
            continue
        for name in names:
            source = member(data, name)
            if source.resolve() != source.absolute() or source.is_symlink():
                continue
            try:
                if not source.is_file() and source.exists():
                    continue
                if digest(source) != files[name]:
                    continue
                source.unlink()
            except FileNotFoundError:
                pass
            except PermissionError:
                continue
            del files[name]
            changed = True
    return changed


def candidates(streaming, extras, receipt):
    # A file can appear in both receipts. Only inspect paths created by setup.
    names = set(extras)
    names.update("IDAS3/data/" + rel for rel in receipt)
    directories = {}
    packs = map_packs(extras)
    for name in names:
        path = installed_file(streaming, name, packs)
        if path.suffix.lower() not in SKIP:
            directories.setdefault(path.parent, set()).add(path.name)
    for parent, wanted in sorted(directories.items()):
        # Do not follow a player's junction to another game or a shared library.
        if not parent.is_dir() or parent.resolve() != parent.absolute():
            continue
        with os.scandir(parent) as entries:
            for entry in entries:
                if entry.name not in wanted or not entry.is_file(follow_symlinks=False):
                    continue
                info = entry.stat(follow_symlinks=False)
                if info.st_size < 256 * 1024 or info.st_file_attributes & (
                        stat.FILE_ATTRIBUTE_COMPRESSED | stat.FILE_ATTRIBUTE_REPARSE_POINT | stat.FILE_ATTRIBUTE_SPARSE_FILE):
                    continue
                if os.stat(entry.path, follow_symlinks=False).st_nlink == 1:
                    yield entry.path


def save_storage_stamp(marker, stamp):
    with tempfile.NamedTemporaryFile(dir=marker.parent, delete=False, mode="w", encoding="utf-8") as f:
        json.dump({"stamp": stamp}, f)
    os.replace(f.name, marker)


def compact_install(cache, setup, extras, receipt, restored=False, unchanged_storage_from=None):
    if os.name != "nt":
        return
    marker = cache / "disc-storage.json"
    stamp = [(cache / "disc-receipt.json").stat().st_mtime_ns,
             (setup / "additional-content.json").stat().st_mtime_ns]
    if marker.is_file() and not restored:
        try:
            previous = json.loads(marker.read_text()).get("stamp")
            if previous == stamp:
                return
            if unchanged_storage_from is not None and previous == [unchanged_storage_from, stamp[1]]:
                # Removing scratch files or replacing the excluded native DLL
                # creates no new compression candidates.
                save_storage_stamp(marker, stamp)
                return
        except (OSError, ValueError):
            pass
    compact = Path(os.environ["SystemRoot"]) / "System32/compact.exe"
    fs_name = ctypes.create_unicode_buffer(64)
    volume_path = ctypes.create_unicode_buffer(32768)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    supported = (compact.is_file() and
                 kernel.GetVolumePathNameW(str(cache), volume_path, len(volume_path)) and
                 kernel.GetVolumeInformationW(volume_path.value, None, 0, None, None, None, fs_name, len(fs_name)) and
                 fs_name.value == "NTFS")
    files = list(candidates(cache.parent, extras, receipt)) if supported else []
    if files:
        print(f"Reducing disk use for {len(files)} game files. This can take a few minutes.", flush=True)
    base_args = [str(compact), "/C", "/EXE:XPRESS8K", "/I", "/Q"]
    # Windows limits command-line length. The game can read every file even
    # if compression is unavailable, a file is busy or setup is interrupted.
    batch = []
    length = 0
    for path in files + [None]:
        if batch and (path is None or length + len(path) > 16000):
            subprocess.run(base_args + batch, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW, check=False)
            batch = []
            length = 0
        if path is not None:
            batch.append(path)
            length += len(path) + 3
    save_storage_stamp(marker, stamp)
