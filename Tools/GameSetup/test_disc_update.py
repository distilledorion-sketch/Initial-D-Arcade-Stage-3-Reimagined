import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import disc_courses
import disc_audio
import disc_extras
import disc_portable
import disc_setup
from disc_storage import trim_conversion_files


class DiscUpdateTests(unittest.TestCase):
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
            with patch.object(disc_setup, "prepare_disc_inputs", return_value=(None, None, "rom/gds-0033.chd")), \
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
