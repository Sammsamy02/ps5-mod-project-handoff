"""Rebuild the observer-revision composition. New-revision only; never touches
the frozen native-full-runtime-aligned-entry-1 build-1.

Adapted from the frozen spike recipe build_local.py with exactly two changes:
  1. source list: probe/observer.c replaces probe/startup_input.c and
     probe/memory_lua.c (the observer loads no startup input and performs no
     memory/Lua access), and the Lua include path is dropped for the same
     reason.
  2. output tree: this worker's build-1/ instead of the frozen build-1/.
Compile flags, LLVM/SDK toolchain, pinned full-runtime object, and the
ld.lld -r invocation are byte-for-byte the frozen recipe's.

Local verification already performed (see receipts/finish-pass.txt): all nine
units compiled -Werror-clean and the ld.lld -r link succeeded; objects remain
in host-test/target-smoke/. This script is the reproducible form.

After this script, package (requires dotnet, currently absent on this Mac):
  cd source/harness && dotnet run -c Release -- \
      <worker-dir>/build-1/combined-full.o \
      <frozen dir>/import-contract.json \
      <worker-dir>/build-1/modbridge_runtime_aligned_observer.prx
The harness Program.cs pins the frozen 191-row contract by sha256 and emits
the PRX with moduleName/exportLibraryName modbridge_runtime_aligned_observer.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess

base = Path(__file__).resolve().parent
root = base.parents[2]  # .../what
out = base / "build-1"
llvm = root / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
sdk = root / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
full = root / (".superpowers/native-dependency-spike/"
               "native-ffield-registration-trace-1/build-1/native-stage27-local-deps.o")
contract = root / (".superpowers/native-dependency-spike/"
                   "native-full-runtime-aligned-entry-1/import-contract.json")
assert hashlib.sha256(full.read_bytes()).hexdigest() == (
    "3cbde16051daf30294dfe23c8aff04980757beb742f8b859ea0eef3f69e67fb9"), "pinned full object moved"
assert hashlib.sha256(contract.read_bytes()).hexdigest() == (
    "58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c"), "pinned contract moved"
environment = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
records = []

def run(argv, **extra):
    result = subprocess.run(argv, env=environment, capture_output=True, text=True)
    records.append(dict(argv=argv, rc=result.returncode, stdout=result.stdout,
                        stderr=result.stderr, **extra))
    out.mkdir(exist_ok=True)
    (out / "compile-commands.json").write_text(json.dumps(records, indent=2) + "\n")
    if result.returncode != 0:
        raise RuntimeError(result.stderr)

objects = []
for name in ["import_references.S", "probe/probe.c", "probe/observer.c",
             "lifecycle/lifecycle.c", "lifecycle/module_entry.c",
             "lifecycle/constructor_boundary.cpp", "lifecycle/sentinels.c",
             "import_check.c", "diagnostic.c"]:
    source = base / "source" / name
    target = out / (source.stem + ".o")
    compiler = sdk / "bin" / ("prospero-clang++" if source.suffix == ".cpp" else "prospero-clang")
    run([str(compiler), "-Wall", "-Wextra", "-Werror", "-O1", "-fPIC",
         "-fno-stack-protector", "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
         *["-I" + str(path) for path in [base / "source" / "generated",
                                         base / "source" / "probe",
                                         base / "source" / "lifecycle",
                                         base / "source"]],
         "-c", str(source), "-o", str(target)],
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    objects.append(str(target))

run([str(llvm / "ld.lld"), "-r", "-m", "elf_x86_64", "-o",
     str(out / "combined-full.o"), *objects, str(full)])
print(f"Observer compilation and full-runtime composition passed: {len(records)} commands")
