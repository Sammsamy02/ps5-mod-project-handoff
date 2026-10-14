#!/usr/bin/env python3
"""Compile/link native Nexus metadata API; never execute or contact a device."""
import json
import os
from pathlib import Path
import shlex
import subprocess

scope = Path(__file__).resolve().parent
workspace = scope.parents[2]
project = workspace / "outputs/ps5-mod-bridge"
sdk = project / "native-host/build/sdk-v0.43/ps5-payload-sdk"
llvm = project / "native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
resource = workspace / ".superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21"
output = scope / "native"
output.mkdir(exist_ok=True)
env = os.environ.copy()
for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH"):
    env.pop(name, None)
env["LLVM_CONFIG"] = str(llvm / "llvm-config")
common = ["-Wall", "-Wextra", "-Werror", "-Os", "-nostdinc",
          "-resource-dir", str(resource), "-isystem", str(resource / "include"),
          "-isystem", str(sdk / "target/include"),
          "-I", str(project / "native-host"), "-I", str(project / "include"),
          "-isystem", str(project / "third_party")]
sources = [(project / "native-host/tests/nexus_caller.c", "c11", "prospero-clang"),
           (project / "native-host/nexus_bridge.cpp", "c++20", "prospero-clang++"),
           (project / "src/nexus.cpp", "c++20", "prospero-clang++")]
records = []

def run(argv):
    result = subprocess.run(argv, env=env, capture_output=True, text=True, timeout=60)
    records.append({"argv": list(map(str, argv)), "returncode": result.returncode,
                    "stdout": result.stdout, "stderr": result.stderr})
    (output / "commands.json").write_text(json.dumps(records, indent=2)+"\n")
    result.check_returncode()
    return result.stdout

objects = []
for source, standard, compiler in sources:
    obj = output / (source.stem + ".o")
    # libc++'s C-header wrappers must precede compiler and libc C headers.
    cpp_headers = ["-isystem", str(sdk / "target/include/c++/v1")] if standard == "c++20" else []
    run([str(sdk / "bin" / compiler), "-std="+standard, *cpp_headers, *common,
         "-MD", "-MF", str(obj)+".d", "-c", str(source), "-o", str(obj)])
    objects.append(str(obj))

include_paths = set()
for dependency in output.glob("*.o.d"):
    include_paths.update(shlex.split(dependency.read_text().replace("\\\n", " "))[1:])
allowed = (project.resolve(), sdk.resolve(), resource.resolve())
unexpected = [str(Path(p).resolve()) for p in include_paths
              if not any(Path(p).resolve().is_relative_to(root) for root in allowed)]
if unexpected:
    raise RuntimeError("native header fallback: "+", ".join(unexpected))

elf = output / "nexus-metadata-caller.elf"
run([str(sdk / "bin/prospero-clang++"), "-isystem", str(sdk / "target/include/c++/v1"), *common, *objects,
     "-Wl,--unresolved-symbols=report-all", "-Wl,-z,now",
     "-Wl,-z,max-page-size=0x4000", "-o", str(elf)])
imports = run([str(llvm / "llvm-nm"), "--undefined-only", str(elf)])
(output / "imports.txt").write_text(imports)
headers = run([str(llvm / "llvm-readobj"), "--file-headers", "--needed-libs", str(elf)])
(output / "elf-inspection.txt").write_text(headers)
receipt = {"scope": "native metadata module/C ABI compile and link only",
           "target_executed": False, "elf": str(elf), "elf_bytes": elf.stat().st_size,
           "target": "x86_64-sie-ps5", "sdk": "v0.43", "llvm": "21.1.8",
           "include_paths": sorted(include_paths), "unexpected_headers": unexpected,
           "command_exit_codes": [r["returncode"] for r in records],
           "imports": imports.splitlines(), "installation_supported": False}
(output / "receipt.json").write_text(json.dumps(receipt, indent=2)+"\n")
print(json.dumps({k: receipt[k] for k in ("scope", "target_executed", "elf_bytes", "unexpected_headers")}))
