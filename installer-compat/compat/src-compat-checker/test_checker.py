#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_checker.py -- unit + integration tests for checker.py.

Mac-only host tests: pure static file processing, no FTP, no payloads,
no launches, no console contact, no Lua execution.
Run:  python3 -m unittest test_checker -v   (or python3 test_checker.py)
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import checker  # noqa: E402

SAMPLE_MOD = os.path.join(HERE, "sample_mod.lua")
SAMPLE_CATALOG = os.path.join(HERE, "sample_catalog.json")

# Minimal-schema catalog (exactly the fields the catalog agents produce;
# no "kind"/"type" fields -- kind must be derived from name shape).
MINI_CATALOG = [
    {"name": "RegisterKeyBind", "category": "input", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "RegisterHook", "category": "hooking", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "Loop", "category": "threading", "ps5_runtime_status": "implemented-untested"},
    {"name": "FindFirstOf", "category": "reflection", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "GetPlayerController", "category": "game", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "FName", "category": "core-type", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "FName:ToString", "category": "core-type", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "FString", "category": "core-type", "ps5_runtime_status": "implemented-untested"},
    {"name": "FString:Len", "category": "core-type", "ps5_runtime_status": "missing"},
    {"name": "ExecuteAsync", "category": "threading", "ps5_runtime_status": "missing"},
    {"name": "ExecuteInGameThread", "category": "threading", "ps5_runtime_status": "behaves-differently"},
    {"name": "UObject:GetFullName", "category": "reflection", "ps5_runtime_status": "implemented-untested"},
    {"name": "UObject:IsValid", "category": "reflection", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "AActor:GetActorLocation", "category": "game", "ps5_runtime_status": "implemented+console-proven"},
    {"name": "EInputEvent.IE_Pressed", "category": "input", "ps5_runtime_status": "implemented+console-proven"},
]


def scan_text(lua_text, catalog=MINI_CATALOG):
    """Write lua text + catalog to a temp dir and scan it (host-only)."""
    with tempfile.TemporaryDirectory() as td:
        mod = os.path.join(td, "mod.lua")
        cat = os.path.join(td, "catalog.json")
        with open(mod, "w", encoding="utf-8") as fh:
            fh.write(lua_text)
        with open(cat, "w", encoding="utf-8") as fh:
            json.dump(catalog, fh)
        return checker.scan(mod, cat)


def apis_by_name(result):
    out = {}
    for bucket_key in ("missing", "behaves_differently"):
        for r in result[bucket_key]:
            out[r["api"]] = r
    for r in result["unknown"]:
        out[r["api"]] = r
    # matched-ok APIs live in details_per_file; index those too
    for d in result["details_per_file"].values():
        for a in d["apis"]:
            out.setdefault(a["api"], a)
    return out


class TestCatalogParsing(unittest.TestCase):
    def test_minimal_schema_derives_kind_from_name(self):
        with tempfile.TemporaryDirectory() as td:
            cat = os.path.join(td, "c.json")
            with open(cat, "w") as fh:
                json.dump(MINI_CATALOG, fh)
            c = checker.Catalog.load(cat)
        self.assertIn("FindFirstOf", c.globals)
        self.assertIn("FName", c.types)                     # category mentions "type"
        self.assertIn(("FName", "ToString"), c.methods)     # "T:m" -> method
        self.assertIn(("EInputEvent", "IE_Pressed"), c.statics)  # "T.m" -> constant

    def test_versioned_wrapper_accepted(self):
        with tempfile.TemporaryDirectory() as td:
            cat = os.path.join(td, "c.json")
            with open(cat, "w") as fh:
                json.dump({"catalog_version": "v9", "entries": MINI_CATALOG}, fh)
            c = checker.Catalog.load(cat)
        self.assertEqual(c.version, "v9")
        self.assertIn("RegisterHook", c.globals)


