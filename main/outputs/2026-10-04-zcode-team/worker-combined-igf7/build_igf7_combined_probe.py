#!/usr/bin/env python3
"""build_igf7_combined_probe.py — igf7- build of the COMBINED probe
(worker-combined-igf7, team 2026-10-04).

Packaging pipeline reused EXACTLY from the frozen igf6 lineage
(worker-igf5-diagnosis-rebuild/build_igf6_probe.py, itself verbatim from
worker-anchor-validation/build_anchor_probe.py: import_references-first
combine, the ALREADY-BUILT Packager.dll used IN PLACE (never rebuilt),
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself).

igf7- combined mission changes (labeled vs igf6):
  - ONE combined module: C2 alternative data-window reads (B,C,D,E,A;
    C2-ALTERNATIVE-SPEC.md sec.7) + rung-3 chunk reads (rc3_probe_chunks)
    + gated batch2 UStruct/UClass predicate (rc3b::run_ustruct_chain),
    all behind ONE shared rc3_vmmap_walk_min span table (one walk, not two).
  - NO expected bytes anywhere: the C2 comparison happens OFFLINE by the
    controller. LOUD GATE: none of the 5 candidate expected_hex strings from
    worker-c2-probe/EXPECTED-VALUES-OFFLINE.json may appear in any source,
    the generated config, the combined object, or the inner ELF.
  - NO GUA predicate replay: the igf6-required gc-fix townfall cpp is NOT
    in this link; the rung3-tested worker-5 townfall cpp is (the exact set
    the 20/20 rung3 host suite validated). No cfgH/cfgL feed exists here.
  - C2 reads are span-gated with explicit derived-exec-span intersection
    refusal (the igf6 crash class is structurally unreachable; igf4 proved
    the gate survives, igf6 proved ungated exec reads do not).
  - freestanding memcpy/memmove/memset/memcmp defined locally
    (src/support_mem.c) so the import table stays EXACTLY the 7 verbatim
    frozen-contract rows — zero new identities.
  - syscall gate INVERTED as in igf6: ZERO raw `syscall` instructions in
    the combined object and no 0F 05 byte pair in the final inner ELF .text.
  - build_id prefix igf7-; module name/path/log path unchanged.
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
SRC = HERE / "src"
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
TEAM = WHAT / "outputs/2026-10-04-zcode-team"

# Consumed sibling sources, compiled IN PLACE (read-only; shas recorded).
C2 = TEAM / "worker-c2-probe/src"
B2L = TEAM / "worker-batch2-live-prep/src"
R3 = TEAM / "worker-rung3-chunk-probe/src"
W5 = TEAM / "worker-5-layout-validator/src"
B2 = TEAM / "worker-layout-batch2/src"
EV = TEAM / "worker-c2-probe/EXPECTED-VALUES-OFFLINE.json"

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
PACKAGER_DLL = TEAM / "worker-anchor-validation/harness/bin/Release/net10.0/Packager.dll"

# Checkpoint facts (zcode_v3_MAPPING_BREAKTHROUGH + igf4/igf6 records).
EXPECTED_BASE = 0x400000
EXPECTED_EXEC_EXTENT = 0x747C000
GUA_PROFILE_VADDR = 0x93BA110          # IG_RT_GUOBJECT_ARRAY = base + this
CANDS = {  # id -> (vaddr, len)  (C2-ALTERNATIVE-SPEC.md sec.5)
    "B": (0x7826782, 8),
    "C": (0x75CFFDA, 32),
    "D": (0x7B8845D, 19),
    "E": (0x7DD2A40, 64),
    "A": (0xBC5F250, 36),
}

MODULE = "modbridge_ingame_facts"      # UNCHANGED lineage
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"

# (name, directory) — every consumed source, compiled in place.
C_SOURCES = [
    ("record_sink.c", C2), ("ingame_libc.c", C2), ("sentinels.c", C2),
    ("live_gua_reads.c", B2L), ("rung3_chunk_addon.c", R3),
    ("support_mem.c", SRC), ("module_entry.c", SRC),
]
ASM_SOURCES = [("import_references.S", C2), ("boundaries.S", C2)]
CPP_SOURCES = [
    ("townfall_layout_probes.cpp", W5),
    ("reflection_layout_predicates.cpp", B2),
    ("rung3_batch2_bridge.cpp", R3),
    ("combined_probe.cpp", SRC),
]
# Inputs whose bytes this build depends on (headers recorded, not compiled).
RECORDED_HEADERS = [
    ("record_sink.h", C2), ("ingame_libc.h", C2), ("ingame_abi.h", C2),
    ("anchor_store.h", C2), ("live_gua_reads.h", B2L),
    ("rung3_chunk_addon.h", R3), ("rung3_batch2_bridge.hpp", R3),
    ("townfall_layout_probes.hpp", W5),
    ("reflection_layout_predicates.hpp", B2),
]

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


def check_no_expected_bytes(dest: Path) -> list:
    """LOUD gate: none of the 5 C2 expected_hex strings may exist in any
    input source, the generated config, the combined object, or the inner
    ELF (no self-fulfilling probe; comparison is offline controller work)."""
    ev = json.loads(EV.read_text())
    needles = [bytes.fromhex(c["expected_hex"]) for c in ev["candidates"]]
    checked = []
    files = [d / f for f, d in C_SOURCES + ASM_SOURCES + CPP_SOURCES] + \
            [d / f for f, d in RECORDED_HEADERS] + \
            [dest / "generated" / "ingame_config.h"]
    for f in files:
        blob = f.read_bytes()
        for hx in needles:
            if hx in blob:
                sys.exit(f"LOUD BUILD FAILURE: expected-bytes leak in {f}")
        checked.append({"file": str(f), "sha256": sha256(f),
                        "expected_bytes_absent": True})
    for f in (dest / "combined-ingame.o",):
        blob = f.read_bytes()
        for hx in needles:
            if hx in blob:
                sys.exit(f"LOUD BUILD FAILURE: expected-bytes leak in {f}")
    return checked


def build_once(dest: Path, build_id: str, receipt) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = dest / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    cfg = ["// GENERATED by build_igf7_combined_probe.py — do not edit by hand.",
           "#pragma once",
           f'#define IG_BUILD_ID "{build_id}"',
           f'#define IG_LOG_PATH "{LOG_PATH}"',
           "// C2 alternative candidate windows (vaddrs; rt = base + vaddr)."]
    for cid in ("B", "C", "D", "E", "A"):
        v, l = CANDS[cid]
        cfg.append(f"#define IG_VADDR_CAND_{cid} 0x{v:x}ull")
        cfg.append(f"#define IG_LEN_CAND_{cid} {l}u")
    cfg += [
        "// Rung-3 anchor: GUA runtime address (single-load-bias base 0x400000"
        " + profile vaddr 0x93ba110; live-confirmed by the igf4 gua_read record).",
        f"#define IG_RT_GUOBJECT_ARRAY 0x{EXPECTED_BASE + GUA_PROFILE_VADDR:x}ull",
        "// Geometry cross-check constants (informational; the map decides).",
        f"#define IG_EXPECTED_BASE 0x{EXPECTED_BASE:x}ull",
        f"#define IG_EXPECTED_EXEC_EXTENT 0x{EXPECTED_EXEC_EXTENT:x}ull",
        "// NO expected bytes are defined here or anywhere in this module.",
    ]
    (gen / "ingame_config.h").write_text("\n".join(cfg) + "\n")

    inc = ["-I", str(dest), "-I", str(C2)]  # dest so "generated/..." resolves

    objs = []
    for f, d in C_SOURCES:
        o = dest / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", d / f, "-o", o], receipt)
        objs.append(o)
    for f, d in ASM_SOURCES:
        o = dest / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", d / f, "-o", o], receipt)
        objs.append(o)
    for f, d in CPP_SOURCES:
        o = dest / (f.replace(".cpp", ".o"))
        run([CLANGXX] + CPPFLAGS + inc + ["-std=c++17", "-c", d / f, "-o", o],
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

    # INVERTED syscall gate: ZERO raw `syscall` instructions (igf6 parity).
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count != 0:
        sys.exit(f"LOUD BUILD FAILURE: {syscall_count} raw syscall "
                 f"instruction(s) in combined object (igf7 must have 0)")

    # LOUD GATE: expected bytes absent from all inputs + combined object.
    leak_rows = check_no_expected_bytes(dest)

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
        "scope": ("Owned igf7 combined-probe import identities: the SAME 7 "
                  "console-proven verbatim rows the loaded-and-logged "
                  "v2/v3/igf4 lineage carried. NO mprotect of any kind — "
                  "no import, no shim, no raw syscall. Freestanding mem* "
                  "defined locally (support_mem.c); ZERO new identities."),
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
            "reason": "stay dropped from v2 isolation; igf7 adds NO new identity"},
        "contract": {"schema": 1, "consumer": "worker-combined-igf7 derived Packager",
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
               "replaces": "igf6-ad9bda1fc50a (same path, same ini line; igf6 "
                           "died on its first UNGATED exec-span read; igf7 is "
                           "the combined C2-data + rung3-chunk probe: every "
                           "foreign read is span-gated on one same-invocation "
                           "VMMAP walk, exec spans are refused explicitly)",
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
    # And no expected bytes in the inner ELF either.
    ev = json.loads(EV.read_text())
    elf_blob = bare_sdk.read_bytes()
    for c in ev["candidates"]:
        if bytes.fromhex(c["expected_hex"]) in elf_blob:
            sys.exit("LOUD BUILD FAILURE: expected-bytes leak in inner ELF")

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf7-combined-c2data-rung3",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json"},
        "consumed_inputs": [
            {"file": f, "dir": str(d), "sha256": sha256(d / f)}
            for f, d in C_SOURCES + ASM_SOURCES + CPP_SOURCES + RECORDED_HEADERS
        ],
        "predicate_provenance": {
            "townfall": "worker-5-layout-validator (the rung3 20/20-tested set)",
            "note": ("the igf6-required gc-fix townfall cpp is NOT in this "
                     "link: this probe never replays the cfgH GUA-header "
                     "predicate; batch2 rfl::probe_ustruct_chain is unchanged"),
        },
        "read_mechanism": {
            "impl": ("ONE rc3_vmmap_walk_min walk -> shared igu_span table; "
                     "igu_read()/igu_read_u64() through span->data only after "
                     "a covering READABLE-span proof; C2 candidates gated "
                     "FIRST (refusal records), exec-span intersection refused"),
            "syscall_instruction_objects": syscall_count,
            "syscall_gate": "asserted == 0 (igf6 parity)",
            "import_row_added": False},
        "expected_bytes_provenance": {
            "offline_expected_values": str(EV),
            "embedded_in_module": False,
            "absence_gate": leak_rows},
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
                 "Townfall launch via ploader (see STAGE-PLAN.md)."}
    (dest / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    return receipt_out


def main() -> None:
    dest_arg = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    # Build-id seed over every consumed source byte + the log path.
    h = hashlib.sha256()
    for f, d in sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES):
        h.update((d / f).read_bytes())
    h.update(LOG_PATH.encode())
    h.update(b"igf7-combined")
    build_id = f"igf7-{h.hexdigest()[:12]}"

    if dest_arg:
        if dest_arg.exists():
            sys.exit(f"refuse to overwrite existing build dir {dest_arg}")
        r = build_once(dest_arg, build_id, [])
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
        r = build_once(d, build_id, [])
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
    summary = {"marker": "modbridge.ingamefacts.build",
               "variant": "igf7-combined-c2data-rung3",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(HERE / f"{MODULE}.prx"),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "read_mechanism": r1["read_mechanism"],
               "predicate_provenance": r1["predicate_provenance"],
               "expected_bytes_provenance": {
                   k: v for k, v in r1["expected_bytes_provenance"].items()
                   if k != "absence_gate"},
               "consumed_inputs": r1["consumed_inputs"],
               "native_executed": False, "in_game_results": False}
    shutil.copyfile(d1 / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({k: summary[k] for k in ("build_id", "deliverable",
                                              "determinism", "run1")}))
    summary["commands"] = r1["commands"]
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
