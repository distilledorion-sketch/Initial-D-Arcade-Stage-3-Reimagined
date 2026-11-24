"""Store imported course variants together and restore byte-identical runtime files."""
import hashlib
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import tempfile
import zipfile

from disc_extras import digest, map_packs, maps_folder, member

PACK_DIR = "course-packs"
CHUNK = 1024 * 1024


def course_files(course, files):
    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", course) or course + "/menu.idastex" not in files:
        raise ValueError(f"Unknown course pack {course}")
    return {n: info for n, info in files.items() if n.startswith(course + "/")}


def local_path(path):
    if path.resolve() != path.absolute() or path.is_symlink():
        raise ValueError(f"Cannot install maps through a linked path: {path}")
    return path


def prepare_maps(streaming, files):
    maps = maps_folder(streaming)
    for folder in (maps, maps.parent / "Cars", maps.parent / "HUD"):
        local_path(folder).mkdir(parents=True, exist_ok=True)
    for course in sorted(map_packs(files)):
        source = local_path(streaming / course)
        if not source.is_dir():
            continue
        target = local_path(maps / course)
        if not target.exists():
            # Same-volume rename keeps custom files and existing hard links.
            source.rename(target)
            continue
        # A player may already have a Mods folder. Merge missing stock files,
        # leaving conflicting copies and unrecognised files available to them.
        parents = {source}
        for name, (size, expected) in course_files(course, files).items():
            old = local_path(member(streaming, name))
            new = local_path(member(maps, name))
            parents.update(p for p in old.parents if p.is_relative_to(source))
            if not old.is_file():
                continue
            if not new.exists():
                new.parent.mkdir(parents=True, exist_ok=True)
                old.rename(new)
            elif new.is_file() and old.stat().st_size == size == new.stat().st_size:
                if digest(old) == expected == digest(new):
                    old.unlink()
        for folder in sorted(parents, key=lambda p: len(p.parts), reverse=True):
            if folder.is_dir() and not any(folder.iterdir()):
                folder.rmdir()
    return maps


def write_pack(archive, course, files, open_source):
    entries = course_files(course, files)
    # Adjacent variants share large runs of identical track geometry and textures.
    order = sorted(entries, key=lambda n: (PurePosixPath(n).suffix, PurePosixPath(n).name, n))
    # Some day/night variants exceed 64 MiB each. Keep a whole variant in
    # the compression window so shared geometry can be encoded only once.
    filters = [{"id": lzma.FILTER_LZMA2, "preset": 4, "dict_size": 96 * CHUNK, "mf": lzma.MF_HC4}]
    seen = set()
    with archive.open(zipfile.ZipInfo(f"setup/{PACK_DIR}/{course}.tar.xz"), "w") as dest:
        with lzma.LZMAFile(dest, "w", format=lzma.FORMAT_XZ, filters=filters) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.USTAR_FORMAT) as tar:
                for name in order:
                    size, expected = entries[name]
                    if expected in seen:
                        continue
                    seen.add(expected)
                    header = tarfile.TarInfo(expected)
                    header.size = size
                    with open_source(name) as source:
                        tar.addfile(header, source)


def restore_pack(packed, maps, files):
    course = packed.name.removesuffix(".tar.xz")
    entries = course_files(course, files)
    expected = {checksum: size for size, checksum in entries.values()}
    seen = set()
    with tempfile.TemporaryDirectory(prefix=".idas3-course-", dir=maps) as temp:
        work = Path(temp)
        try:
            archive = packed.open("rb")
        except FileNotFoundError:
            # Another launcher finished restoring this pack after enumeration.
            return
        with archive, lzma.LZMAFile(archive) as compressed:
            with tarfile.open(fileobj=compressed, mode="r|") as tar:
                for entry in tar:
                    key = entry.name
                    if not entry.isreg() or not re.fullmatch("[0-9a-f]{64}", key) or key in seen or expected.get(key) != entry.size:
                        raise ValueError(f"Unexpected content in {packed.name}")
                    checksum = hashlib.sha256()
                    with tar.extractfile(entry) as source, (work / key).open("xb") as dest:
                        while block := source.read(CHUNK):
                            dest.write(block)
                            checksum.update(block)
                    if checksum.hexdigest() != key:
                        raise ValueError(f"Damaged course data in {packed.name}")
                    seen.add(key)
            # Read the XZ footer and check its checksum before installing any files.
            while compressed.read(CHUNK):
                pass
        if seen != expected.keys():
            raise ValueError(f"Incomplete course pack {packed.name}")
        for name, (_, key) in entries.items():
            dest = local_path(member(maps, name))
            dest.parent.mkdir(parents=True, exist_ok=True)
            if dest.is_file():
                previous_hash = digest(dest)
                if previous_hash == key:
                    continue
                # Keep an edited file beside the updated bundled map. Its hash
                # gives repeat installations the same backup name.
                backup = local_path(dest.with_name(dest.name + "." + previous_hash + ".previous"))
                if backup.exists():
                    if digest(backup) != previous_hash:
                        raise ValueError(f"Previous map file was changed: {backup}")
                else:
                    backup_stage = work / "previous"
                    try:
                        os.link(dest, backup_stage)
                    except OSError:
                        shutil.copy2(dest, backup_stage)
                    backup_stage.rename(backup)
            pending = work / "installing"
            try:
                os.link(work / key, pending)
            except OSError:
                shutil.copyfile(work / key, pending)
            try:
                os.replace(pending, dest)
            except PermissionError:
                # An earlier launch can already be using this verified file.
                if not dest.is_file() or digest(dest) != key:
                    raise
                pending.unlink()
    # The restored game files are self-contained. A ZIP re-extraction supplies
    # the pack again if the player ever needs to repair a damaged installation.
    try:
        packed.unlink(missing_ok=True)
    except PermissionError as error:
        if getattr(error, "winerror", 0) != 32:
            raise
        # A concurrent reader will remove the archive after it closes.


def restore_pending(streaming, setup, files):
    maps = prepare_maps(streaming, files)
    folder = setup / PACK_DIR
    packs = sorted(folder.glob("*.tar.xz"))
    restored = False
    for packed in packs:
        print(f"Installing {packed.name.split('.')[0]} course files...", flush=True)
        try:
            restore_pack(packed, maps, files)
            restored = True
        except (OSError, EOFError, lzma.LZMAError, tarfile.TarError, ValueError) as error:
            print(f"Skipping optional track {packed.name}: {error}", flush=True)
    return restored
