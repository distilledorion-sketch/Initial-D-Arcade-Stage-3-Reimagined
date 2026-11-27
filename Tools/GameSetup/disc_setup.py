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
JAPANESE_PIC = ("adb4bdcd65159dc9ec2b08eb21c9c832ef019af54b64017c203c2b1bc9cc3250", 16384)
JAPANESE_CHDS = {
    "cd23e6277a59be0e340b80b82d957488b23f7c2f136454d5853a1b7bae179a1f",
    "ab83771a8f19cc1fbe69b361c1be47216217a1a9c65b233cb117b451a1852d54",
}
# CHDv5 combined SHA-1 values from MAME's initdv3e and initdv3j disc records.
# The header selects the converter. Verification still checks the actual data.
DISC_REGIONS = {
    "98fb1bd119fc33ef14fcaba3eb2347836469a75b": "gds-0033",
    "d92bca7c8a7920c99b23710f5bdbeed1fbec12d2": "gds-0032c",
}
SECURITY_NAMES = {"gds-0033": "317-0384-com.pic", "gds-0032c": "317-0379-jpn.pic"}
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

def disc_region(source):
    with source.open("rb") as stream:
        header = stream.read(124)
    if (len(header) == 124 and header[:16] == b"MComprHD\x00\x00\x00\x7c\x00\x00\x00\x05"
            and header[84:104].hex() in DISC_REGIONS):
        return DISC_REGIONS[header[84:104].hex()]
    raise ValueError("Select an Arcade Stage 3 English GDS-0033 or Japanese Rev C GDS-0032C CHD.")


def check_player_disc(source, region):
    if region == "gds-0033":
        check_source(source, CHD, "GDS-0033 disc image")
    elif disc_region(source) != region or digest(source) not in JAPANESE_CHDS:
        raise ValueError("Select a verified Japanese GDS-0032C player disc.")


def read_pic(source, scratch, region="gds-0033"):
    filename = SECURITY_NAMES[region]
    signature = JAPANESE_PIC if region == "gds-0032c" else PIC
    if source.suffix.lower() != ".zip":
        check_source(source, signature, filename)
        return source
    with zipfile.ZipFile(source) as archive:
        files = [entry for entry in archive.infolist()
                 if entry.filename.rsplit("/", 1)[-1].lower() == filename]
        if len(files) != 1 or files[0].file_size != signature[1]:
            raise ValueError(f"The selected archive must contain one matching {filename} for {region.upper()}.")
        pic = scratch / "security.pic"
        pic.write_bytes(archive.read(files[0]))
    check_source(pic, signature, filename)
    return pic

def run(command, log):
    print(f"  {Path(command[0]).stem} {Path(command[1]).stem if len(command)>1 else ''}", flush=True)
    log.flush()
    start = log.tell()
    result = subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        # Some activation logs live in a temporary directory removed on failure.
        # Include this command's final output before handing the error to the launcher.
        label = Path(command[1] if command[0] == sys.executable else command[0]).name
        with open(log.name, "rb") as reader:
            end = reader.seek(0, os.SEEK_END)
            reader.seek(max(start, end - 8192))
            reason = reader.read().decode("utf-8", errors="replace").strip()
        raise ValueError(f"{label} stopped (exit code {result.returncode}).\n{reason}")

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
        if receipt["version"] != VERSION or not receipt_region(receipt) or not receipt["files"]:
            return False
        return inventory(target / "data") == receipt["files"]
    except (OSError, ValueError, KeyError, TypeError):
        return False

def generate(game, project, converters, helpers, log, previous_data=None):
    image = game / "idas3_main_0C020000.bin"
    host = game / "driveA/HOSTFS"
    from arcade_program import JAPANESE, identify
    layout = identify(image.read_bytes())
    if not (host / "path/k_df_path.bin").is_file():
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
    convert("export_original_time_attack_analysis", image, "--hostfs", host, "--native", project, "--proof", project / "verification/time_attack_analysis.json")
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
    reference = None
    if previous_data is not None:
        region = "gds-0032c" if layout == JAPANESE else "gds-0033"
        reference = json.loads((region_recipes(region) / "reference-checks.json").read_text())
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
    convert("extract_original_ranking_attract", image, host, project, "--artwork-only")
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


