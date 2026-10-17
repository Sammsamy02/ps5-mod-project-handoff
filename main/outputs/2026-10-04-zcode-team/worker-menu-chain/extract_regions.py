#!/usr/bin/env python3
"""Region extractor for the worker-menu-chain host fixture.

Extracts the REAL code regions (byte-exact, no re-typing) so the fixture drives
the actual shipped chain as ONE host run:

  from the STAGED unit (worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp,
  sha256 6f97f8a1727b7e49..., the c92c92ff base + the ladder fold):
    key_table            : the REAL Lua `Key` table registration (main.lua's
                           Key.F7 value flows from the REAL Input::Key enum)
    dispatch_scope       : the PROSPERO NativeLuaActionDispatchScope guard
                           (incl. the anonymous-namespace NativeProcessEventHookId)
    marshal_state        : kFuncPtrMarshalExecutor tag + [R3] FuncPtr-marshal
                           state + is_funcptr_marshal_armed (+ the [R3-LADDER]
                           funcptr_marshal_usable helper)
    process_simple_actions : the REAL simple-action drain body (executes the
                           enqueued closure on the draining thread)
    process_delayed_actions: the REAL delayed drain (the marshal hook calls it;
                           empty on this chain, must compile + run real)
    funcptr_marshal_hook : the REAL [R3] game-thread pump body (publication
                           gate -> IsInGameThreadRaw -> drain)
    availability         : is_engine_tick_hook_available / is_process_event_hook_available
    hook_state           : ensure_hook_thread_exists / get_hook_lua / make_hook_state
    ensure_hooks         : ensure_engine_tick_hooked / ensure_process_event_hooked
                           (the traced throw sites; enqueue-first arm callees)
    binding_execute_in_game_thread : the REAL ExecuteInGameThread registration
                           (EL enqueue-ladder fold, marshal-first-when-armed)

  from the BASE unit (worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp,
  sha256 c92c92ff66686bf6..., LuaMod c92c92ff per the task):
    binding_register_keybind : the REAL RegisterKeyBind registration - the
                           keybind dispatch glue (callback lambda -> registry
                           ref -> call_function, registered onto the program's
                           input handler)

Anchors are content markers. Every extracted region is hashed into
regions-manifest.json. Byte-exactness is cross-checked against the accepted
worker-r3-ladder host-test extraction (regions.json) for the four shared
regions, and both bindings are asserted IDENTICAL across the two lineages
(the ladder fold must not have touched them).

Usage:
  extract_regions.py <staged_LuaMod.cpp> <control_LuaMod.cpp> <main.lua> <outdir>
      full extraction; asserts all three pinned sources and cross-checks the
      shared-region hashes against the accepted worker-r3-ladder extraction
  extract_regions.py --verify <outdir>
      provenance-only: re-hash the extracted *.inc files against the on-disk
      regions-manifest.json (which records the source sha256s asserted at
      extraction time). Use when a shared lineage file drifted AFTER the
      pinned extraction — the fixture must rebuild from the recorded regions,
      never from drifted content.
"""
import hashlib
import json
import sys
from pathlib import Path


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def find_line(lines, needle, start=0, required=True):
    for i in range(start, len(lines)):
        if needle in lines[i]:
            return i
    if required:
        raise SystemExit(f"anchor not found: {needle!r} (from line {start})")
    return -1


def extract(lines, start_needle, end_needle, end_offset, start_offset=0):
    start = find_line(lines, start_needle) + start_offset
    assert start >= 0, f"start_offset underflow for {start_needle!r}"
    end = find_line(lines, end_needle, start + 1)
    return "\n".join(lines[start : end + end_offset]) + "\n"


STAGED_REGIONS = (
    (
        "key_table",
        "LuaMadeSimple::Lua::Table key_table = lua.prepare_new_table();",
        'key_table.make_global("Key");',
        1,
    ),
    (
        "dispatch_scope",
        "// This protects Lua action state, not arbitrary engine code or memory.",
        "auto owns_dispatch() const noexcept -> bool { return OwnsDispatch; }",
        4,  # then `};` / `}` / `#endif` (slice: last included = end+offset-1)
        -3,  # start at the `#ifdef __PROSPERO__` line (comment -> `namespace` -> `{` -> `#ifdef`)
    ),
    (
        "marshal_state",
        "// [R3-L10LM] File-local executor tag for the FuncPtr-marshal drain",
        "static auto process_simple_actions",
        -1,
    ),
    (
        "process_simple_actions",
        "static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void",
        "// Process pending NotifyOnNewObject callbacks that were queued from StaticConstructObject",
        -1,
    ),
    (
        "process_delayed_actions",
        "template <GameThreadExecutionMethod Executor>",
        "auto static process_event_hook",
        -1,
    ),
    (
        "funcptr_marshal_hook",
        "// [R3] The FuncPtr-marshal drain body (DIFF 1; R3D 4.1/4.3). Registered as a",
        "auto static engine_tick_hook",
        -1,
    ),
    (
        "availability",
        "// Local convenience wrappers for Capabilities functions",
        "// [R3] Identity pre-check",
        -2,
    ),
    (
        "hook_state",
        "// Private helper: Ensures hook thread exists",
        "auto static make_async_state(LuaMod* mod, const LuaMadeSimple::Lua& lua) -> void",
        -1,
    ),
    (
        "ensure_hooks",
        "// Helper to ensure engine tick hook is registered",
        "auto LuaMod::setup_lua_global_functions_main_state_only() const -> void",
        -1,
    ),
    (
        "binding_execute_in_game_thread",
        'm_lua.register_function("ExecuteInGameThread"',
        "// ExecuteInGameThreadWithDelay - executes callback after a time delay",
        -1,
    ),
)

