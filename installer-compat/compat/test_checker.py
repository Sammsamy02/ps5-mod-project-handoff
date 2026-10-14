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
# The REAL TownfallItemSpawner mod source (staged read-only copy) and the REAL
# merged catalog -- see compat/townfall-item-spawner-grade.{json,md}.
TOWNSFALL_MAIN_LUA = os.path.join(
    HERE, "townfall-item-spawner-mod", "Scripts", "main.lua")
REAL_MERGED_CATALOG = os.path.join(HERE, "catalog-real-merged.json")
# Test fixture catalog: INVENTED ps5_runtime_status values, used ONLY to
# exercise the grader's buckets (see its _comment field). Real data lives in
# catalog-real-merged.json (all statuses empty until filled from evidence).
SAMPLE_CATALOG = os.path.join(HERE, "sample_catalog_TEST_FIXTURE.json")

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


class TestBlankRuntimeStatus(unittest.TestCase):
    """Empty/missing ps5_runtime_status must grade unknown, never compatible
    or partly-compatible ("unknown - runtime status not filled"). Since the
    round-5 policy change a blank status OUTRANKS everything except a parse
    error: not in the catalog, missing, behaves-differently and PC-only
    input all ride along inside the unknown result instead of setting the
    grade."""

    BLANK_CATALOG = [
        {"name": "RegisterKeyBind", "category": "input", "ps5_runtime_status": ""},
        {"name": "FindFirstOf", "category": "reflection",
         "ps5_runtime_status": "implemented+console-proven"},
    ]

    def test_empty_status_grades_unknown_not_partly(self):
        r = scan_text(
            "RegisterKeyBind('F7', function() print('x') end)\n",
            catalog=self.BLANK_CATALOG)
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertEqual([b["api"] for b in r["status_blank"]],
                         ["RegisterKeyBind"])
        self.assertEqual(len(r["behaves_differently"]), 0)
        self.assertEqual(len(r["missing"]), 0)

    def test_missing_status_key_counts_as_blank(self):
        r = scan_text(
            "MysteryCataloged()\n",
            catalog=[{"name": "MysteryCataloged", "category": "x"}])
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertEqual(len(r["status_blank"]), 1)

    def test_whitespace_status_counts_as_blank(self):
        r = scan_text(
            "MysteryCataloged()\n",
            catalog=[{"name": "MysteryCataloged", "category": "x",
                      "ps5_runtime_status": "   "}])
        self.assertEqual(r["grade"], "unknown")
        self.assertEqual(len(r["status_blank"]), 1)

    def test_blank_plus_unlisted_api_still_unknown(self):
        r = scan_text(
            "RegisterKeyBind('F7', function() print('x') end)\n"
            "TotallyUnlisted()\n",
            catalog=self.BLANK_CATALOG)
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertIn("TotallyUnlisted", [u["api"] for u in r["unknown"]])

    def test_blank_forces_unknown_missing_still_listed(self):
        # Round-5 policy: ANY blank ps5_runtime_status the mod calls means
        # NO verdict is possible -> grade unknown, even when a genuinely
        # missing API exists. The missing API stays listed in its section.
        r = scan_text(
            "local function helper()\n"
            "    ExecuteAsync()\n"          # genuinely missing, peripheral
            "end\n"
            "RegisterKeyBind('F7', function() print('x') end)\n",
            catalog=self.BLANK_CATALOG + [
                {"name": "ExecuteAsync", "category": "threading",
                 "ps5_runtime_status": "missing"}])
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertEqual(r["missing"][0]["api"], "ExecuteAsync")
        self.assertEqual(len(r["status_blank"]), 1)

    def test_ok_statuses_still_grade_compatible(self):
        r = scan_text("FindFirstOf('A')\n", catalog=self.BLANK_CATALOG)
        self.assertEqual(r["grade"], "compatible")
        self.assertEqual(len(r["status_blank"]), 0)

    def test_blank_status_suggestion_is_catalog_side(self):
        r = scan_text(
            "RegisterKeyBind('F7', function() print('x') end)\n",
            catalog=self.BLANK_CATALOG)
        self.assertIn("ps5_runtime_status", r["status_blank"][0]["suggestion"])
        text = checker.render_summary(r)
        self.assertIn("STATUS BLANK", text)
        self.assertIn("Catalog-side action", text)

    def test_cataloged_stdlib_override_beats_builtin_assumption(self):
        # The REAL catalog registers print (UE4SS replaces the Lua builtin)
        # with an EMPTY status: the call must surface as status-blank
        # ("unknown - runtime status not filled"), not silently "builtin".
        r = scan_text(
            "print('x')\n",
            catalog=[{"name": "print", "category": "debug",
                      "ps5_runtime_status": ""}])
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertEqual([b["api"] for b in r["status_blank"]], ["print"])
        # A stdlib name NOT in the catalog keeps the old builtin treatment.
        r2 = scan_text("print('x')\n")
        self.assertEqual(r2["builtin_calls"], 1)
        self.assertEqual(r2["grade"], "unknown")  # nothing to verify


