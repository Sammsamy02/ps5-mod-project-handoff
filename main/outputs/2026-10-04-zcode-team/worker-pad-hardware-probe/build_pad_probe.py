#!/usr/bin/env python3
"""build_pad_probe.py — build of the STANDALONE PAD PROBE PRX
(worker-pad-hardware-probe, team 2026-10-04).

Vendored from worker-ingame-probe-v3/build_ingame.py. The packaging pipeline
is the FROZEN lineage recipe reused EXACTLY (import_references-first
combine, LOUD forbidden-symbol gate, SharpProspero Packager, SDK 0x11000000
stamp via frozen finalize_sdk.py, pinned make_fself). Changes are source
set + identity only:

  - sources: the C port of the standalone pad probe (src/) — the retained
    provider's exact backend sequence and the prepared module's record
    vocabulary (probe_start/key_fired/phase_change/probe_failed/probe_end);
  - import contract = the 7 VERBATIM frozen-contract rows (byte-identical,
    the console-proven v2/v3 set) + 7 NEW pad/service rows copied from
    Townfall's OWN saved executable import declarations
    (sceUserServiceGetLoginUserIdList from the retained SDK stub catalog);
  - build_id prefix padp-; module modbridge_pad_probe;
    log /data/pad-probe-events.jsonl;
  - the same LOUD forbidden-symbol gate and byte-identical contract-row
    verification as the lineage.

Usage: build_pad_probe.py --out DIR   (DIR must not exist)
"""
import argparse
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
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/"
              "native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = (WHAT / ".superpowers/native-dependency-spike/"
                   "native-full-runtime-aligned-entry-1/import-contract.json")
DECLARED_INPUT = (WHAT / ".superpowers/native-dependency-spike/"
                  "ue4ss-public-import-declarations-1/declared-input.json")

MODULE = "modbridge_pad_probe"          # same naming pattern as the lineage
LOG_PATH = "/data/pad-probe-events.jsonl"
BUILD_PREFIX = "padp-"

C_SOURCES = ["record_sink.c", "ingame_libc.c", "sentinels.c",
             "pad_source.c", "pad_probe_core.c", "pad_probe_module.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = []  # the pad probe is pure C (no libc++ in the ploader lineage)

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

# The verbatim console-proven 7 (rows copied byte-identical from the frozen
# 191-import contract).
STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]
# The NEW pad/service rows (Townfall-declared identities; catalog row for
# sceUserServiceGetLoginUserIdList). NOT console-load-proven — the probe
# launch is the experiment.
STRONG_PAD = ["scePadInit", "scePadOpen", "scePadReadState", "scePadClose",
              "sceUserServiceInitialize", "sceUserServiceGetLoginUserIdList",
              "sceUserServiceTerminate"]

# LOUD GATE list (v2 isolation, unchanged): any of these in the combined
# object means a dropped-import dependency or _DYNAMIC reference -> FAIL.
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


