import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

if os.name == "nt":
    import disc_portable
    import save_recovery as recovery


@unittest.skipUnless(os.name == "nt", "Windows game")
class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.parent = Path(self.temp.name)
        self.game = self.install("Current")
        self.old = self.install("Earlier")
        self.dest = self.game / "UserData"
        self.src = self.old / "UserData"
        self.windows = self.parent / "LocalLow/Chris/Initial D Unity"

    def put(self, path, content):
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_bytes(content.encode("utf-8"))

    def install(self, name):
        folder = self.parent / name
        self.put(folder / "InitialDUnity_Data/app.info", "Chris\nInitial D Arcade Stage 3 Reimagined\n")
        (folder / "UserData").mkdir()
        return folder

    def card(self, folder, slot, wins):
        path = folder / f"userdata-unity-scene/saves/slot_{slot}"
        self.put(path / "save.txt", f"idas3-save-slot-v1 0 100 {wins} 1 164 0 0 0 0 -\n")
        self.put(path / "driver_profiles_v1/car_00.profile", "profile "+str(wins))
        return path

    def test_marked_empty_upgrade_finds_external_card_options_hud_and_replays(self):
        self.put(self.dest / "migration-v1.json", '{"files":1}')
        card = self.card(self.src, 1, 57)
        self.put(card / "driver_profiles_v1/best.idghost", "ghost")
        for name, content in [("game-options.json",'{"hudMeterStyle":40,"hudOrnamentId":17}'),
                              ("controls.json",'{"version":5}'), ("replays/local.idreplay","recording")]:
            self.put(self.src / "userdata-unity-scene" / name, content)
        self.put(self.old / "Mods/HUD/game2d/texture_053.png","custom needle")
        current, found = recovery.candidates(self.game, self.windows)
        self.assertFalse(current["cards"])
        self.assertEqual([str(self.src)], [s["path"] for s in found if s["missing"]])
        self.assertEqual(recovery.game_data(self.old), recovery.game_data(self.src))
        entries = list(recovery.entries(self.src, self.old / "Mods/HUD"))
        self.assertGreater(recovery.restore(self.game, entries), 0)
        for rel, source in entries:
            self.assertEqual(recovery.fingerprint(source), recovery.fingerprint(self.game / rel))
        self.assertEqual(recovery.restore(self.game, entries), 0)

    def test_overlapping_cards_do_not_mix_and_other_slots_survive(self):
        slot = self.card(self.dest, 1, 3)
        self.put(slot / "driver_profiles_v1/car_10.profile","only current")
        other = self.card(self.dest, 4, 2)
        self.card(self.src, 1, 10)
        recovery.restore(self.game, recovery.entries(self.src))
        self.assertFalse((slot / "driver_profiles_v1/car_10.profile").exists())
        self.assertTrue((other / "save.txt").is_file())
        original, = (self.dest / "Save recovery backups").glob("*/before/UserData/*/saves/slot_1")
        self.assertEqual((original / "driver_profiles_v1/car_10.profile").read_text(), "only current")

    def test_save_and_settings_are_untouched_without_explicit_cross_install_import(self):
        self.card(self.dest, 1, 40)
        self.card(self.src, 1, 1)
        self.put(self.dest / "userdata-unity-scene/controls.json", '{"active":true}')
        self.put(self.src / "userdata-unity-scene/controls.json", '{"active":false}')
        before = recovery.fingerprint(self.dest)
        _, found = recovery.candidates(self.game, self.windows)
        self.assertFalse(any(s["missing"] for s in found))
        self.assertEqual(recovery.fingerprint(self.dest), before)

    def test_recover_interrupted_commit_with_destination_already_backed_up(self):
        f = Path("UserData/userdata-unity-scene/settings.txt")
        self.put(self.game / f,"old")
        stage = self.dest / recovery.PENDING
        self.put(stage / "new" / f,"new")
        self.put(stage / "plan.json",json.dumps({"paths":[f.as_posix()]}))
        before = stage / "before" / f
        before.parent.mkdir(parents=True)
        (self.game / f).rename(before)
        recovery.commit(self.game)
        recovery.commit(self.game)
        self.assertEqual((self.game / f).read_text(),"new")
        self.assertFalse(stage.exists())

    def test_appearance_backup_restores_only_reset_fields(self):
        from argparse import Namespace
        root = self.dest / "userdata-unity-scene"
        self.put(root / "game-options.json",'{"hudMeterStyle":0,"hudOrnamentId":0,"volume":15}')
        self.put(root / "game-options.json.previous",'{"hudMeterStyle":40,"hudOrnamentId":12,"volume":90}')
        recovery.run(Namespace(action="appearance"), self.game, self.windows)
        self.assertEqual(json.loads((root / "game-options.json").read_text()),
                         {"hudMeterStyle":40,"hudOrnamentId":12,"volume":15})

    def test_legacy_profiles_kept_together_and_migration_resumes_same_route(self):
        self.card(self.windows, 1, 19)
        self.card(self.dest, 1, 6)
        disc_portable.migrate(self.dest, self.windows, self.parent/"temp", "")
        staged = self.dest / recovery.WINDOWS_COPY
        self.assertIn(" 19 ",(staged / "userdata-unity-scene/saves/slot_1/save.txt").read_text())
        self.assertIn(" 6 ",(self.dest / "userdata-unity-scene/saves/slot_1/save.txt").read_text())
        new = self.install("Second") / "UserData"
        oldcard = self.card(self.windows, 2, 10)
        self.put(self.windows / "userdata-unity-scene/settings.txt","settings")
        transfer = disc_portable.transfer
        def fail(source, target, data):
            if source.name == "settings.txt":raise OSError("busy")
            return transfer(source, target, data)
        with patch.object(disc_portable,"transfer",fail), self.assertRaises(OSError):
            disc_portable.migrate(new,self.windows,self.parent/"temp","")
        disc_portable.migrate(new,self.windows,self.parent/"temp","")
        self.assertFalse((new/recovery.WINDOWS_COPY).exists())
        self.assertTrue((new/"userdata-unity-scene/settings.txt").is_file())
        self.assertTrue((new/oldcard.relative_to(self.windows)/"save.txt").is_file())


if __name__ == "__main__":
    unittest.main()
