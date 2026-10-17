"""Rebuild the owned local composition. No network or console operations."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

base = Path(__file__).resolve().parent
root = base.parents[2]
out = base / "build-1"
llvm = root / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
sdk = root / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
lua = root / ".superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include"
environment = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
records = []

def run(argv, **extra):
    result = subprocess.run(argv, env=environment, capture_output=True, text=True)
    records.append(dict(argv=argv, rc=result.returncode, stdout=result.stdout,
                        stderr=result.stderr, **extra))
    (out / "compile-commands.json").write_text(json.dumps(records, indent=2) + "\n")
    if result.returncode != 0:
        raise RuntimeError(result.stderr)

objects = []
for name in ["import_references.S", "probe/probe.c", "probe/startup_input.c",
             "probe/memory_lua.c", "lifecycle/lifecycle.c", "lifecycle/module_entry.c",
             "lifecycle/constructor_boundary.cpp", "lifecycle/sentinels.c",
             "import_check.c", "diagnostic.c"]:
    source = base / name
    target = out / (source.stem + ".o")
    compiler = sdk / "bin" / ("prospero-clang++" if source.suffix == ".cpp" else "prospero-clang")
    run([str(compiler), "-Wall", "-Wextra", "-Werror", "-O1", "-fPIC",
         "-fno-stack-protector", "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
         *["-I" + str(path) for path in [base / "generated", base / "probe", base / "lifecycle", base, lua]],
         "-c", str(source), "-o", str(target)],
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    objects.append(str(target))

full = root / ".superpowers/native-dependency-spike/native-ffield-registration-trace-1/build-1/native-stage27-local-deps.o"
assert hashlib.sha256(full.read_bytes()).hexdigest() == "3cbde16051daf30294dfe23c8aff04980757beb742f8b859ea0eef3f69e67fb9"
run([str(llvm / "ld.lld"), "-r", "-m", "elf_x86_64", "-o",
     str(out / "combined-full.o"), *objects, str(full)])
print(f"Native compilation and full-runtime composition passed: {len(records)} commands")
