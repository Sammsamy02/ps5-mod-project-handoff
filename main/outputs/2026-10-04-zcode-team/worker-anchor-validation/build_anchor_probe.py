#!/usr/bin/env python3
"""build_anchor_probe.py — igf4- build of the focused ANCHOR VALIDATION PRX
(worker-anchor-validation, team 2026-10-04).

Vendored from worker-ingame-probe-v3/build_ingame.py. The packaging pipeline
is reused EXACTLY (frozen lineage recipe; import_references-first combine,
SharpProspero Packager, SDK 0x11000000 stamp via frozen finalize_sdk.py,
pinned make_fself). Labeled changes:

  - source set: focused probe (mapscan/anchor_probe/module_entry + vendored
    worker-5 predicate + the v3 writer/libc/sentinels/imports/boundaries);
    the v3 experiments/vmmap-dump/base-hunt/reflection TUs are NOT built —
    controller correction #6 (no map dump) and the one-launch focused
    validation mission;
  - generated config now carries the ANCHOR FACTS + the EXPECTED prologue
    bytes, parsed from this worker's expected_bytes_tick_sco.txt with LOUD
    provenance checks (image sha256 pinned to the survey receipt, file
    offsets recomputed as profile_vaddr+0x4000) — the artifact provably
    derives from the retained-image evidence;
  - import contract = the SAME 7 VERBATIM frozen-contract rows (unchanged);
  - build_id prefix igf4-; module name/path/log path unchanged (same-file
    replacement of igf3-, no ini change);
  - optional argv[1] = destination directory (determinism double-build).

Predicate provenance: src/townfall_layout_probes.{hpp,cpp} are byte-identical
copies of worker-5-layout-validator/src (the batch1 GUA predicate). After the
PS5 compile this script cross-checks the built object against the
INDEPENDENT worker-predicate-compile bundle (ps5/townfall_layout_probes.o):
identical bytes prove source identity + compile determinism across workers;
any divergence is a LOUD failure.
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
SRC = HERE / "src"

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
CLANGXX = SDK_BIN / "prospero-clang++"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json"

# Retained image provenance (expected-bytes extraction evidence).
EVIDENCE = HERE / "expected_bytes_tick_sco.txt"
IMAGE_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
LOAD0_OFF_DELTA = 0x4000          # LOAD0: p_offset=0x4000, p_vaddr=0x0
VADDR_TICK = 0x2FAC8C0
VADDR_SCO = 0x490AA0

# Independent PS5 compile of the same predicate (worker-predicate-compile).
CROSS_PRED_O = (WHAT / "outputs/2026-10-04-zcode-team/"
                "worker-predicate-compile/ps5/townfall_layout_probes.o")

MODULE = "modbridge_ingame_facts"  # UNCHANGED: same-file replacement lineage
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "vmmap_walk.c", "mapscan.c",
             "sentinels.c", "module_entry.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = ["anchor_probe.cpp", "townfall_layout_probes.cpp"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]
CPPFLAGS = CFLAGS + ["-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics"]

# The COMPLETE import identity set (igf4 == v3 == v2 == the console-proven 7).
STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]
WEAK_NAMES = []

FORBIDDEN_SYMBOLS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC",
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


def load_expected_bytes() -> dict:
    """Parse the extraction evidence with LOUD provenance checks."""
    ev = json.loads(EVIDENCE.read_text())
    if ev["image_sha256"] != IMAGE_SHA:
        sys.exit(f"LOUD FAILURE: retained-image sha drifted: {ev['image_sha256']}")
    for key, vaddr in (("tick", VADDR_TICK), ("sco", VADDR_SCO)):
        entry = ev[key]
        want_off = vaddr + LOAD0_OFF_DELTA
        if int(entry["file_offset"], 16) != want_off:
            sys.exit(f"LOUD FAILURE: {key} file_offset != vaddr+0x4000")
        if len(entry["expected16_hex"]) != 32:
            sys.exit(f"LOUD FAILURE: {key} expected16_hex not 16 bytes")
    return ev


def main() -> None:
    receipts = []
    dest = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    # 0. Frozen lineage provenance check (hash only; never modified).
    frozen_actual = sha256(FROZEN_PRX)
    frozen_ok = frozen_actual == FROZEN_SHA
    if not frozen_ok:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    ev = load_expected_bytes()
    tick_hex = ev["tick"]["expected16_hex"]
    sco_hex = ev["sco"]["expected16_hex"]

    # Fresh build dir.
    seed = hashlib.sha256(b"".join(
        (SRC / f).read_bytes() for f in sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
        if (SRC / f).exists()
    ) + LOG_PATH.encode() + b"igf4").hexdigest()[:12]
    out = dest if dest else HERE / f"build-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")
    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    # 1. Generated config (build id pins the exact source set; igf4- prefix).
    build_id = f"igf4-{seed}"
    (gen / "ingame_config.h").write_text(
        "// GENERATED by build_anchor_probe.py — do not edit by hand.\n"
        "#pragma once\n"
        f"#define IG_BUILD_ID \"{build_id}\"\n"
        f"#define IG_LOG_PATH \"{LOG_PATH}\"\n"
        "// Profile vaddrs (worker-2 townfall-ps5-ue56.profile.json facts;\n"
        "// consumed as ARGUMENTS per the vendored predicates' provenance\n"
        "// rule, never embedded there).\n"
        "#define IG_VADDR_GUOBJECT_ARRAY 0x93ba110ull\n"
        "#define IG_VADDR_UGAMEENGINE_TICK 0x2fac8c0ull\n"
        "#define IG_VADDR_STATIC_CONSTRUCT_OBJECT 0x490aa0ull\n"
        "// Checkpoint zcode_v3_MAPPING_BREAKTHROUGH single-load-bias facts:\n"
        "// base=0x400000, runtime = base + profile_vaddr, all segments.\n"
        "#define IG_IMAGE_BASE 0x400000ull\n"
        "#define IG_RT_UGAMEENGINE_TICK 0x33ac8c0ull\n"
        "#define IG_RT_STATIC_CONSTRUCT_OBJECT 0x890aa0ull\n"
        "#define IG_RT_GUOBJECT_ARRAY 0x97ba110ull\n"
        "// Expected prologue bytes extracted from the RETAINED image\n"
        "// (expected_bytes_tick_sco.txt; decrypted-eboot.bin sha256\n"
        "// 3b6cfad4…; file offset = profile_vaddr + 0x4000 per LOAD0\n"
        "// p_offset=0x4000/p_vaddr=0x0, re-verified from the ELF header).\n"
        f"#define IG_TICK_FILE_OFF 0x{VADDR_TICK + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_SCO_FILE_OFF 0x{VADDR_SCO + LOAD0_OFF_DELTA:x}ull\n"
        f"#define IG_TICK_EXPECTED_HEX \"{tick_hex}\"\n"
        f"#define IG_SCO_EXPECTED_HEX \"{sco_hex}\"\n"
        "#define IG_IMAGE_SHA16 \"3b6cfad47dd17fea\"\n")

    inc = ["-I", str(gen), "-I", str(SRC)]

    # 2. Compile.
    objs = []
    pred_obj = None
    for f in C_SOURCES:
        o = out / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)
    for f in ASM_SOURCES:
        o = out / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)
    for f in CPP_SOURCES:
        o = out / (f.replace(".cpp", ".o"))
        run([CLANGXX] + CPPFLAGS + ["-std=c++17", "-c", SRC / f, "-o", o],
            receipts)
        objs.append(o)
        if f == "townfall_layout_probes.cpp":
            pred_obj = o

    # 2b. Cross-worker predicate object check (provenance, not a dependency).
    cross = {"reference": str(CROSS_PRED_O), "reference_sha256": None,
             "ours_sha256": None, "match": None}
    if CROSS_PRED_O.exists():
        cross["reference_sha256"] = sha256(CROSS_PRED_O)
        cross["ours_sha256"] = sha256(pred_obj)
        cross["match"] = cross["reference_sha256"] == cross["ours_sha256"]
        if not cross["match"]:
            sys.exit("LOUD FAILURE: our predicate object diverges from the "
                     f"independent worker-predicate-compile bundle: {cross}")
    else:
        cross["match"] = "reference-absent"

    combined = out / "combined-ingame.o"
    # Lineage order: the typed import-reference object FIRST so ld -r keeps
    # its FUNC types when merging with the C objects' NOTYPE undefineds.
    ref = out / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipts)

    # 3. LOUD GATE: forbidden symbols must not appear anywhere in the link.
    nm = run([NM, combined], receipts)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    bad = sorted(seen & set(FORBIDDEN_SYMBOLS))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: dropped-import dependency or _DYNAMIC "
                 f"reference present in combined object: {bad}")

    # 4. Import contract: the 7 verbatim rows, verified byte-identical.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        assert json.dumps(rows[n], sort_keys=True) == \
               json.dumps(imports[-1], sort_keys=True)
        provenance[n] = "verbatim:frozen-191-contract (row copied byte-identical)"
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned igf4 anchor-validation probe import identities: the "
                  "SAME 7 console-proven verbatim rows the loaded-and-logged "
                  "v2 and the v3 lineage carried (v2 run evidence: 56 "
                  "records). No interface bytes or loader acceptance beyond "
                  "that lineage claimed."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen", "dlclose"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": "stay dropped from v2 isolation; v2 loaded+logged with 7 rows"},
        "contract": {"schema": 1, "consumer": "worker-anchor-validation derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(rows[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged from frozen")

    # 5. Derived harness build + package.
    harness_dll = HERE / "harness" / "bin" / "Release" / "net10.0" / "Packager.dll"
    run([DOTNET, "build", HERE / "harness" / "Packager.csproj", "-c", "Release",
         "--ignore-failed-sources"], receipts, cwd=HERE)
    bare = out / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, harness_dll, combined, cpath, csha, bare], receipts)

    # 6. SDK stamp via frozen finalize_sdk.py (read-only use) + own profile.
    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v4 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "v3 igf3-de0c0cb644d3 (same path, same ini line; "
                           "v3 stalled after its VMMAP record — this probe "
                           "has no dump by design)",
               "entry_marker_first": True,
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (out / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = out / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, out / "profile.json", bare_sdk], receipts)

    # 7. Fake-self wrap with the pinned SDK script.
    final = out / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipts)

    # 8. Artifact checks + receipt.
    run(["python3", HERE / "check_anchor_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf4-anchor-validation",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": frozen_ok, "recipe": "package-commands.json"},
        "expected_bytes_provenance": {
            "evidence": str(EVIDENCE), "image_sha256": ev["image_sha256"],
            "tick": {"vaddr": hex(VADDR_TICK),
                     "file_off": hex(VADDR_TICK + LOAD0_OFF_DELTA),
                     "hex": tick_hex},
            "sco": {"vaddr": hex(VADDR_SCO),
                    "file_off": hex(VADDR_SCO + LOAD0_OFF_DELTA),
                    "hex": sco_hex}},
        "predicate_cross_worker_check": cross,
        "commands": receipts,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in STRONG_VERBATIM},
        "weak_imports": [],
        "forbidden_symbol_gate": {"names": FORBIDDEN_SYMBOLS, "status": "passed"},
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. In-game facts require the user-coordinated "
                 "Townfall launch via ploader (see STAGE-PLAN-ANCHOR.md)."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "variant", "final_prx", "final_sha256",
                       "final_bytes", "contract_sha256", "strong_imports",
                       "weak_imports", "forbidden_symbol_gate",
                       "predicate_cross_worker_check")}))


if __name__ == "__main__":
    main()
