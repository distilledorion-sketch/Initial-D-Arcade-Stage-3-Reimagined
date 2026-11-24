"""Recover player-owned data between Windows installations without merging two cards."""
import argparse
import json
import sys
from pathlib import Path
import shutil
import tempfile
import uuid

from disc_portable import ROOT, USER, checksum, local_low, locked, plain, write_text

ROOTS = ("userdata-unity-scene", "userdata")
SETTINGS = ("settings.txt", "native_selection.txt", "controls.json", "controller-device.json",
            "game-options.json", "race_music.txt", "sound-room.json")
HISTORY = ("time_attack_records_v1.csv", "hakone_personal_v1.csv", "online_result_v1.json",
           "bunta_timeout_progress.txt")
MARKER = "save-recovery-v2.json"
PENDING = ".pending-save-recovery"
WINDOWS_COPY = "Previous Windows profile"


def game_data(folder):
    folder = plain(folder.absolute())
    if folder.name.lower() == "userdata":
        folder = folder.parent
    info = plain(folder / "InitialDUnity_Data/app.info")
    if (not info.is_file() or info.stat().st_size > 256 or
        info.read_text(encoding="utf-8-sig").splitlines() not in
            (["Chris", "Initial D Unity"], ["Chris", "Initial D Arcade Stage 3 Reimagined"])):
        raise ValueError("Choose the previous Initial D game folder, or its UserData folder.")
    return plain(folder / "UserData")


def summary(data):
    cards = []
    names = set()
    for base in ROOTS:
        root = plain(data / base)
        for i in range(1, 6):
            f = plain(root / "saves" / f"slot_{i}" / "save.txt")
            if f.is_file():
                cards.append(f"{base}/slot_{i}")
        for name in SETTINGS:
            if plain(root / name).is_file():
                names.add(f"{base}/{name}")
    appearance = {}
    options = plain(data / ROOTS[0] / "game-options.json")
    if options.is_file():
        try:
            obj = json.loads(options.read_text("utf-8-sig"))
            appearance = {key: obj.get(key, 0) for key in ("hudMeterStyle", "hudOrnamentId")}
        except (ValueError, AttributeError):
            pass
    return {"cards": cards, "settings": sorted(names), "appearance": appearance}


def has_data(data):
    state = summary(data)
    return bool(state["cards"] or state["settings"] or any(
        plain(data / base / "driver_profiles_v1").is_dir() for base in ROOTS))


def candidates(root, windows):
    dest = root / "UserData"
    current = summary(dest)
    found = []
    sources = [windows, dest / WINDOWS_COPY]
    for sibling in root.parent.iterdir():
        if sibling == root:
            continue
        try:
            if (sibling / "InitialDUnity_Data/app.info").is_file():
                sources.append(game_data(sibling))
        except (OSError, ValueError):
            continue
    for source in dict.fromkeys(sources):
        if source == dest or not source.is_dir():
            continue
        try:
            state = summary(source)
            if has_data(source):
                missing = bool(set(state["cards"]) - set(current["cards"]) or
                               set(state["settings"]) - set(current["settings"]) or
                               any(v and not current["appearance"].get(k) for k, v in state["appearance"].items()))
                if not current["cards"] and any(plain(source / base / "driver_profiles_v1").is_dir() for base in ROOTS):
                    missing = True
                found.append(dict(path=str(source), missing=missing, **state))
        except (OSError, ValueError):
            continue
    return current, found


def entries(data, hud=None):
    # Each occupied slot is one unit: a different card must never inherit its cars.
    for base in ROOTS:
        root = plain(data / base)
        for i in range(1, 6):
            slot = root / "saves" / f"slot_{i}"
            if plain(slot / "save.txt").is_file():
                yield Path("UserData", base, "saves", slot.name), slot
        for name in ("driver_profiles_v1", "online_records_v1", "saves/.no-legacy-import"):
            f = plain(root / name)
            if f.exists():
                yield Path("UserData", base, name), f
        for name in SETTINGS + HISTORY:
            for suffix in ("", ".previous", ".bak"):
                f = plain(root / (name + suffix))
                if f.is_file():
                    yield Path("UserData", base, f.name), f
        for folder in ("replays", "ghosts"):
            directory = plain(root / folder)
            if directory.is_dir():
                for f in directory.rglob("*"):
                    if plain(f).is_file() and f.suffix.lower() in (".idreplay", ".idghost", ".csv", ".json"):
                        yield Path("UserData", base, folder) / f.relative_to(directory), f
    if hud and plain(hud).is_dir():
        for f in hud.rglob("*"):
            if plain(f).is_file() and f.suffix.lower() in (".png", ".json"):
                yield Path("Mods/HUD") / f.relative_to(hud), f


