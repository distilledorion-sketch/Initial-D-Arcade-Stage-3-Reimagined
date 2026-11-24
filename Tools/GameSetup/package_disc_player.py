"""Make a Windows game package from an isolated player and a closed list of tools."""
import argparse
import ast
import io
import json
import native_recipe
import disc_objects
import disc_billboards
import disc_extras
from disc_setup import PROGRAM, digest
from pathlib import Path
import re
import sys
import zipfile
import tempfile
from build_portable_launcher import LAUNCHER_NAME, build as build_launcher

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TOOLS = {"chdman.exe", "idas3_game_file_setup.exe", "fsca-table.h"}
PLAYER_CONTENT = {"D3D12", "InitialDUnity_Data", "MonoBleedingEdge", "dstorage.dll",
                  "dstoragecore.dll", "InitialDUnity.exe", "UnityPlayer.dll", "UnityCrashHandler64.exe"}

def converter_sources():
    lookup = {p.stem: p for p in (ROOT / "Native/tools").glob("*.py")}
    for name in ("Import-OriginalAura", "Import-ChallengerAssets", "Import-ChallengerGeometry"):
        lookup[name] = ROOT / "Tools" / (name + ".py")
    pending = [HERE / name for name in ("disc_audio.py", "disc_setup.py", "disc_objects.py")]
    selected = set(pending)
    while pending:
        source = pending.pop()
        for n in ast.walk(ast.parse(source.read_text(encoding="utf-8-sig"))):
            if isinstance(n, ast.Import):
                candidates = [a.name for a in n.names]
            elif isinstance(n, ast.ImportFrom):
                candidates = [n.module or ""]
            elif isinstance(n, ast.Constant) and isinstance(n.value, str):
                candidates = [n.value.removesuffix(".py")]
            else:
                continue
            for name in candidates:
                dependency = lookup.get(name)
                if dependency and dependency not in selected:
                    selected.add(dependency)
                    pending.append(dependency)
    return sorted(p for p in selected if p.parent != HERE)