class TestPcOnlyInput(unittest.TestCase):
    """PC-only KEYBOARD input dependencies (Key.*/ModifierKey.* constant
    tables): cataloged, constants readable, but PS5 has no keyboard -- the
    checker must report them as a dedicated "pc-only input" difference,
    distinct from "missing" (API absent) and from "not in catalog"."""

    KEY_CATALOG = [
        {"name": "RegisterKeyBind", "category": "input",
         "ps5_runtime_status": "implemented+console-proven"},
        {"name": "Key", "kind": "type", "category": "input",
         "pc_only_input": True, "constants": ["F7", "UP_ARROW", "DOWN_ARROW"],
         "pc_only_behavior_notes": "Windows virtual-key codes; PS5 has no "
         "keyboard - keybinds require runtime-side controller mapping (see "
         "PS5Pad.ini)", "ps5_runtime_status": ""},
        {"name": "ModifierKey", "kind": "type", "category": "input",
         "pc_only_input": True, "constants": ["SHIFT", "CONTROL", "ALT"],
         "ps5_runtime_status": ""},
    ]

    def test_key_constant_reported_pc_only_input_not_status_blank(self):
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        pc = [p["api"] for p in r["pc_only_input"]]
        self.assertEqual(pc, ["Key.F7"])
        self.assertEqual(len(r["status_blank"]), 0)   # flag wins over blank
        self.assertEqual(len(r["missing"]), 0)        # distinct from missing
        self.assertEqual(len(r["unknown"]), 0)        # cataloged now
        self.assertEqual(r["grade"], "partly-compatible")

    def test_pc_only_input_reason_mentions_controller_mapping(self):
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        self.assertIn("PC-only input dependency", r["grade_reason"])
        self.assertIn("controller mapping", r["grade_reason"])
        self.assertIn("PS5 has no keyboard", r["grade_reason"])
        self.assertIn("Key.F7", r["grade_reason"])

    def test_pc_only_input_suggestion_text(self):
        r = scan_text(
            "RegisterKeyBind(Key.UP_ARROW, function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        rec = r["pc_only_input"][0]
        self.assertIn("controller mapping", rec["suggestion"])
        self.assertIn("PS5Pad.ini", rec["suggestion"])
        text = checker.render_summary(r)
        self.assertIn("PC-ONLY INPUT", text)
        self.assertIn("Key.UP_ARROW", text)

    def test_central_pc_only_input_is_not_unsupported(self):
        # "unsupported" is reserved for a central MISSING API; a central
        # pc-only input dependency exists but needs translation instead.
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n"
            "RegisterKeyBind(Key.UP_ARROW, function() print('x') end)\n"
            "RegisterKeyBind(Key.DOWN_ARROW, function() print('x') end)\n"
            "RegisterKeyBind(Key.DOWN_ARROW, function() print('x') end)\n"
            "RegisterKeyBind(Key.DOWN_ARROW, function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        self.assertEqual(r["grade"], "partly-compatible")
        self.assertTrue(all(p["central"] for p in r["pc_only_input"]))

    def test_blank_status_outranks_pc_only_input(self):
        # Round-5 policy reversal: a PC-only input dependency does NOT
        # upgrade the grade to partly-compatible when ANY called API has a
        # blank ps5_runtime_status; the keyboard difference rides along
        # INSIDE the unknown result as a known PS5 difference.
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n",
            catalog=[
                {"name": "RegisterKeyBind", "category": "input",
                 "ps5_runtime_status": ""},   # blank: no verdict possible
                {"name": "Key", "kind": "type", "category": "input",
                 "pc_only_input": True, "constants": ["F7"],
                 "ps5_runtime_status": ""}])
        self.assertEqual(r["grade"], "unknown")
        self.assertIn("runtime status not filled", r["grade_reason"])
        self.assertIn("PC-only input dependency", r["grade_reason"])
        self.assertEqual([p["api"] for p in r["pc_only_input"]], ["Key.F7"])
        self.assertEqual([d["api"] for d in r["known_ps5_differences"]],
                         ["Key.F7"])
        self.assertTrue(any("KNOWN PS5 DIFFERENCE" in n for n in r["notes"]))

    def test_dedicated_status_without_flag(self):
        # Filling ps5_runtime_status with the dedicated "pc-only-input"
        # value reaches the same bucket without the boolean flag.
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n",
            catalog=[
                {"name": "Key", "kind": "type", "category": "input",
                 "constants": ["F7"],
                 "ps5_runtime_status": "pc-only-input"}])
        self.assertEqual([p["api"] for p in r["pc_only_input"]], ["Key.F7"])
        self.assertEqual(r["grade"], "partly-compatible")

    def test_modifierkey_constant_is_pc_only_input(self):
        r = scan_text(
            "RegisterKeyBind(Key.F7, {ModifierKey.CONTROL}, "
            "function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        self.assertEqual({p["api"] for p in r["pc_only_input"]},
                         {"Key.F7", "ModifierKey.CONTROL"})

    def test_pc_only_input_counts_as_matched(self):
        r = scan_text(
            "RegisterKeyBind(Key.F7, function() print('x') end)\n",
            catalog=self.KEY_CATALOG)
        # Key.F7 resolves against the catalog (type-ref) and counts matched.
        self.assertEqual(r["matched_calls"], 2)  # RegisterKeyBind + Key.F7
        self.assertEqual(r["unknown_calls"], 0)

    def test_real_catalog_key_modifierkey_entries(self):
        # The REAL catalog now carries Key/ModifierKey TYPE entries with the
        # constants extracted from the UE4SS port source (register_input_globals).
        with open(REAL_MERGED_CATALOG) as fh:
            entries = json.load(fh)["entries"]
        by_name = {e["name"]: e for e in entries}
        key = by_name.get("Key")
        mod = by_name.get("ModifierKey")
        self.assertIsNotNone(key)
        self.assertIsNotNone(mod)
        for e in (key, mod):
            self.assertEqual(e["kind"], "type")
            self.assertEqual(e["category"], "input")
            self.assertTrue(e.get("pc_only_input"))
            self.assertEqual(e["ps5_runtime_status"], "")
            self.assertFalse(e["needs_live_game"])  # constants readable w/o game
        self.assertEqual(len(key["constants"]), 165)
        for c in ("F7", "UP_ARROW", "DOWN_ARROW", "LEFT_ARROW",
                  "RIGHT_ARROW", "RETURN"):
            self.assertIn(c, key["constants"])
        # The six LEFT_/RIGHT_ variants are commented out in both the port
        # and upstream register_input_globals: only 3 are actually registered.
        self.assertEqual(mod["constants"], ["SHIFT", "CONTROL", "ALT"])


