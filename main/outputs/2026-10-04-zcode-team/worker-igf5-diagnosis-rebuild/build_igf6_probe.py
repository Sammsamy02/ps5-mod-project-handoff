#!/usr/bin/env python3
"""build_igf6_probe.py — igf6- build of the DIRECT-READ code-anchor probe
(worker-igf5-diagnosis-rebuild, team 2026-10-04).

Packaging pipeline reused EXACTLY from the frozen lineage
(worker-anchor-validation/build_anchor_probe.py as reused verbatim by
worker-code-anchor-reads/build_code_anchor_probe.py: import_references-first
combine, the ALREADY-BUILT Packager.dll used IN PLACE (never rebuilt),
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself). Labeled
changes vs the igf5 build:

  - mission: DIRECT reads of the two code anchors (plain 16-byte
    dereference). NO mprotect, NO raw syscall, NO permission gate — per
    the Part 1 binary diagnosis: igf5 was loader-structurally identical to
    igf4 and its ONLY execution-relevant delta was the inline mprotect
    syscall shim (2 raw `syscall` sites, rax=74), correlated with the
    zero-record + console/game crash event;
  - syscall gate INVERTED: the combined object must contain ZERO raw
    `syscall` instructions, and the final inner ELF .text must contain NO
    0F 05 byte pair (defense in depth; igf5 asserted >=1);
  - no build-time SDK syscall/PROT constant parsing (nothing to parse —
    the mechanism is gone);
  - GUA predicate: the worker-predicate-gc-fix CORRECTED predicate is
    REQUIRED (loud failure if missing), compiled in, replayed cfgH-style
    over a STACK copy of 96 direct-read bytes (predicate does zero
    live-memory derefs of its own; cfgL withheld in this minimal rebuild);
  - vendored byte-identity is LOUD-verified against the igf4 lineage
    (11 shared files), the igf5 build (anchor_pages), and the gcfix
    bundle (fixed predicate cpp, pinned sha);
  - expected bytes are read from the igf4 evidence file AND cross-checked
    against the INDEPENDENT worker-expected-bytes bundle (unchanged);
  - import contract = the SAME 7 VERBATIM frozen-contract rows (unchanged);
  - build_id prefix igf6-; module name/path/log path unchanged (same-file
    replacement of igf5-, no ini change);
  - no-arg mode: determinism double-build (run1 + run2), byte-equality
    comparison, final PRX copied to the worker directory.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
SRC = HERE / "src"
TEAM = WHAT / "outputs/2026-10-04-zcode-team"
AV = TEAM / "worker-anchor-validation"      # igf4 lineage (read-only)
CA = TEAM / "worker-code-anchor-reads"      # igf5 build (read-only)
EB = TEAM / "worker-expected-bytes"         # independent evidence
GCFIX = TEAM / "worker-predicate-gc-fix" / "vendored-fixed" / "src"

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

# Controller checkpoint facts (zcode_v3_MAPPING_BREAKTHROUGH + igf4 VMMAP).
EXEC_SPAN_START = 0x400000
EXEC_SPAN_END = 0x787C000
PAGE_SIZE = 0x4000  # 16 KiB (SDK PAGE_SHIFT 14; informational here)

# The CORRECTED predicate (worker-predicate-gc-fix vendored-fixed), pinned.
GCFIX_CPP_SHA = "bae7cd9a669d20be37547136f309ea8e8ae91b3b569e6b028249dc3b15ede649"

MODULE = "modbridge_ingame_facts"  # UNCHANGED: same-file replacement lineage
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "vmmap_walk.c", "mapscan.c",
             "anchor_pages.c", "sentinels.c", "module_entry.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = ["code_anchor_direct.cpp", "townfall_layout_probes.cpp"]

# Files that MUST be byte-identical to the igf4 lineage (and igf5 build,
# which LOUD-verified the same identity vs igf4).
VENDORED_VS_IGF4 = ["record_sink.c", "record_sink.h", "ingame_libc.c",
                    "ingame_libc.h", "vmmap_walk.c", "mapscan.c",
                    "anchor_store.h", "ingame_abi.h", "import_references.S",
                    "boundaries.S", "sentinels.c",
                    "townfall_layout_probes.hpp"]
# Files that MUST be byte-identical to the igf5 build (pure helpers).
VENDORED_VS_IGF5 = ["anchor_pages.c", "anchor_pages.h"]

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
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect",
    "igcar_sys_mprotect",  # the REMOVED igf5 shim — must never return
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


def check_vendored() -> list:
    rows = []
    for f in VENDORED_VS_IGF4:
        a, b = AV / "src" / f, SRC / f
        sa, sb = sha256(a), sha256(b)
        if sa != sb:
            sys.exit(f"LOUD FAILURE: vendored {f} diverged from igf4 lineage")
        rows.append({"file": f, "sha256": sb, "identical_to": str(a)})
    for f in VENDORED_VS_IGF5:
        a, b = CA / "src" / f, SRC / f
        sa, sb = sha256(a), sha256(b)
        if sa != sb:
            sys.exit(f"LOUD FAILURE: vendored {f} diverged from igf5 build")
        rows.append({"file": f, "sha256": sb, "identical_to": str(a)})
    g = GCFIX / "townfall_layout_probes.cpp"
    s = SRC / "townfall_layout_probes.cpp"
    if sha256(g) != GCFIX_CPP_SHA:
        sys.exit(f"LOUD FAILURE: gcfix cpp sha drifted: {sha256(g)}")
    if sha256(g) != sha256(s):
        sys.exit("LOUD FAILURE: src predicate cpp != gcfix vendored-fixed")
    rows.append({"file": "townfall_layout_probes.cpp", "sha256": sha256(s),
                 "identical_to": str(g), "pinned": GCFIX_CPP_SHA})
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


def text_has_syscall_pair(elf: Path) -> bool:
    """True iff the .text section of the ELF contains the 0F 05 byte pair."""
    b = elf.read_bytes()
    shoff = int.from_bytes(b[40:48], "little")
    shnum = int.from_bytes(b[60:62], "little")
    shstr = int.from_bytes(b[62:64], "little")
    hdr = lambda i: shoff + 64 * i
    sec = lambda i: (int.from_bytes(b[hdr(i) + 24:hdr(i) + 32], "little"),
                     int.from_bytes(b[hdr(i) + 32:hdr(i) + 40], "little"))
    stroff, strsz = sec(shstr)
    for i in range(shnum):
        nameoff = int.from_bytes(b[hdr(i):hdr(i) + 4], "little")
        name = b[stroff + nameoff:stroff + nameoff + 8].split(b"\0")[0]
        if name == b".text":
            off, size = sec(i)
            return b"\x0f\x05" in b[off:off + size]
    sys.exit("LOUD FAILURE: .text section not found in inner ELF")


def build_once(dest: Path, build_id: str, ev: dict, receipt) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = dest / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    (gen / "ingame_config.h").write_text(
        "// GENERATED by build_igf6_probe.py — do not edit by hand.\n"
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
        "// Controller checkpoint VMMAP facts. INFORMATIONAL in igf6: the\n"
        "// direct reads are NOT gated on this (NX gates execute, not read;\n"
        "// prot=0x4 is kernel policy, not hardware read enforcement).\n"
        f"#define IG_EXEC_SPAN_START 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_EXEC_SPAN_END 0x{EXEC_SPAN_END:x}ull\n"
        f"#define IG_PAGE_SIZE 0x{PAGE_SIZE:x}ull\n"
        "// Expected prologue bytes (retained image; cross-worker confirmed).\n"
        f"#define IG_TICK_FILE_OFF 0x{VADDR_TICK + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_SCO_FILE_OFF 0x{VADDR_SCO + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_TICK_EXPECTED_HEX \"{ev['tick']['expected16_hex']}\"\n"
        f"#define IG_SCO_EXPECTED_HEX \"{ev['sco']['expected16_hex']}\"\n"
        "#define IG_IMAGE_SHA16 \"3b6cfad47dd17fea\"\n"
        "// igf6: corrected predicate REQUIRED (gc-fix vendored-fixed).\n"
        "#define IG_PRED_GC_FIX_PRESENT 1\n")

    inc = ["-I", str(dest), "-I", str(SRC)]  # dest so "generated/..." resolves

    objs = []
    for f in C_SOURCES:
        o = dest / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)
    for f in ASM_SOURCES:
        o = dest / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)
    for f in CPP_SOURCES:
        o = dest / (f.replace(".cpp", ".o"))
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

    # INVERTED syscall gate: ZERO raw `syscall` instructions (igf5 asserted
    # >=1 for its mprotect shim; that mechanism is REMOVED in igf6).
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count != 0:
        sys.exit(f"LOUD BUILD FAILURE: {syscall_count} raw syscall "
                 f"instruction(s) in combined object (igf6 must have 0)")

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
        "scope": ("Owned igf6 direct-read probe import identities: the "
                  "SAME 7 console-proven verbatim rows the loaded-and-logged "
                  "v2/v3/igf4 lineage carried. NO mprotect of any kind — "
                  "no import, no shim, no raw syscall."),
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
            "reason": "stay dropped from v2 isolation; igf6 adds NO new identity"},
        "contract": {"schema": 1, "consumer": "worker-igf5-diagnosis-rebuild derived Packager",
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
               "replaces": "igf5-4186048a6dd6 (same path, same ini line; igf5 "
                           "produced zero records + crashes; igf6 rebuilds the "
                           "anchor reads as plain direct dereferences with no "
                           "syscall of any kind)",
               "entry_marker_first": True,
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (dest / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = dest / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, dest / "profile.json", bare_sdk], receipt)
    final = dest / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipt)

    # Defense in depth: the FINAL inner ELF .text must have no 0F 05 pair.
    if text_has_syscall_pair(bare_sdk):
        sys.exit("LOUD BUILD FAILURE: 0F 05 byte pair present in inner ELF .text")

    run(["python3", HERE / "check_igf6_artifact.py", dest], receipt)

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf6-direct-read",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json"},
        "vendored_byte_identity": receipt_vendored,
        "read_mechanism": {
            "impl": "plain direct dereference (16-byte loop), NO mprotect, "
                    "NO raw syscall, NO permission gate, NO handler",
            "justification": ["known code addresses inside mapped exec span",
                              "x86-64 NX gates execute, not read",
                              "VMMAP prot=0x4 is kernel policy, not hardware",
                              "igf4 gate refused on policy; igf5 mprotect shim "
                              "is the sole execution-relevant delta correlated "
                              "with zero-record + crash and is removed"],
            "syscall_instruction_objects": syscall_count,
            "syscall_gate": "asserted == 0 (inverted from igf5 >=1)",
            "import_row_added": False},
        "predicate": {
            "source": "worker-predicate-gc-fix vendored-fixed",
            "cpp_sha256": GCFIX_CPP_SHA,
            "cfgL": "not_run (minimal rebuild; cfgH stack-copy feed only)"},
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
                 "Townfall launch via ploader (see STAGE-PLAN-IGF6.md)."}
    (dest / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    return receipt_out


receipt_vendored: list = []


def main() -> None:
    global receipt_vendored
    dest_arg = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    receipt_vendored = check_vendored()
    ev = load_expected_bytes()

    seed_srcs = sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
    h = hashlib.sha256()
    for f in seed_srcs:
        h.update((SRC / f).read_bytes())
    h.update(LOG_PATH.encode())
    h.update(b"igf6")
    seed = h.hexdigest()[:12]
    build_id = f"igf6-{seed}"

    if dest_arg:
        if dest_arg.exists():
            sys.exit(f"refuse to overwrite existing build dir {dest_arg}")
        r = build_once(dest_arg, build_id, ev, [])
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
        r = build_once(d, build_id, ev, [])
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
    summary = {"marker": "modbridge.ingamefacts.build", "variant": "igf6-direct-read",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(HERE / f"{MODULE}.prx"),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "read_mechanism": r1["read_mechanism"],
               "predicate": r1["predicate"],
               "expected_bytes_provenance": r1["expected_bytes_provenance"],
               "vendored_byte_identity": receipt_vendored,
               "native_executed": False, "in_game_results": False}
    shutil.copyfile(d1 / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({k: summary[k] for k in ("build_id", "deliverable",
                                              "determinism", "run1")}))
    summary["commands"] = r1["commands"]
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
