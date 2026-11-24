"""Prepare the same ROM-free Windows payload for standalone setup and the released updater."""
from pathlib import Path
import argparse
import hashlib
import json
from disc_extras import map_packs, ranked_files
import shutil
import tempfile
import zipfile

from build_portable_launcher import LAUNCHER_NAME, build

HERE = Path(__file__).resolve().parent
PREFIX = "InitialDUnity_Data/GameSetup/"
STREAMING = "InitialDUnity_Data/StreamingAssets/"


def assemble(base_package, player, output, program=None):
    if output.exists():
        raise FileExistsError(output)
    if (player / "InitialDUnity_Data/StreamingAssets").exists():
        raise ValueError("Use the isolated Windows player.")
    data = player / "InitialDUnity_Data"
    with (data / "data.unity3d").open("rb") as bundle:
        if bundle.read(8) != b"UnityFS\x00" or (data / "globalgamemanagers").stat().st_size:
            raise ValueError("Use the compressed player build with update compatibility files.")
    original_plugin = data / "Plugins/x86_64/Idas3Unity.dll"
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as work, zipfile.ZipFile(base_package) as old:
        work = Path(work)
        temp = work / "package.zip"
        launcher = work / LAUNCHER_NAME
        build(launcher)
        # Older packages kept the setup tools in a different directory.
        recipes = [name for name in old.namelist() if name.endswith("/native-recipe.json")]
        if len(recipes) != 1:
            raise ValueError("Expected one native repair recipe in the base package.")
        old_setup = recipes[0][:-len("native-recipe.json")]
        recipe = json.loads(old.read(old_setup + "native-recipe.json"))
        library = original_plugin.read_bytes()
        template = None
        if hashlib.sha256(library).hexdigest() != recipe["library"]:
            from disc_setup import PROGRAM
            from native_recipe import build as native_recipe
            if program is None or hashlib.sha256(program.read_bytes()).hexdigest() != PROGRAM:
                raise ValueError("Supply the verified original program to rebuild the native disc recipe.")
            template, recipe = native_recipe(library, program.read_bytes())
        extra = json.loads(old.read(old_setup + "additional-content.json"))
        extra.update(ranked_files(HERE.parents[1]))
        packs = map_packs(extra)
        ghost = {}
        for name in ("Mesh.json", "Shadow.png", "Body.png"):
            source = HERE.parents[1] / "Assets/Resources/TimeAttackGhost" / (name if name.endswith(".json") else name + ".bytes")
            relative = "TimeAttackGhost/" + name
            ghost[relative] = source
            extra[relative] = [source.stat().st_size, hashlib.sha256(source.read_bytes()).hexdigest()]
        with zipfile.ZipFile(temp, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as target:
            for info in old.infolist():
                name = info.filename
                if name.startswith(old_setup):
                    relative = name[len(old_setup):]
                    if relative in ("save_recovery.py", "Recover-PlayerData.ps1", "additional-content.json", "package-version.txt", "windows-56-files.json.gz", "runtime/InitialDUnity.bin", "skin_workshop.py"):
                        continue
                    if template is not None and relative in ("native-player.template", "native-recipe.json"):
                        continue
                    dest = PREFIX + relative
                    if relative.startswith("course-packs/"):
                        continue
                    if relative in ("converters/extract_original_menus.py", "converters/texture_bank.py"):
                        target.write(HERE.parents[1] / "Native/tools" / Path(relative).name, dest)
                        continue
                    local = HERE / relative
                    if local.parent == HERE and local.is_file() and local.suffix in (".py", ".ps1"):
                        target.write(local, dest)
                        continue
                elif name.startswith(STREAMING) and name[len(STREAMING):] in extra and not name.startswith(STREAMING + "IDAS3/"):
                    if name[len(STREAMING):].partition("/")[0] in packs or name[len(STREAMING):] in ghost:
                        continue
                    dest = name
                else:
                    continue
                packed = zipfile.ZipInfo(dest, info.date_time)
                packed.compress_type = info.compress_type
                with old.open(info) as source, target.open(packed, "w") as stream:
                    shutil.copyfileobj(source, stream, 1024 * 1024)
            if template is not None:
                target.writestr(PREFIX + "native-player.template", template)
                target.writestr(PREFIX + "native-recipe.json", json.dumps(recipe))
            for relative, source in ghost.items():
                target.write(source, STREAMING + relative)
            target.writestr(PREFIX + "additional-content.json", json.dumps(extra, sort_keys=True))
            for path in sorted(player.rglob("*")):
                if not path.is_file() or path == original_plugin:
                    continue
                name = path.relative_to(player).as_posix()
                target.write(path, PREFIX + "runtime/InitialDUnity.bin" if name == "InitialDUnity.exe" else name)
            target.write(HERE / "windows-56-files.json.gz", PREFIX + "windows-56-files.json.gz")
            target.write(launcher, "InitialDUnity.exe")  # Path accepted by already-released updaters.
            target.write(HERE / "package-version.txt", PREFIX + "package-version.txt")
            for name in ("skin_workshop.py", "save_recovery.py", "Recover-PlayerData.ps1"):
                target.write(HERE / name, PREFIX + name)
            target.write(HERE / "READ ME.txt", "READ ME.txt")
            target.write(HERE / "Replay Viewer.cmd", "Replay Viewer.cmd")
        with zipfile.ZipFile(temp) as finished:
            names = finished.namelist()
            excluded = (".chd", ".pic", ".cue", ".gdi", ".dtpk", ".idms", ".idasmesh", ".idasasm", "/idas3unity.dll")
            if any(name.lower().endswith(excluded) or "initdv3e.zip" in name.lower() for name in names):
                raise ValueError("Original arcade content in update package.")
            if len(names) != len({name.lower() for name in names}) or finished.testzip():
                raise ValueError("The completed update failed verification.")
        temp.rename(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_package", type=Path)
    parser.add_argument("player", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--program", type=Path)
    args = parser.parse_args()
    assemble(args.base_package, args.player, args.output, args.program)