def extract_disc_tracks(chd, security, temp, helpers, log):
    region = disc_region(chd)
    chdman = helpers / "chdman.exe"
    if digest(chdman) != HELPERS[chdman.name]:
        raise ValueError("The disc image reader does not match the tested version.")
    pic = read_pic(security, temp, region)
    if region == "gds-0033":
        check_source(chd, CHD, "Arcade Stage 3 GDS-0033 CHD")
    else:
        run([chdman, "verify", "-i", chd], log)
    run([chdman, "extractcd", "-i", chd, "-o", temp / "source.cue",
         "-ob", temp / "track%t.bin", "-sb"], log)
    return region, pic


def extract_original(chd, security, temp, helpers, log, *, expand_assets=False):
    region, pic = extract_disc_tracks(chd, security, temp, helpers, log)
    private = temp / "original"
    if region == "gds-0032c":
        from disc_japan import extract
        return extract(temp / "track3.bin", pic, private, expand_assets=expand_assets)
    if digest(helpers / "idas3_game_file_setup.exe") != HELPERS["idas3_game_file_setup.exe"]:
        raise ValueError("The game file extractor does not match the tested version.")
    private.mkdir()
    run([helpers / "idas3_game_file_setup.exe", "--bin", temp / "track3.bin",
         "--pic", pic, "--output", private], log)
    if digest(private / "idas3_main_0C020000.bin") != PROGRAM:
        raise ValueError("The extracted game program failed verification.")
    return private


def prepare_disc_inputs(chd, security, target):
    region = disc_region(chd)
    if target.parts[-3:] != ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
        return chd, security, str(chd), region
    from disc_portable import import_original
    root = target.parents[2]
    relative = f"rom/{region}.chd"
    import_original(chd, security, root, region, link_disc=False)
    return root / relative, root / "rom" / SECURITY_NAMES[region], relative, region


def receipt_region(receipt):
    source = receipt.get("source")
    if source == CHD[0]:
        return "gds-0033"
    if isinstance(source, str) and source in JAPANESE_CHDS:
        return "gds-0032c"
    return None


def region_recipes(region):
    if region == "gds-0033":
        return HERE
    if region == "gds-0032c":
        return HERE / "regions" / region
    raise ValueError("Unrecognized base disc.")


def recorded_disc(folder):
    try:
        receipt = json.loads((folder / "disc-receipt.json").read_text(encoding="utf-8"))
        if receipt_region(receipt) and isinstance(receipt.get("files"), dict):
            return receipt
    except (OSError, ValueError, AttributeError):
        pass
    return None


def matches_conversion(receipt):
    if not receipt or not receipt_region(receipt) or receipt.get("version") != VERSION or "disc-player/Idas3Unity.dll" not in receipt["files"]:
        return False
    for bank in ("v3sV00wait", "v3sV10boost", "v3sV10loadwait", "v3sS01card", "select01", "selvswait", "v3sV01challenge", "selreceive"):
        prefix = f"original_assets/menus/v3/{bank}"
        if any(f"{prefix}/{name}" not in receipt["files"] for name in (bank + ".idasmesh", "textures/textures.idastex")):
            return False
    reference_path = region_recipes(receipt_region(receipt)) / "reference-checks.json"
    if not reference_path.is_file():
        return False
    reference = json.loads(reference_path.read_text())
    return all(receipt["files"].get(name) == expected for name, expected in reference.items())


def windows_inventory():
    with gzip.open(HERE / "windows-56-files.json.gz", "rt", encoding="utf-8") as file:
        return json.load(file)


