"""Host-only (Mac) tests for uninstall.py.

Run:  python3 -m unittest -v test_uninstall.py
No FTP, no payloads, no launches: everything happens in a temp dir.
"""

import hashlib
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import uninstall
from uninstall import (
    ConfirmationRequiredError,
    ModStillRequiredError,
    RollbackError,
    UnsafePathError,
    UninstallError,
    Uninstaller,
    load_manifest,
)

BASE_LOADLIST = "# UE4SS mod loadlist\nPreExistingMod : 1\n"
BASE_PLOADER = "# ploader autoload\npayload=/data/payloads/other.prx\n"
SHARED_SETTINGS = "[Engine]\nGraphicsMode=2\nConsoleEnabled=1\n"


def sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


class UninstallTestCase(unittest.TestCase):
    def setUp(self):
        # MODBRIDGE_TEST_TMPDIR (Mac-only host testing hook): rerun this
        # suite on a case-sensitive APFS disk image (test_case_sensitivity).
        tmp = tempfile.TemporaryDirectory(
            prefix="uninstall-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.addCleanup(tmp.cleanup)
        self.game = Path(tmp.name) / "game"
        (self.game / "Mods").mkdir(parents=True)
        (self.game / "UE4SS-settings.ini").write_text(SHARED_SETTINGS, encoding="utf-8")
        (self.game / "Mods" / "mods.txt").write_text(BASE_LOADLIST, encoding="utf-8")
        (self.game / "ploader.ini").write_text(BASE_PLOADER, encoding="utf-8")
        self.state_path = self.game / "install_state.json"
        self._state = {"game_version": "1.004.000", "mods": {}}
        self._save_state()
        self.un = Uninstaller(self.game,
                              state_path=self.state_path,
                              transaction_root=self.game / ".uninstall-transactions")

    def _save_state(self):
        self.state_path.write_text(json.dumps(self._state, indent=2), encoding="utf-8")

    def install_mod(self, mod_id, files, requires=(), config_entries=None):
        """Fake installer: write files, append config lines, record manifest+state."""
        manifest = {
            "mod_id": mod_id,
            "name": mod_id,
            "mod_version": "1.0.0",
            "requires": list(requires),
            "files": [],
            "config_entries": {},
        }
        for rel, content in files.items():
            p = self.game / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content, encoding="utf-8")
            manifest["files"].append({"path": rel, "sha256": sha(content)})
        for cfg_rel, lines in (config_entries or {}).items():
            cfg = self.game / cfg_rel
            cfg.parent.mkdir(parents=True, exist_ok=True)
            with open(cfg, "a", encoding="utf-8") as fh:
                for line in lines:
                    fh.write(line + "\n")
            manifest["config_entries"][cfg_rel] = list(lines)
        record_dir = self.game / ".install-records"
        record_dir.mkdir(exist_ok=True)
        record = record_dir / ("%s.json" % mod_id)
        record.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        self._state["mods"][mod_id] = {
            "manifest": str(record),
            "requires": list(requires),
            "files": [dict(f) for f in manifest["files"]],
        }
        self._save_state()
        return manifest

    def install_spawner(self):
        return self.install_mod(
            "spawner",
            files={
                "Mods/spawner/scripts/main.lua": "-- spawner main\n",
                "Mods/spawner/mods.d/spawner.ini": "[spawner]\nenabled=1\n",
            },
            config_entries={
                "Mods/mods.txt": ["SpawnerMod : 1"],
                "ploader.ini": ["payload=/data/payloads/spawner.prx"],
            },
        )

    def install_hudtweaks(self, requires=()):
        return self.install_mod(
            "hudtweaks",
            files={"Mods/hudtweaks/scripts/hud.lua": "-- hud\n"},
            requires=requires,
            config_entries={
                "Mods/mods.txt": ["HudTweaks : 1"],
                "ploader.ini": ["payload=/data/payloads/hudtweaks.prx"],
            },
        )

    # ------------------------------------------------------------------

    def test_clean_uninstall(self):
        manifest_a = self.install_spawner()
        self.install_hudtweaks()

        res = self.un.uninstall(manifest_a)

        self.assertEqual(res.mod_id, "spawner")
        self.assertFalse(res.dry_run)
        # exactly the manifest files are gone (incl. pruned empty dirs)
        self.assertFalse((self.game / "Mods/spawner/scripts/main.lua").exists())
        self.assertFalse((self.game / "Mods/spawner/mods.d/spawner.ini").exists())
        self.assertFalse((self.game / "Mods/spawner").exists())
        # other mods' files survive
        self.assertTrue((self.game / "Mods/hudtweaks/scripts/hud.lua").exists())
        # loadlist: only this mod's row removed
        loadlist = (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8")
        self.assertNotIn("SpawnerMod", loadlist)
        self.assertIn("PreExistingMod : 1", loadlist)
        self.assertIn("HudTweaks : 1", loadlist)
        # ploader.ini: only this mod's line removed
        ploader = (self.game / "ploader.ini").read_text(encoding="utf-8")
        self.assertNotIn("spawner.prx", ploader)
        self.assertIn("payload=/data/payloads/other.prx", ploader)
        self.assertIn("payload=/data/payloads/hudtweaks.prx", ploader)
        # shared config file untouched
        self.assertEqual((self.game / "UE4SS-settings.ini").read_text(encoding="utf-8"),
                         SHARED_SETTINGS)
        # state updated
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertNotIn("spawner", state["mods"])
        self.assertIn("hudtweaks", state["mods"])
        self.assertEqual(state["game_version"], "1.004.000")

    def test_modified_file_needs_confirmation_then_proceeds(self):
        manifest_a = self.install_spawner()
        tampered = self.game / "Mods/spawner/scripts/main.lua"
        tampered.write_text("-- tampered\n", encoding="utf-8")

        with self.assertRaises(ConfirmationRequiredError) as ctx:
            self.un.uninstall(manifest_a)
        err = ctx.exception
        self.assertIn("modified since install", str(err))
        self.assertEqual([f.path for f in err.modified_files],
                         ["Mods/spawner/scripts/main.lua"])
        # nothing was touched by the refused attempt
        self.assertTrue(tampered.exists())
        self.assertIn("SpawnerMod",
                      (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8"))
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertIn("spawner", state["mods"])

        res = self.un.uninstall(manifest_a, confirm_modified=True)
        self.assertFalse(tampered.exists())
        self.assertEqual(res.removed_modified, ["Mods/spawner/scripts/main.lua"])

    def test_confirmation_callback(self):
        manifest_a = self.install_spawner()
        (self.game / "Mods/spawner/scripts/main.lua").write_text("changed\n",
                                                                encoding="utf-8")
        seen = []

        def ask(modified):
            seen.extend(f.path for f in modified)
            return True

        self.un.uninstall(manifest_a, confirm_modified=ask)
        self.assertEqual(seen, ["Mods/spawner/scripts/main.lua"])
        self.assertFalse((self.game / "Mods/spawner/scripts/main.lua").exists())

    def test_dependent_mod_blocks_uninstall(self):
        manifest_a = self.install_spawner()
        self.install_hudtweaks(requires=["spawner"])

        with self.assertRaises(ModStillRequiredError) as ctx:
            self.un.uninstall(manifest_a)
        self.assertEqual(ctx.exception.dependents, ["hudtweaks"])
        self.assertIn("hudtweaks", str(ctx.exception))
        # nothing was removed by the refused attempt
        self.assertTrue((self.game / "Mods/spawner/scripts/main.lua").exists())
        self.assertIn("SpawnerMod",
                      (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8"))

        # dependent first, then the dependency: both succeed
        self.un.uninstall(load_manifest(self.game / ".install-records" / "hudtweaks.json"))
        res = self.un.uninstall(manifest_a)
        self.assertEqual(sorted(res.removed + res.removed_modified),
                         sorted(f["path"] for f in manifest_a["files"]))

    def test_rollback_restores_uninstall(self):
        manifest_a = self.install_spawner()
        self.install_hudtweaks()
        originals = {
            "Mods/spawner/scripts/main.lua": "-- spawner main\n",
            "Mods/spawner/mods.d/spawner.ini": "[spawner]\nenabled=1\n",
        }

        res = self.un.uninstall(manifest_a)
        token = res.rollback_token
        self.assertIsNotNone(token)
        self.assertFalse((self.game / "Mods/spawner/scripts/main.lua").exists())
        self.assertNotIn("SpawnerMod",
                         (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8"))

        summary = self.un.rollback(token)
        self.assertEqual(summary["mod_id"], "spawner")
        self.assertEqual(sorted(summary["files_restored"]),
                         sorted(originals.keys()))
        for rel, content in originals.items():
            p = self.game / rel
            self.assertTrue(p.exists(), rel)
            self.assertEqual(uninstall.sha256_file(p), sha(content))
        self.assertIn("SpawnerMod : 1",
                      (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8"))
        self.assertIn("payload=/data/payloads/spawner.prx",
                      (self.game / "ploader.ini").read_text(encoding="utf-8"))
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertIn("spawner", state["mods"])
        self.assertIn("hudtweaks", state["mods"])

        # double rollback refused
        with self.assertRaises(RollbackError):
            self.un.rollback(token)

    def test_missing_file_is_reported_not_fatal(self):
        manifest_a = self.install_spawner()
        (self.game / "Mods/spawner/mods.d/spawner.ini").unlink()

        res = self.un.uninstall(manifest_a)
        self.assertEqual(res.missing, ["Mods/spawner/mods.d/spawner.ini"])
        self.assertFalse((self.game / "Mods/spawner/scripts/main.lua").exists())
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertNotIn("spawner", state["mods"])

    def test_shared_file_is_kept(self):
        self.install_mod("sharedlib", files={"Mods/shared/overlay.bin": "overlay\n"})
        manifest_a = self.install_mod("spawner2",
                                      files={"Mods/shared/overlay.bin": "overlay\n"})
        res = self.un.uninstall(manifest_a)
        self.assertEqual(res.shared_kept, ["Mods/shared/overlay.bin"])
        self.assertTrue((self.game / "Mods/shared/overlay.bin").exists())
        self.assertEqual((self.game / "Mods/shared/overlay.bin").read_text(encoding="utf-8"),
                         "overlay\n")
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertIn("sharedlib", state["mods"])
        self.assertNotIn("spawner2", state["mods"])

    def test_only_recorded_line_count_removed(self):
        # a stray duplicate row (e.g. from a re-install) must survive:
        # only the one row the installer recorded is removed
        manifest_a = self.install_spawner()
        self.install_hudtweaks()
        with open(self.game / "Mods" / "mods.txt", "a", encoding="utf-8") as fh:
            fh.write("SpawnerMod : 1\n")

        self.un.uninstall(manifest_a)
        loadlist = (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8")
        self.assertEqual(loadlist.count("SpawnerMod : 1"), 1)
        self.assertIn("HudTweaks : 1", loadlist)
        self.assertIn("PreExistingMod : 1", loadlist)

    def test_unsafe_manifest_path_rejected(self):
        manifest_a = self.install_spawner()
        evil = json.loads(json.dumps(manifest_a))
        evil["files"].append({"path": "../outside.txt", "sha256": "0" * 64})
        with self.assertRaises(UnsafePathError):
            self.un.plan(evil)
        with self.assertRaises(UnsafePathError):
            self.un.uninstall(evil, confirm_modified=True)
        self.assertTrue((self.game / "Mods/spawner/scripts/main.lua").exists())

    def test_dry_run_changes_nothing(self):
        manifest_a = self.install_spawner()
        res = self.un.uninstall(manifest_a, dry_run=True)
        self.assertTrue(res.dry_run)
        self.assertIsNone(res.rollback_token)
        self.assertTrue((self.game / "Mods/spawner/scripts/main.lua").exists())
        self.assertIn("SpawnerMod",
                      (self.game / "Mods" / "mods.txt").read_text(encoding="utf-8"))
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertIn("spawner", state["mods"])
        self.assertFalse((self.game / ".uninstall-transactions").exists())

    def test_unknown_mod_refused(self):
        self.install_spawner()
        ghost = {"mod_id": "ghost", "files": [], "config_entries": {}}
        with self.assertRaises(UninstallError):
            self.un.uninstall(ghost)


if __name__ == "__main__":
    unittest.main(verbosity=2)
