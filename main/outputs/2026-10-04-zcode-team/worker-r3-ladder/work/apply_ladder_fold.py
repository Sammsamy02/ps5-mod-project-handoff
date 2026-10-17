#!/usr/bin/env python3
"""Apply the [R3-LADDER] marshal-first fold to the worker-r3-ladder staging copy.

Base: worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp (c92c92ff...)
Out:  worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp

Every anchor is asserted unique before substitution; any mismatch aborts with
no write (the file is written only after all six edits verified in memory).
"""
from pathlib import Path

STAGED = Path(__file__).resolve().parent.parent / "staging/luamod-l11-r3-ladder/LuaMod.cpp"

# ---------------------------------------------------------------- helper --
HELPER_ANCHOR = """    static auto is_funcptr_marshal_armed() -> bool
    {
        return m_funcptr_marshal_armed.load(std::memory_order_acquire);
    }
"""
HELPER_NEW = HELPER_ANCHOR + """
#ifdef __PROSPERO__
    // [R3-LADDER] (worker-r3-ladder 2026-10-05; RUNBOOK l11-marshal-integration
    // 5.1 item 1, the WithDelay-family remainder) Usable-pump predicate for the
    // ladder re-order. The marshal is the game-thread pump only while the rev-B
    // pairing law holds: the initializer deferred its EngineTick registrations
    // (R3MarshalPumpPending, DIFF 2). armed && !pump_pending is that pairing
    // contradiction - a marshal armed while the initializer owns the pump -
    // and is treated as a PROVABLY DEAD drain: the ExecuteInGameThread /
    // WithDelay-family ladders refuse loudly instead of silently queueing onto
    // it or silently arming a second EngineTick pump. Deliberately NOT a
    // "drain succeeded" test: between arming and the first carrier invocation
    // the identity publication is legitimately pending, and queueing then is
    // the designed lazy semantics (R3D 4.4a); publication refusal remains the
    // drain body's own fail-closed gate (funcptr_marshal_hook).
    static auto funcptr_marshal_usable() -> bool
    {
        return is_funcptr_marshal_armed() && Unreal::R3MarshalPumpPending();
    }
#endif
"""

# ------------------------------------------------- WithDelay-family gate --
METHOD_LINE = "            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;\n"


def withdelay_gate(fn: str) -> str:
    return METHOD_LINE + """#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 item 1 remainder; distinct-ID revision
            // worker-r3-ladder 2026-10-05): marshal-first when the marshal is
            // the USABLE game-thread pump. Address-availability is NOT pump
            // ownership: in the walk-permitted world the initializer's vtable
            // fallback assigns TickInternal, so the availability-first order
            // below reached ensure_engine_tick_hooked() BEFORE the enqueue, its
            // refused detour registration threw "EngineTick callback
            // registration failed", and the closure was lost with the armed
            // marshal bypassed (the exact 5.1 trap). The usable marshal takes
            // the method FIRST - deterministically, EngineTick and ProcessEvent
            // defaults alike - and an armed-but-dead drain (pump_pending=false,
            // the rev-B pairing contradiction) refuses loudly instead of
            // falling back silently.
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
            else if (is_funcptr_marshal_armed())
            {
                lua.throw_error(\"""" + fn + """: R3 FuncPtr-marshal carrier is armed but its drain is dead (pump_pending=false); refusing to queue onto a dead pump and refusing to arm a second EngineTick pump\");
            }
#endif
"""


def afterframes_old(fn: str) -> str:
    return """            // Frame-based delays require EngineTick - cannot use ProcessEvent
            if (!is_engine_tick_hook_available())
            {
                lua.throw_error(\"""" + fn + """: EngineTick hook is not available (AOB scan failed). Frame-based delays require EngineTick.\");
            }
"""


def afterframes_new(fn: str) -> str:
    return """            // Frame-based delays require EngineTick - cannot use ProcessEvent
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 family closure; worker-r3-ladder
            // 2026-10-05): with the marshal the usable pump, no EngineTick
            // detour may be attempted here - the old path fell through to
            // ensure_engine_tick_hooked(), whose refused registration threw and
            // discarded the closure while the marshal sat armed. The marshal
            // drain deliberately skips frame-based actions
            // (process_delayed_actions: frame delays remain EngineTick-only),
            // so a frame-based request is refused LOUDLY and precisely instead
            // of being silently stalled on a pump that can never count it.
            if (funcptr_marshal_usable())
            {
                lua.throw_error(\"""" + fn + """: frame-based delays need an EngineTick pump; only the R3 FuncPtr-marshal pump exists in this world and its drain cannot count frames\");
            }
#endif
""" + afterframes_old(fn)


WITHDELAY_FNS = (
    ("No overload found for function 'ExecuteInGameThreadWithDelay'.", "ExecuteInGameThreadWithDelay"),
    ("No overload found for function 'RetriggerableExecuteInGameThreadWithDelay'.", "RetriggerableExecuteInGameThreadWithDelay"),
    ("No overload found for function 'LoopInGameThreadWithDelay'.", "LoopInGameThreadWithDelay"),
)
AFTERFRAMES_FNS = ("ExecuteInGameThreadAfterFrames", "LoopInGameThreadAfterFrames")


def main() -> None:
    text = STAGED.read_text(encoding="utf-8")

    # 1. helper after is_funcptr_marshal_armed
    assert text.count(HELPER_ANCHOR) == 1, f"helper anchor count {text.count(HELPER_ANCHOR)}"
    text = text.replace(HELPER_ANCHOR, HELPER_NEW, 1)

    # 2-4. WithDelay family: insert after THIS function's method-selection line
    # 4 occurrences: ExecuteInGameThread (already EL-fixed at ~4997) + the three
    # WithDelay-family functions this revision targets.
    assert text.count(METHOD_LINE) == 4, f"method-line count {text.count(METHOD_LINE)}"
    for marker, fn in WITHDELAY_FNS:
        mi = text.find(marker)
        assert mi != -1, f"marker missing: {fn}"
        assert text.find(marker, mi + 1) == -1, f"marker not unique: {fn}"
        li = text.find(METHOD_LINE, mi)
        assert li != -1, f"method line not after marker: {fn}"
        end = li + len(METHOD_LINE)
        text = text[:li] + withdelay_gate(fn) + text[end:]

    # 5-6. AfterFrames pair
    for fn in AFTERFRAMES_FNS:
        old = afterframes_old(fn)
        assert text.count(old) == 1, f"afterframes anchor count {fn}: {text.count(old)}"
        text = text.replace(old, afterframes_new(fn), 1)

    # guards: refs = 1 (definition line) + 5 (three WithDelay + two AfterFrames)
    n_usable = text.count("funcptr_marshal_usable()")
    assert n_usable == 6, f"usable refs {n_usable}"
    assert text.count("[R3-LADDER]") == 6, text.count("[R3-LADDER]")

    STAGED.write_text(text, encoding="utf-8")
    print(f"applied 6 hunks; bytes {len(text)}")


if __name__ == "__main__":
    main()