class TestDoesNotParse(unittest.TestCase):
    """Parse-failure detection (review finding #15): a .lua file that fails
    the heuristic syntax check must grade "does not parse" with the file
    and line -- never "unknown - runtime status not filled" or anything
    derived from garbage call extraction. Valid Lua is never flagged."""

    def test_unclosed_string_literal_grades_does_not_parse(self):
        r = scan_text(
            "RegisterKeyBind(Key.F7, function()\n"
            "    local s = \"unclosed\n"          # unterminated string, line 2
            "    print(s)\n"
            "end)\n",
            catalog=[{"name": "RegisterKeyBind", "category": "input",
                      "ps5_runtime_status": ""}])  # blank would say unknown
        self.assertEqual(r["grade"], "does not parse")
        self.assertEqual(r["parse_errors"],
                         [{"file": "mod.lua", "line": 2,
                           "error": "unterminated string"}])
        # reason carries the file and the line
        self.assertIn("mod.lua:2", r["grade_reason"])
        self.assertIn("unterminated string", r["grade_reason"])
        # the broken file contributed NO calls (extraction would be garbage)
        self.assertEqual(r["total_calls"], 0)

    def test_unclosed_block_grades_does_not_parse(self):
        r = scan_text(
            "local function f()\n"
            "    if true then\n"
            "        print('x')\n"
            "    -- missing two 'end's here\n",
            catalog=[])
        self.assertEqual(r["grade"], "does not parse")
        # the function (line 1) is the FIRST unclosed block; the nested if
        # rides along as "+1 more"
        self.assertTrue(r["parse_errors"][0]["error"].startswith(
            "unclosed 'function' block"), r["parse_errors"][0]["error"])
        self.assertEqual(r["parse_errors"][0]["line"], 1)
        self.assertIn("mod.lua:1", r["grade_reason"])

    def test_invalid_token_grades_does_not_parse(self):
        r = scan_text("local a = 1 $ 2\nprint(a)\n", catalog=[])
        self.assertEqual(r["grade"], "does not parse")
        self.assertEqual(r["parse_errors"][0]["error"],
                         "invalid character '$'")
        self.assertEqual(r["parse_errors"][0]["line"], 1)

    def test_parse_error_outranks_blank_status(self):
        # a syntax error beats even the blank-status unknown verdict
        r = scan_text(
            "MysteryCataloged()\nlocal s = 'never closed\n",
            catalog=[{"name": "MysteryCataloged", "category": "x",
                      "ps5_runtime_status": ""}])
        self.assertEqual(r["grade"], "does not parse")
        self.assertNotIn("runtime status not filled", r["grade_reason"])

    def test_broken_file_excluded_clean_files_still_scanned(self):
        # a multi-file mod: one broken file grades the whole mod
        # "does not parse", but the clean sibling is still scanned and
        # reported per-file.
        with tempfile.TemporaryDirectory() as td:
            with open(os.path.join(td, "good.lua"), "w") as fh:
                fh.write("local n = FName('x')\nreturn n:ToString()\n")
            with open(os.path.join(td, "bad.lua"), "w") as fh:
                fh.write("local s = \"unclosed\n")
            cat = os.path.join(td, "catalog.json")
            with open(cat, "w") as fh:
                json.dump(MINI_CATALOG, fh)
            r = checker.scan(td, cat)
        self.assertEqual(r["grade"], "does not parse")
        self.assertEqual([e["file"] for e in r["parse_errors"]], ["bad.lua"])
        good = r["details_per_file"]["good.lua"]
        self.assertNotIn("parse_error", good)
        self.assertGreater(good["matched_calls"], 0)

    def test_summary_lists_parse_errors(self):
        r = scan_text("local s = \"unclosed\n", catalog=[])
        text = checker.render_summary(r)
        self.assertIn("GRADE: DOES NOT PARSE", text)
        self.assertIn("PARSE ERRORS", text)
        self.assertIn("mod.lua:1", text)
        self.assertIn("unterminated string", text)

    def test_valid_lua_constructs_not_flagged(self):
        # common VALID constructs the heuristic must never flag
        ok_sources = [
            "#!/usr/bin/env lua\nprint('hi')\n",                 # shebang
            "--[==[ ]] inside is a comment ]==]\nprint(1)\n",     # level-2
            "repeat local x = 1 until x\n",                      # repeat
            "for i = 1, 3 do while i > 0 do i = i - 1 end end\n",  # nested
            "local s = \"escaped \\\" quote\" .. 'it\\'s'\n",     # escapes
            "local t = {a = {b = 1}}\nreturn t.a[t.b]\n",        # brackets
            "-- don't count apostrophes in comments\nprint(1)\n",
            "local bits = (a & b) | (c ~ d) << 2 >> 1 // 3\n",   # 5.3+ ops
            "local s = \"spans \\z\n   lines\" .. 'x'\n",        # 5.2+ \z escape
        ]
        for src in ok_sources:
            self.assertIsNone(checker.lua_syntax_error(src), src)


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