def recover_japanese_artwork(target, files):
    """Carry the player's verified Japanese UI banks across the English disc conversion."""
    from disc_portable import plain
    prefix = "localization/ja/"
    data = target / "data"
    installed = plain(data / "localization/ja")
    user = target.parents[2] / "UserData"
    # A previous activation can have published the directory but stopped before
    # saving its receipt. In that case recheck the installed copy first.
    if prefix + "manifest.json" in files and installed.is_dir():
        return False
    if installed.exists():
        candidates = [data]
    else:
        candidates = [folder / "data" for folder in
                      (user / ".disc-cache-upgrade", *sorted(user.glob("Previous Windows Assets*")))
                      if plain(folder / "data/localization/ja").is_dir()]
    if not candidates:
        return False
    legacy = {name: item[1] for name, item in windows_inventory().items() if name.startswith(prefix)}
    references = []
    current = HERE / "japanese-artwork-checks.json"
    if current.is_file():
        references.append({prefix + name: checksum for name, checksum in
                           json.loads(current.read_text(encoding="utf-8")).items()})
    if legacy:
        references.append(legacy)
    for source in candidates:
        expected = next((entries for entries in references if entries and all(
            (path := plain(source / name)).is_file() and digest(path) == checksum
            for name, checksum in entries.items())), None)
        if expected is None:
            continue
        if source != data:
            installed.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(dir=installed.parent) as temp:
                pack = Path(temp) / "ja"
                for name, checksum in expected.items():
                    dest = pack / name[len(prefix):]
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(plain(source / name), dest)
                    if digest(dest) != checksum:
                        raise ValueError(f"Japanese artwork changed during recovery: {name}")
                pack.rename(installed)
        files.update(expected)
        return True
    return False


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


def install(chd, security, target, helpers, converters, previous_scores=None):
    chd, security, source_reference, region = prepare_disc_inputs(chd, security, target)
    receipt = recorded_disc(target)
    if matches_conversion(receipt) and receipt_region(receipt) == region and valid_cache(target):
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
            private = extract_original(chd, security, temp, helpers, log, expand_assets=True)
            from native_recipe import restore
            restore(private / "idas3_main_0C020000.bin", region_recipes(region), project / "data/disc-player/Idas3Unity.dll")
            generate(private, project, converters, helpers, log, target / "data" if upgrade else previous_scores)
        disc_extras.install_cache_files(project, HERE, disc_extras.manifest(HERE), {})
        files = inventory(project / "data")
        from disc_storage import trim_conversion_files
        trim_conversion_files(project / "data", files)
        proof = json.loads((region_recipes(region) / "reference-checks.json").read_text())
        for name, expected in proof.items():
            if files.get(name) != expected:
                raise ValueError(f"Disc conversion failed the reference check for {name}.")
        save_receipt(project, {"version": VERSION, "source": digest(chd), "files": files})
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

def region_cache_folder(target):
    from disc_portable import plain
    plain(target)
    if target.parts[-3:] != ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
        raise ValueError("Select the game's active asset folder to switch the ROM.")
    return plain(target.parents[1] / "RegionCache")


def recover_region_switch(target):
    from disc_portable import plain
    folder = region_cache_folder(target)
    marker = plain(folder / "switch-from.txt")
    if not marker.is_file():
        return
    previous = marker.read_text(encoding="utf-8").strip()
    if previous not in SECURITY_NAMES:
        raise ValueError("Unrecognized ROM switch record. The installed files have been preserved.")
    if not target.exists():
        parked = plain(folder / previous)
        if receipt_region(recorded_disc(parked) or {}) != previous:
            raise ValueError("The previous regional installation could not be recovered.")
        parked.rename(target)
    if recorded_disc(target) is None:
        raise ValueError("The active regional installation is incomplete.")
    marker.unlink()


