#!/usr/bin/env python3
"""Region extractor for the worker-r3-ladder host integration test.

Extracts the REAL code regions (byte-exact, no re-typing) from the two staged
LuaMod.cpp lineages so the harness drives the actual binding source:

  marshal_state : kFuncPtrMarshalExecutor tag + [R3] FuncPtr-marshal state +
                  is_funcptr_marshal_armed (+, fixed only, the NEW
                  funcptr_marshal_usable helper — the one expected drift)
  availability  : is_engine_tick_hook_available / is_process_event_hook_available
  hook_state    : ensure_hook_thread_exists / get_hook_lua / make_hook_state
  ensure_hooks  : ensure_engine_tick_hooked / ensure_process_event_hooked
                  (contains the traced throw site "EngineTick callback
                  registration failed")
  binding_*     : the REAL WithDelay-family registrations under test:
                  ExecuteInGameThreadWithDelay, RetriggerableExecuteInGameThreadWithDelay,
                  ExecuteInGameThreadAfterFrames, LoopInGameThreadWithDelay,
                  LoopInGameThreadAfterFrames

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


def strip_helper_block(fixed_marshal: str) -> str:
    """Remove the [R3-LADDER] helper block (ifdef..endif incl. the blank line
    that separates it from the neighbours) so the shared marshal_state can be
    compared byte-for-byte against control."""
    lines = fixed_marshal.splitlines()
    comment = find_line(lines, "// [R3-LADDER] (worker-r3-ladder")
    # the helper opens with `#ifdef __PROSPERO__` directly above the comment
    start = comment - 1
    assert lines[start].strip() == "#ifdef __PROSPERO__", lines[start]
    endif = find_line(lines, "#endif", comment)
    # the fixed region spans both blank lines adjacent to the helper (the
    # control region stops at the `}`); remove ifdef..endif and BOTH blanks so
    # the stripped region is byte-identical to control
    begin = start
    if begin > 0 and lines[begin - 1].strip() == "":
        begin -= 1
    end = endif + 1
    if end < len(lines) and lines[end].strip() == "":
        end += 1
    return "\n".join(lines[:begin] + lines[end:]) + "\n"


BINDINGS = (
    (
        "binding_withdelay",
        'm_lua.register_function("ExecuteInGameThreadWithDelay"',
        "// RetriggerableExecuteInGameThreadWithDelay - executes callback after a time delay, resets timer if called again with same handle",
    ),
    (
        "binding_retriggerable",
        'm_lua.register_function("RetriggerableExecuteInGameThreadWithDelay"',
        "// ExecuteInGameThreadAfterFrames - executes callback after a frame delay",
    ),
    (
        "binding_afterframes",
        'm_lua.register_function("ExecuteInGameThreadAfterFrames"',
        "// LoopInGameThreadWithDelay - executes callback repeatedly with a time delay",
    ),
    (
        "binding_loopwithdelay",
        'm_lua.register_function("LoopInGameThreadWithDelay"',
        "// LoopInGameThreadAfterFrames - executes callback repeatedly with a frame delay",
    ),
    (
        "binding_loopafterframes",
        'm_lua.register_function("LoopInGameThreadAfterFrames"',
        "// ResetDelayedActionTimer - resets the timer for any delayed action using the original delay (only if owned by calling mod)",
    ),
)

THROW_MESSAGE = "EngineTick callback registration failed"
DEAD_DRAIN_MESSAGE = "its drain is dead (pump_pending=false)"
FRAMES_MESSAGE = "its drain cannot count frames"


def extract_world(path: Path, outdir: Path) -> dict:
    text = path.read_bytes()
    lines = text.decode("utf-8").splitlines()
    regions = {}

    regions["marshal_state"] = extract(
        lines,
        "// [R3-L10LM] File-local executor tag for the FuncPtr-marshal drain",
        "static auto process_simple_actions",
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
    for name, start_needle, end_needle in BINDINGS:
        regions[name] = extract(lines, start_needle, end_needle, -1)

    outdir.mkdir(parents=True, exist_ok=True)
    hashes = {"source": {"path": str(path), "sha256": sha256(text)}}
    for name, body in regions.items():
        data = body.encode("utf-8")
        (outdir / f"{name}.inc").write_bytes(data)
        hashes[name] = sha256(data)

    # World guards (proves the test worlds differ exactly where intended).
    hashes["has_marshal_usable_helper"] = "funcptr_marshal_usable" in regions["marshal_state"]
    hashes["has_ladder_marker_withdelay"] = "[R3-LADDER]" in regions["binding_withdelay"]
    hashes["has_ladder_marker_retriggerable"] = "[R3-LADDER]" in regions["binding_retriggerable"]
    hashes["has_ladder_marker_afterframes"] = "[R3-LADDER]" in regions["binding_afterframes"]
    hashes["has_ladder_marker_loopwithdelay"] = "[R3-LADDER]" in regions["binding_loopwithdelay"]
    hashes["has_ladder_marker_loopafterframes"] = "[R3-LADDER]" in regions["binding_loopafterframes"]
    hashes["has_dead_drain_refusal"] = DEAD_DRAIN_MESSAGE in regions["binding_withdelay"]
    hashes["has_frames_refusal"] = FRAMES_MESSAGE in regions["binding_afterframes"]
    hashes["throw_site_present"] = THROW_MESSAGE in regions["ensure_hooks"]
    bindings_complete = all(
        regions[name].rstrip().endswith("});") for name, _, _ in BINDINGS
    )
    hashes["bindings_complete"] = bindings_complete
    return hashes


def main() -> None:
    control = Path(sys.argv[1])
    fixed = Path(sys.argv[2])
    outdir = Path(sys.argv[3])
    report = {
        "control": extract_world(control, outdir / "control"),
        "fixed": extract_world(fixed, outdir / "fixed"),
    }
    c, f = report["control"], report["fixed"]

    # The fix markers must exist ONLY in fixed.
    assert not c["has_marshal_usable_helper"], "control unexpectedly has the usable-pump helper"
    assert f["has_marshal_usable_helper"], "fixed missing the usable-pump helper"
    for key in (
        "has_ladder_marker_withdelay",
        "has_ladder_marker_retriggerable",
        "has_ladder_marker_afterframes",
        "has_ladder_marker_loopwithdelay",
        "has_ladder_marker_loopafterframes",
    ):
        assert not c[key], f"control unexpectedly patched: {key}"
        assert f[key], f"fixed missing ladder marker: {key}"
    assert f["has_dead_drain_refusal"], "fixed missing the dead-drain refusal message"
    assert not c["has_dead_drain_refusal"], "control unexpectedly has the dead-drain refusal"
    assert f["has_frames_refusal"], "fixed missing the frame-capability refusal message"

    # Shared regions must be byte-identical (the fix touches only the ladders
    # and adds the helper inside marshal_state).
    assert c["availability"] == f["availability"], "availability drifted"
    assert c["hook_state"] == f["hook_state"], "hook_state drifted"
    assert c["ensure_hooks"] == f["ensure_hooks"], "ensure_hooks drifted (throw sites must be untouched)"
    assert c["throw_site_present"] and f["throw_site_present"]
    assert strip_helper_block(f["marshal_state"] and (outdir / "fixed/marshal_state.inc").read_text()) == (outdir / "control/marshal_state.inc").read_text(), (
        "marshal_state differs beyond the funcptr_marshal_usable helper block"
    )
    # Each binding differs (every tested binding carries the fold).
    for name, _, _ in BINDINGS:
        assert c[name] != f[name], f"binding unexpectedly identical: {name}"
    assert c["bindings_complete"] and f["bindings_complete"], "binding region truncated"

    (outdir / "regions.json").write_text(json.dumps(report, indent=1))
    print("regions extracted OK")
    for world in ("control", "fixed"):
        print(world, "source", report[world]["source"]["sha256"][:16])


if __name__ == "__main__":
    main()