def fingerprint(path):
    if plain(path).is_file():
        return checksum(path)
    return {f.relative_to(path).as_posix(): checksum(f)
            for f in path.rglob("*") if plain(f).is_file() and f.suffix != ".lock"}


def commit(root):
    pending = plain(root / "UserData" / PENDING)
    if not pending.exists():
        return
    plan = json.loads(plain(pending / "plan.json").read_text("utf-8"))
    for raw in plan["paths"]:
        rel = Path(raw)
        if (rel.anchor or ".." in rel.parts or len(rel.parts) < 3 or
            rel.parts[:2] not in (("Mods", "HUD"), *(("UserData", r) for r in ROOTS))):
            raise ValueError("Invalid recovery path")
        staged = plain(pending / "new" / rel)
        live = plain(root / rel)
        before = plain(pending / "before" / rel)
        if staged.exists():
            live.parent.mkdir(exist_ok=True, parents=True)
            if live.exists():
                before.parent.mkdir(exist_ok=True, parents=True)
                live.rename(before)
            staged.rename(live)
        elif not live.exists():
            raise OSError(f"Recovery could not find {rel}.")
    backups = plain(root / "UserData/Save recovery backups")
    backups.mkdir(exist_ok=True)
    pending.rename(backups / uuid.uuid4().hex)


def restore(root, items):
    data = plain(root / "UserData")
    commit(root)
    with tempfile.TemporaryDirectory(dir=data, ignore_cleanup_errors=True) as scratch:
        work = Path(scratch)
        paths = []
        for rel, src in items:
            src = plain(src)
            dest = plain(root / rel)
            expected = fingerprint(src)
            if dest.exists() and fingerprint(dest) == expected:
                continue
            out = work / "new" / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            if src.is_dir():
                shutil.copytree(src, out, ignore=shutil.ignore_patterns("*.lock"))
            else:
                shutil.copy2(src, out)
            if fingerprint(out) != expected or fingerprint(src) != expected:
                raise OSError(f"Source changed while copying {rel}. Close the other game and retry.")
            paths.append(rel.as_posix())
        if paths:
            write_text(work / "plan.json", json.dumps({"paths": paths}))
            work.rename(plain(data / PENDING))
            commit(root)
    return len(paths)


def cosmetics(data):
    root = plain(data / ROOTS[0])
    now = plain(root / "game-options.json")
    old = plain(root / "game-options.json.previous")
    if not (now.is_file() and old.is_file()):
        return {}
    try:
        active = json.loads(now.read_text("utf-8-sig"))
        previous = json.loads(old.read_text("utf-8-sig"))
        return {key: previous[key] for key in ("hudMeterStyle", "hudOrnamentId")
                if active.get(key) == 0 and type(previous.get(key)) is int and previous[key] > 0}
    except (ValueError, UnicodeError, AttributeError):
        return {}


def run(args, root, windows):
    data = plain(root / "UserData")
    with locked(plain(data / "migration.lock")):
        commit(root)
        if args.action == "check":
            current, found = candidates(root, windows)
            print(json.dumps({"current": current, "sources": found, "appearance": cosmetics(data)}))
        elif args.action == "copy":
            source = plain(Path(args.source).absolute())
            if source not in (windows, data / WINDOWS_COPY):
                source = game_data(source)
            if source == data or source.is_relative_to(data) and source != data / WINDOWS_COPY:
                raise ValueError("Choose a different installation.")
            hud = source.parent / "Mods/HUD" if source not in (windows, data / WINDOWS_COPY) else None
            items = list(entries(source, hud))
            if not items:
                raise ValueError("No saved cards, settings or HUD files found in that folder.")
            print(f"Restored {restore(root, items)} changed items.")
        elif args.action == "appearance":
            updates = cosmetics(data)
            if updates:
                rel = Path("UserData", ROOTS[0], "game-options.json")
                options = json.loads((root / rel).read_text("utf-8-sig"))
                with tempfile.TemporaryDirectory(dir=data) as work:
                    file = Path(work) / rel.name
                    file.write_text(json.dumps(dict(options, **updates), indent=4), encoding="utf-8")
                    restore(root, [(rel, file)])
        elif args.action == "done":
            write_text(plain(data / MARKER), '{"version":2}\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("check", "copy", "appearance", "done"))
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    if args.action == "copy" and args.source is None:
        parser.error("Select a source to restore.")
    try:
        run(args, ROOT, local_low() / "Chris/Initial D Unity")
    except (OSError, ValueError) as error:
        sys.exit(str(error))
