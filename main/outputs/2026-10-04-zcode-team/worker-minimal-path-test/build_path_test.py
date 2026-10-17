#!/usr/bin/env python3
"""build_path_test.py — worker-minimal-path-test (team 2026-10-04).

Smallest ploader PRX probe: writes PATH_TEST (UE4SS.log writability) and
ACTIVATION_TEST (loadlist.txt 16-byte read) lines, per CLAIM.json.

Packaging pipeline reused EXACTLY from the frozen igd1 minimal diagnostic
(worker-v3-startup-diag/diag/build_ingame.py, itself the frozen v2/v3
lineage): import_references-first `ld -r` combine, SharpProspero Packager,
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself,
two-run determinism with byte-equality comparison, artifact checker gate.

Deltas vs igd1 (labeled, per CLAIM.json):
  - MODULE modbridge_path_test (NEW staged name; the disabled
    modbridge_ingame_facts.prx file/line is NOT touched).
  - Primary runtime log = /data/plugins/UE4SS.log (the path under test);
    fallback = /data/plugins/modbridge_path_test.fallback.log (only written
    if the primary open fails; records errno + both lines).
  - Import table = the SAME 7 console-proven verbatim rows + ONE verbatim
    frozen-contract row: POSIX `read` (16-byte loadlist read). Zero derived
    identities. Loud first-use-in-lineage label on `read`.
  - build_id prefix mpt1-.
  - The Packager harness is COPIED into this worker dir and built here, so
    no build ever writes into the frozen diag worker directory.
  - no-arg mode: determinism double-build (run1 + run2), byte-equality
    comparison, final PRX also copied to this worker directory root.
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

DIAG = WHAT / "outputs/2026-10-04-zcode-team/worker-v3-startup-diag"

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json"

MODULE = "modbridge_path_test"
LOG_PATH = "/data/plugins/UE4SS.log"
LOADLIST_PATH = "/data/modbridge/roots/PPSA33286/loadlist.txt"
FALLBACK_PATH = "/data/plugins/modbridge_path_test.fallback.log"
C_SOURCES = ["path_test_entry.c", "sentinels.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]
CPP_SOURCES = []  # NO C++ ANYWHERE in this probe

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

# The igd1 console-proven 7 verbatim rows, UNTOUCHED, + exactly one added
# verbatim frozen row for the mission's 16-byte read (see CLAIM.json).
STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime", "read"]
CONSOLE_PROVEN_7 = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                    "sysctl", "getpid", "clock_gettime"]

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


def ensure_vendored() -> None:
    """Copy the frozen diag support sources + Packager harness ONCE into this
    worker dir. Originals stay read-only; copies make this build hermetic."""
    pairs = [("diag/src/sentinels.c", "src/sentinels.c"),
             ("diag/src/boundaries.S", "src/boundaries.S")]
    for rel_src, rel_dst in pairs:
        s, d = DIAG / rel_src, HERE / rel_dst
        if not d.exists():
            shutil.copyfile(s, d)
    if not (HERE / "harness/Packager.csproj").exists():
        shutil.copytree(DIAG / "diag" / "harness", HERE / "harness",
                        ignore=shutil.ignore_patterns("bin", "obj"))


def build_once(tag: str) -> Path:
    receipts = []
    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    seed = hashlib.sha256(b"".join(
        (SRC / f).read_bytes() for f in sorted(C_SOURCES + ASM_SOURCES + CPP_SOURCES)
        if (SRC / f).exists()
    ) + LOG_PATH.encode() + LOADLIST_PATH.encode() + tag.encode()).hexdigest()[:12]
    out = HERE / f"build-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")
    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    build_id = f"mpt1-{seed}"
    (gen / "pt_config.h").write_text(
        "// GENERATED by build_path_test.py (worker-minimal-path-test) — do not edit.\n"
        "#pragma once\n"
        f"#define PT_BUILD_ID \"{build_id}\"\n"
        f"#define PT_LOG_PATH \"{LOG_PATH}\"\n"
        f"#define PT_LOADLIST_PATH \"{LOADLIST_PATH}\"\n"
        f"#define PT_FALLBACK_PATH \"{FALLBACK_PATH}\"\n")

    inc = ["-I", str(gen), "-I", str(SRC)]

    objs = []
    for f in C_SOURCES:
        o = out / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)
    for f in ASM_SOURCES:
        o = out / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)

    combined = out / "combined-path_test.o"
    ref = out / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipts)

    nm = run([NM, combined], receipts)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    bad = sorted(seen & set(FORBIDDEN_SYMBOLS))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: forbidden symbol present: {bad}")

    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        provenance[n] = ("verbatim:frozen-191-contract (row copied byte-identical;"
                         " console-proven 7 carried by igd1/v2/v3)"
                         if n in CONSOLE_PROVEN_7 else
                         "verbatim:frozen-191-contract (row copied byte-identical;"
                         " FIRST USE in lineage: invoked here for the 16-byte"
                         " loadlist read; identity family of the console-loaded"
                         " 191-row aligned-entry module)")
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned MINIMAL PATH-TEST PRX import identities: the 7 "
                  "console-proven verbatim rows carried by igd1/v2/v3, "
                  "UNTOUCHED, plus exactly ONE verbatim frozen-contract row "
                  "(`read`) for the 16-byte loadlist read. The module opens "
                  "/data/plugins/UE4SS.log, writes PATH_TEST + "
                  "ACTIVATION_TEST lines, closes, and returns 0."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen", "dlclose",
                               "sceKernelRead"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": ("stay dropped from v2 isolation; v2 loaded+logged with 7 rows. "
                       "sceKernelRead absent from the frozen 191-row contract; the "
                       "POSIX-name `read` verbatim row is used instead.")},
        "contract": {"schema": 1, "consumer": "worker-minimal-path-test derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(rows[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged from frozen")

    harness_dll = HERE / "harness" / "bin" / "Release" / "net10.0" / "Packager.dll"
    run([DOTNET, "build", HERE / "harness" / "Packager.csproj", "-c", "Release",
         "--ignore-failed-sources"], receipts, cwd=HERE)
    bare = out / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, harness_dll, combined, cpath, csha, bare], receipts)

    profile = {"title_id": "PPSA33286",
               "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH,
               "loadlist_path": LOADLIST_PATH,
               "fallback_path": FALLBACK_PATH,
               "marker": f"modbridge.pathtest/mpt1 minimal build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "nothing; modbridge_ingame_facts.prx left as-is (disabled)",
               "entry_lines": ["PATH_TEST: /data/plugins/UE4SS.log writable at "
                               "<monotonic-ns>",
                               "ACTIVATION_TEST: loadlist.txt readable, first "
                               "16 bytes: <hex>"],
               "return_0_always": True, "minimal_variant": True,
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (out / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = out / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, out / "profile.json", bare_sdk], receipts)

    final = out / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipts)

    run(["python3", HERE / "check_path_test_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.pathtest.build",
        "schema": 1,
        "variant": "minimal-path-test",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "loadlist_path": LOADLIST_PATH,
        "fallback_path": FALLBACK_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": True, "recipe": "package-commands.json"},
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
        "notes": "MINIMAL PATH TEST: open UE4SS.log + two lines + 16-byte "
                 "loadlist read + close + return 0. Local packaging only; "
                 "staging per STAGE-PLAN.md."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "variant", "final_prx", "final_sha256",
                       "final_bytes", "contract_sha256")}))
    return out


def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "determinism"
    if mode == "build":
        build_once("mpt1")
    elif mode == "determinism":
        ensure_vendored()
        r1 = build_once("mpt1")
        moved = HERE / "determinism" / "run1"
        moved.parent.mkdir(exist_ok=True)
        r1.rename(moved)
        r2 = build_once("mpt1")
        moved2 = HERE / "determinism" / "run2"
        r2.rename(moved2)
        ok = True
        for rel in (f"{MODULE}.prx",
                    f"bare-original/{MODULE}.prx",
                    f"bare-sdk/{MODULE}.prx",
                    "combined-path_test.o", "import-contract.json"):
            a, b = (moved / rel).read_bytes(), (moved2 / rel).read_bytes()
            same = hashlib.sha256(a).hexdigest() == hashlib.sha256(b).hexdigest()
            ok &= same
            print(f"{'IDENTICAL' if same else 'DIVERGED'}: {rel} "
                  f"{hashlib.sha256(a).hexdigest()[:16]}")
        shutil.copyfile(moved / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
        print("DETERMINISM-PASSED" if ok else "DETERMINISM-FAILED")
        sys.exit(0 if ok else 1)
    else:
        sys.exit(f"unknown mode {mode}")


if __name__ == "__main__":
    main()
