"""Host-only (Mac) tests for conflicts.py.

Run:  python3 -m unittest -v test_conflicts.py
No FTP, no payloads, no launches: everything happens in a temp dir.
"""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from conflicts import (
    HOOK_CONFLICT,
    KEYBIND_CONFLICT,
    MISSING_DEPENDENCY,
    PATH_CONFLICT,
    VERSION_MISMATCH,
    InstalledRegistry,
    ModSpec,
    check_batch,
    check_install,
    parse_hooks,
    parse_keybinds,
)

LIB_LUA = (
    "-- lib-b bindings\n"
    "RegisterKeyBind(Key.F, function() ToggleMenu() end)\n"
    "RegisterKeyBind(Key.H, OnHelp)\n"
    "-- RegisterKeyBind(Key.J, function() end) -- disabled\n"
    "RegisterHook('/Script/Engine.PlayerController:Tick', function() end)\n"
    'RegisterHook("/Game/Blueprints/BP_HUD:UpdateHUD", UpdateHUD_CB)\n'
)


class ConflictTestCase(unittest.TestCase):
    def setUp(self):
        # MODBRIDGE_TEST_TMPDIR (Mac-only host testing hook): rerun this
        # suite on a case-sensitive APFS disk image (test_case_sensitivity).
        tmp = tempfile.TemporaryDirectory(
            prefix="conflicts-test-",
            dir=os.environ.get("MODBRIDGE_TEST_TMPDIR") or None)
        self.addCleanup(tmp.cleanup)
        self.game = Path(tmp.name) / "game"
        lib = self.game / "Mods/libb/scripts/lib.lua"
        lib.parent.mkdir(parents=True)
        lib.write_text(LIB_LUA, encoding="utf-8")
        self.state = {
            "game_version": "1.004.000",
            "mods": {
                "lib-b": {
                    "requires": [],
                    "files": [{"path": "Mods/libb/scripts/lib.lua",
                               "sha256": "0" * 64}],
                }
            },
        }

    def check(self, spec, **kwargs):
        kwargs.setdefault("game_root", self.game)
        return check_install(spec, self.state, **kwargs)

    # ------------------------------------------------------------------

    def test_path_conflict_detected(self):
        spec = ModSpec("cand", files=("Mods/libb/scripts/lib.lua",))
        rep = self.check(spec)
        self.assertFalse(rep.ok)
        blockers = rep.blockers
        self.assertTrue(blockers)
        c = blockers[0]
        self.assertEqual(c.kind, PATH_CONFLICT)
        self.assertEqual(c.detail["owner"], "lib-b")
        self.assertEqual(set(c.mods), {"cand", "lib-b"})

        # a distinct path is clean
        clean = self.check(ModSpec("cand", files=("Mods/cand/scripts/main.lua",)))
        self.assertTrue(clean.ok)

        # the same mod reinstalling its own file is not a conflict
        reinstall = self.check(ModSpec("lib-b", files=("Mods/libb/scripts/lib.lua",)))
        self.assertTrue(reinstall.ok)

    def test_keybind_conflict_is_warning(self):
        spec = ModSpec("cand",
                       lua_sources={"main.lua":
                                    "RegisterKeyBind(Key.F, function() end)\n"})
        rep = self.check(spec)
        self.assertTrue(rep.ok)  # warnings never block
        kinds = [c.kind for c in rep.conflicts]
        self.assertIn(KEYBIND_CONFLICT, kinds)
        c = next(c for c in rep.conflicts if c.kind == KEYBIND_CONFLICT)
        self.assertEqual(c.severity, "warning")
        self.assertEqual(c.detail["key"], "F")
        self.assertIn("lib-b", c.mods)

        other = self.check(ModSpec("cand",
                                   lua_sources={"main.lua":
                                                "RegisterKeyBind(Key.K, function() end)\n"}))
        self.assertEqual([c for c in other.conflicts if c.kind == KEYBIND_CONFLICT],
                         [])

    def test_keybind_parser_variants_and_comments(self):
        text = (
            "-- RegisterKeyBind(Key.F, function() end)\n"
            "RegisterKeyBind(Key.LEFT_CONTROL, fn1)\n"
            'RegisterKeyBind("g", fn2)\n'
            "RegisterKeyBind(75, fn3)\n"
            "RegisterKeyBind(Key.H, fn4) -- used to be Key.F\n"
        )
        self.assertEqual(parse_keybinds(text), ["LEFT_CONTROL", "G", "75", "H"])
        self.assertEqual(parse_hooks(LIB_LUA), [
            "/Script/Engine.PlayerController:Tick",
            "/Game/Blueprints/BP_HUD:UpdateHUD",
        ])

    def test_hook_conflict_is_warning(self):
        spec = ModSpec("cand",
                       lua_sources={"main.lua":
                                    'RegisterHook("/Script/Engine.PlayerController:Tick",\n'
                                    "              function() end)\n"})
        rep = self.check(spec)
        self.assertTrue(rep.ok)
        c = next(c for c in rep.conflicts if c.kind == HOOK_CONFLICT)
        self.assertEqual(c.severity, "warning")
        self.assertEqual(c.detail["hook"], "/Script/Engine.PlayerController:Tick")
        self.assertIn("lib-b", c.mods)

        other = self.check(ModSpec("cand",
                                   lua_sources={"main.lua":
                                                'RegisterHook("/Script/Engine.PlayerController:BeginPlay", fn)\n'}))
        self.assertEqual([c for c in other.conflicts if c.kind == HOOK_CONFLICT],
                         [])

    def test_version_mismatch(self):
        rep = self.check(ModSpec("cand", game_versions=(">=2.000",)))
        self.assertFalse(rep.ok)
        self.assertEqual(rep.blockers[0].kind, VERSION_MISMATCH)
        self.assertEqual(rep.blockers[0].detail["target"], "1.004.000")

        self.assertTrue(self.check(ModSpec("cand", game_versions=("1.004.000",))).ok)
        self.assertTrue(self.check(ModSpec("cand", game_versions=(">=1.004", "<2"))).ok)
        self.assertTrue(self.check(ModSpec("cand")).ok)  # no constraint -> skip

        # explicit target overrides the state's game_version
        rep2 = self.check(ModSpec("cand", game_versions=("1.004.000",)),
                          game_version="2.000.000")
        self.assertFalse(rep2.ok)

    def test_missing_dependency(self):
        rep = self.check(ModSpec("cand", requires=("absent-mod",)))
        self.assertFalse(rep.ok)
        c = rep.blockers[0]
        self.assertEqual(c.kind, MISSING_DEPENDENCY)
        self.assertEqual(c.detail["dependency"], "absent-mod")

        self.assertTrue(self.check(ModSpec("cand", requires=("lib-b",))).ok)

    def test_batch_conflicts_and_peer_dependency(self):
        lua = ("RegisterKeyBind(Key.G, function() end)\n"
               "RegisterHook('/Script/Mods.Common:Init', InitCB)\n")
        c1 = ModSpec("c1", files=("Mods/dupe/x.lua",), lua_sources={"x.lua": lua})
        c2 = ModSpec("c2", files=("Mods/dupe/x.lua",), lua_sources={"x.lua": lua})
        c3 = ModSpec("c3", requires=("c1",), lua_sources={"y.lua": ""})

        rep = check_batch([c1, c2, c3], self.state, game_root=self.game)

        kinds = [c.kind for c in rep.conflicts]
        self.assertEqual(kinds.count(PATH_CONFLICT), 1)      # deduped pair
        self.assertEqual(kinds.count(KEYBIND_CONFLICT), 1)   # deduped pair
        self.assertEqual(kinds.count(HOOK_CONFLICT), 1)      # deduped pair
        self.assertNotIn(MISSING_DEPENDENCY, kinds)          # c1 satisfies c3 in-batch
        self.assertFalse(rep.ok)                             # PATH_CONFLICT blocks

        pc = next(c for c in rep.conflicts if c.kind == PATH_CONFLICT)
        self.assertEqual(set(pc.mods), {"c1", "c2"})
        self.assertEqual(pc.detail["path"], "Mods/dupe/x.lua")

    def test_registry_from_state_file(self):
        state_file = self.game / "install_state.json"
        state_file.write_text(json.dumps(self.state), encoding="utf-8")
        reg = InstalledRegistry.from_state_file(state_file, game_root=self.game)
        self.assertEqual(reg.owners_of("Mods/libb/scripts/lib.lua"), ["lib-b"])
        self.assertEqual(reg.owners_of("Mods/other/lua"), [])
        self.assertEqual(reg.keybind_index().get("F"), {"lib-b"})
        self.assertEqual(reg.keybind_index().get("J"), None)  # commented out in lib
        self.assertEqual(sorted(reg.hook_index()["/Script/Engine.PlayerController:Tick"]),
                         ["lib-b"])
        self.assertEqual(reg.game_version, "1.004.000")

    def test_clean_candidate_reports_nothing(self):
        spec = ModSpec("clean",
                       files=("Mods/clean/scripts/main.lua",),
                       requires=("lib-b",),
                       game_versions=("1.004",),
                       lua_sources={"main.lua":
                                    "RegisterKeyBind(Key.Z, function() end)\n"})
        rep = self.check(spec)
        self.assertEqual(rep.conflicts, [])
        self.assertTrue(rep.ok)


if __name__ == "__main__":
    unittest.main(verbosity=2)