class TestTownfallItemSpawnerGrade(unittest.TestCase):
    """Integration: the REAL TownfallItemSpawner mod vs the REAL merged catalog.

    Pins the shipped grade artifact (compat/townfall-item-spawner-grade.json).
    Round-5 grade policy: ANY blank ps5_runtime_status among the APIs the
    mod calls (RegisterKeyBind, ExecuteInGameThread, ... -- all 9 core APIs
    are still blank) means NO verdict is possible -> grade "unknown -
    runtime status not filled". The six Key.*/ModifierKey.* keyboard keybind
    constants stay cataloged and detected, but are now listed as a KNOWN PS5
    DIFFERENCE inside the unknown result; they no longer upgrade the grade
    to partly-compatible. Mac-only static analysis; no console contact. If
    this test turns red after a catalog update, re-run merge_catalogs.py
    first, then re-grade the artifact.
    """

    @classmethod
    def setUpClass(cls):
        cls.result = checker.scan(TOWNSFALL_MAIN_LUA, REAL_MERGED_CATALOG)

    def test_townfall_inputs_exist(self):
        for p in (TOWNSFALL_MAIN_LUA, REAL_MERGED_CATALOG):
            self.assertTrue(os.path.isfile(p), p)

    def test_townfall_grade_unknown_with_keyboard_note(self):
        # Round-5 policy reversal (was partly-compatible - PC-only input
        # dependency): blank statuses among the mod's core APIs make the
        # grade "unknown - runtime status not filled", WITH the keyboard
        # keybind difference listed as a known PS5 difference inside the
        # result -- it does not upgrade the grade.
        self.assertEqual(self.result["grade"], "unknown")
        self.assertIn("runtime status not filled", self.result["grade_reason"])
        self.assertIn("PC-only input dependency", self.result["grade_reason"])
        self.assertIn("PS5 has no keyboard", self.result["grade_reason"])
        # known PS5 difference section, within the unknown result
        self.assertEqual(
            {d["api"] for d in self.result["known_ps5_differences"]},
            {"Key.F7", "Key.UP_ARROW", "Key.DOWN_ARROW",
             "Key.LEFT_ARROW", "Key.RIGHT_ARROW", "Key.RETURN"})
        self.assertTrue(all(d["kind"] == "pc-only-input"
                            for d in self.result["known_ps5_differences"]))
        self.assertTrue(any("KNOWN PS5 DIFFERENCE" in n
                            for n in self.result["notes"]))

    def test_townfall_blank_status_core_apis(self):
        blank = {r["api"] for r in self.result["status_blank"]}
        self.assertLessEqual(
            {"RegisterKeyBind", "ExecuteInGameThread", "StaticFindObject",
             "StaticConstructObject", "FindFirstOf", "LoadAsset"}, blank)
        # the blank statuses are WHY the grade is unknown
        self.assertTrue(any("runtime status not filled" in n
                            for n in self.result["notes"]))

    def test_townfall_six_key_constants_are_pc_only_input(self):
        # The six Key.* keybinds stay in the dedicated pc_only_input bucket
        # (central: top-level RegisterKeyBind args) even though the grade is
        # unknown -- the bucket listing is unchanged, only the grade policy
        # changed.
        pc = {r["api"] for r in self.result["pc_only_input"]}
        self.assertEqual(
            pc,
            {"Key.F7", "Key.UP_ARROW", "Key.DOWN_ARROW",
             "Key.LEFT_ARROW", "Key.RIGHT_ARROW", "Key.RETURN"})
        self.assertTrue(all(r["central"] for r in self.result["pc_only_input"]))
        self.assertEqual([u["api"] for u in self.result["unknown"]], [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