def switch_region(target, requested, chd, security, helpers, converters):
    from disc_portable import plain, write_text
    recover_region_switch(target)
    current = receipt_region(recorded_disc(target) or {})
    if not current or requested not in SECURITY_NAMES:
        raise ValueError("Finish the base installation before switching ROMs.")
    if requested == current:
        return
    folder = region_cache_folder(target)
    folder.mkdir(exist_ok=True)
    parked = plain(folder / current)
    incoming = plain(folder / requested)
    if parked.exists():
        raise ValueError(f"The previous regional cache already exists at {parked}. Both copies were preserved.")
    if not incoming.exists():
        if not chd or not security or disc_region(chd) != requested:
            raise ValueError(f"Select the {requested.upper()} disc and its matching security file.")
        check_player_disc(chd, requested)
        install(chd, security, incoming, helpers, converters, previous_scores=target / "data")
    receipt = recorded_disc(incoming) or {}
    if receipt_region(receipt) != requested:
        raise ValueError("The cached ROM does not match the selected region.")
    library = "disc-player/Idas3Unity.dll"
    if digest(plain(incoming / "data" / library)) != receipt["files"].get(library):
        raise ValueError(f"The cached {requested.upper()} game library failed verification.")
    write_text(plain(incoming / "disc-source.txt"), f"rom/{requested}.chd")
    marker = plain(folder / "switch-from.txt")
    write_text(marker, current)
    try:
        target.rename(parked)
        incoming.rename(target)
    except OSError:
        recover_region_switch(target)
        raise
    marker.unlink()
    print(f"Selected {requested.upper()}. The {current.upper()} installation is cached for switching back.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chd", type=Path)
    parser.add_argument("--security", type=Path, help="Matching regional PIC or an arcade ZIP containing it")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--helpers", type=Path, default=HERE / "tools")
    parser.add_argument("--converters", type=Path, default=HERE / "converters")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--base-region", action="store_true", help="Identify the disc used to generate an existing cache.")
    parser.add_argument("--switch-region", choices=tuple(SECURITY_NAMES), help="Prepare and select a complete regional installation.")
    parser.add_argument("--recover-region", action="store_true", help="Recover an interrupted ROM switch before starting setup.")
    parser.add_argument("--extract-originals", action="store_true",
                        help="Extract the original game files from either supported disc into a new directory.")
    parser.add_argument("--add-japanese", action="store_true",
                        help="Install Japanese arcade artwork from the player's Japanese Rev C disc.")
    parser.add_argument("--japanese-hostfs", type=Path,
                        help="Import the original menus from an extracted Japanese Rev C HOSTFS into an existing game.")
    parser.add_argument("--activate", action="store_true", help="Install the disc-restored Windows player library.")
    parser.add_argument("--relink", action="store_true", help="Reconnect a verified cache to its moved source disc.")
    args = parser.parse_args()
    if args.recover_region:
        recover_region_switch(args.output.absolute())
        return
    if args.switch_region:
        switch_region(args.output.absolute(), args.switch_region, args.chd, args.security, args.helpers, args.converters)
        return
    if args.base_region:
        receipt = recorded_disc(args.output)
        if receipt is None:
            raise ValueError("The installed game has no recognizable disc receipt.")
        print(receipt_region(receipt))
        return
    if args.extract_originals:
        if not args.chd or not args.security:
            parser.error("Select a CHD and its matching security PIC or archive.")
        output = args.output.absolute()
        if output.exists():
            raise ValueError("Choose a new destination for the extracted files.")
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=output.parent) as work:
            with tempfile.NamedTemporaryFile(prefix="idas3-disc-", suffix=".log", dir=output.parent, delete=False) as log:
                private = extract_original(args.chd, args.security, Path(work), args.helpers, log)
            private.rename(output)
        print(f"Extracted original game files in {output}.")
        return
    if args.japanese_hostfs or args.add_japanese:
        from disc_language import convert
        from disc_portable import plain
        target = plain(args.output.absolute())
        receipt = recorded_disc(target)
        if not matches_conversion(receipt):
            raise ValueError("Finish installing the base game before importing additional language artwork.")
        data = plain(target / "data")
        output = plain(data / "localization/ja")
        manifest = plain(output / "manifest.json")
        if (manifest.is_file() and receipt["files"].get("localization/ja/manifest.json") == digest(manifest)
                and "localization/ja/original_assets/hud/game2d/game2d.idasmesh" in receipt["files"]):
            print("Japanese arcade artwork is already installed.")
            return
        expected = json.loads((HERE / "japanese-artwork-checks.json").read_text(encoding="utf-8"))
        if not output.exists():
            if args.japanese_hostfs:
                convert(args.japanese_hostfs.resolve(), data / "original_assets", output, expected)
            else:
                if not args.chd or not args.security or disc_region(args.chd) != "gds-0032c":
                    parser.error("Select your Japanese Rev C disc and its matching security PIC or archive.")
                with tempfile.TemporaryDirectory(dir=target.parent) as work:
                    with tempfile.NamedTemporaryFile(prefix="idas3-ja-", suffix=".log", dir=target.parent, delete=False) as log:
                        private = extract_original(args.chd, args.security, Path(work), args.helpers, log)
                    convert(private / "driveA/HOSTFS", data / "original_assets", output, expected)
        # A retry can encounter a fully published directory with an older receipt.
        for name, checksum in expected.items():
            file = plain(output / name)
            if not file.is_file() or digest(file) != checksum:
                raise ValueError("Existing Japanese artwork differs. Your files have been preserved.")
            receipt["files"]["localization/ja/" + name] = checksum
        save_receipt(target, receipt)
        print("Verified Japanese arcade artwork.")
        return
    if args.activate:
        target = args.output.resolve()
        if target.parts[-3:] != ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
            raise ValueError("Select the disc player's installation cache.")
        source = target / "data/disc-player/Idas3Unity.dll"
        destination = target.parents[1] / "Plugins/x86_64/Idas3Unity.dll"
        # An update may retain a cache generated by the previous disc player.
        # Publish each file atomically and make an interrupted upgrade resumable.
        receipt_path = target / "disc-receipt.json"
        receipt = recorded_disc(target)
        if not matches_conversion(receipt):
            sys.exit(4)  # An older extraction recipe needs the full conversion, even when its own receipt is intact.
        base_region = receipt_region(receipt)
        recipes = region_recipes(base_region)
        expected = json.loads((recipes / "native-recipe.json").read_text())["library"]
        key = "disc-player/Idas3Unity.dll"
        previous_stamp = receipt_path.stat().st_mtime_ns
        recorded = receipt["files"][key]
        if recorded != expected:
            actual = digest(source)
            if actual not in (recorded, expected) or receipt.get("version") != VERSION:
                raise ValueError("The installed disc player does not match its verified receipt.")
            with tempfile.TemporaryDirectory(dir=target.parent, prefix=".idas3-upgrade-") as work:
                scratch = Path(work)
                if actual != expected:
                    if not args.chd:
                        sys.exit(3)
                    region = disc_region(args.chd)
                    check_player_disc(args.chd, region)
                    from native_recipe import restore, restore_from_cache
                    rebuilt = scratch / "Idas3Unity.dll"
                    if not restore_from_cache(source, recipes, rebuilt):
                        if region != base_region:
                            raise ValueError(f"This upgrade needs tables absent from the cache. Supply the original {base_region.upper()} base disc.")
                        if not args.security:
                            sys.exit(3)
                        with (scratch / "extraction.log").open("w") as log:
                            private = extract_original(args.chd, args.security, scratch, args.helpers, log)
                        restore(private / "idas3_main_0C020000.bin", recipes, rebuilt)
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
        recovered_language = recover_japanese_artwork(target, receipt["files"])
        trimmed = trim_conversion_files(target / "data", receipt["files"])
        if added or trimmed or recovered_language:
            save_receipt(target, receipt)
        try:
            compact_install(target, HERE, extra_files, receipt["files"], restored,
                            unchanged_storage_from=previous_stamp if (trimmed or recorded != expected) and not (added or recovered_language) else None)
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
        region = disc_region(args.chd)
        check_player_disc(args.chd, region)
        target = args.output.resolve()
        if target.parts[-3:] == ("InitialDUnity_Data", "StreamingAssets", "IDAS3"):
            from disc_portable import import_original
            import_original(args.chd.resolve(), None, target.parents[2], region)
            source_reference = f"rom/{region}.chd"
        else:
            source_reference = str(args.chd.resolve())
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
        parser.error("Select the Arcade Stage 3 disc and its matching security PIC or ZIP.")
    install(args.chd.resolve(), args.security.resolve(), args.output.resolve(),
            args.helpers.resolve(), args.converters.resolve())

if __name__ == "__main__":
    main()

