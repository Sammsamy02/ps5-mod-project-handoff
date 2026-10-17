#!/usr/bin/env python3
"""Verify portable PE admission, entry lifecycle and ownership; compile its PS5 TU.

The host tests use caller-owned data storage and never execute PE instructions.
The separate verify_execution.py owns the locally built PE execution fixture.
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
IMAGE = HERE.parent / "windows-image"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/windows-execution-adapter-1")
    parser.add_argument("--host-only", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    receipt = {
        "status": "IN_PROGRESS", "commands": [], "sources": {},
        "host_test_executed": False, "pe_instructions_executed": False,
        "target_object_executed": False, "console_contact": False,
    }

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
        sources = [HERE / "pe_module.c", HERE / "pe_module.h", HERE / "tests/test_pe_module.c",
                   Path(__file__).resolve(), IMAGE / "pe_image.c", IMAGE / "pe_image.h"]
        receipt["sources"] = {str(path.relative_to(ROOT)): sha(path) for path in sources}
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror"]
        source_args = [IMAGE / "pe_image.c", HERE / "pe_module.c", HERE / "tests/test_pe_module.c"]
        run("host-compile", [args.host_cc, *flags, *source_args, "-o", out / "test-module"])
        receipt["host_result"] = run("host-test", [out / "test-module"])
        receipt["host_test_executed"] = True
        receipt["entry_lifecycle_tested"] = True
        if args.sanitize:
            run("sanitized-compile", [args.host_cc, *flags, "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", *source_args, "-o", out / "test-module-sanitized"])
            receipt["sanitizer_result"] = run("sanitized-test", [out / "test-module-sanitized"])
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            obj = out / "pe_module.o"
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            run("target-compile", [compiler, *flags, "-O2", "-fPIC", "-MD", "-MF", out / "pe_module.d",
                "-c", HERE / "pe_module.c", "-o", obj], env)
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise RuntimeError("Target object must be AMD64 ELF ET_REL")
            undefined = run("target-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj])
            names = {line.split()[0] for line in undefined.splitlines() if len(line.split()) >= 2}
            allowed = {"mb_pe_inspect", "mb_pe_prepare", "mb_pe_visit_exports", "memcmp", "memcpy", "memset"}
            if not names <= allowed:
                raise RuntimeError("Unexpected target imports: " + repr(sorted(names - allowed)))
            receipt["target_compile"] = {"method": "direct C frontend", "compiler_sha256": sha(compiler),
                "object_sha256": sha(obj), "bytes": len(data), "imports": sorted(names)}
            dependencies = (out / "pe_module.d").read_text().replace("\\\n", " ").split()[1:]
            receipt["target_dependencies"] = {name: sha(name) for name in dependencies if Path(name).is_file()}
        receipt["status"] = "ADAPTER_HOST_TESTS_PASSED" if args.host_only else "ADAPTER_HOST_TESTS_AND_TARGET_COMPILE_PASSED"
        save()
        print(json.dumps({"receipt": str(out / "receipt.json"), "status": receipt["status"]}, indent=2))
    except Exception as error:
        receipt["status"] = "FAILED"
        receipt["error"] = str(error)
        save()
        raise


if __name__ == "__main__":
    main()
