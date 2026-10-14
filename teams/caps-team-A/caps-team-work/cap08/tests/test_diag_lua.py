#!/usr/bin/env python3
"""Generated zzintake diagnostic: probe choice (generic, from the load graph),
syntax (luac -p) and behaviour under a stub UE4SS harness (host Lua)."""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import test_intake_synthetic as ts  # noqa: E402

RUNS = 8  # the stub LoopAsync calls the callback this many times (the real one never stops)

HARNESS = r'''
local main, mode, list = arg[1], arg[2], arg[3]
RUN = 0
local objs = {}
local function obj(path, parent)
  return {IsValid=function(self) return true end,
          GetFullName=function(self) return "Object "..path end, Parent=parent}
end
local order = {}
for line in io.lines(list) do
  local o, p = line:match("^(%S+)%s*(%S*)$")
  if o then
    local par = nil
    if p ~= "" then par = obj(p, nil) end
    if mode == "wrong" and p ~= "" then par = obj("/Game/Other/M_Wrong.M_Wrong", nil) end
    if mode == "unread" and p ~= "" then par = nil end
    objs[o] = obj(o, par)
    order[#order + 1] = o
  end
end
-- "drop=<substring>": that object is never loaded
local drop = mode:match("^drop=(.+)$")
function StaticFindObject(p)
  if mode == "late" and RUN < 4 then return nil end
  if drop and p:find(drop, 1, true) then return nil end
  return objs[p]
end
function ExecuteInGameThread(f) f() end
function LoopAsync(ms, f) for i = 1, ]] .. RUNS .. [[ do RUN = i if f() then print("LOOP-STOPPED") break end end end
dofile(main)
'''.replace("]] .. RUNS .. [[", str(RUNS))


def lua_available():
    return shutil.which("lua") and shutil.which("luac")


def build_world(tmp):
    """Base game: a mesh using two material instances.  The mod REPLACES the mesh
    and one instance, adds its own PC-only material + texture + instance, and
    also replaces an instance nothing else in the mod uses (self-only)."""
    w = ts.World(tmp)
    w.base([ts.base_material("/Game/Base/M_Master"),
            ts.mic("/Game/Base/MI_Skin", "/Game/Base/M_Master"),
            ts.mic("/Game/Base/MI_Lonely", "/Game/Base/M_Master"),
            ts.mesh("/Game/Base/SK_Body", ["/Game/Base/MI_Skin"])])
    w.mod([ts.mesh("/Game/Base/SK_Body", ["/Game/Base/MI_Skin", "/Game/Mod/MI_Child"]),
           ts.mic("/Game/Base/MI_Skin", "/Game/Mod/M_PC"),
           ts.mic("/Game/Base/MI_Lonely", "/Game/Mod/M_PC"),
           ts.pc_material("/Game/Mod/M_PC", ["/Game/Mod/T_A"]), ts.tex("/Game/Mod/T_A"),
           ts.mic("/Game/Mod/MI_Child", "/Game/Mod/M_PC")])
    return w


class ProbeChoiceTests(unittest.TestCase):
    """Pure Python: no Lua needed."""
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cap08-probe-")
        w = build_world(self.tmp)
        self.diag = os.path.join(self.tmp, "diag")
        w.run(extra=["--emit-diag", self.diag])
        self.r = w.report
        self.main = open(os.path.join(self.diag, "zzintake", "Scripts", "main.lua")).read()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_probe_set_is_linked_and_skips_self_only(self):
        import chunk_intake as ci
        e = ci.diag_entries(self.r)
        by_obj = {x["o"].rsplit(".", 1)[-1]: x for x in e}
        print("  probes:", [(i + 1, x["o"].rsplit(".", 1)[-1], x["g"], x["b"]) for i, x in enumerate(e)])
        self.assertEqual(e[0]["o"].rsplit(".", 1)[-1], "SK_Body")      # the linker comes first
        self.assertEqual((by_obj["SK_Body"]["g"], by_obj["SK_Body"]["b"]), ("R", []))
        self.assertEqual((by_obj["MI_Skin"]["g"], by_obj["MI_Skin"]["b"]), ("R", [1]))
        self.assertIn("p", by_obj["MI_Skin"])                           # parent check on a replaced object
        for n in ("M_PC", "T_A", "MI_Child"):
            self.assertEqual(by_obj[n]["g"], "N")
            self.assertIn(1, by_obj[n]["b"], n)                        # every new probe is linked
        self.assertNotIn("MI_Lonely", by_obj)                          # reached only from itself: skipped
        self.assertEqual(self.r["diag"]["probes"], len(e))
        self.assertEqual(self.r["diag"]["replaced_probes"], 2)

    def test_pure_add_mod_falls_back_to_unlinked_probes(self):
        import chunk_intake as ci
        tmp = os.path.join(self.tmp, "w2")
        w = ts.World(tmp)
        w.base([ts.base_material("/Game/Base/M_Master")])
        w.mod([ts.pc_material("/Game/Mod/M_PC", ["/Game/Mod/T_A"]), ts.tex("/Game/Mod/T_A")])
        w.run()
        e = ci.diag_entries(w.report)
        self.assertGreater(len(e), 0)
        self.assertTrue(all(x["g"] == "N" and x["b"] == [] for x in e))


