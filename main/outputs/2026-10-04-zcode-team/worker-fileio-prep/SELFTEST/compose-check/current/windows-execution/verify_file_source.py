#!/usr/bin/env python3
"""Check actual immutable file intake on the host and compile the native PS5 adapter."""
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
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/pe-file-source-1")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / name for name in ["pe_file_source.h", "pe_file_source.c", "pe_loader.h",
        "pe_dependencies.h", "pe_module.h", "tests/test_pe_file_source.c"]]
    sources += [HERE.parent / "windows-image/pe_image.h", Path(__file__).resolve()]
    receipt = {"status": "IN_PROGRESS", "commands": [],
        "source_files": {str(p.relative_to(ROOT)): sha(p) for p in sources},
        "red_phase": "Stub failed first actual file acquisition assertion, exit1",
        "console_contact": False, "ps5_execution": False, "pe_instructions_executed": False}

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, command, env=None):
        command = list(map(str, command))
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True, timeout=60)
        (out / (label + ".stdout")).write_text(completed.stdout)
        (out / (label + ".stderr")).write_text(completed.stderr)
        receipt["commands"].append({"label": label, "argv": command, "exit": completed.returncode})
        save()
        if completed.returncode:
            raise RuntimeError(f"{label} failed: {out / (label + '.stderr')}")
        return completed.stdout

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-ffreestanding", "-fno-builtin"]
        code = [HERE / "pe_file_source.c", HERE / "tests/test_pe_file_source.c"]
        run("host-build", [args.host_cc, *flags, *code, "-o", out / "test"])
        receipt["host_result"] = run("host-file-intake", [out / "test"]).strip()
        run("sanitized-build", [args.host_cc, *flags, "-fsanitize=address,undefined", *code, "-o", out / "test-san"])
        run("sanitized-file-intake", [out / "test-san"])
        compiler = args.sdk.resolve() / "bin/prospero-clang"
        llvm = args.llvm_bin.resolve()
        env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
        obj = out / "pe_file_source.o"
        run("native-compile", [compiler, *flags, "-O2", "-fPIC", "-MD", "-MF", out / "native.d",
            "-c", HERE / "pe_file_source.c", "-o", obj], env)
        data = obj.read_bytes()
        if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
            raise RuntimeError("Expected native AMD64 ELF64LE ET_REL")
        undefined = run("native-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj])
        names = sorted(line.split()[0] for line in undefined.splitlines() if line.strip())
        if names != sorted(["__error", "close", "fstat", "malloc", "free", "open", "read"]):
            raise RuntimeError("Unexpected native dependencies: " + repr(names))
        run("native-headers", [llvm / "llvm-readobj", "--file-headers", "--relocations", obj])
        receipt["native"] = {"object_bytes": len(data), "object_sha256": sha(obj),
            "undefined_symbols": names, "compiler_wrapper_sha256": sha(compiler), "clang_sha256": sha(llvm / "clang")}
        receipt["native_dependencies"] = {}
        for word in (out / "native.d").read_text().replace("\\\n", " ").split()[1:]:
            dep = Path(word)
            if dep.is_file():
                receipt["native_dependencies"][str(dep)] = sha(dep)
        for name, digest in receipt["source_files"].items():
            if sha(ROOT / name) != digest:
                raise RuntimeError("Source changed during verification: " + name)
        receipt.update(status="FILE_INTAKE_HOST_AND_PS5_OBJECT_PASSED",
            scope="Actual regular-file immutable snapshot intake and explicit refusal checks; host and ASan/UBSan pass; PS5 object compiled only, system imports not resolved in game")
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}))
    except Exception as error:
        receipt.update(status="FAILED", error=str(error))
        save()
        raise


if __name__ == "__main__":
    main()
