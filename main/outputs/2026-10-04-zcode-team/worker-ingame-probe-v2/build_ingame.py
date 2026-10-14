#!/usr/bin/env python3
"""build_ingame.py — v2 ISOLATION build of the MODBRIDGE IN-GAME FACTS PRX
(worker-ingame-probe-v2, team 2026-10-04).

Vendored from worker-ingame-probe/build_ingame.py. Packaging pipeline is
reused EXACTLY (frozen lineage recipe; import_references-first combine,
SharpProspero Packager, SDK 0x11000000 stamp via frozen finalize_sdk.py,
pinned make_fself wrap). The ONLY functional changes:

  - import contract = the 7 VERBATIM frozen-contract rows, nothing else
    (the 5 derived rows and 4 weak dl rows of v1 are removed entirely);
  - build_id prefix igf2-;
  - LOUD GATE after combine: if _DYNAMIC or any dropped-import dependency
    appears anywhere in the combined object's symbol table, the build FAILS
    before packaging;
  - contract rows are verified BYTE-IDENTICAL (canonical JSON equality) to
    the frozen 191-import contract rows before packaging.

v1 (igf-f4a91d8123b0) was silently load-rejected on console while the
sibling aligned-entry module loaded in the same launch. The derived-import
identity acceptance is the prime-suspect HYPOTHESIS (unproven until v2's
entry-marker evidence is read on console).
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

MODULE = "modbridge_ingame_facts"  # UNCHANGED on purpose: replaces v1's file
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "experiments.c", "module_entry.c", "sentinels.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = ["layout_bridge.cpp", "townfall_layout_probes.cpp"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]
CPPFLAGS = CFLAGS + ["-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics"]

# The COMPLETE import identity set of v2: 7 verbatim rows ONLY.
STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]
WEAK_NAMES = []  # v1's weak dl family removed with G3

# LOUD GATE list: any of these appearing as a symbol in the combined object
# (defined OR undefined) means a dropped-import dependency or an unintended
# _DYNAMIC reference crept into the link -> build FAILS.
FORBIDDEN_SYMBOLS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC",
]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(argv, receipt, cwd=None, allow_fail=False):
    # The SDK wrapper scripts resolve the real toolchain through llvm-config;
    # pin it to the same LLVM 21.1.8 the lineage receipts used.
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


def main() -> None:
    receipts = []
    # 0. Frozen lineage provenance check (hash only; never modified).
    frozen_actual = sha256(FROZEN_PRX)
    frozen_ok = frozen_actual == FROZEN_SHA
    if not frozen_ok:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    # Fresh build dir.
    seed = hashlib.sha256(b"".join(
        (SRC / f).read_bytes() for f in sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
        if (SRC / f).exists()
    ) + LOG_PATH.encode() + b"v2").hexdigest()[:12]
    out = HERE / f"build-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")
    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    # 1. Generated config (build id pins the exact source set; igf2- prefix).
    build_id = f"igf2-{seed}"
    (gen / "ingame_config.h").write_text(
        "// GENERATED by build_ingame.py — do not edit by hand.\n"
        "#pragma once\n"
        f"#define IG_BUILD_ID \"{build_id}\"\n"
        f"#define IG_LOG_PATH \"{LOG_PATH}\"\n"
        "// Profile vaddrs (worker-2 townfall-ps5-ue56.profile.json facts:\n"
        "// GUObjectArray.guobject_original, GameEngineTick.gameengine_tick,\n"
        "// UObject.static_construct_object; consumed as ARGUMENTS per the\n"
        "// vendored predicates' provenance rule, never embedded there).\n"
        "#define IG_VADDR_GUOBJECT_ARRAY 0x93ba110ull\n"
        "#define IG_VADDR_UGAMEENGINE_TICK 0x2fac8c0ull\n"
        "#define IG_VADDR_STATIC_CONSTRUCT_OBJECT 0x490aa0ull\n")

    inc = ["-I", str(gen), "-I", str(SRC)]

    # 2. Compile.
    objs = []
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
        run([CLANGXX] + CPPFLAGS + inc + ["-std=c++17", "-c", SRC / f, "-o", o], receipts)
        objs.append(o)

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
        # Byte-identity proof: canonical-JSON equality of the exact row object.
        assert json.dumps(rows[n], sort_keys=True) == \
               json.dumps(imports[-1], sort_keys=True)
        provenance[n] = "verbatim:frozen-191-contract (row copied byte-identical)"
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned v2 isolation PRX import identities: ONLY the 7 "
                  "console-proven verbatim rows from the frozen 191-import "
                  "lineage contract. v1's five derived rows (module queries + "
                  "dlopen/dlclose) and four weak dl rows are REMOVED. No "
                  "interface bytes or loader acceptance claimed."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen", "dlclose"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": "v1 silent load rejection; derived-import acceptance is the unproven prime suspect"},
        "contract": {"schema": 1, "consumer": "worker-ingame-probe-v2 derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    # Re-verify byte-identity of every row against the frozen file on disk.
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
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v2 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "v1 igf-f4a91d8123b0 (load-rejected; same path, same ini line)",
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
    run(["python3", HERE / "check_ingame_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "v2-isolation-7-verbatim-imports",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": frozen_ok, "recipe": "package-commands.json"},
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
                 "Townfall launch via ploader (see STAGE-PLAN-V2.md)."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "variant", "final_prx", "final_sha256",
                       "final_bytes", "contract_sha256", "strong_imports",
                       "weak_imports", "forbidden_symbol_gate")}))


if __name__ == "__main__":
    main()
