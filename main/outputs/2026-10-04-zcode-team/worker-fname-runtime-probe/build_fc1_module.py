#!/usr/bin/env python3
"""build_fc1_module.py — worker-fname-runtime-probe (team 2026-10-04).

Builds + host-tests the FC-1 runtime verification ADD-ON module for the
NEXT probe iteration (igf6 or a combined igf5 build). This is a
PREPARATION delivery: it does NOT package a PRX and does NOT execute on a
console — the controller vendors src/fc1_*.c|cpp into the next probe build
(see INTEGRATION.md).

Pipeline:
  1. verify the frozen capture sha256, then RE-EXTRACT the target 64 bytes
     and the 9 call-site window byte sequences (file_offset = vaddr+0x4000);
     loud-fail on any drift vs (a) the checked-in fixtures JSON,
     (b) worker-fname-fc1-materialize/calls_by_anchor.materialized.json,
     (c) worker-expected-bytes/bytes_fname.bin;
  2. parse SYS_mprotect / PROT_* / PAGE_SHIFT from the PINNED SDK headers
     at build time (igf5 convention; nothing hardcoded);
  3. generate fc1_config.h (real constants, for the igf6 integration) and
     the host-test config + fixtures (synthetic runtime addresses, captured
     byte arrays);
  4. target compile check: every module TU compiles clean with the pinned
     prospero clang under -Wall -Wextra -Werror;
  5. host build (clang) with the host mprotect shim override + synthetic
     arenas; run all 8 scenarios; run_host_checks.py asserts every verdict;
  6. emit build-receipt.json.
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]
assert WHAT.name == "what", WHAT
TEAM = WHAT / "outputs/2026-10-04-zcode-team"
SRC = HERE / "src"
TESTS = HERE / "tests"
FIXTURES_JSON = HERE / "fixtures/fc1_windows_captured.json"

CAPTURE = (WHAT / ".superpowers/native-observations/"
                "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
CAPTURE_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
CAPTURE_SIZE = 194241424
LOAD0_DELTA = 0x4000
IMAGE_BASE = 0x400000

MATERIALIZED = TEAM / ("worker-fname-fc1-materialize/"
                       "calls_by_anchor.materialized.json")
EXPECTED_BIN = TEAM / "worker-expected-bytes/bytes_fname.bin"

# Pinned SDK (constants parsed at build time; NEVER hardcoded here).
SDK_INC = WHAT / ("outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/"
                  "ps5-payload-sdk/target/include")
SYSCALL_H = SDK_INC / "sys/syscall.h"
MMAN_H = SDK_INC / "sys/mman.h"
PARAM_H = SDK_INC / "machine/param.h"
CLANG = (WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/"
         "ps5-payload-sdk/bin/prospero-clang")
CLANGXX = (WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/"
           "ps5-payload-sdk/bin/prospero-clang++")

# Controller checkpoint facts (same as igf5's generated config).
EXEC_SPAN_START = 0x400000
EXEC_SPAN_END = 0x787C000
EXEC_PROT_FALLBACK = 0x4
PAGE_SIZE = 0x4000

TARGET_VADDR = 0x23D590
TARGET_RT = IMAGE_BASE + TARGET_VADDR
ANCHOR_NAMES = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape",
                "RenderCore"]
ANCHOR_STR_VADDR = [0x782706E, 0x799DB12, 0x75EEFC0, 0x763151E, 0x79F02E2]

# Synthetic host layout (must match tests/fc1_host_env.c; enforced by the
# compile-time #error guards there).
HOST_EXEC_BASE = 0x110000000
HOST_DATA_BASE = 0x110010000
HOST_WINDOW_STRIDE = 128
HOST_STR_OFF = [0x00, 0x40, 0x80, 0xC0, 0x100]

CFLAGS_TARGET = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC",
                 "-fno-stack-protector", "-fvisibility=hidden",
                 "-ffreestanding", "-fno-builtin",
                 "-fno-asynchronous-unwind-tables"]
CPPFLAGS_TARGET = CFLAGS_TARGET + ["-fno-exceptions", "-fno-rtti",
                                   "-fno-threadsafe-statics", "-std=c++17"]
CFLAGS_HOST = ["-Wall", "-Wextra", "-Werror", "-O1", "-g", "-std=c11"]
CPPFLAGS_HOST = ["-Wall", "-Wextra", "-Werror", "-O1", "-g", "-std=c++17"]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


LLVM_BIN = (WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
            "LLVM-21.1.8-macOS-ARM64/bin")  # prospero wrapper's clang host


def run(argv, receipt, allow_fail=False):
    env = dict(__import__("os").environ)
    # The prospero wrapper scripts locate their host LLVM via LLVM_CONFIG
    # (same mechanism as the igf5 build receipt).
    env["LLVM_CONFIG"] = str(LLVM_BIN / "llvm-config")
    r = subprocess.run([str(a) for a in argv], capture_output=True,
                       text=True, env=env)
    receipt.append({"argv": [str(a) for a in argv], "rc": r.returncode,
                    "stdout": r.stdout[-4000:], "stderr": r.stderr[-4000:]})
    if r.returncode != 0 and not allow_fail:
        print(receipt[-1]["stdout"])
        print(receipt[-1]["stderr"])
        sys.exit(f"FAILED: {argv}")
    return r


# ---- step 1: extraction + three-way cross-check ------------------------

def extract(receipt):
    data = CAPTURE.read_bytes()
    if len(data) != CAPTURE_SIZE or sha256(CAPTURE) != CAPTURE_SHA:
        sys.exit("LOUD FAILURE: frozen capture drifted")

    def rd(vaddr, n):
        off = vaddr + LOAD0_DELTA
        return data[off:off + n]

    import struct
    target64 = rd(TARGET_VADDR, 64)
    windows = []
    checked = json.loads(FIXTURES_JSON.read_text())
    if checked["target64_hex"] != target64.hex():
        sys.exit("LOUD FAILURE: target64 drifted vs checked-in fixtures")
    for w in checked["windows"]:
        anchor, wstart, call, s = (w["anchor"], int(w["start_vaddr"], 16),
                                   int(w["call_vaddr"], 16),
                                   int(w["anchor_str_vaddr"], 16))
        ln = call + 5 - wstart
        b = rd(wstart, ln)
        cb = rd(call, 5)
        rel = struct.unpack("<i", cb[1:5])[0]
        if cb[0] != 0xE8 or call + 5 + rel != TARGET_VADDR:
            sys.exit(f"LOUD FAILURE: call-site arithmetic failed at "
                     f"{hex(call)}")
        if b.hex() != w["bytes_hex"]:
            sys.exit(f"LOUD FAILURE: window bytes drifted at {hex(wstart)}")
        windows.append({"anchor": ANCHOR_NAMES.index(anchor),
                        "start_vaddr": wstart, "call_vaddr": call,
                        "anchor_str_vaddr": s, "len": ln, "bytes": b})

    # Independent bundles must agree.
    mat = json.loads(MATERIALIZED.read_text())
    if mat["target"]["bytes64_hex"] != target64.hex():
        sys.exit("LOUD FAILURE: target64 disagrees with fc1-materialize")
    eb = EXPECTED_BIN.read_bytes()
    # bytes_fname.bin is the first 32 bytes (hard-gate half) of the 64.
    if eb.hex() != target64[:32].hex():
        sys.exit("LOUD FAILURE: target64 disagrees with worker-expected-bytes")
    mat_sites = [(int(c["call_vaddr"], 16), int(c["windows"][0]["start_vaddr"], 16))
                 for c in mat["call_sites"]]
    for (mcv, mwv), w in zip(mat_sites, windows):
        if mcv != w["call_vaddr"] or mwv != w["start_vaddr"]:
            sys.exit("LOUD FAILURE: call-site table drifted vs materialized")

    receipt.append({"step": "extract", "capture_sha256": sha256(CAPTURE),
                    "target64": target64.hex(),
                    "windows": [{"call_vaddr": hex(w["call_vaddr"]),
                                 "len": w["len"]}
                                for w in windows]})
    return target64, windows


# ---- step 2: pinned SDK constants ---------------------------------------

def parse_sdk(receipt):
    txt = SYSCALL_H.read_text()
    m = re.search(r"#define\s+SYS_mprotect\s+(\d+)", txt)
    if not m:
        sys.exit(f"LOUD FAILURE: SYS_mprotect not in {SYSCALL_H}")
    sys_mprotect = int(m.group(1))
    txt = MMAN_H.read_text()
    prots = {}
    for name in ("PROT_NONE", "PROT_READ", "PROT_WRITE", "PROT_EXEC"):
        m = re.search(rf"#define\s+{name}\s+(0x[0-9a-fA-F]+|\d+)", txt)
        if not m:
            sys.exit(f"LOUD FAILURE: {name} not in {MMAN_H}")
        prots[name] = int(m.group(1), 0)
    if prots["PROT_READ"] != 1 or prots["PROT_EXEC"] != 4:
        sys.exit(f"LOUD FAILURE: unexpected PROT numbering {prots}")
    txt = PARAM_H.read_text()
    shifts = sorted({int(x) for x in
                     re.findall(r"#define\s+PAGE_SHIFT\s+(\d+)", txt)})
    if 14 not in shifts:
        sys.exit(f"LOUD FAILURE: PAGE_SHIFT 14 not among {shifts}")
    receipt.append({"step": "sdk_constants", "sys_mprotect": sys_mprotect,
                    "prots": prots, "page_shifts": shifts})
    return {"sys_mprotect": sys_mprotect, "prots": prots}


# ---- step 3: generated configs + fixtures ------------------------------

def emit_config(build_id, target64, windows, host):
    lines = [
        "// GENERATED by build_fc1_module.py — do not edit by hand.",
        "#pragma once",
        f'#define IGFC1_BUILD_ID "{build_id}"',
        "#define IGFC1_IMAGE_SHA16 \"3b6cfad47dd17fea\"",
        f'#define IGFC1_TARGET64_HEX "{target64.hex()}"',
        f"#define IGFC1_VADDR_FNAME_CTOR 0x{TARGET_VADDR:x}ull",
        f"#define IGFC1_RT_FNAME_CTOR 0x{TARGET_RT if not host else HOST_EXEC_BASE + 0x4000:x}ull",
        f"#define IGFC1_IMAGE_BASE 0x{IMAGE_BASE:x}ull",
        f"#define IGFC1_LOAD0_DELTA 0x{LOAD0_DELTA:x}ull",
        f"#define IGFC1_EXEC_SPAN_START 0x{EXEC_SPAN_START if not host else HOST_EXEC_BASE:x}ull",
        f"#define IGFC1_EXEC_SPAN_END 0x{EXEC_SPAN_END if not host else HOST_EXEC_BASE + 0x8000:x}ull",
        f"#define IGFC1_EXEC_PROT_FALLBACK 0x{EXEC_PROT_FALLBACK:x}",
        f"#define IGFC1_PAGE_SIZE 0x{PAGE_SIZE:x}ull",
        f"#define IGCAR_SYS_MPROTECT {SDK['sys_mprotect']}",
        f"#define IGFC1_PROT_READ 0x{SDK['prots']['PROT_READ']:x}",
        f"#define IGFC1_PROT_EXEC 0x{SDK['prots']['PROT_EXEC']:x}",
        "#define IGFC1_SEQ_BASE 10",
    ]
    for i, name in enumerate(ANCHOR_NAMES):
        u16 = name.encode("utf-16-le") + b"\x00\x00"
        content = (IMAGE_BASE + ANCHOR_STR_VADDR[i] if not host
                   else HOST_DATA_BASE + HOST_STR_OFF[i])
        lines.append(f'#define IGFC1_ANCHOR{i}_NAME "{name}"')
        lines.append(f'#define IGFC1_ANCHOR{i}_U16_HEX "{u16.hex()}"')
        lines.append(f"#define IGFC1_ANCHOR{i}_CONTENT_RT 0x{content:x}ull")
    for i, w in enumerate(windows):
        wv, cv = w["start_vaddr"], w["call_vaddr"]
        if host:
            wrt = HOST_EXEC_BASE + i * HOST_WINDOW_STRIDE
            crt = wrt + w["len"] - 5
        else:
            wrt, crt = IMAGE_BASE + wv, IMAGE_BASE + cv
        srt = (IMAGE_BASE + w["anchor_str_vaddr"] if not host
               else HOST_DATA_BASE + HOST_STR_OFF[w["anchor"]])
        lines.append(f"#define IGFC1_SITE{i}_WINDOW_VADDR 0x{wv:x}ull")
        lines.append(f"#define IGFC1_SITE{i}_CALL_VADDR 0x{cv:x}ull")
        lines.append(f"#define IGFC1_SITE{i}_WINDOW_RT 0x{wrt:x}ull")
        lines.append(f"#define IGFC1_SITE{i}_CALL_RT 0x{crt:x}ull")
        lines.append(f"#define IGFC1_SITE{i}_ANCHOR_RT 0x{srt:x}ull")
        lines.append(f'#define IGFC1_SITE{i}_CALL_HEX '
                     f'"{w["bytes"][-5:].hex()}"')
    return "\n".join(lines) + "\n"


def emit_fixtures(windows, target64):
    lines = [
        "// GENERATED by build_fc1_module.py — captured FC1 byte fixtures",
        "// (re-extracted from the frozen capture at build time).",
        "#ifndef IGFC1_FIXTURES_H",
        "#define IGFC1_FIXTURES_H",
        "#include <stdint.h>",
        f"#define FC1_NSITES {len(windows)}",
        "static const uint8_t fc1_fix_target64[64] = {",
    ]
    lines.append("  " + ",".join(f"0x{b:02x}" for b in target64) + "};")
    lines.append("static const uint8_t fc1_fix_window[FC1_NSITES][64] = {")
    for w in windows:
        pad = w["bytes"] + b"\x00" * (64 - len(w["bytes"]))
        lines.append("  {" + ",".join(f"0x{b:02x}" for b in pad) + "},")
    lines.append("};")
    lines.append("static const unsigned fc1_fix_window_len[FC1_NSITES] = {"
                 + ",".join(str(w["len"]) for w in windows) + "};")
    lines.append("static const int fc1_fix_window_anchor[FC1_NSITES] = {"
                 + ",".join(str(w["anchor"]) for w in windows) + "};")
    lines.append("static const uint64_t fc1_fix_window_vaddr[FC1_NSITES] = {"
                 + ",".join(f"0x{w['start_vaddr']:x}ull" for w in windows)
                 + "};")
    lines.append("static const uint64_t fc1_fix_call_vaddr[FC1_NSITES] = {"
                 + ",".join(f"0x{w['call_vaddr']:x}ull" for w in windows)
                 + "};")
    lines.append("#endif /* IGFC1_FIXTURES_H */")
    return "\n".join(lines) + "\n"


# ---- main ----------------------------------------------------------------

SDK = {}
receipt = []


def main():
    global SDK
    seed_srcs = ["fc1_pages.c", "fc1_windows.c", "fc1_probe.cpp"]
    h = hashlib.sha256()
    for f in seed_srcs:
        h.update((SRC / f).read_bytes())
    h.update(b"igf6-fc1prep")
    build_id = f"igf6-fc1prep-{h.hexdigest()[:12]}"

    target64, windows = extract(receipt)
    SDK = parse_sdk(receipt)

    gendir = HERE / "generated-target"
    gendir.mkdir(exist_ok=True)
    (gendir / "fc1_config.h").write_text(
        emit_config(build_id + "-target", target64, windows, host=False))
    hostgen = TESTS / "generated-host"
    if hostgen.exists():
        shutil.rmtree(hostgen)
    hostgen.mkdir()
    (hostgen / "fc1_config.h").write_text(
        emit_config(build_id + "-host", target64, windows, host=True))
    (hostgen / "fc1_fixtures.h").write_text(emit_fixtures(windows, target64))

    out = HERE / "host-build"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir()

    # ---- step 4: target compile check (pinned SDK toolchain) -----------
    tdir = HERE / "target-check"
    if tdir.exists():
        shutil.rmtree(tdir)
    tdir.mkdir()
    inc_t = ["-I", str(gendir), "-I", str(SRC)]
    for f, is_cpp in (("fc1_pages.c", False), ("fc1_windows.c", False),
                      ("record_sink.c", False), ("fc1_probe.cpp", True)):
        cc = CLANGXX if is_cpp else CLANG
        flags = CPPFLAGS_TARGET if is_cpp else CFLAGS_TARGET
        run([cc] + flags + inc_t + ["-c", SRC / f, "-o", tdir / (f + ".o")],
            receipt)
    receipt.append({"step": "target_compile_check",
                    "toolchain": "pinned-sdk-prospero-clang",
                    "tus": ["fc1_pages.c", "fc1_windows.c", "record_sink.c",
                            "fc1_probe.cpp"],
                    "status": "clean_-Werror"})

    # ---- step 5: host build + tests -------------------------------------
    inc_h = ["-I", str(TESTS / "host-shims"), "-I", str(hostgen),
             "-I", str(SRC)]
    objs = []
    for f, is_cpp in (("src/fc1_pages.c", False), ("src/fc1_windows.c", False),
                      ("src/record_sink.c", False),
                      ("tests/fc1_host_env.c", False),
                      ("tests/host_main.c", False),
                      ("src/fc1_probe.cpp", True)):
        cc = "clang++" if is_cpp else "clang"
        flags = CPPFLAGS_HOST if is_cpp else CFLAGS_HOST
        if is_cpp:
            flags = flags + ["-DIGFC1_HOST_TEST"]
        o = out / (Path(f).stem + ".o")
        run([cc] + flags + inc_h + ["-c", HERE / f, "-o", o], receipt)
        objs.append(o)
    hostbin = out / "host_main"
    run(["clang++", "-o", hostbin] + objs, receipt)

    tests_out = HERE / "tests/out"
    if tests_out.exists():
        shutil.rmtree(tests_out)
    tests_out.mkdir()
    run([hostbin, str(tests_out)], receipt)
    run(["python3", TESTS / "run_host_checks.py", str(tests_out)], receipt)

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf6-fc1prep-add-on-module",
        "build_id": build_id,
        "kind": "preparation_module_not_natively_executed",
        "capture": {"path": str(CAPTURE), "sha256": sha256(CAPTURE)},
        "extraction_cross_checks": ["checked-in fixtures JSON",
                                    "worker-fname-fc1-materialize JSON",
                                    "worker-expected-bytes/bytes_fname.bin"],
        "sdk_constants": SDK,
        "generated": [str(gendir / "fc1_config.h"),
                      str(hostgen / "fc1_config.h"),
                      str(hostgen / "fc1_fixtures.h")],
        "target_compile_check": "clean under pinned prospero clang -Werror",
        "host_tests": {"scenarios": 8,
                       "checker": "tests/run_host_checks.py",
                       "status": "passed"},
        "commands": receipt,
        "native_executed": False,
        "in_game_results": False,
        "notes": "PREPARATION module for igf6 / combined igf5. Packaging "
                 "happens in the next probe build (INTEGRATION.md). No "
                 "console claims are made by this delivery.",
    }
    (HERE / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    print(json.dumps({"build_id": build_id,
                      "target_check": "clean",
                      "host_tests": "8/8 scenarios passed",
                      "receipt": str(HERE / "build-receipt.json")}))


if __name__ == "__main__":
    main()
