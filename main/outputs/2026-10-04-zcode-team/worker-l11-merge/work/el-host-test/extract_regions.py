#!/usr/bin/env python3
"""Region extractor for the worker-l11-enqueue-ladder host integration test.

Extracts the REAL code regions (byte-exact, no re-typing) from the two staged
LuaMod.cpp lineages so the harness drives the actual binding source:

  marshal_state  : kFuncPtrMarshalExecutor tag + [R3] FuncPtr-marshal state +
                   is_funcptr_marshal_armed (the armed-store the test flips)
  process_simple_actions : the REAL queue drain both pumps use
  availability   : is_engine_tick_hook_available / is_process_event_hook_available
  hook_state     : ensure_hook_thread_exists / get_hook_lua / make_hook_state
  ensure_hooks   : ensure_engine_tick_hooked / ensure_process_event_hooked
                   (contains the traced throw site "EngineTick callback
                   registration failed")
  binding        : the REAL ExecuteInGameThread registration (the tested unit)

Anchors are content markers, so the fixed lineage's shifted line numbers do
not matter. Every extracted region is hashed and recorded in regions.json.

Usage: extract_regions.py <control_LuaMod.cpp> <fixed_LuaMod.cpp> <outdir>
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


def extract(lines, start_needle, end_needle, end_offset):
    start = find_line(lines, start_needle)
    end = find_line(lines, end_needle, start + 1)
    return "\n".join(lines[start : end + end_offset]) + "\n"


def extract_world(path: Path, outdir: Path):
    text = path.read_bytes()
    lines = text.decode("utf-8").splitlines()
    regions = {}

    regions["marshal_state"] = extract(
        lines,
        "// [R3-L10LM] File-local executor tag for the FuncPtr-marshal drain",
        "static auto process_simple_actions",
        -1,
    )
    regions["process_simple_actions"] = extract(
        lines,
        "static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void",
        "// Process pending NotifyOnNewObject callbacks that were queued",
        -1,
    )
    regions["availability"] = extract(
        lines,
        "// Local convenience wrappers for Capabilities functions",
        "// [R3] Identity pre-check",
        -2,
    )
    regions["hook_state"] = extract(
        lines,
        "// Private helper: Ensures hook thread exists",
        "auto static make_async_state(LuaMod* mod, const LuaMadeSimple::Lua& lua) -> void",
        -1,
    )
    regions["ensure_hooks"] = extract(
        lines,
        "// Helper to ensure engine tick hook is registered",
        "auto LuaMod::setup_lua_global_functions_main_state_only() const -> void",
        -1,
    )
    # NOTE (worker-l11-enqueue-ladder restart): the previous end anchor
    # "        });" substring-matched the INNER 16-space "});" of the fixed
    # lineage's order-guarantee TRY block, truncating the binding BEFORE the
    # loud-stall log. Anchor on the next binding's comment instead; -1 keeps
    # the register_function closer "});" (extract's stop index is exclusive).
    regions["binding"] = extract(
        lines,
        'm_lua.register_function("ExecuteInGameThread"',
        "        // ExecuteInGameThreadWithDelay - executes callback after a time delay",
        -1,
    )

    outdir.mkdir(parents=True, exist_ok=True)
    hashes = {"source": {"path": str(path), "sha256": sha256(text)}}
    for name, body in regions.items():
        data = body.encode("utf-8")
        (outdir / f"{name}.inc").write_bytes(data)
        hashes[name] = sha256(data)

    # Fix / control distinctness guards (proves the test worlds differ).
    hashes["has_order_guarantee_comment"] = "THE ORDER GUARANTEE" in regions["binding"]
    hashes["has_marshal_first_ladder"] = "Marshal-first when armed" in regions["binding"]
    hashes["has_stall_log"] = "closure enqueued but the EngineTick pump is not registered" in regions["binding"]
    hashes["binding_complete"] = regions["binding"].rstrip().endswith("});")
    hashes["throw_site_present"] = "EngineTick callback registration failed" in regions["ensure_hooks"]
    return hashes


def main() -> None:
    control = Path(sys.argv[1])
    fixed = Path(sys.argv[2])
    outdir = Path(sys.argv[3])
    report = {
        "control": extract_world(control, outdir / "control"),
        "fixed": extract_world(fixed, outdir / "fixed"),
    }
    # The bug must exist ONLY in control; the fix markers ONLY in fixed.
    assert report["control"]["has_order_guarantee_comment"] is False, "control binding unexpectedly patched"
    assert report["fixed"]["has_order_guarantee_comment"] is True, "fixed binding missing the order-guarantee"
    assert report["fixed"]["has_marshal_first_ladder"] is True, "fixed binding missing the marshal-first ladder"
    assert report["fixed"]["has_stall_log"] is True, "fixed binding missing the loud-stall log"
    assert report["control"]["has_stall_log"] is False, "control binding unexpectedly contains the stall log"
    assert report["control"]["binding_complete"] and report["fixed"]["binding_complete"], "binding region truncated"
    assert report["control"]["throw_site_present"] and report["fixed"]["throw_site_present"]
    # The throw site region must be byte-identical in both worlds (the fix does
    # not touch ensure_engine_tick_hooked; the ladder order around it changes).
    assert report["control"]["ensure_hooks"] == report["fixed"]["ensure_hooks"], "ensure_hooks drifted"
    assert report["control"]["marshal_state"] == report["fixed"]["marshal_state"]
    assert report["control"]["process_simple_actions"] == report["fixed"]["process_simple_actions"]
    assert report["control"]["availability"] == report["fixed"]["availability"]
    assert report["control"]["hook_state"] == report["fixed"]["hook_state"]
    assert report["control"]["binding"] != report["fixed"]["binding"], "bindings unexpectedly identical"
    (outdir / "regions.json").write_text(json.dumps(report, indent=1))
    print("regions extracted OK")
    for world in ("control", "fixed"):
        print(world, "source", report[world]["source"]["sha256"][:16])
        print(world, "binding", report[world]["binding"][:16])


if __name__ == "__main__":
    main()
