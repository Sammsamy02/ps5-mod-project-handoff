#!/usr/bin/env python3
"""Host tests for worker-app-modmanager: the mod-manager core loop against a
fake /data/modbridge tree, plus the app<->console contract artifacts.

Everything runs against the PROVEN composed installer binary (vendored here,
sha-pinned) and REAL formats: records.txt, loadlist.txt, mfb bundles,
e2e-cmd.json files, e2e-status.jsonl. No network, no console.

Run:  python3 test/run_tests.py
"""

import json
import os
import shutil
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.join(HERE, "..", "app")
sys.path.insert(0, APP)

import modmgr  # noqa: E402
import console_link as cl  # noqa: E402

BIN = os.path.join(HERE, "..", "vendored", "modinst_composed")
BIN_SHA = "f2215f2ffd92b63c7d642089e60c37e10ba199ff279d0aaa201ff35324e21de6"
TITLE = "PPSA33286"

# Pinned from worker-e2e-staging-final/cmd/SHASUMS.txt (STAGED-E2E-COMMANDS.md section 2)
STAGED_SHAS = {
    "recover":                 "a414d015e3d5bfd2f6614090e2940c19522c44e03c5ecde6e8adb1c81e2dcab1",
    "stage0-records":          "ef2e203663e002fa311995c37a6cd5ae5060cf1ff5b07f6b8a8adfed0f919ac3",
    "stage1-register":         "a7be7864a7c44c0739b2afadf8944b421846eddcb6cb133ccd83040b222f97e3",
    "stage2a-prepare-archive": "77e7e03a801e75327ce856f6c9afda1635b8733cc5fe41666924231957f714b0",
    "stage2b-1-register":      "7c4f9d8045a8ad6979f848edbeb3d0b10352aaed5c27b7169d19a9755a296827",
    "stage2b-2-register":      "140c6bbebcca323eccc36de363cc82fd5ff740d268f4baf5811fcf8ffb4a1c2a",
    "stage2b-3-prepare":       "1ce4f601d71e31e74ba620761d7e1bea1af63f489de1204e1f30aa376cd4b509",
    "stage3-install":          "d7834dd599fb3f36c846756a49563ee1dc3206e214751ee774275d713b21af52",
    "stage4-activated":        "f09a87ba8648942aad16d25c1e53b2b5e1de0b42a62679837ec0fe8ef6fa42ef",
    "stage5-mods-started":     "a63cf9c5ee5e4a667a49d9a983da949890d83538e50e91cf4e3ca4499ee3e8f5",
    "stage6-1-remove":         "789474c8972d100ce862fa1560fd294b3c2f76a5b807fe60946f028086a6cb81",
    "stage6-2-remove":         "d35255483210bdb919073ef0071c0dd41e27aec01611ead021046f6883026407",
    "stage6-3-remove":         "780964191a76a047b8cd72d83f25c6397cb7f6f7f088ffabe5f002a2217ca009",
}


def make_fixture():
    """A fake /data/modbridge: state/ roots/ intake/ + caps + two bundles."""
    base = tempfile.mkdtemp(prefix="modbridge-")
    state = os.path.join(base, "state")
    roots = os.path.join(base, "roots")
    intake = os.path.join(base, "intake")
    for d in (state, roots, intake):
        os.makedirs(d)
    with open(os.path.join(state, "caps.tsv"), "w") as f:
        f.write("# runtime components on PS5 (worker-7 seed)\n"
                "component pak-loader available\n"
                "component ue4ss-ps5 integrating\n")
    b = modmgr.Installer(BIN, state, roots)
    lua = os.path.join(intake, "mm-lua.mfb")
    b.run_raw(["make-bundle", "--id", "mm-lua", "--title", TITLE,
               "--framework", "ue4ss-ps5",
               "--file", "lua_mod,ue4ss/Mods/mm-lua/Scripts/main.lua=print('mm-lua')",
               "--file", "lua_mod,ue4ss/Mods/mm-lua/enabled.txt=",
               "-o", lua])
    pak = os.path.join(intake, "mm-pak.mfb")
    b.run_raw(["make-bundle", "--id", "mm-pak", "--title", TITLE, "--prio", "50",
               "--file", "pak,mods/~mods/mm-pak_P.pak=PAKBYTES-0123456789",
               "-o", pak])
    zpath = os.path.join(intake, "zb-lua.zip")
    with zipfile.ZipFile(zpath, "w") as z:
        z.writestr("ue4ss/Mods/zb-lua/Scripts/main.lua", "print('zb-lua')")
        z.writestr("ue4ss/Mods/zb-lua/enabled.txt", "")
    return {"base": base, "state": state, "roots": roots, "intake": intake,
            "lua": lua, "pak": pak, "zip": zpath}


