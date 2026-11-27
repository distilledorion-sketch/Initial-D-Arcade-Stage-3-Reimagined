import hashlib
import io
import json
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import disc_courses
import disc_audio
import disc_extras
import disc_language
import disc_portable
import disc_setup
import native_recipe
from disc_storage import trim_conversion_files


class DiscUpdateTests(unittest.TestCase):
    def test_whole_rom_switch_round_trip_and_interrupted_rename(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "InitialDUnity_Data/StreamingAssets/IDAS3"
            bank = disc_setup.region_cache_folder(target)
            english, japanese = "gds-0033", "gds-0032c"
            library = "disc-player/Idas3Unity.dll"
            for folder, source, region in ((target, disc_setup.CHD[0], english),
                    (bank / japanese, sorted(disc_setup.JAPANESE_CHDS)[0], japanese)):
                plugin = folder / "data" / library
                plugin.parent.mkdir(parents=True)
                plugin.write_bytes(region.encode())
                (folder / "data/region-specific-dialogue").write_text(region)
                disc_setup.save_receipt(folder, {"version": disc_setup.VERSION, "source": source,
                                                 "files": {library: disc_setup.digest(plugin)}})
            with patch.object(disc_setup, "install", side_effect=AssertionError("cached region was rebuilt")):
                for requested in (japanese, english):
                    disc_setup.switch_region(target, requested, None, None, None, None)
                    self.assertEqual((target / "data" / library).read_bytes(), requested.encode())
                    self.assertEqual((target / "data/region-specific-dialogue").read_text(), requested)
                    self.assertEqual((target / "disc-source.txt").read_text(), f"rom/{requested}.chd")
            (bank / "switch-from.txt").write_text(english)
            target.rename(bank / english)
            disc_setup.recover_region_switch(target)
            self.assertEqual(disc_setup.receipt_region(disc_setup.recorded_disc(target)), english)
            self.assertTrue((bank / japanese).is_dir())
            self.assertFalse((bank / "switch-from.txt").exists())
            (bank / japanese / "data" / library).write_bytes(b"damaged")
            with self.assertRaisesRegex(ValueError, "failed verification"):
                disc_setup.switch_region(target, japanese, None, None, None, None)
            self.assertEqual(disc_setup.receipt_region(disc_setup.recorded_disc(target)), english)

    def test_failed_native_converter_reports_its_own_output(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            converter = folder / "broken_converter.py"
            converter.write_text("import sys\nprint('cannot write converted file', file=sys.stderr)\nsys.exit(17)")
            with (folder / "temporary-extraction.log").open("wb") as log:
                log.write(b"Previous successful tool\n")
                with self.assertRaises(ValueError) as failure:
                    disc_setup.run([sys.executable, converter], log)
            self.assertIn("broken_converter.py stopped (exit code 17)", str(failure.exception))
            self.assertIn("cannot write converted file", str(failure.exception))
            self.assertNotIn("Previous successful tool", str(failure.exception))

    def test_regional_native_template_only_uses_the_selected_disc(self):
        common = bytes(range(48))
        japanese = bytes(range(64, 112))
        english = bytes(range(128, 176))
        library = b"host" + common + japanese
        template, recipe = native_recipe.build(library, japanese + common)
        self.assertNotIn(common, template)
        self.assertNotIn(japanese, template)
        self.assertEqual(native_recipe.find_spans(library, english + common, recipe["spans"]), [])
        # Shared source ranges are allowed. Export-exclusive data in a Rev C
        # distribution must still be detected even when adjacent to a shared range.
        mixed = library + english
        spans = native_recipe.find_spans(mixed, english + common, recipe["spans"])
        self.assertEqual(spans, [(len(library), 0, len(english))])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            image, output = root / "program", root / "player"
            (root / "native-player.template").write_bytes(template)
            (root / "native-recipe.json").write_text(json.dumps(recipe))
            image.write_bytes(japanese + common)
            native_recipe.restore(image, root, output)
            self.assertEqual(output.read_bytes(), library)
            image.write_bytes(english + common)
            with self.assertRaises(ValueError):
                native_recipe.restore(image, root, output)
            self.assertEqual(output.read_bytes(), library)

    def test_language_conversion_rejects_a_partial_or_wrong_region_asset_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = {"original_assets/hud/game2d/game2d.idasmesh": "bad"}
            target = root / "out/ja"
            import sys
            converter_path = str(Path(__file__).resolve().parents[2] / "Native/tools")
            with patch.object(sys, "path", [converter_path, *sys.path]):
                with self.assertRaisesRegex(ValueError, "Missing original model tables"):
                    disc_language.convert(root / "HOSTFS", root / "english", target, expected)
                for folder in ("a", "b"):
                    path = root / "HOSTFS/model" / folder / "game2d_pol.tbl"
                    path.parent.mkdir(parents=True)
                    path.touch()
                with self.assertRaisesRegex(ValueError, "Ambiguous disc model"):
                    disc_language.convert(root / "HOSTFS", root / "english", target, expected)
            self.assertFalse(target.exists())

    def test_japanese_disc_pairing_is_independent_of_english_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            disc = root / "collection/initdv3j/renamed.chd"
            disc.parent.mkdir(parents=True)
            header = bytearray(124)
            header[:16] = b"MComprHD" + (124).to_bytes(4, "big") + (5).to_bytes(4, "big")
            header[84:104] = bytes.fromhex("d92bca7c8a7920c99b23710f5bdbeed1fbec12d2")
            disc.write_bytes(header)
            self.assertEqual(disc_setup.disc_region(disc), "gds-0032c")
            english = root / "rom/317-0384-com.pic"
            english.parent.mkdir()
            english.write_bytes(b"my English PIC")
            payload = b"Japanese PIC fixture"
            with zipfile.ZipFile(root / "collection/initdv3j.zip", "w") as archive:
                archive.writestr("initdv3j/317-0379-JPN.pic", payload)
            signature = hashlib.sha256(payload).hexdigest(), len(payload)
            with patch.object(disc_setup, "JAPANESE_PIC", signature):
                self.assertTrue(disc_portable.find_security([disc], root, "gds-0032c"))
                self.assertEqual((root / "rom/317-0379-jpn.pic").read_bytes(), payload)
                self.assertEqual(english.read_bytes(), b"my English PIC")
                with self.assertRaises(ValueError):
                    disc_setup.read_pic(root / "collection/initdv3j.zip", root, "gds-0033")
                # A plausible header alone must never be sufficient to extract a disc.
                helpers = root / "tools"
                helpers.mkdir()
                chdman = helpers / "chdman.exe"
                chdman.write_bytes(b"fixture")
                with patch.dict(disc_setup.HELPERS, {"chdman.exe": disc_setup.digest(chdman)}), \
                        patch.object(disc_setup, "run", side_effect=ValueError("invalid disc data")) as run:
                    with self.assertRaisesRegex(ValueError, "invalid disc data"):
                        disc_setup.extract_disc_tracks(disc, root / "collection/initdv3j.zip", root, helpers, None)
                    run.assert_called_once_with([chdman, "verify", "-i", disc], None)
            disc.write_bytes(b"incorrect disc")
            with self.assertRaises(ValueError):
                disc_setup.disc_region(disc)

    def test_cached_native_upgrade_requires_verified_source_ranges_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            setup = Path(directory)
            old = setup / "old.dll"
            out = setup / "new.dll"
            old.write_bytes(b"headerYZ..X")
            layout = {native_recipe.checksum(old.read_bytes()): [[6, 91, 2], [10, 90, 1]]}
            (setup / "native-base-spans.json").write_text(json.dumps(layout))
            template = b"new:" + bytes(3)
            (setup / "native-player.template").write_bytes(template)
            recipe = {"template": native_recipe.checksum(template),
                      "library": native_recipe.checksum(b"new:XYZ"), "spans": [[4, 90, 3]]}
            current = setup / "native-recipe.json"
            current.write_text(json.dumps(recipe))
            self.assertTrue(native_recipe.restore_from_cache(old, setup, out))
            self.assertEqual(out.read_bytes(), b"new:XYZ")
            out.unlink()
            for changes in ({"spans": [[4, 92, 3]]}, {"library": "wrong checksum"}):
                current.write_text(json.dumps({**recipe, **changes}))
                if "spans" in changes:
                    self.assertFalse(native_recipe.restore_from_cache(old, setup, out))
                else:
                    with self.assertRaises(ValueError):
                        native_recipe.restore_from_cache(old, setup, out)
                self.assertFalse(out.exists())
            old.write_bytes(b"other version")
            self.assertFalse(native_recipe.restore_from_cache(old, setup, out))

    def test_finds_existing_zip_beside_saved_disc_and_retains_verified_pic(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "game"
            (root / "rom").mkdir(parents=True)
            collection = Path(directory) / "original discs"
            (collection / "initdv3").mkdir(parents=True)
            original_disc = collection / "initdv3/gds-0033.chd"
            original_disc.touch()
            local = root / "rom/317-0384-com.pic"
            local.write_bytes(b"unrecognized")
            (root / "rom/initdv3e.zip").write_bytes(b"broken zip")
            valid = b"test PIC"
            with zipfile.ZipFile(collection / "initdv3.zip", "w") as archive:
                archive.writestr("initdv3e/317-0384-COM.pic", valid)
            with patch.object(disc_setup, "PIC", (hashlib.sha256(valid).hexdigest(), len(valid))):
                self.assertFalse(disc_portable.find_security([], root))
                self.assertTrue(disc_portable.find_security([original_disc], root))
                self.assertEqual(local.read_bytes(), valid)
                self.assertEqual(next(local.parent.glob("*.previous")).read_bytes(), b"unrecognized")
                (collection / "initdv3.zip").unlink()
                self.assertTrue(disc_portable.find_security([], root))

    def test_upgrade_only_reuses_scores_matching_the_disc_reference(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            old = root / "old"
            old.mkdir()
            source = old / "race.idms"
            dest = root / "converted.idms"
            source.write_bytes(b"original score")
            expected = hashlib.sha256(source.read_bytes()).hexdigest()
            self.assertTrue(disc_audio.reuse_score(old, source.name, expected, dest))
            self.assertEqual(dest.read_bytes(), source.read_bytes())
            dest.unlink()
            source.write_bytes(b"edited score")
            self.assertFalse(disc_audio.reuse_score(old, source.name, expected, dest))
            source.unlink()
            self.assertFalse(disc_audio.reuse_score(old, source.name, expected, dest))
            self.assertFalse(dest.exists())

    def test_maps_are_optional_and_independently_installable(self):
        from package_optional_maps import package
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            streaming = root / "InitialDUnity_Data/StreamingAssets"
            streaming.mkdir(parents=True)
            packs = {"ENNA/menu.idastex": b"enna", "HAKONE/menu.idastex": b"hakone"}
            files = {name: (len(data), hashlib.sha256(data).hexdigest()) for name, data in packs.items()}
            for name, data in packs.items():
                path = root / "source" / name
                path.parent.mkdir(parents=True)
                path.write_bytes(data)
            package(root / "source", root / "zips", files)
            disc_extras.verify_external(streaming, files)
            with zipfile.ZipFile(root / "zips/Initial-D-Track-ENNA.zip") as pack:
                self.assertEqual(pack.namelist(), ["ENNA/menu.idastex"])
                pack.extractall(root / "Mods/Maps")
            disc_extras.verify_external(streaming, files)
            (root / "Mods/Maps/ENNA/menu.idastex").write_bytes(b"edit")
            with self.assertRaisesRegex(ValueError, "ENNA"):
                disc_extras.verify_external(streaming, files)
            archive = root / "setup/course-packs/HAKONE.tar.xz"
            archive.parent.mkdir(parents=True)
            archive.write_bytes(b"broken")
            self.assertFalse(disc_courses.restore_pending(streaming, root / "setup", files))
            self.assertTrue(archive.is_file())

    def test_installed_maps_move_once_and_retain_local_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            streaming = root / "InitialDUnity_Data/StreamingAssets"
            course = streaming / "HAKONE"
            course.mkdir(parents=True)
            (course / "menu.idastex").write_bytes(b"menu")
            (course / "custom").write_bytes(b"mine")
            files = {"HAKONE/menu.idastex": (4, hashlib.sha256(b"menu").hexdigest())}
            maps = disc_courses.prepare_maps(streaming, files)
            self.assertEqual(maps, root / "Mods/Maps")
            self.assertFalse(course.exists())
            self.assertEqual((maps / "HAKONE/custom").read_bytes(), b"mine")
            self.assertTrue(all((root / "Mods" / n).is_dir() for n in ("Cars", "HUD")))
            disc_extras.verify_external(streaming, files)
            course.mkdir()
            (course / "menu.idastex").write_bytes(b"edit")
            disc_courses.prepare_maps(streaming, files)
            self.assertEqual((course / "menu.idastex").read_bytes(), b"edit")
            self.assertEqual((maps / "HAKONE/menu.idastex").read_bytes(), b"menu")
            (course / "menu.idastex").write_bytes(b"menu")
            disc_courses.prepare_maps(streaming, files)
            self.assertFalse(course.exists())

    def test_fresh_course_install_and_updated_mod_backup(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            streaming = root / "InitialDUnity_Data/StreamingAssets"
            setup = streaming.parent / "GameSetup"
            packs = setup / disc_courses.PACK_DIR
            packs.mkdir(parents=True)
            streaming.mkdir()
            files = {"ENNA/menu.idastex": [4, hashlib.sha256(b"menu").hexdigest()]}
            archive = io.BytesIO()
            with zipfile.ZipFile(archive, "w") as outer:
                disc_courses.write_pack(outer, "ENNA", files, lambda _: io.BytesIO(b"menu"))
            with zipfile.ZipFile(archive) as outer:
                packed = outer.read("setup/course-packs/ENNA.tar.xz")
            course = root / "Mods/Maps/ENNA"
            for attempt in range(2):
                (packs / "ENNA.tar.xz").write_bytes(packed)
                if attempt:
                    (course / "menu.idastex").write_bytes(b"custom")
                self.assertTrue(disc_courses.restore_pending(streaming, setup, files))
                disc_extras.verify_external(streaming, files)
                self.assertFalse((streaming / "ENNA").exists())
            self.assertFalse(disc_courses.restore_pending(streaming, setup, files))
            self.assertEqual(next(course.glob("*.previous")).read_bytes(), b"custom")

    def test_trim_keeps_runtime_banks_and_player_files_and_resumes(self):
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder)
            bank = "original_assets/menus/test"
            textures = bank + "/textures/"
            decoded = bank + "/original_texture_decoded.bin"
            mesh_dump = bank + "/original_pol.bin"
            missing = bank + "/original_pol.tbl"
            preview = textures + "texture_000.png"
            edited = textures + "texture_001.png"
            loose = "original_assets/loose/textures/texture_002.png"
            recorded = [decoded, mesh_dump, missing, preview, edited, loose,
                        bank + "/test.idasmesh", textures + "textures.idastex"]
            for name in recorded:
                path = data / name
                path.parent.mkdir(exist_ok=True, parents=True)
                path.write_bytes(name.encode())
            files = disc_setup.inventory(data)
            (data / edited).write_bytes(b"player edit")
            (data / missing).unlink()
            custom = data / textures / "texture_099.png"
            custom.write_bytes(b"player addition")
            self.assertTrue(trim_conversion_files(data, files))
            self.assertEqual(set(recorded) - set(files), {decoded, mesh_dump, missing, preview})
            for name in set(recorded) - set(files):
                self.assertFalse((data / name).exists())
            self.assertEqual((data / edited).read_bytes(), b"player edit")
            self.assertEqual(custom.read_bytes(), b"player addition")
            self.assertFalse(trim_conversion_files(data, files))

    def test_trim_requires_intact_runtime_asset(self):
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder)
            bank = data / "original_assets/test/textures"
            bank.mkdir(parents=True)
            compiled = bank / "textures.idastex"
            compiled.write_bytes(b"packed")
            preview = bank / "texture_001.png"
            preview.write_bytes(b"generated")
            files = disc_setup.inventory(data)
            compiled.write_bytes(b"corrupt")
            self.assertFalse(trim_conversion_files(data, files))
            compiled.unlink()
            self.assertFalse(trim_conversion_files(data, files))
            self.assertEqual(preview.read_bytes(), b"generated")

    def test_failed_disc_extraction_keeps_original_installation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "InitialDUnity_Data/StreamingAssets/IDAS3"
            (cache / "data").mkdir(parents=True)
            (cache / "data.manifest.json").write_text('{"schema":"idas3-unity-runtime-data-v1"}')
            original = cache / "data/old"
            original.write_bytes(b"playable")
            with patch.object(disc_setup, "prepare_disc_inputs", return_value=(None, None, "rom/gds-0033.chd", "gds-0033")), \
                    patch.object(disc_setup, "extract_original", side_effect=OSError("interrupted read")):
                with self.assertRaisesRegex(OSError, "interrupted read"):
                    disc_setup.install(None, None, cache, root, root)
            self.assertEqual(original.read_bytes(), b"playable")
            self.assertFalse((root / "UserData/.disc-cache-upgrade").exists())

    def test_public_windows_install_without_development_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            user = root / "UserData"
            previous = user / ".disc-cache-upgrade"
            file = previous / "data/original_tuning/tables.idastune"
            file.parent.mkdir(parents=True)
            file.write_bytes(b"stock tuning")
            custom = file.with_name("my-tuning.txt")
            custom.write_bytes(b"mine")
            inventory = {"original_tuning/tables.idastune": (len(b"stock tuning"), hashlib.sha256(b"stock tuning").hexdigest())}
            inventory["original_tuning/my-tuning.txt"] = (4, "0" * 64)
            with patch.object(disc_setup, "windows_inventory", return_value=inventory), \
                    patch.object(disc_portable, "ROOT", root), patch.object(disc_portable, "USER", user):
                self.assertTrue(disc_setup.is_windows_cache(previous))
                disc_portable.retire_previous_installation()
            kept = user / "Previous Windows Assets/data/original_tuning"
            self.assertFalse((kept / file.name).exists())
            self.assertEqual((kept / custom.name).read_bytes(), b"mine")

    def test_japanese_artwork_survives_upgrade_cleanup_and_interrupted_activation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            user = root / "UserData"
            previous = user / ".disc-cache-upgrade/data"
            target = root / "InitialDUnity_Data/StreamingAssets/IDAS3"
            names = ["localization/ja/manifest.json", "localization/ja/original_assets/bank.idasmesh"]
            entries = {}
            for name in [*names, "original_tuning/tables.idastune"]:
                source = previous / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(name.encode())
                entries[name] = (source.stat().st_size, disc_setup.digest(source))
            (previous / names[1]).write_bytes(b"edited")
            files = {}
            with patch.object(disc_setup, "windows_inventory", return_value=entries), \
                    patch.object(disc_portable, "ROOT", root), patch.object(disc_portable, "USER", user):
                self.assertFalse(disc_setup.recover_japanese_artwork(target, files))
                disc_portable.retire_previous_installation()
                backup = user / "Previous Windows Assets/data"
                self.assertEqual((backup / names[0]).read_bytes(), names[0].encode())
                self.assertEqual((backup / names[1]).read_bytes(), b"edited")
                self.assertFalse((target / "data/localization/ja").exists())
                (backup / names[1]).write_bytes(names[1].encode())
                self.assertTrue(disc_setup.recover_japanese_artwork(target, files))
                self.assertEqual(files, {name: entries[name][1] for name in names})
                self.assertTrue(disc_setup.recover_japanese_artwork(target, {}))
                self.assertFalse(disc_setup.recover_japanese_artwork(target, files))
                for name in names:
                    self.assertEqual((target / "data" / name).read_bytes(), name.encode())

    def test_extracted_japanese_artwork_survives_reconversion_without_the_japanese_disc(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "InitialDUnity_Data/StreamingAssets/IDAS3"
            archived = root / "UserData/.disc-cache-upgrade/data/localization/ja"
            expected = {}
            for name in ("manifest.json", "original_assets/hud/game2d/game2d.idasmesh"):
                path = archived / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
                expected[name] = disc_setup.digest(path)
            (root / "japanese-artwork-checks.json").write_text(json.dumps(expected))
            with patch.object(disc_setup, "HERE", root), patch.object(disc_setup, "windows_inventory", return_value={}):
                files = {}
                self.assertTrue(disc_setup.recover_japanese_artwork(target, files))
                for name, checksum in expected.items():
                    entry = "localization/ja/" + name
                    self.assertEqual(files[entry], checksum)
                    self.assertEqual(disc_setup.digest(target / "data" / entry), checksum)
                self.assertFalse(disc_setup.recover_japanese_artwork(target, files))

    def test_older_extraction_with_same_version_needs_new_conversion(self):
        with tempfile.TemporaryDirectory() as folder:
            setup = Path(folder)
            (setup / "reference-checks.json").write_text(json.dumps({"original_audio/score": "current"}))
            receipt = {"source": disc_setup.CHD[0], "version": disc_setup.VERSION,
                       "files": {"disc-player/Idas3Unity.dll": "previous-native"}}
            with patch.object(disc_setup, "HERE", setup):
                self.assertFalse(disc_setup.matches_conversion(receipt))
                receipt["files"]["original_audio/score"] = "current"
                self.assertFalse(disc_setup.matches_conversion(receipt))
                for bank in ("v3sV00wait", "v3sV10boost", "v3sV10loadwait", "v3sS01card",
                             "select01", "selvswait", "v3sV01challenge", "selreceive"):
                    prefix = f"original_assets/menus/v3/{bank}/"
                    receipt["files"][prefix + bank + ".idasmesh"] = "mesh"
                    receipt["files"][prefix + "textures/textures.idastex"] = "textures"
                self.assertTrue(disc_setup.matches_conversion(receipt))
                receipt["source"] = next(iter(disc_setup.JAPANESE_CHDS))
                self.assertFalse(disc_setup.matches_conversion(receipt))
                japanese = setup / "regions/gds-0032c"
                japanese.mkdir(parents=True)
                (japanese / "reference-checks.json").write_text(json.dumps({"original_audio/score": "japanese"}))
                self.assertFalse(disc_setup.matches_conversion(receipt))
                receipt["files"]["original_audio/score"] = "japanese"
                self.assertTrue(disc_setup.matches_conversion(receipt))

    def test_disc_rebuild_preserves_unrecognized_and_changed_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            user = root / "UserData"
            previous = user / ".disc-cache-upgrade"
            active = root / "InitialDUnity_Data/StreamingAssets/IDAS3"
            files = {name: hashlib.sha256(name.encode()).hexdigest() for name in ("same", "modified")}
            for target in (active, previous):
                (target / "data").mkdir(parents=True)
                for name in files:
                    (target / "data" / name).write_bytes(name.encode())
                (target / "disc-receipt.json").write_text(json.dumps({
                    "source": disc_setup.CHD[0], "version": disc_setup.VERSION, "files": files}))
            (previous / "data/modified").write_bytes(b"my version")
            (previous / "data/unlisted").write_bytes(b"other file")
            with patch.object(disc_portable, "ROOT", root), patch.object(disc_portable, "USER", user):
                disc_portable.retire_previous_installation()
                disc_portable.retire_previous_installation()
            backup = user / "Previous Windows Assets/data"
            self.assertFalse((backup / "same").exists())
            self.assertEqual((backup / "modified").read_bytes(), b"my version")
            self.assertEqual((backup / "unlisted").read_bytes(), b"other file")
            self.assertEqual((active / "data/modified").read_bytes(), b"modified")


if __name__ == "__main__":
    unittest.main()
