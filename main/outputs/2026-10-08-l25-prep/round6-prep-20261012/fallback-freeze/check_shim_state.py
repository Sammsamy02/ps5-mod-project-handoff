#!/usr/bin/env python3
"""check_shim_state.py -- Mac-only, read-only static check of what the path shim
and the B2/B3 features look like inside each frozen build's OWN source snapshot.

Question answered: on the fallback builds (B3, B2b, B1; 1e49188b as the
known-good reference), does the path shim install, or does it switch itself off
with "reason=no-root" before any io/os wrapping? And which self-test-visible
features (GetLuaMemory, ForEachUObject stack fix, IterateGameDirectories PS5
layout, [UEN] logging) does each build carry? B4 is checked as the contrast.

Reads the snapshot tars in outputs/2026-10-08-l26-frozen/<dir>/ IN MEMORY
(tarfile.extractfile); nothing is extracted and no source text is written:
the log carries only line numbers, short hashes and our own marker strings.
Also reads the staged self-test (morning-launch-20261012/upload/.../ueselftest)
and this dir's host-test log (shim-root-lexical-host-test.log). Writes nothing
except stdout. Prints how many checks ran; exits 1 if any failed or zero ran.
"""
import glob
import hashlib
import os
import re
import sys
import tarfile

OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"
FROZEN = os.path.join(OUT, "2026-10-08-l26-frozen")
HERE = os.path.dirname(os.path.abspath(__file__))
SELFTEST = os.path.join(OUT, "2026-10-08-l25-prep/morning-launch-20261012/upload/ue4ss/Mods/ueselftest/Scripts")
HOSTLOG = os.path.join(HERE, "shim-root-lexical-host-test.log")
SRC = "2026-10-08-l26-runtime/typescan-r4/candidate/src/"
INC = "2026-10-08-l26-runtime/typescan-r4/candidate/include/"

FALLBACKS = [
    "1e49188b-acceptance-pass1",           # known-good reference (already frozen)
    "B1-lookup-nocache-b9cc5248",
    "B2b-error-wrapper-memlog-74fab85a",
    "B3-iterators-40b8972b",
]
B4 = "B4-shim-final-stack-d044cf0b"
FALLBACK_SHIM_SHA = "821d74a2"   # LuaPathShim.cpp in 1e49188b/B1/B2b/B3 (snapshot manifests)

n = 0
failed = []


def ok(cond, what):
    global n
    n += 1
    print("  [%s] %s" % ("PASS" if cond else "FAIL", what))
    if not cond:
        failed.append(what)
    return bool(cond)


def load(d):
    tars = glob.glob(os.path.join(FROZEN, d, "snapshot-*.tar.gz"))
    if len(tars) != 1:
        return None, {}
    files = {}
    with tarfile.open(tars[0]) as t:
        for m in t.getmembers():
            if m.isfile() and (m.name.endswith(".cpp") or m.name.endswith(".hpp")):
                files[m.name] = t.extractfile(m).read().decode("utf-8", "replace")
    return os.path.basename(tars[0]), files


def first_line(text, needle, start=0):
    for i, line in enumerate(text.splitlines()[start:], start + 1):
        if needle in line:
            return i
    return None