class TestScanningRules(unittest.TestCase):
    def test_global_call_matched_with_counts_and_sites(self):
        r = scan_text("FindFirstOf('A')\nFindFirstOf('B')\nFindFirstOf('C')\n")
        names = apis_by_name(r)
        self.assertIn("FindFirstOf", names)
        d = r["details_per_file"]["mod.lua"]
        self.assertEqual(d["matched_calls"], 3)
        self.assertEqual(r["grade"], "compatible")

    def test_type_inference_fname_constructor(self):
        r = scan_text("local n = FName('x')\nlocal s = n:ToString()\n")
        names = apis_by_name(r)
        self.assertIn("FName", names)
        self.assertIn("FName:ToString", names)
        self.assertEqual(r["grade"], "compatible")

    def test_runtime_type_method_not_in_catalog_is_unknown(self):
        r = scan_text("local n = FName('x')\nreturn n:Bogus()\n")
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("FName:Bogus", [u["api"] for u in r["unknown"]])

    def test_findfirstof_typing_and_base_chain(self):
        r = scan_text(
            "local t = FindFirstOf('AFairyThing')\n"
            "local s = t:GetFullName()\n"       # via UObject base
            "local v = t:GetActorLocation()\n"  # via AActor base
        )
        names = apis_by_name(r)
        self.assertEqual(names["UObject:GetFullName"]["status"], "implemented-untested")
        self.assertEqual(names["AActor:GetActorLocation"]["status"],
                         "implemented+console-proven")
        self.assertEqual(r["grade"], "compatible")

    def test_findallof_array_element_typing(self):
        r = scan_text(
            "local arr = FindAllOf('AFairyItem')\n"  # not in mini catalog -> unknown global
            "for _, it in pairs(arr) do\n"
            "    it:IsValid()\n"
            "end\n"
        )
        # FindAllOf itself is unknown in the mini catalog -> unknown grade
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("FindAllOf", [u["api"] for u in r["unknown"]])
        names = apis_by_name(r)
        # loop var typing worked: it:IsValid resolved via UObject base, not unresolved
        self.assertIn("UObject:IsValid", names)

    def test_untyped_param_method_is_unresolved_not_unknown(self):
        r = scan_text("local function f(x)\n    return x:Whatever()\nend\n")
        self.assertEqual(r["unresolved_calls"], 1)
        self.assertEqual(r["unknown_calls"], 0)
        self.assertEqual(r["unresolved_method_calls"][0]["api"], "x:Whatever")

    def test_lua_stdlib_not_unknown(self):
        r = scan_text(
            "local t = {}\n"
            "table.insert(t, 5)\n"
            "print(string.format('%d', 1))\n"
            "for k, v in pairs(t) do print(k, tostring(v)) end\n"
        )
        self.assertEqual(r["unknown_calls"], 0)
        self.assertEqual(r["unmatched_calls"], 0)
        self.assertGreater(r["builtin_calls"], 0)
        self.assertEqual(len(r["unknown"]), 0)

    def test_local_functions_and_method_defs_ignored(self):
        r = scan_text(
            "local function Log(msg)\n"
            "    print(msg)\n"
            "end\n"
            "Log('x')\n"
            "local M = {}\n"
            "function M:Run()\n"
            "    return 1\n"
            "end\n"
        )
        self.assertEqual(r["total_calls"], 0)   # print is builtin; Log/M:Run ignored
        self.assertEqual(len(r["unknown"]), 0)
        self.assertEqual(r["unresolved_calls"], 0)

    def test_comments_and_strings_ignored(self):
        r = scan_text(
            "-- ExecuteAsync(1) FName(2) RegisterHook('x')\n"
            "local s = 'FindFirstOf( inside string'\n"
            "--[[ block comment Loop(3) ]]\n"
        )
        self.assertEqual(r["total_calls"], 0)
        self.assertEqual(len(r["unknown"]), 0)

    def test_constant_reference_matched(self):
        r = scan_text("local p = EInputEvent.IE_Pressed\n")
        names = apis_by_name(r)
        self.assertIn("EInputEvent.IE_Pressed", names)
        self.assertEqual(r["grade"], "compatible")

    def test_unknown_static_namespace(self):
        r = scan_text("local m = ModServices.Manager()\n")
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("ModServices.Manager", [u["api"] for u in r["unknown"]])