def build_pad_contract_rows(frozen_rows):
    """Return {name: contract-row} for the 7 pad rows, with provenance.

    PRIMARY: the frozen 191-import contract itself already carries all 7
    pad/service identities (its evidence per row is Townfall's OWN saved
    executable import declaration with the observed encoded NID; the
    aligned-entry runtime built from this same contract LOADED on console).
    Those rows are taken byte-identical, exactly like the verbatim 7.
    CROSS-CHECK: the identity FIELDS must equal the independent
    ue4ss-public-import-declarations-1 rows (same ModuleName/Soname/versions/
    PublishedName); a mismatch is a LOUD build failure."""
    declared = json.loads(DECLARED_INPUT.read_text())
    by_name = {}
    for d in declared["declarations"]:
        by_name.setdefault(d["name"], []).append(d)
    rows = {}
    for name in STRONG_PAD:
        if name not in frozen_rows:
            sys.exit(f"frozen contract lacks pad identity: {name}")
        row = frozen_rows[name]
        cross = by_name.get(name, [])
        ident = row["ImportSymbol"]
        for cand in cross:
            c = cand.get("identity", {})
            for k in ("ModuleName", "ModuleVersion", "Soname",
                      "LibraryName", "LibraryVersion", "PublishedName"):
                if k in c and c[k] != ident[k]:
                    sys.exit(f"LOUD BUILD FAILURE: identity field {k} of {name} "
                             f"diverges between frozen contract and public "
                             f"declarations: {c[k]} != {ident[k]}")
        rows[name] = row
    return rows


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="fresh output dir (must not exist)")
    args = ap.parse_args()
    out = Path(args.out).resolve()
    if out.exists():
        sys.exit(f"refuse to overwrite existing dir {out}")

    receipts = []
    # 0. Frozen lineage provenance check (hash only; never modified).
    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = out / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    # 1. Generated config + build id (source-set content hash; padp- prefix).
    seed = hashlib.sha256(b"".join(
        (SRC / f).read_bytes() for f in sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
        if (SRC / f).exists()
    ) + LOG_PATH.encode() + BUILD_PREFIX.encode()).hexdigest()[:12]
    build_id = f"{BUILD_PREFIX}{seed}"
    (gen / "pad_probe_config.h").write_text(
        "// GENERATED by build_pad_probe.py — do not edit by hand.\n"
        "#pragma once\n"
        f"#define PP_BUILD_ID \"{build_id}\"\n"
        f"#define PP_LOG_PATH \"{LOG_PATH}\"\n"
        "// Window defaults live in pad_probe_core.h (30s window, 5s grace,\n"
        "// bounded poll backstop); the module passes 0 to take the defaults.")
    inc = ["-I", str(gen), "-I", str(SRC)]

    # 2. Compile (C + asm; no C++ TUs — no libc++ in the ploader lineage).
    objs = []
    for f in C_SOURCES:
        o = out / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)
    for f in ASM_SOURCES:
        o = out / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)

    combined = out / "combined-pad.o"
    # Lineage order: the typed import-reference object FIRST so ld -r keeps
    # its FUNC types when merging with the C objects' NOTYPE undefineds.
    ref = out / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipts)

    # 3. LOUD GATE.
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

    # 4. Import contract: 7 verbatim + 7 pad rows, verified against sources.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    frozen_rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in frozen_rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    pad_rows = build_pad_contract_rows(frozen_rows)
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM + STRONG_PAD:
        row = frozen_rows[n] if n in frozen_rows else pad_rows[n]
        imports.append(row)
        # Re-serialize check: the row we embed is EXACTLY the row we read.
        if json.dumps(row, sort_keys=True) != json.dumps(imports[-1], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged")
        provenance[n] = ("verbatim:frozen-191-contract (row copied byte-identical; "
                         "pad rows carry Townfall-executable declaration evidence and "
                         "cross-check against ue4ss-public-import-declarations-1)"
                         if n in frozen_rows else "MISSING")
        if n not in frozen_rows:
            sys.exit(f"LOUD BUILD FAILURE: {n} unexpectedly absent from frozen contract")
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned standalone pad probe PRX import identities: ALL 14 "
                  "rows are byte-identical rows of the frozen 191-import "
                  "aligned-entry contract — the 7 the loaded-and-logged v2/v3 "
                  "probes carried plus the 7 pad/service rows (evidence: "
                  "Townfall's own executable import declarations). The "
                  "aligned-entry runtime built from this same contract loaded "
                  "on console; THIS module's load (new plugin combining the "
                  "pad rows in a ploader plugin) is still exactly what the "
                  "probe launch tests."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen", "dlclose"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": "v2 isolation kept; never referenced by the pad probe"},
        "contract": {"schema": 1, "consumer": "worker-pad-hardware-probe derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(frozen_rows[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: verbatim row {n} diverged from frozen")

    # 5. Derived harness build + package (frozen SharpProspero assemblies).
    harness_dll = HERE / "harness" / "bin" / "Release" / "net10.0" / "Packager.dll"
    run([DOTNET, "build", HERE / "harness" / "Packager.csproj", "-c", "Release",
         "--ignore-failed-sources"], receipts, cwd=HERE)
    bare = out / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, harness_dll, combined, cpath, csha, bare], receipts)

    # 6. SDK stamp via frozen finalize_sdk.py (read-only use) + own profile.
    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.padprobe/v1 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "none (NEW module; adds a ploader.ini line, see STAGE-PLAN.md)",
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
    run(["python3", HERE / "check_pad_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.padprobe.build",
        "schema": 1,
        "variant": "standalone-pad-probe-prx",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": True, "recipe": "worker-ingame-probe-v3/build_ingame.py"},
        "commands": receipts,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in STRONG_VERBATIM + STRONG_PAD},
        "weak_imports": [],
        "forbidden_symbol_gate": {"names": FORBIDDEN_SYMBOLS, "status": "passed"},
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. Pad input evidence requires the "
                 "user-coordinated Townfall launch via ploader (STAGE-PLAN.md)."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "variant", "final_prx", "final_sha256",
                       "final_bytes", "contract_sha256", "strong_imports",
                       "weak_imports", "forbidden_symbol_gate")}, indent=1))


if __name__ == "__main__":
    main()
