#!/usr/bin/env python3
"""Test the native Windows heap owner and directly compile its PS5 C object.

This verifier exercises native C methods only. The separate typed bridge and
self-built DLL fixture own Windows ABI/execution and actual thread evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/windows-heap-owner-1")
    parser.add_argument("--host-only", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / "mb_win_heap.c", HERE / "mb_win_heap.h",
               HERE / "tests/test_win_heap.c", Path(__file__).resolve()]
    receipt = {"status": "IN_PROGRESS", "commands": [],
               "sources": {str(p.relative_to(ROOT)): sha(p) for p in sources},
               "host_native_calls_executed": False, "pe_instructions_executed": False,
               "target_object_executed": False, "actual_threads_tested": False,
               "console_contact": False}

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, command, env=None):
        command = list(map(str, command))
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (label + ".stdout")).write_text(completed.stdout)
        (out / (label + ".stderr")).write_text(completed.stderr)
        receipt["commands"].append({"label": label, "argv": command, "exit": completed.returncode})
        save()
        if completed.returncode:
            raise RuntimeError(f"{label}: see {out / (label + '.stderr')}")
        return completed.stdout.strip()

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffreestanding", "-fno-builtin"]
        source_args = [HERE / "mb_win_heap.c", HERE / "tests/test_win_heap.c"]
        run("host-compile", [args.host_cc, *flags, *source_args, "-o", out / "test-heap"])
        receipt["host_result"] = run("host-test", [out / "test-heap"])
        receipt["host_native_calls_executed"] = True
        if args.sanitize:
            run("sanitized-compile", [args.host_cc, *flags, "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", *source_args, "-o", out / "test-heap-sanitized"])
            receipt["sanitizer_result"] = run("sanitized-test", [out / "test-heap-sanitized"])
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            obj = out / "mb_win_heap.o"
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            run("target-compile", [compiler, *flags, "-fPIC", "-MD", "-MF", out / "mb_win_heap.d",
                "-c", HERE / "mb_win_heap.c", "-o", obj], env)
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise RuntimeError("Target object must be AMD64 ELF ET_REL")
            undefined = run("target-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj])
            if undefined:
                raise RuntimeError("Unexpected heap owner imports: " + undefined)
            receipt["target_compile"] = {"method": "direct PS5 C frontend", "compiler_sha256": sha(compiler),
                "object_sha256": sha(obj), "bytes": len(data), "imports": []}
            dependencies = (out / "mb_win_heap.d").read_text().replace("\\\n", " ").split()[1:]
            receipt["target_dependencies"] = {name: sha(name) for name in dependencies if Path(name).is_file()}
        after = {str(p.relative_to(ROOT)): sha(p) for p in sources}
        if receipt["sources"] != after:
            raise RuntimeError("Source files changed during verification")
        receipt["sources_stable"] = True
        receipt["status"] = "HEAP_OWNER_HOST_PASSED" if args.host_only else "HEAP_OWNER_HOST_AND_PS5_OBJECT_PASSED"
        save()
        print(json.dumps({"receipt": str(out / "receipt.json"), "status": receipt["status"]}, indent=2))
    except Exception as error:
        receipt["status"] = "FAILED"
        receipt["error"] = str(error)
        save()
        raise


if __name__ == "__main__":
    main()
