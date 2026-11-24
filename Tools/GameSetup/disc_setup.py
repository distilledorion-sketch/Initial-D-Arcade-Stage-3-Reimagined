"""Build a private Reimagined asset cache from the owner's GDS-0033 disc set."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile
import disc_extras

HERE = Path(__file__).resolve().parent
CHD = ("9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e", 236940011)
PIC = ("6e959bb221d99a8e2d4d3490964b190fccaa07ec99cbb6539dcd929720623a9a", 17152)
PROGRAM = "efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335"
HELPERS = {
    "chdman.exe": "8a74468e3b0879698835b57c3b58e88e5a51e4de73bee6ef755c28530b5b040f",
    "idas3_game_file_setup.exe": "a6281d8a2625205c2cdf963950b55372b0f19164839574a3ce92b454cb6bc08b",
}
VERSION = 2

def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()

def check_source(path, signature, label):
    expected, size = signature
    if not path.is_file() or path.stat().st_size != size or digest(path) != expected:
        raise ValueError(f"Select the matching original {label}.")

def read_pic(source, scratch):
    if source.suffix.lower() != ".zip":
        check_source(source, PIC, "317-0384-COM security PIC")
        return source
    with zipfile.ZipFile(source) as archive:
        files = [entry for entry in archive.infolist()
                 if entry.filename.rsplit("/", 1)[-1].lower() == "317-0384-com.pic"]
        if len(files) != 1 or files[0].file_size != PIC[1]:
            raise ValueError("The selected archive must contain one matching 317-0384-COM.pic.")
        pic = scratch / "security.pic"
        pic.write_bytes(archive.read(files[0]))
    check_source(pic, PIC, "317-0384-COM security PIC")
    return pic

def run(command, log):
    print(f"  {Path(command[0]).stem} {Path(command[1]).stem if len(command)>1 else ''}", flush=True)
    result = subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise ValueError(f"{Path(command[0]).name} stopped ({result.returncode}). See {log.name}.")

def inventory(data):
    return {file.relative_to(data).as_posix(): digest(file)
            for file in sorted(data.rglob("*")) if file.is_file()}


def save_receipt(target, receipt):
    with tempfile.NamedTemporaryFile(dir=target, mode="w", encoding="utf-8", delete=False) as updated:
        json.dump(receipt, updated, indent=2)
    try:
        os.replace(updated.name, target / "disc-receipt.json")
    finally:
        Path(updated.name).unlink(missing_ok=True)

def valid_cache(target):
    try:
        receipt = json.loads((target / "disc-receipt.json").read_text(encoding="utf-8"))
        if receipt["version"] != VERSION or receipt["source"] != CHD[0] or not receipt["files"]:
            return False
        return inventory(target / "data") == receipt["files"]
    except (OSError, ValueError, KeyError, TypeError):
        return False

def generate(game, project, converters, helpers, log, previous_data=None):
    image = game / "idas3_main_0C020000.bin"
    host = game / "driveA/HOSTFS"
    if digest(image) != PROGRAM or not (host / "path/k_df_path.bin").is_file():
        raise ValueError("The original program or course files failed verification.")
    data = project / "data"
    art = data / "original_assets"
    def convert(name, *args):
        run([sys.executable, converters / (name + ".py"), *args], log)
    convert("export_original_physics_data", image, host, data / "original_physics")
    convert("export_original_fsca_table", helpers / "fsca-table.h", data / "original_physics")
    convert("export_original_rival_data", image, host, data / "original_rival")
    convert("export_original_tuning", image, data / "original_tuning")
    convert("import_courses", "--source", host, "--out", data / "courses")
    for name in ("import_original_audio", "import_original_audio_dsp", "import_original_selection_music"):
        convert(name, "--hostfs", host, "--project", project)
    for name in ("import_original_engine_audio", "import_original_menu_audio", "import_original_race_audio"):
        convert(name, "--hostfs", host, "--image", image, "--project", project)
    for name, out in (("extract_original_hud", "hud"), ("extract_original_results", "results"),
                      ("export_original_navigation", "navigation"), ("extract_original_ending", "ending")):
        convert(name, "--hostfs", host, "--out", art / out)
    convert("extract_original_loading", "--source", host / "model/loading", "--image", image, "--out", art / "loading")
    convert("extract_original_saved_data", game, art / "saved_data")
    convert("extract_original_water_cup", "--hostfs", host, "--out", art / "hud/cupwater")
    convert("extract_original_battle_portraits", "--image", image, "--source", host / "model/face", "--out", art / "hud/faces")
    convert("extract_original_battle_names", "--image", image, "--source", host / "font/gamekana", "--out", art / "hud/names")
    convert("extract_original_menus", "--source", host / "sprite", "--layout", "twiddled", "--out", art / "menus")
    convert("extract_original_menus", "--source", host / "model", "--kind", "model", "--layout", "twiddled",
            "--original-image", image, "--out", art / "menus/v3")
    convert("extract_original_menus", "--source", host / "model", "--kind", "model", "--layout", "twiddled",
            "--bank", "mode_bunta", "--bank", "select0402", "--out", art / "menus/warnings")
    convert("extract_original_session_exit", game, art / "session_exit")
    (project / "src").mkdir(exist_ok=True)
    convert("extract_original_cars", "--image", image, "--hostfs", host, "--project", project,
            "--capture-exe", helpers / "capture_original_assembly.exe")
    convert("import_original_number_plate", "--hostfs", host, "--image", image, "--project", project)
    convert("export_original_car_appearances", "--root", project, "--image", image, "--hostfs", host,
            "--capture-exe", helpers / "capture_car_presentation.exe")
    scratch = game.parent / "car-capture"
    convert("export_original_car_materials", "--root", project, "--image", image, "--hostfs", host,
            "--capture-exe", helpers / "capture_original_car_materials.exe", "--work", scratch)
    convert("export_original_car_colors", "--root", project, "--image", image, "--hostfs", host,
            "--material-exe", helpers / "capture_original_car_materials.exe",
            "--geometry-exe", helpers / "capture_car_presentation.exe", "--work", scratch)
    convert("export_car_geometry_v2", "--root", project, "--image", image, "--hostfs", host,
            "--capture-exe", helpers / "capture_car_presentation.exe")
    convert("export_car_assembly_parts", "--root", project, "--hostfs", host)
    convert("export_car_material_layout", "--root", project, "--hostfs", host, "--image", image)
    convert("extract_course_roster", "--image", image, "--hostfs", host, "--project", project,
            "--courses", "0,1,2,3,4,5,6,7,8", "--weather", "0,1")
    convert("export_course_selections", "--image", image, "--project", project,
            "--capture-exe", helpers / "capture_course_roster.exe", "--courses", "0,1,2,3,4,5,6,7,8")
    convert("export_original_course_crows", "--image", image, "--hostfs", host, "--project", project)
    from disc_objects import generate as generate_objects
    generate_objects(game, project, helpers, log)
    from disc_billboards import generate as generate_billboards
    generate_billboards(host, project, json.loads((HERE / "spectator-layout.json").read_text()))
    complete_frontend(game, project, converters, log)
    complete_driving(game, project, converters, helpers, log)
    convert("export_original_headlight_projection", image, host, project)
    convert("extract_original_time_attack_visit", "--hostfs", host, "--out", art / "time_attack")
    convert("export_original_time_attack_analysis", image, "--native", project, "--proof", project / "verification/time_attack_analysis.json")
    convert("export_original_bunta_dialog", image, host, project)
    gains = data / "original_course_lighting/gain"
    gains.mkdir(parents=True, exist_ok=True)
    for stream in (host / "path").glob("*_sdw.bin"):
        shutil.copyfile(stream, gains / stream.name)
    convert("export_original_demo", image, host, project, "--capture-exe", helpers / "capture_car_presentation.exe")
    convert("export_original_demo_overlays", image, host, project)
    run([helpers / "capture_original_demo_camera.exe", image, project, art / "attract/demo"], log)
    demo = data / "original_models/attract/demo"
    demo.mkdir(parents=True, exist_ok=True)
    run([helpers / "capture_original_demo_course.exe", image, "20", "0", "4089", "31", demo / "course_capture.json"], log)
    convert("export_original_demo_course", image, host, project)
    run([helpers / "capture_original_ranking_records.exe", image, art / "attract/ranking/factory_records.idasrank"], log)
    from disc_audio import generate as generate_audio
    reference = json.loads((HERE / "reference-checks.json").read_text())
    generate_audio(game, project, converters, previous=previous_data, reference=reference)
    audio = data / "original_audio"
    shutil.copyfile(audio / "selection/builtin_samples.bin", audio / "music/builtin_samples.bin")
    shutil.copytree(HERE / "host", data, dirs_exist_ok=True)
    shutil.rmtree(project / "src")
    if (project / "verification").is_dir():
        shutil.rmtree(project / "verification")


def complete_driving(game, project, converters, helpers, log):
    image = game / "idas3_main_0C020000.bin"
    host = game / "driveA/HOSTFS"
    art = project / "data/original_assets"
    def convert(name, *args):
        run([sys.executable, converters / (name + ".py"), *args], log)
    convert("export_original_lamp_trails", "--hostfs", host, "--image", image, "--out", art / "effects/taillight")
    convert("export_original_smoke_grids", "--hostfs", host, "--out", art / "effects/smoke")
    convert("export_driving_effect_runtime", "--hostfs", host, "--out", art / "effects/smoke")
    convert("extract_original_driving_effects", "--hostfs", host, "--out", art / "weather", "--banks", "rainmark")
    convert("export_original_headlights", "--root", project, "--image", image, "--hostfs", host,
            "--capture-exe", helpers / "capture_car_presentation.exe")
    convert("export_original_tuning_preview", "--image", image, "--hostfs", host, "--project", project)
    convert("export_original_tuning_course_presentation", image, host, project)
    complete_stock_scenes(game, project, converters, helpers, log)


def complete_stock_scenes(game, project, converters, helpers, log):
    image = game / "idas3_main_0C020000.bin"
    host = game / "driveA/HOSTFS"
    art = project / "data/original_assets"
    def convert(name, *args):
        run([sys.executable, converters / (name + ".py"), *args], log)
    convert("extract_original_hud", "--hostfs", host, "--out", art / "weather", "--rain-only")
    convert("extract_original_showroom_details", host, image, project)
    convert("export_original_course_lamps", "--hostfs", host, "--image", image, "--project", project,
            "--capture-exe", helpers / "capture_course_roster.exe")
    run([helpers / "capture_original_rival_dialog_data.exe", image, project], log)
    convert("export_original_rival_dialog", image, host, project)
    finish_stock_menus(game, project, converters, helpers, log)


def finish_stock_menus(game, project, converters, helpers, log):
    from extract_original_menus import export_model_menu
    from extract_original_hud import export as export_hud
    host = game / "driveA/HOSTFS"
    image = game / "idas3_main_0C020000.bin"
    art = project / "data/original_assets"
    export_hud(host, art / "weather", [("effect/rainmark","rainmark")])
    for bank, dest in (("v3sT13rankin","results"), ("v3sT14password","results"), ("v3sS08name","model_menus")):
        export_model_menu(host / "model" / bank, art / dest / bank, "twiddled")
    run([sys.executable, converters / "export_car_presentation.py", "--root", project, "--image", image,
         "--hostfs", host, "--capture-exe", helpers / "capture_car_presentation.exe"], log)
    run([helpers / "reference_original_tuning_course_packages.exe", image, project], log)
    run([helpers / "reference_original_tuning_course_pulse.exe", image, art / "tuning_course/pulse.bin"], log)


def complete_frontend(game, project, converters, log):
    image = game / "idas3_main_0C020000.bin"
    host = game / "driveA/HOSTFS"
    data = project / "data"
    art = data / "original_assets"
    def convert(name, *args):
        run([sys.executable, converters / (name + ".py"), *args], log)
    convert("import_original_name_entry", "--image", image, "--project", project)
    convert("extract_original_start_names", "--image", image, "--source", host / "font/namekana",
            "--profile", data / "original_frontend/name_entry.bin", "--out", art / "hud/start_names")
    convert("extract_original_gasstand", "--image", image, "--hostfs", host, "--project", project)
    convert("extract_original_conquer", "--source", host / "model/conquer", "--out", art / "conquer")
    convert("extract_original_ranking_attract", image, host, project)
    convert("export_original_attract_cards", image, host, project)
    convert("export_original_name_entry_presentation", image, host, project)
    convert("export_original_tuning_ui", image, host, art / "tuning")
    convert("extract_original_driving_effects", "--hostfs", host, "--out", art / "effects", "--banks", "bkfire", "smoke")
    convert("extract_original_headlight_glare", "--hostfs", host, "--image", image, "--out", art / "effects/hilight")
    convert("extract_original_environment_effects", "--hostfs", host, "--image", image, "--out", art / "environment")
    convert("Import-OriginalAura", "--source-files", game, "--out", art / "aura")
    challenger = data / "unity/Challenger"
    convert("Import-ChallengerAssets", "--hostfs", host, "--out", challenger, "--audio-root", data / "original_audio/race")
    convert("Import-ChallengerGeometry", "--hostfs", host, "--out", challenger)


def extract_original(chd, security, temp, helpers, log):
    check_source(chd, CHD, "Arcade Stage 3 GDS-0033 CHD")
    for name, expected in HELPERS.items():
        if digest(helpers / name) != expected:
            raise ValueError(f"The extraction tool {name} does not match the tested version.")
    pic = read_pic(security, temp)
    private = temp / "original"
    private.mkdir()
    run([helpers / "chdman.exe", "extractcd", "-i", chd, "-o", temp / "source.cue",
         "-ob", temp / "track%t.bin", "-sb"], log)
    run([helpers / "idas3_game_file_setup.exe", "--bin", temp / "track3.bin",
         "--pic", pic, "--output", private], log)
    if digest(private / "idas3_main_0C020000.bin") != PROGRAM:
        raise ValueError("The extracted game program failed verification.")
    return private


def prepare_disc_inputs(chd, security, target):
    if target.parts[-3:] != ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
        return chd, security, str(chd)
    from disc_portable import import_original, LOCAL_DISC
    root = target.parents[2]
    import_original(chd, security, root)
    return root / LOCAL_DISC, root / "rom/317-0384-com.pic", LOCAL_DISC


def recorded_disc(folder):
    try:
        receipt = json.loads((folder / "disc-receipt.json").read_text(encoding="utf-8"))
        if receipt.get("source") == CHD[0] and isinstance(receipt.get("files"), dict):
            return receipt
    except (OSError, ValueError, AttributeError):
        pass
    return None


def matches_conversion(receipt):
    if not receipt or receipt.get("version") != VERSION or "disc-player/Idas3Unity.dll" not in receipt["files"]:
        return False
    for bank in ("v3sV00wait", "v3sV10boost", "v3sV10loadwait", "v3sS01card", "select01", "selvswait", "v3sV01challenge", "selreceive"):
        prefix = f"original_assets/menus/v3/{bank}"
        if any(f"{prefix}/{name}" not in receipt["files"] for name in (bank + ".idasmesh", "textures/textures.idastex")):
            return False
    reference = json.loads((HERE / "reference-checks.json").read_text())
    return all(receipt["files"].get(name) == expected for name, expected in reference.items())


def windows_inventory():
    with gzip.open(HERE / "windows-56-files.json.gz", "rt", encoding="utf-8") as file:
        return json.load(file)


def is_windows_cache(folder):
    data = folder / "data"
    if not data.is_dir():
        return False
    manifest = folder / "data.manifest.json"
    if manifest.is_file():
        try:
            if manifest.stat().st_size < 32 * 1024 * 1024 and json.loads(manifest.read_text(encoding="utf-8-sig")).get("schema") == "idas3-unity-runtime-data-v1":
                return True
        except (OSError, ValueError, AttributeError):
            pass
    # The public .56 ZIP omitted the development manifest. Check actual stock files.
    from disc_portable import plain
    files = windows_inventory()
    for name in ("original_tuning/tables.idastune", "original_rival/tables.bin",
                 "authored_extensions/title_widescreen_v1.manifest.json"):
        path = plain(data / name)
        size, expected = files[name]
        if path.is_file() and path.stat().st_size == size and digest(path) == expected:
            return True
    return False


def install(chd, security, target, helpers, converters):
    chd, security, source_reference = prepare_disc_inputs(chd, security, target)
    receipt = recorded_disc(target)
    if matches_conversion(receipt) and valid_cache(target):
        from disc_storage import trim_conversion_files
        if trim_conversion_files(target / "data", receipt["files"]):
            save_receipt(target, receipt)
        print(f"Verified existing disc assets in {target}.")
        return
    previous = target.parents[2] / "UserData/.disc-cache-upgrade" if target.parts[-3:] == ("InitialDUnity_Data", "StreamingAssets", "IDAS3") else None
    if previous:
        from disc_portable import plain
        plain(previous)
        plain(target)
        # If Windows stopped between the two renames, restore the previous tree before retrying.
        if not target.exists() and previous.is_dir() and (is_windows_cache(previous) or recorded_disc(previous)):
            previous.rename(target)
    upgrade = previous is not None and target.is_dir() and not previous.exists() and (is_windows_cache(target) or recorded_disc(target))
    if target.exists() and not upgrade:
        raise ValueError(f"The destination already exists with incomplete or different contents. Choose a new directory: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".idas3-source-", dir=target.parent) as work:
        temp = Path(work)
        project = temp / "installation"
        project.mkdir()
        # Keep the last log available after a conversion failure. Each install owns its log.
        with tempfile.NamedTemporaryFile(prefix="idas3-disc-", suffix=".log", dir=target.parent, delete=False) as log:
            print(f"Installation log: {log.name}", flush=True)
            private = extract_original(chd, security, temp, helpers, log)
            from native_recipe import restore
            restore(private / "idas3_main_0C020000.bin", HERE, project / "data/disc-player/Idas3Unity.dll")
            generate(private, project, converters, helpers, log, target / "data" if upgrade else None)
        disc_extras.install_cache_files(project, HERE, disc_extras.manifest(HERE), {})
        files = inventory(project / "data")
        from disc_storage import trim_conversion_files
        trim_conversion_files(project / "data", files)
        proof = json.loads((HERE / "reference-checks.json").read_text())
        for name, expected in proof.items():
            if files.get(name) != expected:
                raise ValueError(f"Disc conversion failed the reference check for {name}.")
        save_receipt(project, {"version": VERSION, "source": CHD[0], "files": files})
        (project / "disc-source.txt").write_text(source_reference, encoding="utf-8")
        if upgrade:
            previous.parent.mkdir(exist_ok=True)
            target.rename(previous)
        try:
            project.rename(target)
        except BaseException:
            if upgrade and not target.exists():
                previous.rename(target)
            raise
        print(f"Installed {len(files)} verified files into {target}", flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chd", type=Path)
    parser.add_argument("--security", type=Path, help="Original 317-0384-COM.pic or initdv3e.zip")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--helpers", type=Path, default=HERE / "tools")
    parser.add_argument("--converters", type=Path, default=HERE / "converters")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--activate", action="store_true", help="Install the disc-restored Windows player library.")
    parser.add_argument("--relink", action="store_true", help="Reconnect a verified cache to its moved source disc.")
    args = parser.parse_args()
    if args.activate:
        target = args.output.resolve()
        if target.parts[-3:] != ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
            raise ValueError("Select the disc player's installation cache.")
        source = target / "data/disc-player/Idas3Unity.dll"
        destination = target.parents[1] / "Plugins/x86_64/Idas3Unity.dll"
        expected = json.loads((HERE / "native-recipe.json").read_text())["library"]
        # An update may retain a cache generated by the previous disc player.
        # Publish each file atomically and make an interrupted upgrade resumable.
        receipt_path = target / "disc-receipt.json"
        receipt = recorded_disc(target)
        if not matches_conversion(receipt):
            sys.exit(4)  # An older extraction recipe needs the full conversion, even when its own receipt is intact.
        key = "disc-player/Idas3Unity.dll"
        previous_stamp = receipt_path.stat().st_mtime_ns
        recorded = receipt["files"][key]
        if recorded != expected:
            actual = digest(source)
            if actual not in (recorded, expected) or receipt.get("source") != CHD[0] or receipt.get("version") != VERSION:
                raise ValueError("The installed disc player does not match its verified receipt.")
            with tempfile.TemporaryDirectory(dir=target.parent, prefix=".idas3-upgrade-") as work:
                scratch = Path(work)
                if actual != expected:
                    if not args.chd or not args.security:
                        sys.exit(3)  # Launcher requests the original security file for this update.
                    from native_recipe import restore
                    with (scratch / "extraction.log").open("w") as log:
                        private = extract_original(args.chd, args.security, scratch, args.helpers, log)
                    rebuilt = scratch / "Idas3Unity.dll"
                    restore(private / "idas3_main_0C020000.bin", HERE, rebuilt)
                    os.replace(rebuilt, source)
                receipt["files"][key] = expected
                updated = scratch / "receipt.json"
                updated.write_text(json.dumps(receipt, indent=2), encoding="utf-8")
                os.replace(updated, receipt_path)
        from disc_courses import restore_pending
        from disc_storage import compact_install, trim_conversion_files
        extra_files = disc_extras.manifest(HERE)
        restored = restore_pending(target.parent, HERE, extra_files)
        from skin_workshop import install_shortcut
        install_shortcut(target.parents[2])
        added = disc_extras.install_cache_files(target, HERE, extra_files, receipt["files"])
        trimmed = trim_conversion_files(target / "data", receipt["files"])
        if added or trimmed:
            save_receipt(target, receipt)
        try:
            compact_install(target, HERE, extra_files, receipt["files"], restored,
                            unchanged_storage_from=previous_stamp if (trimmed or recorded != expected) and not added else None)
        except OSError as error:
            print(f"Game files are ready. Windows skipped disk compression: {error}", flush=True)
        if destination.is_file() and digest(destination) == expected:
            return
        if digest(source) != expected:
            raise ValueError("The reconstructed game library failed verification.")
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as stage:
            with source.open("rb") as original:
                shutil.copyfileobj(original, stage)
        try:
            os.replace(stage.name, destination)
        finally:
            Path(stage.name).unlink(missing_ok=True)
        return
    if args.relink:
        if not args.chd or not valid_cache(args.output):
            raise ValueError("A complete cache and the original disc are needed to reconnect.")
        check_source(args.chd, CHD, "Arcade Stage 3 GDS-0033 CHD")
        _, _, source_reference = prepare_disc_inputs(args.chd.resolve(), None, args.output.resolve())
        with tempfile.NamedTemporaryFile(dir=args.output, delete=False, mode="w", encoding="utf-8") as replacement:
            replacement.write(source_reference)
        os.replace(replacement.name, args.output / "disc-source.txt")
        return
    if args.verify:
        if not valid_cache(args.output):
            raise ValueError("The extracted asset cache is missing, incomplete or changed.")
        files = disc_extras.manifest(HERE)
        receipt = json.loads((args.output / "disc-receipt.json").read_text())["files"]
        for name, (_, expected) in files.items():
            if name.startswith(disc_extras.CACHE) and receipt.get(name[len(disc_extras.CACHE):]) != expected:
                raise ValueError(f"Run Play Reimagined to install {name}.")
        disc_extras.verify_external(args.output.parent, files)
        print("Verified disc assets, music and installed optional tracks.")
        return
    if not args.chd or not args.security:
        parser.error("Select both your GDS-0033 disc image and its security PIC or archive.")
    install(args.chd.resolve(), args.security.resolve(), args.output.resolve(),
            args.helpers.resolve(), args.converters.resolve())

if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        sys.exit(str(exc))