CONTROL_REGIONS = (
    (
        "binding_register_keybind",
        'lua.register_function("RegisterKeyBind"',
        'lua.register_function("UnregisterHook"',
        -1,
    ),
)

# Accepted byte-exactness references: the shared regions must hash EXACTLY like
# the worker-r3-ladder host-test extraction (regions.json, "fixed" world), whose
# PASS receipt is 85aaeac9d888673a... .
EXPECTED_STAGED_SHA = "6f97f8a1727b7e49d32a43041288bfb1f8772a866067d748752f950aafc955cd"
EXPECTED_CONTROL_SHA = "c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0"
EXPECTED_MAIN_LUA_SHA = "f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658"
EXPECTED_SHARED = {
    "availability": "ebd502e8213b3e7df19b07aa6ab882dd43faac26aa28637b9fceef2eb5ac1e62",
    "hook_state": "7707f6bdd54574ccbf96a92d89ca2d37b3c20aec97e9f4d1741eac66df1602b4",
    "ensure_hooks": "4abe43c787173260e09af6d8b4c7cfe3210fcb8bc4482612f6c331a2fcee2540",
    "marshal_state": "0a8b73f863b20ff472c61b2f4b533a02891b8afb07e0c972528a12d543b502e8",
}


def main() -> None:
    if len(sys.argv) >= 3 and sys.argv[1] == "--verify":
        verify_only(Path(sys.argv[2]))
        return

    staged_path = Path(sys.argv[1])
    control_path = Path(sys.argv[2])
    main_lua_path = Path(sys.argv[3])
    outdir = Path(sys.argv[4])
    outdir.mkdir(parents=True, exist_ok=True)

    staged_bytes = staged_path.read_bytes()
    control_bytes = control_path.read_bytes()
    main_lua_bytes = main_lua_path.read_bytes()

    manifest = {
        "staged_source": {"path": str(staged_path), "sha256": sha256(staged_bytes)},
        "control_source": {"path": str(control_path), "sha256": sha256(control_bytes)},
        "main_lua": {"path": str(main_lua_path), "sha256": sha256(main_lua_bytes)},
        "regions": {},
        "cross_checks": {},
    }

    assert manifest["staged_source"]["sha256"] == EXPECTED_STAGED_SHA, (
        f"staged LuaMod.cpp drifted: {manifest['staged_source']['sha256']}"
    )
    assert manifest["control_source"]["sha256"] == EXPECTED_CONTROL_SHA, (
        f"control LuaMod.cpp drifted: {manifest['control_source']['sha256']}"
    )
    assert manifest["main_lua"]["sha256"] == EXPECTED_MAIN_LUA_SHA, (
        f"main.lua drifted: {manifest['main_lua']['sha256']} (canonical package-v3 f586043a...)"
    )

    staged_lines = staged_bytes.decode("utf-8").splitlines()
    control_lines = control_bytes.decode("utf-8").splitlines()

    staged_regions = {}
    for name, start_needle, end_needle, end_offset, *rest in STAGED_REGIONS:
        start_offset = rest[0] if rest else 0
        body = extract(staged_lines, start_needle, end_needle, end_offset, start_offset)
        staged_regions[name] = body
        data = body.encode("utf-8")
        (outdir / f"{name}.inc").write_bytes(data)
        manifest["regions"][name] = {"from": "staged", "sha256": sha256(data)}

    for name, start_needle, end_needle, end_offset in CONTROL_REGIONS:
        body = extract(control_lines, start_needle, end_needle, end_offset)
        data = body.encode("utf-8")
        (outdir / f"{name}.inc").write_bytes(data)
        manifest["regions"][name] = {"from": "control", "sha256": sha256(data)}

    # ---- cross-checks ----------------------------------------------------
    # 1. Shared regions byte-match the accepted r3-ladder extraction.
    for name, expected in EXPECTED_SHARED.items():
        actual = manifest["regions"][name]["sha256"]
        assert actual == expected, f"{name} drifted vs r3-ladder extraction: {actual}"
        manifest["cross_checks"][f"{name}_matches_r3_ladder"] = True

    # 2. The two bindings are lineage-stable: extract control-side twins and
    #    require byte-identity (the ladder fold must not have touched them).
    control_eigt = extract(
        control_lines,
        'm_lua.register_function("ExecuteInGameThread"',
        "// ExecuteInGameThreadWithDelay - executes callback after a time delay",
        -1,
    )
    staged_rkb = extract(
        staged_lines,
        'lua.register_function("RegisterKeyBind"',
        'lua.register_function("UnregisterHook"',
        -1,
    )
    manifest["cross_checks"]["binding_execute_in_game_thread_lineage_identical"] = (
        control_eigt == staged_regions["binding_execute_in_game_thread"]
    )
    manifest["cross_checks"]["binding_register_keybind_lineage_identical"] = (
        staged_rkb.encode("utf-8") == (outdir / "binding_register_keybind.inc").read_bytes()
    )
    assert manifest["cross_checks"]["binding_execute_in_game_thread_lineage_identical"]
    assert manifest["cross_checks"]["binding_register_keybind_lineage_identical"]

    # 3. Chain-completeness guards on the extracted bodies.
    checks = {
        "has_funcptr_marshal_usable": "funcptr_marshal_usable" in staged_regions["marshal_state"],
        "marshal_first_fold_present": "is_funcptr_marshal_armed())" in staged_regions["binding_execute_in_game_thread"]
        and "kFuncPtrMarshalExecutor" in staged_regions["binding_execute_in_game_thread"],
        "no_backend_throw_present": "neither EngineTick nor ProcessEvent hooks are available"
        in staged_regions["binding_execute_in_game_thread"],
        "loud_stall_present": "closure enqueued but the EngineTick pump is not registered"
        in staged_regions["binding_execute_in_game_thread"],
        "drain_publication_gate_present": "R3MarshalPublishGameThreadIdentity()" in staged_regions["funcptr_marshal_hook"]
        and "IsInGameThreadRaw()" in staged_regions["funcptr_marshal_hook"],
        "dispatch_scope_guard_present": "NativeLuaActionDispatchScope" in staged_regions["dispatch_scope"]
        and "NativeProcessEventHookId" in staged_regions["dispatch_scope"],
        "keybind_lambda_present": "lua_keybind_callback_lambda" in manifest_regions_bytes(outdir)
        and "register_keydown_event" in manifest_regions_bytes(outdir),
        "key_table_has_f7": "Input::Key::F7" in staged_regions["key_table"],
        "drain_executes_simple_actions": "process_simple_actions(LuaMod::m_game_thread_actions)"
        in staged_regions["funcptr_marshal_hook"],
    }
    for key, ok in checks.items():
        manifest["cross_checks"][key] = ok
        assert ok, f"chain-completeness guard failed: {key}"

    (outdir / "regions-manifest.json").write_text(json.dumps(manifest, indent=1))
    print("regions extracted OK")
    print("staged ", manifest["staged_source"]["sha256"][:16])
    print("control", manifest["control_source"]["sha256"][:16])
    print("mainlua", manifest["main_lua"]["sha256"][:16])
    for name, info in manifest["regions"].items():
        print(f"  {name}: {info['sha256'][:16]} ({info['from']})")