def package(args):
    if args.output.exists():
        raise ValueError("Use a new output ZIP.")
    if args.python.name.lower() != "python313" or not (args.python / "python313.dll").is_file():
        raise ValueError("This package recipe requires 64-bit Windows CPython 3.13.")
    if not (args.player / "InitialDUnity.exe").is_file() or any(args.player.rglob("StreamingAssets")):
        raise ValueError("Supply the isolated disc player without game assets.")
    if {p.name for p in args.player.iterdir()} != PLAYER_CONTENT:
        raise ValueError("Unexpected files in the player build.")
    runtime = args.python
    dependencies = converter_sources()
    partial = args.output.with_suffix(".pending")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(partial, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        extra_members = disc_extras.write_content(z, args.extras_from, ROOT, args.music_from)
        if digest(args.program) != PROGRAM:
            raise ValueError("A verified developer-side program image is required to identify source data.")
        bridge = args.player / "InitialDUnity_Data/Plugins/x86_64/Idas3Unity.dll"
        template, recipe = native_recipe.build(bridge.read_bytes(), args.program.read_bytes())
        z.writestr("setup/native-player.template", template)
        z.writestr("setup/native-recipe.json", json.dumps(recipe))
        print(f"Removed {sum(s[2] for s in recipe['spans'])} original-program bytes from the distributed library.")
        with tempfile.TemporaryDirectory(dir=args.output.parent) as work:
            launcher = Path(work) / LAUNCHER_NAME
            build_launcher(launcher)
            z.write(launcher, "InitialDUnity.exe")  # Path accepted by already-released updaters.
        for p in sorted(args.player.rglob("*")):
            if p == bridge:
                continue
            if p.is_file():
                rel = p.relative_to(args.player).as_posix()
                z.write(p, "InitialDUnity.bin" if rel == "InitialDUnity.exe" else rel)
        sys.path.insert(0, str(ROOT / "Native/tools"))
        import export_original_course_objects
        layouts = disc_objects.layout_for_packaging(ROOT / "Native", args.program.read_bytes(), export_original_course_objects)
        z.writestr("setup/object-layout.json", json.dumps(layouts))
        reference = ROOT / "Native/data"
        patterns = ["original_audio/*/*.idms", "original_models/courses/*/*.idasobjects",
                    "original_models/courses/*/*.idasbillboards", "original_models/courses/*/*.idaslamps",
                    "original_assets/rival_dialog/dialog.idasdialog", "original_assets/rival_dialog/bunta.idasdialog"]
        checked = {p.relative_to(reference).as_posix():digest(p) for pattern in patterns for p in sorted(reference.glob(pattern))}
        if len(checked) != 203:
            raise ValueError("Incomplete reference set for the original courses, dialogue and scores.")
        z.writestr("setup/reference-checks.json", json.dumps(checked))
        recipes = disc_billboards.layout_for_packaging(ROOT / "Native", args.program.parent / "driveA/HOSTFS")
        z.writestr("setup/spectator-layout.json", json.dumps(recipes, separators=(",",":")))
        for name in ("disc_audio.py", "disc_setup.py", "native_recipe.py", "disc_objects.py", "disc_billboards.py", "disc_extras.py", "disc_courses.py", "disc_storage.py", "disc_portable.py", "skin_workshop.py", "Play.ps1"):
            z.write(HERE / name, "setup/" + name)
        for src in dependencies:
            z.write(src, "setup/converters/" + src.name)
        programs = set(re.findall(r'"((?:capture|reference)_\w+\.exe)"', (HERE / "disc_setup.py").read_text() + (HERE / "disc_objects.py").read_text()))
        for rel in ("native_assets/menu_font/font.bin", "localization/custom-menus/ja.bin",
                    "localization/custom-menus/zh-Hans.bin","localization/custom-menus/LICENSE.txt"):
            z.write(reference / rel, "setup/host/" + rel)
        for name in sorted(programs | TOOLS):
            z.write(args.helpers / name, "setup/tools/" + name)
        # The extraction helpers and the EOS runtime both import the Visual C++ runtime.
        for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
            z.write(args.vc_runtime / name, name)
            z.write(args.vc_runtime / name, "setup/tools/" + name)
        for name in ("python.exe", "python3.dll", "python313.dll", "vcruntime140.dll", "vcruntime140_1.dll", "LICENSE.txt"):
            z.write(runtime / name, "setup/python/" + name)
        for p in (runtime / "DLLs").iterdir():
            if p.suffix in (".dll", ".pyd") and not p.name.startswith(("_test", "_tk", "tcl", "tk8")):
                z.write(p, "setup/python/DLLs/" + p.name)
        library = io.BytesIO()
        with zipfile.ZipFile(library, "w", zipfile.ZIP_DEFLATED) as standard:
            for p in sorted((runtime / "Lib").rglob("*.py")):
                rel = p.relative_to(runtime / "Lib")
                if not set(rel.parts) & {"test", "tests", "site-packages", "ensurepip", "idlelib", "tkinter", "turtledemo"}:
                    standard.write(p, rel.as_posix())
        z.writestr("setup/python/python313.zip", library.getvalue(), compress_type=zipfile.ZIP_STORED)
        z.writestr("setup/python/python313._pth", "python313.zip\n.\nDLLs\n..\n../converters\n../vendor\n")
        for p in (args.vendor / "unicorn").rglob("*"):
            if p.is_file() and (p.suffix == ".py" or p.name == "unicorn.dll"):
                z.write(p, "setup/vendor/" + p.relative_to(args.vendor).as_posix())
        for name in ("COPYING", "COPYING.LGPL2", "COPYING_GLIB", "BINDINGS.txt"):
            z.write(args.unicorn_licenses / name, "setup/licenses/Unicorn/" + name)
        z.write(ROOT / "Native/vendor/minimp3/LICENSE", "setup/licenses/minimp3.txt")
        z.write(args.flycast_license, "setup/licenses/Flycast.txt")
        z.write(args.mame_license, "setup/licenses/MAME.txt")
        for p in sorted((ROOT / "Assets/Resources/Fonts").glob("*.txt")):
            z.write(p, "setup/licenses/" + p.name)
        z.write(HERE / "READ ME.txt", "READ ME.txt")
        for name, flag in (("Play Reimagined.cmd", ""), ("Verify Game Files.cmd", " -Verify")):
            contents = '@echo off\r\npowershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0setup\\Play.ps1"' + flag + '\r\nif errorlevel 1 (pause & exit /b 1)\r\n'
            if flag:
                contents += "pause\r\n"
            z.writestr(name, contents)
    with zipfile.ZipFile(partial) as z:
        paths = z.namelist()
        if len(paths) != len(set(paths)) or any(
            ("/streamingassets/" in p.lower() and p not in extra_members) or p.lower().endswith((".chd",".pic",".dtpk",".idms",".idasmesh",".idasasm",".cue",".gdi"))
            for p in paths
        ):
            raise RuntimeError("Unexpected asset in the player ZIP.")
        if z.testzip():
            raise RuntimeError("Package checksum failed.")
    partial.rename(args.output)
    print(f"Packaged {len(paths)} files including {len(dependencies)} converters: {args.output}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "helpers", "vendor", "flycast-license", "mame-license", "unicorn-licenses", "vc-runtime", "program", "output", "extras-from", "music-from"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--python", type=Path, default=Path(sys.base_prefix))
    parser.add_argument("--setup-output", type=Path, help="Also create the standalone installer from the finished, versioned Windows ZIP.")
    from make_update_package import assemble
    args = parser.parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as scratch:
        args.output = Path(scratch) / "disc-content.zip"
        package(args)
        assemble(args.output, args.player, output)
    if args.setup_output:
        from build_disc_setup import build
        args.setup_output.parent.mkdir(parents=True, exist_ok=True)
        build(output, args.setup_output.resolve())