class TestGrading(unittest.TestCase):
    def test_compatible(self):
        r = scan_text(
            "local function onKey()\n"
            "    local n = FName('x')\n"
            "    print(n:ToString())\n"
            "end\n"
            "RegisterKeyBind('F6', onKey)\n"
        )
        self.assertEqual(r["grade"], "compatible")
        self.assertEqual(len(r["missing"]), 0)
        self.assertEqual(len(r["unknown"]), 0)

    def test_partly_compatible_peripheral_missing(self):
        r = scan_text(
            "local function helper(s)\n"        # never registered -> not main path
            "    local fs = FString(s)\n"
            "    return fs:Len()\n"             # missing, 1 call, peripheral
            "end\n"
            "RegisterKeyBind('F6', function()\n"
            "    local n = FName('ok')\n"
            "    print(n:ToString())\n"
            "end)\n"
        )
        self.assertEqual(r["grade"], "partly-compatible")
        self.assertEqual(r["missing"][0]["api"], "FString:Len")
        self.assertFalse(r["missing"][0]["central"])
        self.assertEqual(len(r["missing"][0]["call_sites"]), 1)

    def test_unsupported_via_handler_registration(self):
        r = scan_text(
            "local function Handler()\n"
            "    ExecuteAsync()\n"              # missing, inside registered handler
            "end\n"
            "RegisterHook('AFairy:DoThing', Handler)\n"
        )
        self.assertEqual(r["grade"], "unsupported")
        self.assertEqual(r["missing"][0]["api"], "ExecuteAsync")
        self.assertTrue(r["missing"][0]["central"])
        self.assertTrue(r["missing"][0]["call_sites"][0]["main_path"])

    def test_unsupported_via_call_count_over_three(self):
        r = scan_text(
            "local function Save()\n"           # plain function, NOT main path
            "    ExecuteAsync()\n"
            "    ExecuteAsync()\n"
            "    ExecuteAsync()\n"
            "    ExecuteAsync()\n"              # 4 calls > 3 -> central
            "end\n"
        )
        self.assertEqual(r["grade"], "unsupported")
        m = r["missing"][0]
        self.assertEqual(m["api"], "ExecuteAsync")
        self.assertEqual(m["call_count"], 4)
        self.assertTrue(m["central"])
        self.assertFalse(m["call_sites"][0]["main_path"])

    def test_unsupported_via_top_level_main_path(self):
        r = scan_text("ExecuteAsync()\n")       # top level = main code path
        self.assertEqual(r["grade"], "unsupported")

    def test_behaves_differently_never_unsupported(self):
        r = scan_text(
            "local function f()\n"
            "    ExecuteInGameThread(function()\n"
            "        print('x')\n"
            "    end)\n"
            "end\n"
            "f()\n"
        )
        self.assertEqual(r["grade"], "partly-compatible")
        self.assertEqual(r["behaves_differently"][0]["api"], "ExecuteInGameThread")

    def test_unknown_grade(self):
        r = scan_text("local function go()\n    MysteryThing()\nend\ngo()\n")
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("MysteryThing", [u["api"] for u in r["unknown"]])

    def test_precedence_unsupported_beats_partly_and_unknown(self):
        r = scan_text(
            "ExecuteAsync()\n"                   # central missing
            "MysteryThing()\n"                   # unknown too
            "local s = FString('x')\n"
            "local function h()\n    return s:Len()\nend\n"  # peripheral missing
        )
        self.assertEqual(r["grade"], "unsupported")
        self.assertEqual(len(r["missing"]), 2)
        self.assertEqual(len(r["unknown"]), 1)


class TestOutputContract(unittest.TestCase):
    def test_json_schema_keys_and_totals(self):
        r = scan_text(
            "RegisterKeyBind('F6', function()\n"
            "    local fs = FString('a')\n"
            "    fs:Len()\n"
            "end)\n"
        )
        for key in ("grade", "total_calls", "matched_calls", "unmatched_calls",
                    "missing", "behaves_differently", "unknown",
                    "details_per_file"):
            self.assertIn(key, r)
        self.assertEqual(r["total_calls"],
                         r["matched_calls"] + r["unmatched_calls"])
        self.assertTrue(r["mac_only"])
        for m in r["missing"]:
            self.assertIn("call_sites", m)
            self.assertIn("status", m)
            for s in m["call_sites"]:
                self.assertIn("file", s)
                self.assertIn("line", s)

    def test_human_summary_is_mac_only_and_actionable(self):
        text = checker.render_summary(scan_text("ExecuteAsync()\n"))
        self.assertIn("Mac-only", text)
        self.assertIn("UNSUPPORTED", text)
        self.assertIn("ExecuteAsync", text)
        self.assertIn("Runtime-side", text)
        self.assertIn("no console contact", text.lower())


