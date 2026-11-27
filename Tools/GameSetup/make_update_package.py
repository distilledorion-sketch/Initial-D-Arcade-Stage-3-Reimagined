"""Prepare the same ROM-free Windows payload for standalone setup and the released updater."""
from pathlib import Path
import argparse
import hashlib
import json
from disc_extras import CACHE, WIDESCREEN_TITLE, digest, map_packs, ranked_files
import shutil
import tempfile
import zipfile

from build_portable_launcher import LAUNCHER_NAME, build

HERE = Path(__file__).resolve().parent
PREFIX = "InitialDUnity_Data/GameSetup/"
STREAMING = "InitialDUnity_Data/StreamingAssets/"


def assemble(base_package, player, output, program=None, japanese_plugin=None, japanese_program=None):
    from package_disc_player import converter_sources, write_japanese_base
    converters = {"converters/" + path.name: path for path in converter_sources()}
    helpers = {name + ".exe" for name in (
        "capture_original_assembly", "capture_car_presentation", "capture_original_car_materials",
        "capture_course_roster", "capture_course_object_selections", "capture_original_demo_course",
        "capture_original_ranking_records", "capture_original_rival_dialog_data",
        "reference_original_tuning_course_pulse", "reference_original_tuning_course_packages",
        "capture_original_demo_camera")}
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
        recipes = [name for name in ("setup/native-recipe.json", PREFIX+"native-recipe.json") if name in old.namelist()]
        if len(recipes) != 1:
            raise ValueError("Expected one native repair recipe in the base package.")
        old_setup = recipes[0][:-len("native-recipe.json")]
        recipe = json.loads(old.read(old_setup + "native-recipe.json"))
        layouts = json.loads((HERE / "native-base-spans.json").read_text())
        if old_setup + "native-base-spans.json" in old.namelist():
            layouts.update(json.loads(old.read(old_setup + "native-base-spans.json")))
        layouts[recipe["library"]] = recipe["spans"]
        library = original_plugin.read_bytes()
        regional = old_setup + "regions/gds-0032c/"
        previous_japan = regional+"native-recipe.json" in old.namelist()
        if (japanese_plugin is None) != (japanese_program is None) or (japanese_plugin is not None and program is None):
            raise ValueError("Rebuilding the Japanese base requires its player library and both original programs.")
        if not previous_japan and japanese_plugin is None:
            raise ValueError("This older package has no Japanese base. Build the Rev C player and supply both regional inputs.")
        template = None
        if hashlib.sha256(library).hexdigest() != recipe["library"]:
            from disc_setup import PROGRAM
            from native_recipe import build as native_recipe
            if program is None or hashlib.sha256(program.read_bytes()).hexdigest() != PROGRAM:
                raise ValueError("Supply the verified original program to rebuild the native disc recipe.")
            template, recipe = native_recipe(library, program.read_bytes())
            if previous_japan and japanese_plugin is None:
                raise ValueError("Refresh the Japanese native player alongside the English player.")
        extra = json.loads(old.read(old_setup + "additional-content.json"))
        extra.update(ranked_files(HERE.parents[1]))
        title = HERE.parents[1] / "Native/data" / WIDESCREEN_TITLE
        title_member = "additional-data/" + WIDESCREEN_TITLE
        extra[CACHE + WIDESCREEN_TITLE] = [title.stat().st_size, digest(title)]
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
                    if japanese_plugin is not None and relative.startswith("regions/gds-0032c/"):
                        continue
                    if relative in converters or relative in ("native-base-spans.json", "disc_language.py", "disc_japan.py", "japanese-artwork-checks.json"):
                        continue
                    if relative in (title_member, "save_recovery.py", "Recover-PlayerData.ps1", "additional-content.json", "package-version.txt", "windows-56-files.json.gz", "runtime/InitialDUnity.bin", "skin_workshop.py"):
                        continue
                    if template is not None and relative in ("native-player.template", "native-recipe.json"):
                        continue
                    dest = PREFIX + relative
                    if relative.startswith("course-packs/"):
                        continue
                    if relative.startswith("tools/") and relative[6:] in helpers:
                        target.write(HERE.parents[1] / "Native/bin" / relative[6:], dest)
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
            target.writestr(PREFIX + "native-base-spans.json", json.dumps(layouts, separators=(",", ":")))
            if japanese_plugin is not None:
                previous_layouts = {}
                if previous_japan:
                    previous_layouts.update(json.loads(old.read(regional+"native-base-spans.json")))
                    previous_recipe = json.loads(old.read(regional+"native-recipe.json"))
                    previous_layouts[previous_recipe["library"]] = previous_recipe["spans"]
                write_japanese_base(target, PREFIX, japanese_plugin, japanese_program, program, previous_layouts)
            from disc_language import reference
            project = HERE.parents[1] / "Native"
            target.writestr(PREFIX + "japanese-artwork-checks.json", json.dumps(reference(project / "data/localization/ja")))
            target.write(HERE / "disc_language.py", PREFIX + "disc_language.py")
            target.write(HERE / "disc_japan.py", PREFIX + "disc_japan.py")
            for relative, source in sorted(converters.items()):
                target.write(source, PREFIX + relative)
            if template is not None:
                target.writestr(PREFIX + "native-player.template", template)
                target.writestr(PREFIX + "native-recipe.json", json.dumps(recipe))
            target.write(title, PREFIX + title_member)
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
            if any(name.lower().endswith(excluded) or Path(name).name.lower().startswith("initdv3") and name.lower().endswith(".zip") for name in names):
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
    parser.add_argument("--japanese-program", type=Path)
    parser.add_argument("--japanese-plugin", type=Path)
    args = parser.parse_args()
    assemble(args.base_package, args.player, args.output, args.program, args.japanese_plugin, args.japanese_program)
