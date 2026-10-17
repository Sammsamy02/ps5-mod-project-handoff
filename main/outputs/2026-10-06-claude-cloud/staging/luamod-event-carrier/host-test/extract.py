#!/usr/bin/env python3
"""Extract the event-carrier functions VERBATIM from the staged LuaMod.cpp into
extracted.inc for the host test (no edits; markers must match exactly once)."""
import sys, pathlib
src = pathlib.Path(sys.argv[1]).read_text()
def cut(start, end):
    i = src.index(start); assert src.count(start) == 1, start
    j = src.index(end, i); return src[i:j]
parts = [
    cut("    static auto r3_marshal_lazy_arm() -> void; // defined after", "#endif\n\n    static auto process_simple_actions"),
    cut("    static auto r3_event_carrier_precheck(", "    // =====================================================================================\n    // [R3] THE CALIBRATION CONSUMER"),
    cut("    static auto r3_marshal_lazy_arm() -> void\n    {", "#endif\n\n    // Helper to ensure engine tick hook is registered"),
]
pathlib.Path(sys.argv[2]).write_text("\n".join(parts))
print("extracted", sum(p.count("\n") for p in parts), "lines")
