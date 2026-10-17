#!/usr/bin/env python3
"""emit_gate_decl.py — Team S1 (B3/S2). Emit a GATE-INPUT shim header from a
profile .ini so field_gate.py (--decl) and layout_binder_gate.py (argv[1]) run
against the TARGET GAME's layout + engine version with ZERO gate code changes.

Both gates read the per-game member layout from a CompiledNativeLayoutDeclarations
.hpp whose 4th line is the member INI string literal and whose [NativeLayout]
header carries EngineMajor/EngineMinor (field_gate prunes version-dead branches
for exactly that version — so a UE4 profile makes the gates check the UE4
branches, which READINESS S2 asks for). In the data world the runtime no longer
compiles such a header; this shim is a GATE INPUT ONLY, generated per game from
the profile's [layout.members] / [layout.vtables] blocks. It is never compiled
into the runtime.

Usage: python3 -I gen/emit_gate_decl.py --profile gen/townfall-profile.ini --out /tmp/gate-decl.hpp
"""
import argparse, os, re, sys

def read_blocks(ini_path):
    members = vtables = None
    cur = None; buf = []; grab = None
    for line in open(ini_path, errors="replace"):
        s = line.rstrip("\n")
        t = s.strip()
        if grab:                       # inside a <<< >>> block: only >>> ends it
            if t == ">>>":
                if grab == "layout.members": members = "\n".join(buf)
                elif grab == "layout.vtables": vtables = "\n".join(buf)
                grab = None
            else:
                buf.append(s)          # [NativeLayout] etc. are block CONTENT
            continue
        if t.startswith("[") and t.endswith("]"):
            cur = t[1:-1]; continue
        if t == "<<<" and cur in ("layout.members", "layout.vtables"):
            grab = cur; buf = []
    return members, vtables

def digest(ini_path):
    m = re.search(r"^digest=([0-9a-f]{64})", open(ini_path, errors="replace").read(), re.M)
    return m.group(1) if m else "0" * 64

def cxx_escape(text):
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--profile", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    members, vtables = read_blocks(a.profile)
    if not members or not vtables:
        print("gate-decl: profile missing [layout.members]/[layout.vtables]"); sys.exit(2)
    d = digest(a.profile)
    # Line layout MUST match CompiledNativeLayoutDeclarations.hpp: the members
    # literal is the 4th line (layout_binder_gate reads split('\n')[3]);
    # field_gate reads the first string literal + the [NativeLayout] header.
    # EXACT line layout of CompiledNativeLayoutDeclarations.hpp: members literal
    # on line index 3 (layout_binder_gate reads split('\n')[3]); field_gate uses
    # the first string literal regardless of line. Line 0 #pragma once, line 1
    # namespace, line 2 the aggregate open, line 3 members, line 4 vtables.
    out = []
    out.append("#pragma once")                                                 # 0
    out.append("namespace RC::Platform::NativeLayouts {")                        # 1
    out.append("inline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{")  # 2
    out.append('"%s",' % cxx_escape(members + "\n"))                            # 3  <- members
    out.append('"%s",' % cxx_escape(vtables + "\n"))                            # 4  <- vtables
    out.append('"gate-input-shim",')                                            # 5
    out.append('"%s"' % d)                                                      # 6
    out.append("};")
    out.append("}")
    out.append("// GENERATED GATE-INPUT SHIM (Team S1 emit_gate_decl.py) — never compiled into the runtime.")
    with open(a.out, "w") as f:
        f.write("\n".join(out) + "\n")
    maj = re.search(r"EngineMajor=(\d+)", members)
    mino = re.search(r"EngineMinor=(\d+)", members)
    print("wrote %s (members literal on line index 3; EngineMajor=%s EngineMinor=%s)"
          % (a.out, maj.group(1) if maj else "?", mino.group(1) if mino else "?"))
    print("Point field_gate at it with --decl; pass it to layout_binder_gate as argv[1].")

if __name__ == "__main__":
    main()
