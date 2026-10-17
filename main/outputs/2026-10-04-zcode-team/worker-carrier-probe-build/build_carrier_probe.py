#!/usr/bin/env python3
"""build_carrier_probe.py — cc1 build of the carrier-name census ridealong
probe (worker-carrier-probe-build, team 2026-10-04).

Packaging pipeline reused EXACTLY from the frozen lineage
(worker-fc1-validator-probe/build_fc1pool_probe.py as reused verbatim from
worker-record-sink-addon/build_qdrv_probe.py: import_references-first
combine, the ALREADY-BUILT Packager.dll used IN PLACE (never rebuilt),
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself). Labeled
changes vs the fc1pool1 build:

  - mission: the cc1 CARRIER-NAME CENSUS ridealong
    (worker-r3-carrier-prep/CARRIER-CANDIDATES.md sec 5-6): a data-side-only
    standalone prx that censuses the LIVE FName pool for the fixed
    53-name candidate table (3 ranked carrier candidates + 19 walk
    class-strings + the AActor/UAnimInstance ctor-family decoder controls +
    "None"/"ByteProperty" positive controls + "PostRender" /
    "ZZZNotARealFunctionQQ" negative controls; every name image-confirmed
    with a linked-label vaddr), under qdrv1's GUA admission gates (header
    rcodes 1-7 + the 10000-count layout gate) and fc1's VALIDATED pool
    decoder. Runs in EVERY launch window including T-A. NO hooks, NO
    engine writes, NO mprotect, NO raw syscall, NO mutex imports.
  - module name / build_id prefix: modbridge_carrier / cc1- (own loader
    identity; the fc1pool1, qdrv1, igf6, observer and composition modules
    are untouched).
  - own single-writer channel: /data/modbridge-carrier-facts.jsonl
    (CANDIDATES sec 5/6.2: NOT qdrv1's file).
  - imports: TEN verbatim frozen-contract rows — the igf 7 PLUS nanosleep
    (fc1pool1 precedent) PLUS pthread_create/pthread_detach (the qdrv1/
    observer-lineage precedent; ONE detached census thread, 30 polls x 4 s;
    qdrv1's pthread_self is DROPPED — cc1 publishes no thread-identity
    fold). Every row byte-identical from the SAME frozen 191-row contract.
    No derived identity, nothing else new.
  - vendored identity is LOUD-verified per file: BYTE-IDENTICAL files vs
    the fc1pool1 build (which itself verified vs the qdrv1/igf5 lineage),
    LABELED-CHANGE files measured + pinned (import_references.S,
    module_entry.c), NEW files recorded (carrier_driver.c/.h,
    carrier_libc.c).
  - the Packager module-name patch is length-preserving:
    "modbridge_ingame_facts" (22) -> "modbridge_carrier" (17) + 5 NULs,
    BEFORE finalize/make_fself so all downstream hashes cover the patched
    bytes; check_carrier_artifact.py re-verifies the final container.
  - syscall gate stays == 0; forbidden-symbol gate unchanged; determinism
    double-build (run1 + run2), byte-equality comparison, final PRX copied
    to the worker directory.
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
FC1 = TEAM / "worker-fc1-validator-probe"           # vendored lineage (read-only)

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
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
PACKAGER_DLL = TEAM / "worker-anchor-validation/harness/bin/Release/net10.0/Packager.dll"  # used IN PLACE

# Controller checkpoint facts (zcode_v3_MAPPING_BREAKTHROUGH single-load bias)
# + worker-fname-ctor-audit FNAME-VERDICT.md sec 4-5 (pool chain) — the SAME
# pinned labels fc1pool1 validated; cc1 adds no address of its own.
EXEC_SPAN_START = 0x400000
EXEC_SPAN_END = 0x787C000
PAGE_SIZE = 0x4000
VADDR_GUOBJECT_ARRAY = 0x93BA110  # worker-2 profile; live-validated (igf4)
VADDR_FNAME_POOL = 0x9365EC0      # ctor-audit: LEA target in the interning core
VADDR_INIT_FLAG = 0x9365EA0       # ctor-audit: pool init guard byte
RT_FNAME_POOL = EXEC_SPAN_START + VADDR_FNAME_POOL  # 0x9765ec0
RT_GUOBJECT_ARRAY = EXEC_SPAN_START + VADDR_GUOBJECT_ARRAY  # 0x97ba110

MODULE = "modbridge_carrier"       # NEW identity: late-load add-on, own file
LOG_PATH = "/data/modbridge-carrier-facts.jsonl"  # OWN channel (single writer)
UE4SS_LOG_PATH = "/data/plugins/UE4SS.log"        # ONE line per launch

C_SOURCES = ["record_sink.c", "ingame_libc.c", "vmmap_walk.c", "mapscan.c",
             "sentinels.c", "carrier_libc.c", "carrier_driver.c",
             "module_entry.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]

# Files that MUST be byte-identical to the fc1pool1 build (which verified
# vs the qdrv1/igf5 lineage).
VENDORED_VS_FC1 = ["record_sink.c", "record_sink.h", "ingame_libc.c",
                   "ingame_libc.h", "ingame_abi.h", "anchor_store.h",
                   "vmmap_walk.c", "mapscan.c", "sentinels.c", "boundaries.S"]
# LABELED-CHANGE files: measured + pinned here (updated consciously).
LABELED_PINNED = {
    "import_references.S":
        "8a8966a057871d84eac469abee58334ba2812576eace7d553e7d28ce8c0d11cb",
    "module_entry.c":
        "a394104958686dbc497f8beaad1a7179ef93e85d87f70472475886cc665197ec",
}
# NEW files (recorded, never compared to a lineage).
NEW_FILES = ["carrier_driver.c", "carrier_driver.h", "carrier_libc.c"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

STRONG_VERBATIM = ["__error", "clock_gettime", "getpid", "nanosleep",
                   "pthread_create", "pthread_detach",
                   "sceKernelClose", "sceKernelOpen", "sceKernelWrite", "sysctl"]
FLAGGED_ADDITIONS = ["nanosleep", "pthread_create", "pthread_detach"]
# beyond the igf 7: nanosleep (fc1pool1 precedent) + pthread_create/
# pthread_detach (qdrv1/observer precedent; qdrv1's pthread_self is dropped)

FORBIDDEN_SYMBOLS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect", "scePthreadMprotect",
    "igcar_sys_mprotect",
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


def check_vendored(receipt) -> dict:
    rows = {}
    for f in VENDORED_VS_FC1:
        a, b = FC1 / "src" / f, SRC / f
        if sha256(a) != sha256(b):
            sys.exit(f"LOUD FAILURE: vendored {f} diverged from the fc1pool1 build")
        rows[f] = {"sha256": sha256(b), "identical_to": str(a)}
    for f in LABELED_PINNED:
        got = sha256(SRC / f)
        pin = LABELED_PINNED[f]
        if pin is not None and got != pin:
            sys.exit(f"LOUD FAILURE: labeled-change {f} drifted vs pin: {got}")
        rows[f] = {"sha256": got, "labeled_change": True,
                   "pinned": pin if pin is not None else got}
    for f in NEW_FILES:
        rows[f] = {"sha256": sha256(SRC / f), "new_file": True}
    return rows


def patch_module_name(bare: Path, receipt) -> int:
    """LABELED POST-PACKAGING TRANSFORM (the Packager.dll is used IN PLACE and
    hardcodes moduleName/exportLibraryName 'modbridge_ingame_facts'; this
    probe requires its OWN loader identity, distinct from igf6/qdrv1/fc1pool1/
    observer/composition). Pure LENGTH-PRESERVING string substitution in the
    bare ELF ('modbridge_ingame_facts' 22 bytes -> 'modbridge_carrier' 17 +
    5 NULs); runs BEFORE finalize/make_fself so every downstream hash and the
    fself wrapper cover the PATCHED bytes; check_carrier_artifact.py
    re-verifies the final container against the patched bare. Deterministic:
    pure function of input bytes."""
    b = bytearray(bare.read_bytes())
    old = b"modbridge_ingame_facts"
    new = b"modbridge_carrier" + b"\x00" * 5
    assert len(old) == len(new), (len(old), len(new))
    n = b.count(old)
    if n < 1:
        sys.exit("LOUD FAILURE: Packager module-name string not found in bare ELF")
    b = b.replace(old, new)
    if bytes(b).count(old) != 0:
        sys.exit("LOUD FAILURE: residual Packager module-name string after patch")
    bare.write_bytes(bytes(b))
    receipt.append({"transform": "patch_module_name", "occurrences": n,
                    "new_name": MODULE})
    return n


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


def build_once(dest: Path, build_id: str, receipt) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = dest / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    (gen / "ingame_config.h").write_text(
        "// GENERATED by build_carrier_probe.py — do not edit by hand.\n"
        "#pragma once\n"
        f"#define IG_BUILD_ID \"{build_id}\"\n"
        f"#define IG_LOG_PATH \"{LOG_PATH}\"\n"
        f"#define IG_UE4SS_LOG_PATH \"{UE4SS_LOG_PATH}\"\n"
        "// GUA header window (worker-2 profile vaddr; live-validated layout A\n"
        "// by worker-anchor-validation: NumElements=71959, first_gc=35477).\n"
        f"#define IG_VADDR_GUOBJECT_ARRAY 0x{VADDR_GUOBJECT_ARRAY:x}ull\n"
        f"#define IG_RT_GUOBJECT_ARRAY 0x{RT_GUOBJECT_ARRAY:x}ull\n"
        "// FNamePool chain labels (worker-fname-ctor-audit FNAME-VERDICT.md\n"
        "// sec 4-5, operand-derived file-image labels, sha-pinned capture;\n"
        "// pool decoder LIVE-VALIDATED by fc1pool1):\n"
        "// pool label 0x9365ec0 (LEA target in the interning core 0x242470),\n"
        "// init guard byte label 0x9365ea0; runtime = base + label with the\n"
        "// proven single load bias base=0x400000; the probe re-derives base\n"
        "// per launch from the GUA anchor and never trusts the constant.\n"
        f"#define IG_VADDR_FNAME_POOL 0x{VADDR_FNAME_POOL:x}ull\n"
        f"#define IG_RT_FNAME_POOL 0x{RT_FNAME_POOL:x}ull\n"
        f"#define IG_VADDR_INIT_FLAG 0x{VADDR_INIT_FLAG:x}ull\n"
        f"#define IG_IMAGE_BASE 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_EXPECTED_BASE 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_EXEC_SPAN_START 0x{EXEC_SPAN_START:x}ull\n"
        f"#define IG_EXEC_SPAN_END 0x{EXEC_SPAN_END:x}ull\n"
        f"#define IG_PAGE_SIZE 0x{PAGE_SIZE:x}ull\n")

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

    # syscall gate (igf6 inversion, kept): ZERO raw `syscall` instructions.
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count != 0:
        sys.exit(f"LOUD BUILD FAILURE: {syscall_count} raw syscall "
                 f"instruction(s) in combined object (cc1 must have 0)")

    # Import contract: TEN verbatim rows of the frozen 191-row contract.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        provenance[n] = ("verbatim:frozen-191-contract (row copied byte-identical)"
                         + (" [FLAGGED ADDITION beyond the igf 7: "
                            "nanosleep=fc1pool1 grace-sleep precedent; "
                            "pthread_create/pthread_detach=qdrv1/observer "
                            "one-thread precedent; pthread_self dropped — no "
                            "thread-identity fold]" if n in FLAGGED_ADDITIONS
                           else ""))
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned cc1 carrier-census import identities: TEN console-"
                  "contract VERBATIM rows of the frozen 191-row contract. "
                  "THREE flagged additions beyond the igf 7 (nanosleep, "
                  "pthread_create, pthread_detach — each an existing lineage "
                  "precedent). pthread_self is dropped relative to qdrv1. "
                  "NO mutex import of any kind, no module-query, no dl*, "
                  "no mprotect, no raw syscall."),
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
            "qdrv1_rows_dropped_here": ["pthread_self"],
            "reason": "stay dropped from v2 isolation; cc1 adds only the "
                      "three flagged verbatim rows (nanosleep, "
                      "pthread_create, pthread_detach)"},
        "contract": {"schema": 1, "consumer": "worker-carrier-probe-build derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = dest / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    (dest / "expected-imports.json").write_text(
        json.dumps({"names": sorted(STRONG_VERBATIM),
                    "note": "oracle for check_carrier_artifact.py; decoded "
                            "dynsym name set must equal this exactly"},
                   indent=1) + "\n")
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
    patch_module_name(bare, receipt)

    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v4 build={build_id}",
               "build_id": build_id,
               "variant": "cc1-carrier-census-ridealong",
               "sdk_version": "0x11000000",
               "stages": "9th ini entry alongside the final-ini layout "
                         "(own late-load module); see STAGE-PLAN.md",
               "entry_marker_first": True, "promotion_allowed": False,
               "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (dest / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = dest / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, dest / "profile.json", bare_sdk], receipt)
    final = dest / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipt)

    # Defense in depth: the FINAL inner ELF .text must have no 0F 05 pair.
    if text_has_syscall_pair(bare_sdk):
        sys.exit("LOUD BUILD FAILURE: 0F 05 byte pair present in inner ELF .text")

    run(["python3", HERE / "check_carrier_artifact.py", dest], receipt)

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "cc1-carrier-census-ridealong",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "ue4ss_log_path": UE4SS_LOG_PATH,
        "module": MODULE,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json (igf6-adapted)"},
        "vendored_identity": receipt_vendored,
        "imports": {"rows": STRONG_VERBATIM,
                    "provenance": provenance,
                    "flagged_additions": FLAGGED_ADDITIONS,
                    "all_verbatim_frozen_rows": True},
        "read_mechanism": {
            "impl": "FNamePool data census: current per-invocation sysctl "
                    "KERN_PROC_VMMAP scan (controller correction #1 "
                    "discipline) gates EVERY dereference; qdrv1 GUA "
                    "admission + 10000 layout gate, fc1-validated pool "
                    "decoder, then the budget-bounded 53-name census walk. "
                    "NO mprotect, NO raw syscall, NO code reads, NO writes, "
                    "NO hooks, NO mutex.",
            "syscall_instruction_objects": syscall_count,
            "syscall_gate": "asserted == 0",
            "import_rows_added_beyond_igf7": 3},
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


receipt_vendored: dict = {}


def main() -> None:
    global receipt_vendored
    dest_arg = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    receipt_vendored = check_vendored([])

    seed_srcs = sorted(C_SOURCES + ASM_SOURCES)
    h = hashlib.sha256()
    for f in seed_srcs:
        h.update((SRC / f).read_bytes())
    h.update(LOG_PATH.encode())
    h.update(b"cc1")
    seed = h.hexdigest()[:12]
    build_id = f"cc1-{seed}"

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
               "variant": "cc1-carrier-census-ridealong",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(HERE / f"{MODULE}.prx"),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "vendored_identity": receipt_vendored,
               "imports": r1["imports"],
               "read_mechanism": r1["read_mechanism"],
               "native_executed": False, "in_game_results": False}
    shutil.copyfile(d1 / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({k: summary[k] for k in ("build_id", "deliverable",
                                              "determinism", "run1")}))
    summary["commands"] = r1["commands"]
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