def func_span(text, signature):
    """1-based [start, end] of a C++ function body by brace matching."""
    lines = text.splitlines()
    s = first_line(text, signature)
    if s is None:
        return None, None
    depth, seen = 0, False
    for i in range(s - 1, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if "{" in lines[i]:
            seen = True
        if seen and depth == 0:
            return s, i + 1
    return s, None


def shim_block(luamod):
    s = first_line(luamod, "extern auto LuaPathShim_install(lua_State*")
    e = first_line(luamod, "LuaPathShim_install(lua.get_lua_state()")
    if s is None or e is None or e < s:
        return None, None, None
    blk = "\n".join(luamod.splitlines()[s - 1:e])
    return s, e, hashlib.sha256(blk.encode()).hexdigest()[:12]


def main():
    ref_block = None
    print("== fallback builds: shim must switch itself off (no-root) ==")
    for d in FALLBACKS:
        tarname, files = load(d)
        print("-- %s (%s, %d .cpp/.hpp members) --" % (d, tarname, len(files)))
        if not ok(tarname is not None and files, "%s: snapshot tar found and read" % d):
            continue
        prog = files.get(SRC + "UE4SSProgram.cpp", "")
        luamod = files.get(SRC + "LuaMod.cpp", "")
        shim = files.get(SRC + "LuaPathShim.cpp", "")
        ok(prog and luamod and shim, "%s: UE4SSProgram.cpp, LuaMod.cpp, LuaPathShim.cpp present" % d)

        # 1. native mods directory is the RELATIVE literal
        fs, fe = func_span(prog, "auto UE4SSProgram::setup_mod_directory_path() -> void")
        body = "\n".join(prog.splitlines()[fs - 1:fe]) if fs and fe else ""
        rel = first_line(prog, 'm_mods_directories = {std::filesystem::path("ue4ss/Mods")};')
        ok(fs and fe and rel and fs < rel < fe,
           "%s: setup_mod_directory_path (UE4SSProgram.cpp:%s-%s) sets the relative literal \"ue4ss/Mods\" at :%s"
           % (d, fs, fe, rel))
        ok("configured_working_directory" not in prog,
           "%s: UE4SSProgram.cpp never reads configured_working_directory (the B4 root source)" % d)
        ok("mod_source" in body, "%s: that assignment is inside the native mod_source branch" % d)

        # 2. LuaMod.cpp shim block: lexical strip of get_mods_directory()
        s, e, h = shim_block(luamod)
        blk = "\n".join(luamod.splitlines()[s - 1:e]) if s else ""
        ok(s is not None and "get_mods_directory()).lexically_normal()" in blk
           and 'filename() == "Mods"' in blk and blk.count("parent_path()") == 2,
           "%s: LuaMod.cpp:%s-%s shim block derives the root lexically from get_mods_directory() (block sha %s)"
           % (d, s, e, h))
        if ref_block is None:
            ref_block = h
        ok(h == ref_block, "%s: shim block identical to 1e49188b's (%s)" % (d, ref_block))

        # 3. LuaPathShim.cpp: no-root return comes before any io/os wrapping
        sh = hashlib.sha256(shim.encode()).hexdigest()
        ok(sh.startswith(FALLBACK_SHIM_SHA), "%s: LuaPathShim.cpp sha %s... (== fallback shim %s)" % (d, sh[:8], FALLBACK_SHIM_SHA))
        guard = first_line(shim, "if (context->modbridge_root.empty())")
        msg = first_line(shim, "[SHIM] path-resolver disabled reason=no-root")
        ret = first_line(shim, "return;", msg or 0)
        io_wrap = first_line(shim, 'wrap_into(L, io_new, context_index, "open", io_open_wrapper);')
        os_wrap = first_line(shim, 'wrap_into(L, -1, context_index, "remove", os_remove_wrapper);')
        ok(guard and msg and ret and guard < msg < ret <= msg + 2,
           "%s: LuaPathShim.cpp:%s empty-root guard prints reason=no-root at :%s and returns at :%s" % (d, guard, msg, ret))
        ok(io_wrap and ret and ret < io_wrap, "%s: the return (:%s) precedes the io wrapping (:%s)" % (d, ret, io_wrap))
        ok(os_wrap and ret < os_wrap,
           "%s: latent relative-index os wrap still present at :%s but unreachable after the no-root return" % (d, os_wrap))

        # 4. no production caller of the test-only root override
        callers = [name for name, text in files.items()
                   if re.search(r"set_modbridge_root_override\s*\(", text)
                   and not name.endswith(("LuaPathShim.cpp", "LuaPathShim.hpp"))]
        ok(not callers, "%s: test-only set_modbridge_root_override has no caller in the snapshot outside LuaPathShim.cpp/.hpp "
           "(%d snapshot .cpp/.hpp files scanned)" % (d, len(files)))

    print("== B4 contrast: root supplied, no relative os wrap ==")
    tarname, files = load(B4)
    if ok(tarname is not None and files, "%s: snapshot tar found and read" % B4):
        prog = files.get(SRC + "UE4SSProgram.cpp", "")
        shim = files.get(SRC + "LuaPathShim.cpp", "")
        luamod = files.get(SRC + "LuaMod.cpp", "")
        fs, fe = func_span(prog, "auto UE4SSProgram::setup_mod_directory_path() -> void")
        cwd = first_line(prog, "m_native_context.configured_working_directory")
        ok(fs and fe and cwd and fs < cwd < fe,
           "B4: setup_mod_directory_path (UE4SSProgram.cpp:%s-%s) reads configured_working_directory at :%s" % (fs, fe, cwd))
        s, e, h = shim_block(luamod)
        ok(h == ref_block, "B4: LuaMod.cpp:%s-%s shim block is the same as the fallbacks' (only the mods dir differs)" % (s, e))
        ok('wrap_into(L, -1, context_index, "remove"' not in shim,
           "B4: LuaPathShim.cpp has no relative-index os wrap (fixed, absolute os_index)")

    print("== feature presence per build (self-test visible) ==")
    feats = [
        ("GetLuaMemory binding", SRC + "LuaMod.cpp", '"GetLuaMemory"', {"1e49188b-acceptance-pass1": False, "B1-lookup-nocache-b9cc5248": False}),
        ("ForEachUObject per-iteration stack restore", SRC + "LuaMod.cpp", "fou_baseline",
         {"1e49188b-acceptance-pass1": False, "B1-lookup-nocache-b9cc5248": False, "B2b-error-wrapper-memlog-74fab85a": False}),
        ("IterateGameDirectories PS5 flat layout", SRC + "LuaMod.cpp", "igd_flat_layout",
         {"1e49188b-acceptance-pass1": False, "B1-lookup-nocache-b9cc5248": False, "B2b-error-wrapper-memlog-74fab85a": False}),
        ("UEnum.ForEachName [UEN] lines", SRC + "LuaType/LuaUEnum.cpp", "[UEN] ForEachName",
         {"1e49188b-acceptance-pass1": False, "B1-lookup-nocache-b9cc5248": False, "B2b-error-wrapper-memlog-74fab85a": False}),
    ]
    for d in FALLBACKS[1:] + [B4]:
        _, files = load(d)
        for name, member, needle, absent_in in feats:
            want = d not in absent_in
            have = needle in files.get(member, "")
            ok(have == want, "%s: %s %s" % (d, name, "present" if want else "absent"))

    print("== staged self-test: what a shim-less run can print ==")
    main_lua = open(os.path.join(SELFTEST, "main.lua"), encoding="utf-8").read()
    p11 = open(os.path.join(SELFTEST, "part11.lua"), encoding="utf-8").read()
    pu = first_line(main_lua, 'P("progress-io unavailable (resume disabled)")')
    ok(pu is not None, "main.lua:%s prints 'progress-io unavailable' when no progress path opens" % pu)
    ml = main_lua.splitlines()
    os_ = first_line(main_lua, "local function ONE(e)")
    oe_ = next((i for i in range(os_ or len(ml), len(ml) + 1) if ml[i - 1] == "end"), None) if os_ else None
    one_body = ml[os_ - 1:oe_] if os_ and oe_ else []
    one_fail = [first_line(main_lua, 'if not ok then P(l.."FAIL "'), first_line(main_lua, 'elseif a == false then P(l.."FAIL "')]
    ok(all(one_fail) and sum(l.count('"FAIL "') for l in one_body) == 2,
       "main.lua ONE() (:%s-%s) has exactly 2 FAIL paths: :%s (pcall error) and :%s (false return)"
       % (os_, oe_, one_fail[0], one_fail[1]))
    io_line = [l for l in p11.splitlines() if l.startswith('{"io-roundtrip"')]
    ok(len(io_line) == 1 and "return false" not in io_line[0] and 'return "SKIP", "no-io-path"' in io_line[0],
       "part11.lua:%s io-roundtrip has no 'return false': it can only SKIP (no-io-path when progress-io is unavailable) or PASS"
       % first_line(p11, '{"io-roundtrip"'))
    ok("pcall(GetLuaMemory" in main_lua or "pcall(function() return GetLuaMemory() end)" in main_lua,
       "main.lua calls GetLuaMemory inside pcall (prints 'unavailable' on B1)")

    print("== host test: lexical root derivation (std::filesystem, Apple clang) ==")
    host = open(HOSTLOG, encoding="utf-8").read() if os.path.isfile(HOSTLOG) else ""
    ok('mods_dir="ue4ss/Mods" -> shim_root="" (empty=yes' in host and "# run exit: 0" in host
       and "SUMMARY shim-root-lexical: checks=3 pass=3 fail=0" in host,
       "shim-root-lexical-host-test.log: relative \"ue4ss/Mods\" derives an EMPTY root; absolute derives the root")

    print("SUMMARY shim-state: checks=%d pass=%d fail=%d" % (n, n - len(failed), len(failed)))
    if n == 0:
        print("FAIL: zero checks ran")
        return 1
    if failed:
        print("FAIL: " + "; ".join(failed))
        return 1
    print("RESULT shim-state: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
