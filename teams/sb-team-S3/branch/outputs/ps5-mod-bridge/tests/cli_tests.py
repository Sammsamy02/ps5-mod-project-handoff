#!/usr/bin/env python3
"""Consumer-facing checks against the real compiled command-line program."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge"))


class CliTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(BINARY.is_file(), "CLI implementation is missing")
        self.temp = tempfile.TemporaryDirectory(prefix="modbridge-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_cli(self, *args, expected=0):
        result = subprocess.run([str(BINARY), *map(str, args)], text=True,
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def fixture(self):
        package = self.root / "mixed.zip"
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr("Example/Mods/main.lua", "RegisterHook('/Script/Test.Event', function() end)\n")
            archive.writestr("Example/settings.ini", "enabled=true\n")
        return package

    def write_json(self, name, value):
        path = self.root / name
        path.write_text(json.dumps(value))
        return path

    def graph_plan(self, edge_type, ring=False, supported=True):
        # Flat valid inputs must not exhaust the stack in SCC or component ordering.
        names = [f"node-{i:05d}" for i in range(6500)]
        entries = [{"path": name, "kind": "data", "sha256": hashlib.sha256(b"abc").hexdigest()}
                   for name in names]
        edges = [{"from": names[i], "to": names[i + 1], "type": edge_type}
                 for i in range(len(names) - 1)]
        if ring:
            edges.append({"from": names[-1], "to": names[0], "type": edge_type})
        inventory = self.write_json("graph.json", {"entries": entries, "dependencies": edges})
        target = self.write_json("graph-target.json", {"fingerprints": {"executable": "owned-target"},
                                                       "capabilities": []})
        backends = self.write_json("graph-backends.json", [{"id": "owned.data", "version": "1",
            "mode": "transformer", "kinds": ["data"], "capabilities": [],
            "target_fingerprints": ["executable"], "supports_reference_cycles": supported}])
        result = self.run_cli("plan", inventory, "--target", target, "--backends", backends)
        return names, json.loads(result.stdout), len(result.stdout)

    def test_large_acyclic_graph_has_complete_dependency_order(self):
        names, plan, _ = self.graph_plan("order")
        self.assertEqual(plan["status"], "planned")
        self.assertEqual(plan["ordered_components"], names[::-1])
        self.assertEqual([step["component"] for step in plan["eligible_steps"]], names[::-1])

    def test_large_reference_ring_uses_one_shared_group(self):
        names, plan, output_bytes = self.graph_plan("reference", ring=True)
        self.assertEqual(plan["status"], "planned")
        self.assertEqual(plan["ordered_components"], names[::-1])
        self.assertEqual(len(plan["reference_groups"]), 1)
        group = plan["reference_groups"][0]
        self.assertEqual(group["components"], names)
        self.assertEqual(group["status"], "eligible")
        self.assertEqual({step["group_id"] for step in plan["eligible_steps"]}, {group["group_id"]})
        self.assertLess(output_bytes, 5_000_000)

    def test_large_strict_ring_reports_one_conflict_without_quadratic_output(self):
        names, plan, output_bytes = self.graph_plan("order", ring=True)
        self.assertEqual(plan["status"], "unresolved")
        self.assertEqual(plan["eligible_steps"], [])
        self.assertEqual(plan["ordered_components"], [])
        self.assertEqual(len(plan["conflicts"]), 1)
        conflict = plan["conflicts"][0]
        self.assertEqual(conflict["code"], "ordering_cycle")
        self.assertEqual(conflict["components"], names)
        self.assertEqual(len(plan["unresolved"]), len(names))
        self.assertEqual({item["group_id"] for item in plan["unresolved"]}, {conflict["group_id"]})
        self.assertTrue(all("components" not in item for item in plan["unresolved"]))
        self.assertLess(output_bytes, 3_000_000)

    def test_analyze_produces_real_hash_and_mixed_inventory(self):
        # Catches CLI dropping entries, reporting the wrong input, or upgrading evidence.
        package = self.fixture()
        report = self.root / "report.json"
        self.run_cli("analyze", package, "--output", report)
        data = json.loads(report.read_text())
        self.assertEqual(data["package_sha256"], hashlib.sha256(package.read_bytes()).hexdigest())
        self.assertEqual(data["status"], "analyzed")
        self.assertEqual({entry["path"] for entry in data["entries"]},
                         {"Example/Mods/main.lua", "Example/settings.ini"})
        self.assertNotIn("deployed", data)

    def test_unknown_target_reports_unresolved_runtime(self):
        package = self.fixture()
        inventory = self.root / "inventory.json"
        self.run_cli("analyze", package, "--output", inventory)
        target = self.write_json("target.json", {"platform": "ps5", "title": "Stellar Blade",
                                                 "fingerprints": {}, "capabilities": []})
        backends = self.write_json("backends.json", [])
        result = self.run_cli("plan", inventory, "--target", target, "--backends", backends)
        plan = json.loads(result.stdout)
        self.assertEqual(plan["status"], "unresolved")
        self.assertEqual(plan["stage"], "analyzed")
        self.assertTrue(plan["unresolved"])
        self.assertEqual(plan["eligible_steps"], [])

    def test_invalid_arguments_and_json_have_structured_errors(self):
        cases = [(), ("unknown-command",), ("analyze",),
                 ("analyze", self.root / "missing.zip"),
                 ("analyze", self.fixture(), "--bogus")]
        for args in cases:
            with self.subTest(args=args):
                result = self.run_cli(*args, expected=2)
                error = json.loads(result.stderr)["error"]
                self.assertTrue(error["code"])
                self.assertTrue(error["message"])
        invalid = self.root / "invalid.json"
        invalid.write_text("{")
        result = self.run_cli("cache-key", invalid, expected=2)
        self.assertIn("error", json.loads(result.stderr))

    def test_scratch_roundtrip_and_modified_file_preservation(self):
        scratch = self.root / "scratch"
        prepared = self.root / "prepared"
        prepared.mkdir()
        self.run_cli("scratch-init", scratch)
        original = scratch / "config.txt"
        original.write_text("original")
        (prepared / "replacement.txt").write_text("installed")
        manifest = self.write_json("manifest.json", {"files": [{"source": "replacement.txt",
                         "destination": "config.txt", "sha256": hashlib.sha256(b"installed").hexdigest()}]})
        result = self.run_cli("scratch-deploy", scratch, prepared, manifest)
        transaction = json.loads(result.stdout)["transaction_id"]
        self.assertEqual(original.read_text(), "installed")
        self.run_cli("scratch-remove", scratch, transaction)
        self.assertEqual(original.read_text(), "original")
        result = self.run_cli("scratch-deploy", scratch, prepared, manifest)
        transaction = json.loads(result.stdout)["transaction_id"]
        original.write_text("user edit")
        self.run_cli("scratch-remove", scratch, transaction)
        self.assertEqual(original.read_text(), "user edit")
        recovered = json.loads(self.run_cli("scratch-recover", scratch).stdout)
        self.assertIsInstance(recovered, dict)

    def test_json_nesting_and_node_budget_fail_without_crashing(self):
        # Parsing must reject the structure before copying a deeply nested DOM.
        excessive = self.root / "excessive.json"
        excessive.write_text('{"metadata":' + '[' * 20000 + '0' + ']' * 20000 + '}')
        result = self.run_cli("cache-key", excessive, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")
        excessive.write_text('[' + ','.join('0' for _ in range(100001)) + ']')
        result = self.run_cli("cache-key", excessive, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")

    def test_existing_nonempty_directory_is_not_silently_marked(self):
        scratch = self.root / "ordinary-files"
        scratch.mkdir()
        (scratch / "keep.txt").write_text("keep")
        self.run_cli("scratch-init", scratch, expected=2)
        self.assertFalse((scratch / ".modbridge-scratch").exists())
        self.assertEqual((scratch / "keep.txt").read_text(), "keep")

    def test_output_failure_does_not_mutate_input(self):
        package = self.fixture()
        before = package.read_bytes()
        directory = self.root / "destination-directory"
        directory.mkdir()
        self.run_cli("analyze", package, "--output", directory, expected=2)
        self.assertEqual(package.read_bytes(), before)

    def test_plan_report_cannot_replace_any_input(self):
        inventory = self.root / "inventory.json"
        self.run_cli("analyze", self.fixture(), "--output", inventory)
        target = self.write_json("target.json", {"fingerprints": {}, "capabilities": []})
        backends = self.write_json("backends.json", [])
        for destination in (inventory, target, backends):
            with self.subTest(destination=destination):
                before = destination.read_bytes()
                self.run_cli("plan", inventory, "--target", target, "--backends", backends,
                             "--output", destination, expected=2)
                self.assertEqual(destination.read_bytes(), before)

    def test_directory_report_cannot_change_package_members(self):
        package = self.root / "package-directory"
        package.mkdir()
        member = package / "owned.txt"
        member.write_text("keep my package content")
        for destination in (member, package / "new-report.json"):
            with self.subTest(destination=destination):
                self.run_cli("analyze", package, "--output", destination, expected=2)
                self.assertEqual(member.read_text(), "keep my package content")
                self.assertFalse((package / "new-report.json").exists())

    def test_stdout_write_failure_returns_nonzero(self):
        output = self.root / "readonly-output"
        output.write_text("preserve")
        with output.open("rb") as readonly:
            result = subprocess.run([str(BINARY), "--help"], stdout=readonly,
                                    stderr=subprocess.PIPE, text=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "write_failed")
        self.assertEqual(output.read_text(), "preserve")

    @unittest.skipUnless(os.name == "posix", "POSIX path byte handling")
    def test_non_utf8_path_error_is_serializable(self):
        result = subprocess.run([os.fsencode(BINARY), b"cache-key", b"missing-\xff.json"],
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")

    def reflection_dump(self, platform, functions):
        return {"schema": "modbridge.reflection-dump/1", "platform": platform,
                "image": {"sha256": ("a" if platform == "pc" else "b") * 64,
                          "address_kind": "rva" if platform == "pc" else "vaddr"},
                "engine_version": "5.6",
                "functions": [{"name": name, "address": address} for name, address in functions]}

    def test_symbol_map_build_and_translate_round_trip(self):
        destroy, tick = "/Script/Engine.Actor:K2_DestroyActor", "/Script/Engine.Actor:ReceiveTick"
        pc = self.write_json("pc.json", self.reflection_dump("pc", [(destroy, "0x1000"), (tick, "0x2000")]))
        ps5 = self.write_json("ps5.json", self.reflection_dump("ps5", [(destroy, "0x80001000"), (tick, "0x80005000")]))
        mapping = self.root / "map.json"
        self.run_cli("symbol-map", "--pc", pc, "--ps5", ps5, "--title-id", "PPSA33286",
                     "--console-version", "02.003.000", "--pc-build", "1.03", "--output", mapping)
        built = json.loads(mapping.read_text())
        self.assertEqual(built["schema"], "modbridge.symbol-map/1")
        self.assertEqual(built["stats"]["matched"], 2)
        by_address = json.loads(self.run_cli("translate", "--map", mapping, "--pc-rva", "0x2000").stdout)
        self.assertEqual(by_address, {"status": "translated", "ps5_vaddr": "0x80005000", "names": [tick]})
        interior = json.loads(self.run_cli("translate", "--map", mapping, "--pc-rva", "0x1010").stdout)
        self.assertEqual((interior["status"], interior["ps5_vaddr"]), ("interior", None))
        by_name = json.loads(self.run_cli("translate", "--map", mapping, "--name", destroy).stdout)
        self.assertEqual(by_name, {"status": "translated", "ps5_vaddr": "0x80001000"})
        missing = json.loads(self.run_cli("translate", "--map", mapping, "--name", "/Script/Engine.Actor:Nope").stdout)
        self.assertEqual(missing["status"], "not_found")

    def test_symbol_map_and_translate_reject_bad_arguments(self):
        pc = self.write_json("pc.json", self.reflection_dump("pc", [("/Script/A.B:C", "0x10")]))
        ps5 = self.write_json("ps5.json", self.reflection_dump("ps5", [("/Script/A.B:C", "0x20")]))
        # Missing identity, swapped platforms and a bad map all fail with a structured error.
        for args in (("symbol-map", "--pc", pc, "--ps5", ps5, "--title-id", "PPSA33286", "--pc-build", "1"),
                     ("symbol-map", "--pc", ps5, "--ps5", pc, "--title-id", "PPSA33286",
                      "--console-version", "1", "--pc-build", "1"),
                     ("translate", "--map", pc, "--name", "/Script/A.B:C")):
            with self.subTest(args=args[:3]):
                error = json.loads(self.run_cli(*args, expected=2).stderr)["error"]
                self.assertIn(error["code"], {"invalid_argument", "operation_failed"})
        mapping = self.root / "map.json"
        self.run_cli("symbol-map", "--pc", pc, "--ps5", ps5, "--title-id", "PPSA33286",
                     "--console-version", "1", "--pc-build", "1", "--output", mapping)
        for args in (("--name", "x", "--pc-rva", "0x10"), (), ("--pc-rva", "16"), ("--pc-rva", "0x"),
                     ("--pc-rva", "0x" + "f" * 17), ("--pc-rva", "0x10g")):
            with self.subTest(args=args):
                error = json.loads(self.run_cli("translate", "--map", mapping, *args, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "invalid_argument")


    # --- lua-api: static UE4SS API usage and runtime coverage ---------------------------------
    EXAMPLE_RUNTIME = PROJECT / "examples" / "runtimes" / "ps5-ue4ss-portable.json"
    SPAWNER_LIKE = (
        'local ok, UEHelpers = pcall(require, "UEHelpers")\n'
        '-- ExecuteAsync(ignored) in a comment\n'
        'local note = "RegisterHook in a string"\n'
        'RegisterKeyBind(Key.F7, function() ExecuteInGameThread(toggleMenu) end)\n'
        'RegisterKeyBind(Key.F8, function() ExecuteInGameThread(other) end)\n'
        'local class = StaticFindObject("/Script/UMG.UserWidget")\n'
        'widget:SetText(FText("x"))\n')

    def lua_script(self, text, name="main.lua"):
        path = self.root / name
        path.write_text(text)
        return path

    def lua_runtime(self, functions=None, modules=None, **overrides):
        manifest = {"schema": "modbridge.lua-runtime/1", "runtime": "cli-test", "version": "1",
                    "console_wired": False, "functions": functions or {}, "modules": modules or {}}
        manifest.update(overrides)
        return self.write_json("runtime.json", manifest)

    def test_lua_api_prints_usage_without_a_runtime(self):
        script = self.lua_script(self.SPAWNER_LIKE)
        result = self.run_cli("lua-api", script)
        self.assertEqual(json.loads(result.stdout), {
            "dynamic_require": False,
            "functions": {"ExecuteInGameThread": 2, "FText": 1, "Key": 2, "RegisterKeyBind": 2, "StaticFindObject": 1},
            "modules": ["UEHelpers"], "shadowed": []})
        # Deterministic: the same script always serializes identically.
        self.assertEqual(self.run_cli("lua-api", script).stdout, result.stdout)

    def test_lua_api_shadowed_and_dynamic_require_are_reported(self):
        script = self.lua_script('local function FindFirstOf() end\nFindFirstOf()\nrequire(name)\nExecuteAsync(f)\n')
        data = json.loads(self.run_cli("lua-api", script).stdout)
        self.assertEqual(data["shadowed"], ["FindFirstOf"])
        self.assertEqual(data["functions"], {"ExecuteAsync": 1})
        self.assertTrue(data["dynamic_require"])

    def test_lua_api_with_runtime_prints_coverage_not_a_works_claim(self):
        script = self.lua_script(self.SPAWNER_LIKE)
        runtime = self.lua_runtime(
            {"RegisterKeyBind": {"status": "implemented", "note": "caller keys"},
             "ExecuteInGameThread": {"status": "absent", "note": "no game thread"}},
            {"UEHelpers": {"status": "partial"}}, console_wired=True)
        data = json.loads(self.run_cli("lua-api", script, "--runtime", runtime).stdout)
        self.assertEqual(data["status"], "blocked")
        self.assertEqual(data["missing"], ["ExecuteInGameThread", "FText", "Key", "StaticFindObject"])
        self.assertEqual(data["partial"], [])
        self.assertEqual(data["partial_modules"], ["UEHelpers"])
        self.assertEqual(data["missing_modules"], [])
        self.assertEqual((data["runtime"], data["runtime_version"], data["console_wired"]), ("cli-test", "1", True))
        rows = {row["name"]: row for row in data["functions"]}
        self.assertEqual([row["name"] for row in data["functions"]], sorted(rows))
        self.assertEqual((rows["RegisterKeyBind"]["uses"], rows["RegisterKeyBind"]["status"],
                          rows["RegisterKeyBind"]["note"]), (2, "implemented", "caller keys"))
        self.assertEqual(rows["FText"]["status"], "absent")  # not listed: fail closed
        for forbidden in ("works", "compatible", "verdict"):
            self.assertNotIn(forbidden, data)

    def test_lua_api_status_is_covered_partial_or_blocked(self):
        implemented = {"RegisterKeyBind": {"status": "implemented"}}
        script = self.lua_script("RegisterKeyBind(1, f)\n")
        covered = json.loads(self.run_cli("lua-api", script, "--runtime", self.lua_runtime(implemented)).stdout)
        self.assertEqual(covered["status"], "covered")
        partial = json.loads(self.run_cli("lua-api", script, "--runtime",
                                          self.lua_runtime({"RegisterKeyBind": {"status": "partial"}})).stdout)
        self.assertEqual((partial["status"], partial["partial"]), ("partial", ["RegisterKeyBind"]))
        dynamic = self.lua_script("RegisterKeyBind(1, f)\nrequire(name)\n", "dyn.lua")
        self.assertEqual(json.loads(self.run_cli("lua-api", dynamic, "--runtime",
                                                 self.lua_runtime(implemented)).stdout)["status"], "partial")
        blocked = json.loads(self.run_cli("lua-api", script, "--runtime", self.lua_runtime()).stdout)
        self.assertEqual((blocked["status"], blocked["missing"]), ("blocked", ["RegisterKeyBind"]))
        # console_wired is reported, never folded into the status.
        wired = json.loads(self.run_cli("lua-api", script, "--runtime",
                                        self.lua_runtime(implemented, console_wired=True)).stdout)
        self.assertEqual((wired["status"], wired["console_wired"]), ("covered", True))

    def test_lua_api_example_runtime_blocks_a_mod_that_needs_the_game_thread(self):
        script = self.lua_script(self.SPAWNER_LIKE)
        data = json.loads(self.run_cli("lua-api", script, "--runtime", self.EXAMPLE_RUNTIME).stdout)
        self.assertEqual(data["runtime"], "ps5-ue4ss-portable")
        self.assertFalse(data["console_wired"])
        self.assertEqual(data["status"], "blocked")
        self.assertIn("ExecuteInGameThread", data["missing"])
        self.assertIn("StaticFindObject", data["missing"])
        self.assertEqual(data["partial_modules"], ["UEHelpers"])
        status = {row["name"]: row["status"] for row in data["functions"]}
        self.assertEqual(status["RegisterKeyBind"], "implemented")
        # A script that stays within what the manifest says is implemented is not blocked.
        small = self.lua_script("local o = CreateInvalidObject()\nRegisterKeyBind(1, f)\nExecuteWithDelay(5, f)\n"
                                "LoopAsync(5, f)\n", "small.lua")
        data = json.loads(self.run_cli("lua-api", small, "--runtime", self.EXAMPLE_RUNTIME).stdout)
        self.assertEqual((data["status"], data["missing"], data["partial"]), ("covered", [], []))

    def test_lua_api_output_file_and_input_protection(self):
        script = self.lua_script(self.SPAWNER_LIKE)
        runtime = self.lua_runtime({"RegisterKeyBind": {"status": "implemented"}})
        report = self.root / "coverage.json"
        result = self.run_cli("lua-api", script, "--runtime", runtime, "--output", report)
        self.assertEqual(result.stdout, "")
        self.assertEqual(json.loads(report.read_text())["status"], "blocked")
        self.assertEqual(report.read_text(), self.run_cli("lua-api", script, "--runtime", runtime).stdout)
        for destination in (script, runtime):
            with self.subTest(destination=destination.name):
                before = destination.read_bytes()
                error = json.loads(self.run_cli("lua-api", script, "--runtime", runtime,
                                                "--output", destination, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "write_failed")
                self.assertEqual(destination.read_bytes(), before)
        # Without --runtime only the script is protected.
        before = script.read_bytes()
        self.run_cli("lua-api", script, "--output", script, expected=2)
        self.assertEqual(script.read_bytes(), before)

    def test_lua_api_rejects_bad_arguments_and_inputs(self):
        script = self.lua_script("RegisterHook(1, f)\n")
        directory = self.root / "a-directory"
        directory.mkdir()
        link = self.root / "link.lua"
        link.symlink_to(script)
        huge = self.root / "huge.lua"
        huge.write_bytes(b" " * (4 * 1024 * 1024 + 1))
        cases = [(("lua-api",), "invalid_argument"),
                 (("lua-api", script, "--bogus", "x"), "invalid_argument"),
                 (("lua-api", script, "--runtime"), "invalid_argument"),
                 (("lua-api", script, "--output", self.root / "a.json", "--output", self.root / "b.json"),
                  "invalid_argument"),
                 (("lua-api", self.root / "missing.lua"), "invalid_input"),
                 (("lua-api", directory), "invalid_input"),
                 (("lua-api", link), "invalid_input"),
                 (("lua-api", huge), "invalid_input")]
        for args, code in cases:
            with self.subTest(args=[str(a) for a in args[:3]]):
                error = json.loads(self.run_cli(*args, expected=2).stderr)["error"]
                self.assertEqual(error["code"], code)
                self.assertTrue(error["message"])
        # The 4 MiB cap is inclusive: a script of exactly 4 MiB is read.
        exact = self.root / "exact.lua"
        exact.write_bytes(b" " * (4 * 1024 * 1024))
        self.assertEqual(json.loads(self.run_cli("lua-api", exact).stdout)["functions"], {})

    def test_lua_api_reports_malformed_lua_and_invalid_manifests(self):
        error = json.loads(self.run_cli("lua-api", self.lua_script('local x = "never closed\n', "bad.lua"),
                                        expected=2).stderr)["error"]
        self.assertEqual(error["code"], "operation_failed")
        self.assertTrue(error["message"].startswith("lua_api: "), error["message"])
        script = self.lua_script("RegisterHook(1, f)\n")
        bad_manifests = [
            self.lua_runtime(extra=True),
            self.lua_runtime({"print": {"status": "absent"}}),                       # not a UE4SS API name
            self.lua_runtime({"RegisterHook": {"status": "works"}}),
            self.lua_runtime({"RegisterHook": {"status": "absent", "note": "x" * 513}}),
            self.lua_runtime(console_wired="no"),
            self.lua_runtime(runtime="Bad Id"),
        ]
        for manifest in bad_manifests:
            with self.subTest(manifest=json.loads(manifest.read_text())):
                error = json.loads(self.run_cli("lua-api", script, "--runtime", manifest, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "operation_failed")
                self.assertTrue(error["message"].startswith("lua_api: "), error["message"])
        not_json = self.root / "not-json.json"
        not_json.write_text("{")
        self.assertIn("error", json.loads(self.run_cli("lua-api", script, "--runtime", not_json, expected=2).stderr))
        self.run_cli("lua-api", script, "--runtime", self.root / "missing.json", expected=2)

    def test_lua_api_is_listed_in_help_and_the_example_manifest_is_valid(self):
        self.assertIn("lua-api SCRIPT [--runtime MANIFEST] [--output REPORT]", self.run_cli("--help").stdout)
        manifest = json.loads(self.EXAMPLE_RUNTIME.read_text())
        self.assertEqual(manifest["schema"], "modbridge.lua-runtime/1")
        self.assertIs(manifest["console_wired"], False)
        script = self.lua_script("local x = 1\n")
        data = json.loads(self.run_cli("lua-api", script, "--runtime", self.EXAMPLE_RUNTIME).stdout)
        self.assertEqual(data["status"], "covered")  # nothing used, nothing missing; manifest validated

    def usage_record(self, functions=None, **overrides):
        usage = {"functions": {"RegisterKeyBind": 2, "ExecuteInGameThread": 1} if functions is None else functions,
                 "modules": ["UEHelpers"], "shadowed": [], "dynamic_require": False}
        usage.update(overrides)
        return usage

    def lua_usage(self, functions=None, name="usage.json", **overrides):
        return self.write_json(name, self.usage_record(functions, **overrides))

    def test_lua_api_usage_form_covers_a_usage_report_like_the_script_form(self):
        script = self.lua_script(self.SPAWNER_LIKE)
        runtime = self.lua_runtime({"RegisterKeyBind": {"status": "implemented"},
                                    "ExecuteInGameThread": {"status": "absent"}}, {"UEHelpers": {"status": "partial"}})
        from_script = self.run_cli("lua-api", script, "--runtime", runtime)
        usage = self.write_json("usage-from-script.json", json.loads(self.run_cli("lua-api", script).stdout))
        from_usage = self.run_cli("lua-api", "--usage", usage, "--runtime", runtime)
        self.assertEqual(from_usage.stdout, from_script.stdout)
        data = json.loads(from_usage.stdout)
        self.assertEqual((data["status"], data["missing"]), ("blocked", ["ExecuteInGameThread", "FText", "Key", "StaticFindObject"]))
        # Option order does not matter, and a usage report merged by hand is covered the same way.
        merged = self.lua_usage({"RegisterKeyBind": 3, "FindFirstOf": 1}, shadowed=["RegisterHook"], dynamic_require=True)
        reordered = json.loads(self.run_cli("lua-api", "--runtime", runtime, "--usage", merged).stdout)
        self.assertEqual((reordered["status"], reordered["missing"], reordered["shadowed"], reordered["dynamic_require"]),
                         ("blocked", ["FindFirstOf"], ["RegisterHook"], True))
        self.assertEqual(reordered["partial_modules"], ["UEHelpers"])
        report = self.root / "usage-coverage.json"
        result = self.run_cli("lua-api", "--usage", merged, "--runtime", runtime, "--output", report)
        self.assertEqual(result.stdout, "")
        self.assertEqual(json.loads(report.read_text()), reordered)
        # Inputs are never replaced by the report.
        for destination in (merged, runtime):
            with self.subTest(destination=destination.name):
                before = destination.read_bytes()
                error = json.loads(self.run_cli("lua-api", "--usage", merged, "--runtime", runtime,
                                                "--output", destination, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "write_failed")
                self.assertEqual(destination.read_bytes(), before)

    def test_lua_api_needs_exactly_one_of_script_or_usage_and_usage_needs_a_runtime(self):
        script = self.lua_script("RegisterHook(1, f)\n")
        usage, runtime = self.lua_usage(), self.lua_runtime()
        cases = [
            ("both", ("lua-api", script, "--usage", usage, "--runtime", runtime)),
            ("both without runtime", ("lua-api", script, "--usage", usage)),
            ("neither, runtime only", ("lua-api", "--runtime", runtime)),
            ("neither, output only", ("lua-api", "--output", self.root / "out.json")),
            ("usage without runtime", ("lua-api", "--usage", usage)),
            ("usage without runtime, with output", ("lua-api", "--usage", usage, "--output", self.root / "out.json")),
            ("usage without a value", ("lua-api", "--usage")),
            ("usage repeated", ("lua-api", "--usage", usage, "--usage", usage, "--runtime", runtime)),
            ("unknown option", ("lua-api", "--usage", usage, "--runtime", runtime, "--bogus", "x")),
        ]
        for label, args in cases:
            with self.subTest(case=label):
                error = json.loads(self.run_cli(*args, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "invalid_argument")
                self.assertTrue(error["message"])
        self.assertFalse((self.root / "out.json").exists())

    def test_lua_api_usage_form_rejects_bad_usage_reports_with_a_structured_error(self):
        runtime = self.lua_runtime({"RegisterKeyBind": {"status": "implemented"}})
        bad = [
            ("unknown key", self.usage_record(extra=True)),
            ("not a catalog name", self.usage_record({"print": 1})),
            ("zero uses", self.usage_record({"RegisterKeyBind": 0})),
            ("string uses", self.usage_record({"RegisterKeyBind": "2"})),
            ("shadowed and counted", self.usage_record({"RegisterKeyBind": 1}, shadowed=["RegisterKeyBind"])),
            ("shadowed not a catalog name", self.usage_record({}, shadowed=["print"])),
            ("modules not a list", self.usage_record(modules="UEHelpers")),
            ("bad module name", self.usage_record(modules=["has space"])),
            ("dynamic_require not a bool", self.usage_record(dynamic_require="yes")),
            ("missing field", {"functions": {}, "modules": [], "shadowed": []}),
            ("not an object", [1]),
        ]
        for label, record in bad:
            usage = self.write_json("bad-usage.json", record)
            with self.subTest(usage=label):
                error = json.loads(self.run_cli("lua-api", "--usage", usage, "--runtime", runtime, expected=2).stderr)["error"]
                self.assertEqual(error["code"], "operation_failed")
                self.assertTrue(error["message"].startswith("lua_api: "), error["message"])
        not_json = self.root / "not-json.json"
        not_json.write_text("{")
        error = json.loads(self.run_cli("lua-api", "--usage", not_json, "--runtime", runtime, expected=2).stderr)["error"]
        self.assertTrue(error["code"] and error["message"])
        error = json.loads(self.run_cli("lua-api", "--usage", self.root / "missing.json", "--runtime", runtime,
                                        expected=2).stderr)["error"]
        self.assertEqual(error["code"], "invalid_input")
        # A bad manifest is still rejected on this path.
        error = json.loads(self.run_cli("lua-api", "--usage", self.lua_usage(), "--runtime",
                                        self.lua_runtime(extra=True), expected=2).stderr)["error"]
        self.assertTrue(error["message"].startswith("lua_api: "), error["message"])

    def test_lua_api_help_lists_the_usage_form(self):
        self.assertIn("lua-api --usage USAGE --runtime MANIFEST [--output REPORT]", self.run_cli("--help").stdout)

    def test_analyze_records_lua_api_next_to_lua_references(self):
        package = self.fixture()
        report = self.root / "report.json"
        self.run_cli("analyze", package, "--output", report)
        entries = {entry["path"]: entry for entry in json.loads(report.read_text())["entries"]}
        facts = entries["Example/Mods/main.lua"]["facts"]
        self.assertEqual(facts["lua_api"], {"dynamic_require": False, "functions": {"RegisterHook": 1},
                                            "modules": [], "shadowed": []})
        self.assertIn("lua_references", facts)
        self.assertNotIn("lua_api", entries["Example/settings.ini"]["facts"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