@unittest.skipUnless(lua_available(), "host lua/luac not installed")
class DiagLuaTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cap08-lua-")
        w = build_world(self.tmp)
        self.diag = os.path.join(self.tmp, "diag")
        w.run(extra=["--emit-diag", self.diag])
        self.report = w.report
        self.main = os.path.join(self.diag, "zzintake", "Scripts", "main.lua")
        # object list for the stub: every probe object, with its predicted parent
        self.objs = os.path.join(self.tmp, "objs.txt")
        with open(self.objs, "w") as f:
            for line in open(self.main):
                if line.startswith(" {o="):
                    o = re.search(r'o="([^"]+)"', line).group(1)
                    p = re.search(r'p="([^"]+)"', line)
                    f.write("%s %s\n" % (o, p.group(1) if p else ""))
        self.harness = os.path.join(self.tmp, "harness.lua")
        open(self.harness, "w").write(HARNESS)
        self.n = self.report["diag"]["probes"]

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _run(self, mode):
        out = subprocess.run(["lua", self.harness, self.main, mode, self.objs], capture_output=True, text=True, timeout=30)
        self.assertEqual(out.returncode, 0, out.stderr)
        return out.stdout

    @staticmethod
    def _sums(out):
        return [l for l in out.splitlines() if "[zi] SUM " in l]

    def test_generated_file_is_valid_lua_and_generic(self):
        self.assertTrue(os.path.isfile(os.path.join(self.diag, "zzintake", "enabled.txt")))
        r = subprocess.run(["luac", "-p", self.main], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        tmpl = open(os.path.join(HERE, "..", "PATCH", "pak-intake", "zzintake_template.lua")).read()
        self.assertNotIn("/Game/", tmpl)  # template carries no game/mod names
        self.assertGreater(self.n, 0)
        print("  diag probes:", self.n, "bytes:", self.report["diag"]["main_lua_bytes"])

    def test_all_found_is_pass_and_probing_never_stops(self):
        out = self._run("all")
        self.assertIn("[zi] Z0 v2 n=%d" % self.n, out)
        sums = self._sums(out)
        self.assertEqual(len(sums), RUNS)            # F1: no self-stop after 150 s
        self.assertNotIn("LOOP-STOPPED", out)
        self.assertIn("found=%d/%d" % (self.n, self.n), sums[-1])
        self.assertRegex(sums[-1], r"parents=(\d+)/\1 ")
        self.assertIn("state=PASS", sums[-1])
        self.assertEqual(out.count("[zi] P1 "), 1)   # P lines only when they change
        self.assertIn("v=F", out)

    def test_late_load_is_not_yet_then_pass(self):
        out = self._run("late")                      # F1: menus/loading take longer than the first probes
        sums = self._sums(out)
        self.assertIn("state=NOT-LOADED-YET", sums[0])
        self.assertIn("f=0:notloaded", out)
        self.assertIn("state=PASS", sums[-1])
        self.assertEqual(out.count("[zi] P1 "), 2)   # printed again when it changed

    def test_unused_replaced_object_is_inconclusive_not_fail(self):
        out = self._run("drop=SK_Body")              # F2: the game never loads that replaced mesh
        self.assertIn("f=0:notloaded", out)
        self.assertNotIn("f=0:missing", out)
        self.assertIn("state=PASS", self._sums(out)[-1])  # MI_Skin (replaced, linked) + new objects prove it

    def test_new_object_missing_under_found_importer_is_fail(self):
        out = self._run("drop=MI_Child")             # SK_Body loaded but its new import is not: mount missed it
        self.assertIn("f=0:missing", out)
        self.assertIn("state=MISSING", self._sums(out)[-1])

    def test_wrong_parent_is_visible(self):
        out = self._run("wrong")
        self.assertIn("par=0:M_Wrong", out)
        self.assertIn("state=PARENT-WRONG", self._sums(out)[-1])

    def test_unread_parent_is_its_own_case(self):
        out = self._run("unread")                    # F2: .Parent read failed or null, not a wrong prediction
        self.assertIn("par=0:none", out)
        self.assertIn("state=PARENT-UNREAD", self._sums(out)[-1])
        self.assertIn("par_wrong=0", self._sums(out)[-1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