class TestSampleModAndCLI(unittest.TestCase):
    """Integration: the shipped sample mod + sample catalog + CLI end-to-end."""

    @classmethod
    def setUpClass(cls):
        cls.result = checker.scan(SAMPLE_MOD, SAMPLE_CATALOG)

    def test_sample_files_exist(self):
        for p in (SAMPLE_MOD, SAMPLE_CATALOG,
                  os.path.join(HERE, "checker.py")):
            self.assertTrue(os.path.isfile(p), p)

    def test_sample_grade_unsupported(self):
        self.assertEqual(self.result["grade"], "unsupported")

    def test_sample_central_blockers(self):
        central = {m["api"] for m in self.result["missing"] if m["central"]}
        self.assertLessEqual(
            {"ExecuteAsync", "ImGui.Begin", "ImGui.Text",
             "ImGui.SetNextWindowSize", "NotifyOnNewObject",
             "RegisterConsoleCommandHandler"}, central)

    def test_sample_peripheral_missing(self):
        periph = {m["api"] for m in self.result["missing"] if not m["central"]}
        self.assertEqual(periph, {"FString:Len", "UObject:GetAddress"})

    def test_sample_behaves_differently(self):
        self.assertIn("ExecuteInGameThread",
                      [b["api"] for b in self.result["behaves_differently"]])

    def test_sample_unknown(self):
        unknown = {u["api"] for u in self.result["unknown"]}
        self.assertEqual(unknown, {"ModConfig_Load", "ModServices.Manager"})

    def test_sample_uses_about_twenty_apis(self):
        names = apis_by_name(self.result)
        self.assertGreaterEqual(len(names), 20)

    def test_sample_totals_consistent(self):
        r = self.result
        self.assertEqual(r["total_calls"], r["matched_calls"] + r["unmatched_calls"])
        self.assertGreater(r["matched_calls"], 15)
        self.assertGreaterEqual(r["builtin_calls"], 5)

    def test_cli_json_output(self):
        cp = subprocess.run(
            [sys.executable, os.path.join(HERE, "checker.py"),
             SAMPLE_MOD, SAMPLE_CATALOG, "--format", "json"],
            capture_output=True, text=True)
        self.assertEqual(cp.returncode, 0, cp.stderr)
        data = json.loads(cp.stdout)
        self.assertEqual(data["grade"], "unsupported")
        self.assertIn("ModConfig_Load", [u["api"] for u in data["unknown"]])

    def test_cli_text_output_and_json_file(self):
        with tempfile.TemporaryDirectory() as td:
            out = os.path.join(td, "report.json")
            cp = subprocess.run(
                [sys.executable, os.path.join(HERE, "checker.py"),
                 SAMPLE_MOD, SAMPLE_CATALOG, "--json", out],
                capture_output=True, text=True)
            self.assertEqual(cp.returncode, 0, cp.stderr)
            self.assertIn("GRADE: UNSUPPORTED", cp.stdout)
            with open(out) as fh:
                data = json.load(fh)
            self.assertEqual(data["grade"], "unsupported")
            self.assertTrue(data["mac_only"])

    def test_cli_directory_input(self):
        with tempfile.TemporaryDirectory() as td:
            sub = os.path.join(td, "Scripts")
            os.makedirs(sub)
            with open(os.path.join(sub, "a.lua"), "w") as fh:
                fh.write("local n = FName('x')\nreturn n:ToString()\n")
            with open(os.path.join(td, "catalog.json"), "w") as fh:
                json.dump(MINI_CATALOG, fh)
            r = checker.scan(td, os.path.join(td, "catalog.json"))
        self.assertEqual(r["grade"], "compatible")
        self.assertIn(os.path.join("Scripts", "a.lua"), r["details_per_file"])

    def test_cli_bad_paths_exit_nonzero(self):
        cp = subprocess.run(
            [sys.executable, os.path.join(HERE, "checker.py"),
             "/nonexistent.lua", SAMPLE_CATALOG],
            capture_output=True, text=True)
        self.assertEqual(cp.returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
