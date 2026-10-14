#!/usr/bin/env python3
"""build_argv_echo.py — G1 ARGV probe build (worker-argv-probe, 2026-10-04).

Follows the recipe of the three console-proven /loadpayload diagnostics
(NEXT_DLL_THREADS.elf, TEST_THIS_DLL.elf, INITONCE_DLL_TEST.elf): a single-C
standalone ELF linked by the ps5-payload-sdk prospero-clang wrapper exactly
like the SDK's own hello_argv / hello_world samples. From worker-ingame-
probe-v2 it inherits only discipline: receipted commands, a loud post-build
gate (check_argv_artifact.py), a saved import proof, and a two-clean-build
determinism check. The PRX import-contract/Packager/finalize/make_fself
pipeline is NOT used here — that lineage is for ploader in-game modules;
this payload is dispatched by the payload manager as a process.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]
assert WHAT.name == "what", WHAT
SRC = HERE / "src" / "main.c"

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
NM = LLVM / "llvm-nm"
READELF = LLVM / "llvm-readelf"
OBJDUMP = LLVM / "llvm-objdump"

LOG_PATH = "/data/modbridge-argv-probe.jsonl"
MODULE = "ARGV_ECHO"
CFLAGS = ["-O1", "-Wall", "-Wextra", "-Werror",
          # lld's default GNU build-id hashes the output path (a/ vs b/),
          # which broke two-clean-build byte determinism; our identity is
          # the ARGV_BUILD_ID string already embedded in the record.
          "-Wl,--build-id=none"]

FORBIDDEN = ["dlopen", "dlsym", "dlclose", "__dlopen", "__dlclose", "__dlsym",
             "sceKernelGetModuleList", "sceKernelGetModuleInfo",
             "sceKernelGetExecutableModuleHandle"]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(argv, receipt, allow_fail=False):
    env = dict(os.environ)
    env["LLVM_CONFIG"] = str(LLVM / "llvm-config")
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                       env=env)
    receipt.append({"argv": [str(a) for a in argv], "rc": r.returncode,
                    "stdout": r.stdout[-4000:], "stderr": r.stderr[-4000:]})
    if r.returncode != 0 and not allow_fail:
        print(json.dumps(receipt[-1], indent=1))
        sys.exit(f"FAILED: {argv}")
    return r


def compile_one(tag: Path, build_id: str, receipts) -> Path:
    tag.mkdir(parents=True, exist_ok=True)
    elf = tag / f"{MODULE}.elf"
    run([CLANG] + CFLAGS +
        [f"-DARGV_BUILD_ID=\"{build_id}\"", f"-DARGV_LOG_PATH=\"{LOG_PATH}\"",
         "-o", elf, SRC], receipts)
    return elf


def main() -> None:
    receipts = []
    for tool in (CLANG, NM):
        if not tool.exists():
            sys.exit(f"missing tool: {tool}")
    dumper = READELF if READELF.exists() else OBJDUMP
    if not dumper.exists():
        sys.exit("no readelf/objdump available for the import proof")

    src_sha = sha256(SRC)
    seed = hashlib.sha256(src_sha.encode() + LOG_PATH.encode()
                          + b"argv-echo-1").hexdigest()[:12]
    build_id = f"argv-{seed}"
    out = HERE / f"build-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")

    # Two independent clean compiles -> determinism proof.
    elf_a = compile_one(out / "a", build_id, receipts)
    elf_b = compile_one(out / "b", build_id, receipts)
    sha_a, sha_b = sha256(elf_a), sha256(elf_b)
    deterministic = sha_a == sha_b
    if not deterministic:
        print(f"NONDETERMINISTIC BUILD: {sha_a} != {sha_b}")

    final = out / f"{MODULE}.elf"
    shutil.copyfile(elf_a, final)
    (out / "determinism").mkdir(exist_ok=True)
    shutil.copyfile(elf_b, out / "determinism" / f"{MODULE}_b.elf")

    # Import proof: dynamic section + dynamic symbol table of the final ELF.
    proof = run([dumper, "-d", "--dyn-symbols", final], receipts)
    (out / "import-proof.txt").write_text(proof.stdout)
    proof_lines = proof.stdout.splitlines()
    dynsym = [l.split().pop() for l in proof_lines
              if "UND" in l and ("FUNC" in l or "NOTYPE" in l or "OBJECT" in l)]
    bad = sorted({s.split("@")[0] for s in dynsym} & set(FORBIDDEN))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: forbidden imports present: {bad}")
    run([NM, final], receipts)  # receipted; surface full symbol table

    # Loud artifact gate.
    run(["python3", HERE / "check_argv_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.argvprobe.build",
        "schema": 1,
        "build_id": build_id,
        "route": "GET /loadpayload:<path> (pldmgr), standalone SDK ELF",
        "source": str(SRC), "source_sha256": src_sha,
        "log_path": LOG_PATH,
        "record_contract": {"marker": "modbridge.argvprobe", "schema": 1,
                            "argc": "recorded verbatim from main()",
                            "argv": "JSON-escaped, <=64 entries, <=64B/entry",
                            "append_mode": True,
                            "determinism": "no timestamps/randomness"},
        "commands": receipts,
        "final_elf": str(final),
        "final_sha256": sha_a,
        "final_bytes": final.stat().st_size,
        "determinism": {"second_build": str(out / 'determinism'),
                        "second_sha256": sha_b,
                        "identical": deterministic},
        "import_proof": str(out / "import-proof.txt"),
        "undefined_dynsym": sorted(dynsym),
        "forbidden_symbol_gate": {"names": FORBIDDEN, "status": "passed" if not bad else "FAILED"},
        "native_executed": False,
        "console_contact": False,
        "notes": "Local packaging only. Console dispatch/collection is the "
                 "controller's job per PROBE-PLAN.md."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("build_id", "final_elf", "final_sha256", "final_bytes",
                       "determinism", "forbidden_symbol_gate")}))


if __name__ == "__main__":
    main()
