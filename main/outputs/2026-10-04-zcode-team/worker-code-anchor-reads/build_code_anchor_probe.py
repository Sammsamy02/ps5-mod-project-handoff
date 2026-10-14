#!/usr/bin/env python3
"""build_code_anchor_probe.py — igf5- build of the CODE-ANCHOR-READS PRX
(worker-code-anchor-reads, team 2026-10-04).

Packaging pipeline reused EXACTLY from the frozen lineage
(worker-anchor-validation/build_anchor_probe.py: import_references-first
combine, the ALREADY-BUILT Packager.dll used IN PLACE (never rebuilt),
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself). Labeled
changes:

  - mission: the mprotect-FIRST reads of the two code anchors the igf4
    safety gate had refused (controller correction: mprotect -> read ->
    restore -> compare; NO raw read of an exec-only page);
  - mprotect mechanism: inline x86-64 syscall shim whose syscall number
    and PROT_* constants are parsed at BUILD TIME from the PINNED SDK's
    own headers (sys/syscall.h SYS_mprotect, sys/mman.h PROT_READ/EXEC,
    machine/param.h PAGE_SHIFT 14) and stamped into the generated config
    — nothing hardcoded, provenance in the receipt;
  - source set: focused probe (record_sink/ingame_libc/vmmap_walk/
    mapscan/anchor_pages/module_entry + import_references.S +
    code_anchor_probe.cpp); NO predicate TU unless worker-predicate-gc-fix
    has delivered its corrected townfall_layout_probes.cpp (hook: copied
    into src/generated-gcfix/ and compiled with IG_PRED_GC_FIX_PRESENT);
  - vendored byte-identity is LOUD-verified against
    worker-anchor-validation/src before anything compiles;
  - expected bytes are read from the igf4 evidence file AND cross-checked
    against the INDEPENDENT worker-expected-bytes bundle (two workers must
    agree, loud failure otherwise);
  - import contract = the SAME 7 VERBATIM frozen-contract rows (unchanged);
  - build_id prefix igf5-; module name/path/log path unchanged (same-file
    replacement of igf4-, no ini change);
  - no-arg mode: determinism double-build (run1 + run2), byte-equality
    comparison, final PRX copied to the worker directory.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
SRC = HERE / "src"
TEAM = WHAT / "outputs/2026-10-04-zcode-team"
AV = TEAM / "worker-anchor-validation"          # igf4 lineage (read-only)
EB = TEAM / "worker-expected-bytes"             # independent evidence
GCFIX = TEAM / "worker-predicate-gc-fix"        # not delivered yet

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
CLANGXX = SDK_BIN / "prospero-clang++"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
OBJDUMP = LLVM / "llvm-objdump"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json"
PACKAGER_DLL = AV / "harness/bin/Release/net10.0/Packager.dll"  # used IN PLACE

# Retained-image provenance (igf4 evidence; cross-checked vs worker-expected-bytes).
EVIDENCE = AV / "expected_bytes_tick_sco.txt"
CROSS_EVIDENCE = EB / "EXPECTED-BYTES.json"
IMAGE_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
LOAD0_OFF_DELTA = 0x4000
VADDR_TICK = 0x2FAC8C0
VADDR_SCO = 0x490AA0

# Pinned SDK headers (constants parsed at build time; NEVER hardcoded here).
SDK_INC = WHAT / ("outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/"
                  "ps5-payload-sdk/target/include")
SYSCALL_H = SDK_INC / "sys/syscall.h"
MMAN_H = SDK_INC / "sys/mman.h"
PARAM_H = SDK_INC / "machine/param.h"

# Controller checkpoint facts (zcode_v3_MAPPING_BREAKTHROUGH + igf4 VMMAP).
EXEC_SPAN_START = 0x400000
EXEC_SPAN_END = 0x787C000
EXEC_PROT_FALLBACK = 0x4   # VMMAP-reported execute-only; == PROT_EXEC
PAGE_SIZE = 0x4000         # 16 KiB (controller directive; SDK PAGE_SHIFT 14)

MODULE = "modbridge_ingame_facts"  # UNCHANGED: same-file replacement lineage
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "vmmap_walk.c", "mapscan.c",
             "anchor_pages.c", "sentinels.c", "module_entry.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = ["code_anchor_probe.cpp"]

# Vendored files MUST be byte-identical to the igf4 lineage (loud check).
VENDORED = ["record_sink.c", "record_sink.h", "ingame_libc.c", "ingame_libc.h",
            "vmmap_walk.c", "mapscan.c", "anchor_store.h", "ingame_abi.h",
            "import_references.S", "boundaries.S", "sentinels.c"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]
CPPFLAGS = CFLAGS + ["-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics"]

STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]

FORBIDDEN_SYMBOLS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect",  # shim, NOT an import
]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(argv, receipt, cwd=None, allow_fail=False):
    env = dict(os.environ)
    env["LLVM_CONFIG"] = str(LLVM / "llvm-config")
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                       cwd=str(cwd) if cwd else None, env=env)
    receipt.append({"argv": [str(a) for a in argv], "rc": r.returncode,
                    "stdout": r.stdout[-4000:], "stderr": r.stderr[-4000:]})
    if r.returncode != 0 and not allow_fail:
        print(json.dumps(receipt[-1], indent=1))
        sys.exit(f"FAILED: {argv}")
    return r


def parse_sdk_constants() -> dict:
    """Parse the pinned SDK headers; loud failure if anything is missing."""
    out = {}
    txt = SYSCALL_H.read_text()
    m = re.search(r"#define\s+SYS_mprotect\s+(\d+)", txt)
    if not m:
        sys.exit(f"LOUD FAILURE: SYS_mprotect not found in {SYSCALL_H}")
    out["sys_mprotect"] = int(m.group(1))
    out["syscall_h"] = str(SYSCALL_H)
    out["syscall_h_line"] = txt[:m.start()].count("\n") + 1
    txt = MMAN_H.read_text()
    prots = {}
    for name in ("PROT_NONE", "PROT_READ", "PROT_WRITE", "PROT_EXEC"):
        m = re.search(rf"#define\s+{name}\s+(0x[0-9a-fA-F]+|\d+)", txt)
        if not m:
            sys.exit(f"LOUD FAILURE: {name} not found in {MMAN_H}")
        prots[name] = int(m.group(1), 0)
    out["prots"] = prots
    out["mman_h"] = str(MMAN_H)
    if prots["PROT_READ"] != 1 or prots["PROT_EXEC"] != 4:
        sys.exit(f"LOUD FAILURE: unexpected PROT numbering: {prots}")
    txt = PARAM_H.read_text()
    shifts = {int(x) for x in re.findall(r"#define\s+PAGE_SHIFT\s+(\d+)", txt)}
    out["page_shift_candidates"] = sorted(shifts)
    out["param_h"] = str(PARAM_H)
    if 14 not in shifts:
        sys.exit(f"LOUD FAILURE: PAGE_SHIFT 14 (16 KiB) not among {sorted(shifts)}")
    return out


def check_vendored() -> list:
    rows = []
    for f in VENDORED:
        a, b = AV / "src" / f, SRC / f
        if not b.exists():
            sys.exit(f"LOUD FAILURE: missing vendored source {b}")
        sa, sb = sha256(a), sha256(b)
        if sa != sb:
            sys.exit(f"LOUD FAILURE: vendored {f} diverged from igf4 lineage")
        rows.append({"file": f, "sha256": sa,
                     "identical_to": str(a)})
    return rows


def load_expected_bytes() -> dict:
    ev = json.loads(EVIDENCE.read_text())
    if ev["image_sha256"] != IMAGE_SHA:
        sys.exit(f"LOUD FAILURE: retained-image sha drifted: {ev['image_sha256']}")
    cross = json.loads(CROSS_EVIDENCE.read_text())
    anchors = {a["symbol"]: a for a in cross["anchors"]}
    out = {}
    for key, vaddr, sym in (("tick", VADDR_TICK, "UGameEngine::Tick"),
                            ("sco", VADDR_SCO, "StaticConstructObject")):
        entry = ev[key]
        want_off = vaddr + LOAD0_OFF_DELTA
        if int(entry["file_offset"], 16) != want_off:
            sys.exit(f"LOUD FAILURE: {key} file_offset != vaddr+0x4000")
        if len(entry["expected16_hex"]) != 32:
            sys.exit(f"LOUD FAILURE: {key} expected16_hex not 16 bytes")
        ind = anchors[sym]["bytes_hex"]
        if ind[:32] != entry["expected16_hex"]:
            sys.exit(f"LOUD FAILURE: worker-expected-bytes {sym} disagrees "
                     f"({ind[:32]} != {entry['expected16_hex']})")
        out[key] = {"expected16_hex": entry["expected16_hex"],
                    "cross_worker_confirmed": True}
    return out


def prepare_gcfix(receipt) -> dict:
    """Hook: use worker-predicate-gc-fix's corrected predicate when present."""
    info = {"present": False, "source": None}
    if not GCFIX.exists():
        return info
    candidates = sorted(p for p in GCFIX.rglob("townfall_layout_probes.cpp")
                        if p.is_file())
    if not candidates:
        return info
    fixed = candidates[0]
    gdir = SRC / "generated-gcfix"
    gdir.mkdir(exist_ok=True)
    shutil.copyfile(fixed, gdir / "townfall_layout_probes.cpp")
    # Prefer the hpp VENDORED BESIDE the fixed cpp (same worker bundle);
    # fall back to the igf4 lineage hpp.
    hpp = fixed.parent / "townfall_layout_probes.hpp"
    if not hpp.exists():
        hpp = AV / "src" / "townfall_layout_probes.hpp"
    shutil.copyfile(hpp, gdir / "townfall_layout_probes.hpp")
    info = {"present": True, "source": str(fixed),
            "source_sha256": sha256(fixed),
            "hpp": str(hpp), "hpp_sha256": sha256(hpp)}
    return info