def manifest_regions_bytes(outdir: Path) -> str:
    parts = []
    for inc in sorted(outdir.glob("*.inc")):
        parts.append(inc.read_text())
    return "\n".join(parts)


def verify_only(outdir: Path) -> None:
    """Re-hash the extracted regions against the on-disk manifest. Fails unless
    every region is byte-identical to the recorded (source-pinned) extraction."""
    manifest_path = outdir / "regions-manifest.json"
    assert manifest_path.exists(), "no regions-manifest.json to verify against"
    manifest = json.loads(manifest_path.read_text())
    print("region provenance verify (no source re-read):")
    print("staged ", manifest["staged_source"]["sha256"][:16])
    print("control", manifest["control_source"]["sha256"][:16])
    print("mainlua", manifest["main_lua"]["sha256"][:16])
    assert manifest["main_lua"]["sha256"] == EXPECTED_MAIN_LUA_SHA
    assert manifest["control_source"]["sha256"] == EXPECTED_CONTROL_SHA, (
        "manifest was not written from the pinned control source"
    )
    assert manifest["staged_source"]["sha256"] == EXPECTED_STAGED_SHA
    for name, info in manifest["regions"].items():
        data = (outdir / f"{name}.inc").read_bytes()
        actual = sha256(data)
        assert actual == info["sha256"], f"region drifted vs manifest: {name}"
        print(f"  {name}: {actual[:16]} OK ({info['from']})")
    for key, expected in manifest["cross_checks"].items():
        assert expected is True, f"recorded cross-check no longer true: {key}"
    print("regions verified OK (byte-identical to the pinned extraction)")


if __name__ == "__main__":
    main()