class Base(unittest.TestCase):
    def setUp(self):
        self.fx = make_fixture()
        self.mm = modmgr.ModManager(BIN, self.fx["state"], self.fx["roots"],
                                    pinned_sha=BIN_SHA)

    def tearDown(self):
        shutil.rmtree(self.fx["base"], ignore_errors=True)

    def loadlist(self):
        return open(os.path.join(self.fx["roots"], TITLE, "loadlist.txt"),
                    encoding="utf-8").read()


class TestList(Base):
    def test_01_empty(self):
        st = self.mm.list_installed(TITLE)
        self.assertEqual(st["mods"], [])
        self.assertFalse(st["installed"])
        self.assertIsNone(st["live_sha"])

    def test_02_list_after_install(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        st = self.mm.list_installed(TITLE)
        self.assertTrue(st["installed"])
        self.assertEqual(st["generation"], 1)
        self.assertFalse(st["loadlist_diverged"])
        m = st["mods"][0]
        self.assertEqual(m["id"], "mm-lua")
        self.assertEqual(m["state"], "installed")
        self.assertTrue(m["enabled"])
        self.assertEqual(m["files"], 2)
        self.assertGreater(m["bytes"], 0)
        # live loadlist grammar + marker
        root = os.path.join(self.fx["roots"], TITLE)
        self.assertTrue(os.path.exists(os.path.join(root, modmgr.MARKER)))
        ll = modmgr.LoadList.load(os.path.join(root, "loadlist.txt"))
        ll.require_header()
        self.assertEqual(ll.mods(), [("mm-lua", "1.0.0")])
        roles = {p: s for _r, p, s in ll.files()}
        self.assertEqual(roles.get("ue4ss/Mods/mm-lua/enabled.txt"), 0)  # empty, presence-only

    def test_03_diverged_detection(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        llp = os.path.join(self.fx["roots"], TITLE, "loadlist.txt")
        with open(llp, "a", encoding="utf-8") as f:
            f.write("# stray out-of-band edit\n")
        st = self.mm.list_installed(TITLE)
        self.assertTrue(st["loadlist_diverged"])


class TestInstall(Base):
    def test_04_install_mfb(self):
        out = self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.assertIn("installed title=%s" % TITLE, out)
        rec = self.mm.inst.records()
        self.assertEqual(rec.mod("mm-lua")["state"], "installed")

    def test_05_install_zip_via_bridge(self):
        out = self.mm.install(TITLE, self.fx["zip"], "zb-lua")
        self.assertIn("installed title=%s" % TITLE, out)
        rec = self.mm.inst.records()
        m = rec.mod("zb-lua")
        self.assertEqual(m["state"], "installed")
        self.assertTrue(m["bundle"].endswith(os.path.join("bundles", "zb-lua.mfb")),
                        "records bundle must be repointed to the bridged mfb")
        self.assertTrue(modmgr.read_mfb_paths(m["bundle"]))  # readable index

    def test_06_second_install_preserves_first(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.mm.install(TITLE, self.fx["pak"], "mm-pak")
        st = self.mm.list_installed(TITLE)
        ids = {m["id"] for m in st["mods"]}
        self.assertEqual(ids, {"mm-lua", "mm-pak"})
        enabled = {m["id"] for m in st["mods"] if m["enabled"]}
        self.assertEqual(enabled, {"mm-lua", "mm-pak"})
        # load order: priority then id (mm-pak prio 50 first, later wins)
        self.assertEqual(modmgr.LoadList.parse(self.loadlist()).mods(),
                         [("mm-pak", "1.0.0"), ("mm-lua", "1.0.0")])

    def test_07_duplicate_register_refused(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.assertEqual(cm.exception.kind, "conflict")

    def test_08_unsupported_source_refused(self):
        p = os.path.join(self.fx["intake"], "nope.bin")
        open(p, "w").write("x")
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.install(TITLE, p, "nope")
        self.assertEqual(cm.exception.kind, "bad_request")


class TestDisableEnable(Base):
    def setUp(self):
        super().setUp()
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.mm.install(TITLE, self.fx["pak"], "mm-pak")

    def test_09_disable_removes_only_that_mods_rows(self):
        before = self.loadlist()
        r = self.mm.disable(TITLE, "mm-lua")
        after = self.loadlist()
        ll = modmgr.LoadList.parse(after)
        self.assertFalse(ll.has_mod("mm-lua"))
        self.assertTrue(ll.has_mod("mm-pak"))
        self.assertEqual([p for _r, p, _s in ll.files()],
                         ["mods/~mods/mm-pak_P.pak"],
                         "exactly the disabled mod's file rows go (bundle attribution)")
        # files stay on disk
        self.assertTrue(os.path.exists(os.path.join(self.fx["roots"], TITLE,
                                                    "ue4ss/Mods/mm-lua/Scripts/main.lua")))
        self.assertTrue(os.path.exists(os.path.join(self.fx["roots"], TITLE,
                                                    "ue4ss/Mods/mm-lua/enabled.txt")))
        # records resync verified through the binary's own reader
        rec = self.mm.inst.records()
        self.assertEqual(rec.game(TITLE)["loadlist_sha"], r["loadlist_sha"])
        # list shows disabled overlay + honest states
        st = self.mm.list_installed(TITLE)
        by = {m["id"]: m for m in st["mods"]}
        self.assertFalse(by["mm-lua"]["enabled"])
        self.assertTrue(by["mm-pak"]["enabled"])
        self.assertEqual(by["mm-lua"]["state"], "installed")  # disable is not uninstall
        self.assertEqual(st["disabled_overlay"], ["mm-lua"])
        _ = before

    def test_10_enable_restores_bytes_exactly(self):
        before = self.loadlist()
        self.mm.disable(TITLE, "mm-lua")
        r = self.mm.enable(TITLE, "mm-lua")
        self.assertEqual(self.loadlist(), before, "enable must restore exact bytes")
        rec = self.mm.inst.records()
        self.assertEqual(rec.game(TITLE)["loadlist_sha"], r["loadlist_sha"])
        st = self.mm.list_installed(TITLE)
        self.assertEqual(st["disabled_overlay"], [])
        self.assertTrue(all(m["enabled"] for m in st["mods"]))

    def test_11_disable_survives_reinstall(self):
        self.mm.disable(TITLE, "mm-lua")
        ini = os.path.join(self.fx["intake"], "mm-cfg.mfb")
        self.mm.inst.run_raw(["make-bundle", "--id", "mm-cfg", "--title", TITLE,
                              "--file", "config,ue4ss/PS5Pad.ini=pad=1",
                              "-o", ini])
        self.mm.install(TITLE, ini, "mm-cfg")
        ll = modmgr.LoadList.parse(self.loadlist())
        self.assertTrue(ll.has_mod("mm-cfg"))
        self.assertFalse(ll.has_mod("mm-lua"),
                         "transactional reinstall must not silently re-enable")
        st = self.mm.list_installed(TITLE)
        self.assertEqual(st["disabled_overlay"], ["mm-lua"])

    def test_12_guards(self):
        root = os.path.join(self.fx["roots"], TITLE)
        # unmanaged root refused
        marker = os.path.join(root, modmgr.MARKER)
        os.rename(marker, marker + ".held")
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.disable(TITLE, "mm-lua")
        self.assertEqual(cm.exception.kind, "bad_request")
        os.rename(marker + ".held", marker)
        # unknown mod refused
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.disable(TITLE, "nope")
        self.assertEqual(cm.exception.kind, "conflict")
        # enable without overlay refused
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.enable(TITLE, "mm-lua")
        self.assertEqual(cm.exception.kind, "not_found")
        # corrupt header refused
        llp = os.path.join(root, "loadlist.txt")
        open(llp, "w").write("garbage\n")
        with self.assertRaises(modmgr.ModMgrError) as cm:
            self.mm.disable(TITLE, "mm-lua")
        self.assertEqual(cm.exception.kind, "bad_request")


class TestUninstall(Base):
    def test_13_uninstall_removal_semantics(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.mm.install(TITLE, self.fx["pak"], "mm-pak")
        gen0 = self.mm.list_installed(TITLE)["generation"]
        self.mm.uninstall(TITLE, "mm-pak")
        st = self.mm.list_installed(TITLE)
        ids = {m["id"]: m for m in st["mods"]}
        self.assertEqual(ids["mm-pak"]["state"], "prepared")   # F-R2 removal: back to prepared
        self.assertEqual(ids["mm-lua"]["state"], "installed")  # survivor untouched
        self.assertEqual(st["generation"], gen0 + 1)
        ll = modmgr.LoadList.parse(self.loadlist())
        self.assertFalse(ll.has_mod("mm-pak"))
        self.assertTrue(ll.has_mod("mm-lua"))
        self.assertFalse(os.path.exists(os.path.join(self.fx["roots"], TITLE,
                                                     "mods/~mods/mm-pak_P.pak")))

    def test_14_uninstall_cleans_overlay(self):
        self.mm.install(TITLE, self.fx["lua"], "mm-lua")
        self.mm.disable(TITLE, "mm-lua")
        self.mm.uninstall(TITLE, "mm-lua")
        self.assertEqual(self.mm.list_installed(TITLE)["disabled_overlay"], [])


class TestConsoleContract(unittest.TestCase):
    def test_15_staged_sequence_reproduces_pinned_shas(self):
        seq = cl.staged_sequence()
        self.assertEqual(len(seq), 13)
        for name, blob in seq:
            self.assertIn(name, STAGED_SHAS, name)
            self.assertEqual(cl.command_file_sha(blob), STAGED_SHAS[name],
                             "%s bytes diverge from the pinned staged file" % name)

    def test_16_schema_refusals(self):
        R = cl.CommandRefused
        S, Rt = "/data/modbridge/state", "/data/modbridge/roots"
        good = lambda **kw: cl.build_command("records", S, Rt, **kw)  # noqa: E731
        cl.validate_command(json.loads(good()))
        with self.assertRaises(R):  # unknown verb
            cl.build_command("make-bundle", S, Rt)
        with self.assertRaises(R):  # mods with records
            cl.build_command("records", S, Rt, mods=["x"])
        with self.assertRaises(R):  # mods + remove
            cl.build_command("install", S, Rt, mods=["a"], options={"remove": "b"})
        with self.assertRaises(R):  # unknown option key (fault injection denied)
            cl.build_command("install", S, Rt, options={"crash_at": "rename1"})
        with self.assertRaises(R):  # option illegal for verb
            cl.build_command("records", S, Rt, options={"rt_state": "activated"})
        with self.assertRaises(R):  # no_auto_bundle must be true-only
            cl.build_command("prepare-archive", S, Rt, options={"no_auto_bundle": False})
        with self.assertRaises(R):  # unknown top-level key
            bad = json.loads(good())
            bad["extra"] = 1
            cl.validate_command(bad)

    def test_17_transcript_pairing(self):
        ok = "\n".join([
            '{"event":"begin","cmd":"install","title":"P","stage":"stage-3-install"}',
            '{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"mm-lua"}',
            '{"event":"end","exit":0}',
        ]) + "\n"
        t = cl.E2eTranscript(ok)
        self.assertEqual(t.pairing(), (1, 1, 0))
        self.assertTrue(t.validate()[0])
        self.assertEqual(t.last_exit(), 0)
        self.assertEqual(len(t.status_events(stage="stage-3-install", outcome="ok")), 1)
        dead = '{"event":"begin","cmd":"install","title":"P","stage":"s"}\n'
        t2 = cl.E2eTranscript(dead)
        self.assertEqual(t2.pairing(), (1, 0, 1))
        self.assertFalse(t2.validate()[0])
        t3 = cl.E2eTranscript('{"event":"truncated"}\n')
        self.assertTrue(t3.truncated)
        self.assertFalse(t3.validate()[0])

    def test_18_pinned_binary_enforced(self):
        with self.assertRaises(modmgr.ModMgrError):
            modmgr.ModManager(BIN, self.fx_dir(), self.fx_dir(),
                              pinned_sha="0" * 64)

    def fx_dir(self):
        import tempfile
        return tempfile.mkdtemp(prefix="sha-")


if __name__ == "__main__":
    unittest.main(verbosity=2)