def build_once(dest: Path, build_id: str, ev: dict, sdk: dict, gcfix: dict,
               receipt) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    (gen / "ingame_config.h").write_text(
        "// GENERATED by build_code_anchor_probe.py — do not edit by hand.\n"
        "#pragma once\n"
        f"#define IG_BUILD_ID \"{build_id}\"\n"
        f"#define IG_LOG_PATH \"{LOG_PATH}\"\n"
        "// Profile vaddrs (worker-2 facts) consumed as ARGUMENTS.\n"
        f"#define IG_VADDR_GUOBJECT_ARRAY 0x93ba110ull\n"
        f"#define IG_VADDR_UGAMEENGINE_TICK 0x{VADDR_TICK:x}ull\n"
        f"#define IG_VADDR_STATIC_CONSTRUCT_OBJECT 0x{VADDR_SCO:x}ull\n"
        "// Checkpoint zcode_v3_MAPPING_BREAKTHROUGH single-load-bias facts:\n"
        "// base=0x400000, runtime = base + profile_vaddr, all segments.\n"
        f"#define IG_IMAGE_BASE 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_RT_UGAMEENGINE_TICK 0x{EXEC_SPAN_START + VADDR_TICK:x}ull\n"
        f"#define IG_RT_STATIC_CONSTRUCT_OBJECT 0x{EXEC_SPAN_START + VADDR_SCO:x}ull\n"
        f"#define IG_RT_GUOBJECT_ARRAY 0x{EXEC_SPAN_START + 0x93ba110:x}ull\n"
        "// Controller checkpoint VMMAP facts: the exec span is execute-only\n"
        "// (prot=0x4); the mprotect-first reads TARGET pages inside it.\n"
        f"#define IG_EXEC_SPAN_START 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_EXEC_SPAN_END 0x{EXEC_SPAN_END:x}ull\n"
        f"#define IG_EXEC_PROT_FALLBACK 0x{EXEC_PROT_FALLBACK:x}\n"
        "// mprotect mechanism constants — PARSED from the PINNED SDK headers\n"
        "// at build time (see build receipt for file:line provenance).\n"
        f"#define IGCAR_SYS_MPROTECT {sdk['sys_mprotect']}\n"
        f"#define IGCAR_PROT_READ 0x{sdk['prots']['PROT_READ']:x}\n"
        f"#define IGCAR_PROT_EXEC 0x{sdk['prots']['PROT_EXEC']:x}\n"
        f"#define IG_PAGE_SIZE 0x{PAGE_SIZE:x}ull\n"
        "// Expected prologue bytes (retained image; cross-worker confirmed).\n"
        f"#define IG_TICK_FILE_OFF 0x{VADDR_TICK + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_SCO_FILE_OFF 0x{VADDR_SCO + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_TICK_EXPECTED_HEX \"{ev['tick']['expected16_hex']}\"\n"
        f"#define IG_SCO_EXPECTED_HEX \"{ev['sco']['expected16_hex']}\"\n"
        "#define IG_IMAGE_SHA16 \"3b6cfad47dd17fea\"\n"
        f"#define IG_PRED_GC_FIX_PRESENT {1 if gcfix['present'] else 0}\n")

    inc = ["-I", str(gen), "-I", str(SRC)]
    if gcfix["present"]:
        inc = ["-I", str(SRC / "generated-gcfix"), "-I", str(gen), "-I", str(SRC)]

    objs = []
    for f in C_SOURCES:
        o = dest / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)
    for f in ASM_SOURCES:
        o = dest / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)
    cpp = list(CPP_SOURCES)
    if gcfix["present"]:
        cpp.append("generated-gcfix/townfall_layout_probes.cpp")
    for f in cpp:
        o = dest / (f.split("/")[-1].replace(".cpp", ".o"))
        run([CLANGXX] + CPPFLAGS + inc + ["-std=c++17", "-c", SRC / f, "-o", o],
            receipt)
        objs.append(o)

    combined = dest / "combined-ingame.o"
    ref = dest / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipt)

    # LOUD GATE: forbidden symbols must not appear anywhere in the link.
    nm = run([NM, combined], receipt)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    bad = sorted(seen & set(FORBIDDEN_SYMBOLS))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: forbidden symbol in combined object: {bad}")

    # Static shim evidence: the combined object must contain the raw syscall
    # instruction (the mprotect shim) — recorded, and asserted >= 1.
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count < 1:
        sys.exit("LOUD BUILD FAILURE: no syscall instruction in combined "
                 "object (mprotect shim missing?)")

    # Import contract: the SAME 7 verbatim rows, byte-identical.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        provenance[n] = "verbatim:frozen-191-contract (row copied byte-identical)"
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned igf5 code-anchor-reads probe import identities: the "
                  "SAME 7 console-proven verbatim rows the loaded-and-logged "
                  "v2/v3/igf4 lineage carried. mprotect is an inline syscall "
                  "shim, NOT an import — no new identity is introduced."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen",
                               "dlclose", "sceKernelMprotect"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": "stay dropped from v2 isolation; mprotect is a shim"},
        "contract": {"schema": 1, "consumer": "worker-code-anchor-reads derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = dest / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(rows[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged from frozen")

    # Package: the ALREADY-BUILT Packager.dll used IN PLACE (read-only).
    if not PACKAGER_DLL.exists():
        sys.exit(f"LOUD FAILURE: Packager.dll missing at {PACKAGER_DLL}")
    bare = dest / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, PACKAGER_DLL, combined, cpath, csha, bare], receipt)

    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v4 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "igf4-4891869337b4 (same path, same ini line; igf4 "
                           "safely refused the exec-only anchor reads; this "
                           "probe adds the mprotect-first reads)",
               "entry_marker_first": True,
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (dest / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = dest / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, dest / "profile.json", bare_sdk], receipt)
    final = dest / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipt)
    run(["python3", HERE / "check_code_anchor_artifact.py", dest], receipt)

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf5-code-anchor-reads",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json"},
        "vendored_byte_identity": receipt_vendored,
        "mprotect_mechanism": {
            "impl": "inline x86-64 syscall shim (NO import identity)",
            "sys_mprotect": sdk["sys_mprotect"],
            "syscall_header": f"{sdk['syscall_h']}:{sdk['syscall_h_line']}",
            "prots": sdk["prots"], "mman_header": sdk["mman_h"],
            "page_shift_candidates": sdk["page_shift_candidates"],
            "page_size": PAGE_SIZE,
            "request_prot": "PROT_READ|PROT_EXEC (add read, keep exec)",
            "restore_prot": "current VMMAP covering span; checkpoint 0x4 fallback",
            "syscall_instruction_objects": syscall_count,
            "import_row_added": False,
            "residual_risk": "kernel syscall number not verifiable on-host; "
                             "controller sign-off required before staging"},
        "expected_bytes_provenance": {
            "evidence": str(EVIDENCE), "image_sha256": IMAGE_SHA,
            "cross_worker": str(CROSS_EVIDENCE),
            "tick": {"vaddr": hex(VADDR_TICK),
                     "file_off": hex(VADDR_TICK + LOAD0_OFF_DELTA),
                     "hex": ev["tick"]["expected16_hex"],
                     "cross_worker_confirmed": ev["tick"]["cross_worker_confirmed"]},
            "sco": {"vaddr": hex(VADDR_SCO),
                    "file_off": hex(VADDR_SCO + LOAD0_OFF_DELTA),
                    "hex": ev["sco"]["expected16_hex"],
                    "cross_worker_confirmed": ev["sco"]["cross_worker_confirmed"]}},
        "predicate_gcfix_hook": gcfix,
        "commands": receipt,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in STRONG_VERBATIM},
        "weak_imports": [],
        "forbidden_symbol_gate": {"names": FORBIDDEN_SYMBOLS, "status": "passed"},
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "inner_elf_sha256": sha256(bare_sdk),
        "inner_elf_bytes": bare_sdk.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. In-game facts require the user-coordinated "
                 "Townfall launch via ploader (see STAGE-PLAN-CODE-ANCHOR.md)."}
    (dest / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    return receipt_out


receipt_vendored: list = []


def main() -> None:
    global receipt_vendored
    dest_arg = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    sdk = parse_sdk_constants()
    receipt_vendored = check_vendored()
    ev = load_expected_bytes()
    gcfix_probe = {"present": False, "source": None}
    # Probe for gcfix WITHOUT copying when doing determinism runs (copy must
    # be identical for both runs; detect once, then both runs see the same).
    if GCFIX.exists():
        cands = sorted(p for p in GCFIX.rglob("townfall_layout_probes.cpp")
                       if p.is_file())
        if cands:
            gcfix_probe = {"present": True, "source": str(cands[0]),
                           "source_sha256": sha256(cands[0]),
                           "hpp": str(AV / "src" / "townfall_layout_probes.hpp"),
                           "hpp_sha256": sha256(AV / "src" / "townfall_layout_probes.hpp")}

    seed_srcs = sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
    h = hashlib.sha256()
    for f in seed_srcs:
        h.update((SRC / f).read_bytes())
    h.update(LOG_PATH.encode())
    h.update(b"igf5")
    h.update(b"gcfix" if gcfix_probe["present"] else b"nogcfix")
    seed = h.hexdigest()[:12]
    build_id = f"igf5-{seed}"

    if dest_arg:
        if dest_arg.exists():
            sys.exit(f"refuse to overwrite existing build dir {dest_arg}")
        gcfix = prepare_gcfix(dict()) if gcfix_probe["present"] else gcfix_probe
        r = build_once(dest_arg, build_id, ev, sdk, gcfix, [])
        print(json.dumps({k: r[k] for k in ("build_id", "variant", "final_prx",
                                            "final_sha256", "final_bytes")}))
        return

    # Determinism double-build.
    runs = []
    for name in ("run1", "run2"):
        d = HERE / "determinism" / name
        if d.exists():
            sys.exit(f"refuse to overwrite {d}")
    for name in ("run1", "run2"):
        d = HERE / "determinism" / name
        gcfix = prepare_gcfix(dict()) if gcfix_probe["present"] else gcfix_probe
        r = build_once(d, build_id, ev, sdk, gcfix, [])
        runs.append((d, r))
        print(f"{name}: build_id={r['build_id']} prx={r['final_sha256']}")
    (d1, r1), (d2, r2) = runs
    compare = {}
    for key, rel in (("final_prx", f"{MODULE}.prx"),
                     ("inner_elf", f"bare-sdk/{MODULE}.prx"),
                     ("combined", "combined-ingame.o"),
                     ("contract", "import-contract.json")):
        a, b = sha256(d1 / rel), sha256(d2 / rel)
        compare[key] = {"run1": a, "run2": b, "equal": a == b}
        if a != b:
            sys.exit(f"LOUD FAILURE: determinism mismatch on {key}: {compare[key]}")
    summary = {"marker": "modbridge.ingamefacts.build", "variant": "igf5-code-anchor-reads",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(HERE / f"{MODULE}.prx"),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "mprotect_mechanism": r1["mprotect_mechanism"],
               "expected_bytes_provenance": r1["expected_bytes_provenance"],
               "predicate_gcfix_hook": r1["predicate_gcfix_hook"],
               "vendored_byte_identity": receipt_vendored,
               "native_executed": False, "in_game_results": False}
    shutil.copyfile(d1 / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({k: summary[k] for k in ("build_id", "deliverable",
                                              "determinism", "run1")}))


if __name__ == "__main__":
    main()
