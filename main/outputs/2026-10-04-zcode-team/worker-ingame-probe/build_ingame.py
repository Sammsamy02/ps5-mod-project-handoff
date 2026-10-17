#!/usr/bin/env python3
"""build_ingame.py — pinned-toolchain build of the MODBRIDGE IN-GAME FACTS PRX.

Replicates the proven packaging recipe of the frozen lineage
(.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1,
checkpoint native_aligned_entry; receipts in build-1/package-commands.json):

  1. verify the frozen lineage PRX hash (read-only provenance check);
  2. compile owned sources with the pinned SDK prospero-clang (freestanding);
  3. combine with the pinned LLVM ld.lld -r;
  4. generate the import contract: identity rows copied VERBATIM from the
     frozen 191-import contract where they exist, plus DERIVED rows for the
     three sceKernel module functions (pattern of the frozen sceKernelOpen/
     sysctl rows: module libkernel v257, soname libkernel.prx, library
     libkernel v1) — derivations are explicit in the contract JSON;
  5. package with the derived C# harness (SharpProspero, frozen assemblies);
  6. stamp SDK 0x11000000 via the frozen finalize_sdk.py + own profile;
  7. wrap with the pinned SDK make_fself.py;
  8. run check_ingame_artifact.py and write a full receipt.

Every command's argv/rc/stdout/stderr plus hashes are recorded. Output goes
to a FRESH build-<id>/ directory; frozen history is never overwritten.
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
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json"

MODULE = "modbridge_ingame_facts"
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "experiments.c", "module_entry.c", "sentinels.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = ["layout_bridge.cpp", "townfall_layout_probes.cpp"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]
CPPFLAGS = CFLAGS + ["-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics"]

# Verbatim identity rows (frozen contract) + derived rows for the module fns.
STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]
STRONG_DERIVED = {
    "sceKernelGetModuleList": "libkernel",
    "sceKernelGetModuleInfo": "libkernel",
    "sceKernelGetExecutableModuleHandle": "libkernel",
    "dlopen": "libkernel",
    "dlclose": "libkernel",
}
WEAK_NAMES = ["dlsym", "__dlopen", "__dlclose", "__dlsym"]


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
    ) + LOG_PATH.encode()).hexdigest()[:12]
    out = HERE / f"build-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")
    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    # 1. Generated config (build id pins the exact source set).
    build_id = f"igf-{seed}"
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

    # 3. Import contract: verbatim rows + explicit derivations.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        provenance[n] = "verbatim:frozen-191-contract"
    for n, lib in STRONG_DERIVED.items():
        imports.append({
            "ImportSymbol": {
                "Name": n, "ModuleName": "libkernel", "ModuleVersion": 257,
                "LibraryName": lib, "LibraryVersion": 1,
                "Soname": "libkernel.prx", "PublishedName": n},
            "symbol_type": "FUNC", "binding": "GLOBAL"})
        provenance[n] = ("derived:pattern-of-frozen-sceKernelOpen/sysctl rows "
                         "(module libkernel v257, soname libkernel.prx, library "
                         "libkernel v1); NID resolution deferred to the frozen "
                         "SharpProspero linker at package time")
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned in-game facts PRX import identities; verbatim rows "
                  "from the frozen 191-import lineage contract plus explicit "
                  "libkernel-pattern derivations for the three module-query "
                  "functions. No interface bytes or loader acceptance."),
        "counts": {"required_strong": len(imports), "required_weak": len(WEAK_NAMES),
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": WEAK_NAMES,
        "identity_provenance": provenance,
        "contract": {"schema": 1, "consumer": "worker-ingame-probe derived Packager",
                     "imports": imports, "unresolved_weak_names": WEAK_NAMES}}
    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)

    # 4. Derived harness build + package.
    harness_dll = HERE / "harness" / "bin" / "Release" / "net10.0" / "Packager.dll"
    run([DOTNET, "build", HERE / "harness" / "Packager.csproj", "-c", "Release",
         "--ignore-failed-sources"], receipts, cwd=HERE)
    bare = out / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, harness_dll, combined, cpath, csha, bare], receipts)

    # 5. SDK stamp via frozen finalize_sdk.py (read-only use) + own profile.
    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v1 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (out / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = out / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, out / "profile.json", bare_sdk], receipts)

    # 6. Fake-self wrap with the pinned SDK script.
    final = out / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipts)

    # 7. Artifact checks + receipt.
    run(["python3", HERE / "check_ingame_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": frozen_ok, "recipe": "package-commands.json"},
        "commands": receipts,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in
                           STRONG_VERBATIM + list(STRONG_DERIVED)},
        "weak_imports": WEAK_NAMES,
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. In-game facts require the user-coordinated "
                 "Townfall launch via ploader (see STAGE-PLAN.md)."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "final_prx", "final_sha256", "final_bytes",
                       "contract_sha256", "strong_imports", "weak_imports")}))


if __name__ == "__main__":
    main()
