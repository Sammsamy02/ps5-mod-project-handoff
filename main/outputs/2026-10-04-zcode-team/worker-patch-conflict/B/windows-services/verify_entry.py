#!/usr/bin/env python3
"""Check native-to-Windows entry calls and build the bridge with the PS5 IR backend."""
import argparse
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import uuid

from verify import HERE, ROOT, data_layout, sha


def layout_values(ir):
    line = next(line for line in ir.splitlines() if line.startswith("@mb_win_entry_layout ="))
    return list(map(int, re.findall(r"i64 (\d+)", line)))


def check_indirect_call(ir):
    definition = next(line for line in ir.splitlines() if line.startswith("define ") and "@mb_win_call_entry(" in line)
    if "win64cc" in definition or not re.search(r"\bi32 @mb_win_call_entry\(", definition):
        raise RuntimeError("Entry bridge must expose native C ABI and return i32")
    parameters = re.findall(r"%[-a-zA-Z0-9_.]+", definition)
    if len(parameters) != 5:
        raise RuntimeError("Expected the five native callback parameters")
    calls = [line for line in ir.splitlines() if re.search(r"\bcall\b", line)]
    if len(calls) != 1:
        raise RuntimeError("Bridge must make exactly one IR call")
    call = calls[0]
    expected = [parameters[1], parameters[2], parameters[3], parameters[4]]
    # The first % identifier is the result, followed by callee and three args.
    identifiers = re.findall(r"%[-a-zA-Z0-9_.]+", call)
    if "call win64cc" not in call or identifiers[1:] != expected:
        raise RuntimeError("Indirect Windows call lost its ABI or argument order")
    if not re.search(r"\bi32 %[-a-zA-Z0-9_.]+\(ptr [^,]+, i32 [^,]+, ptr [^)]+\)", call):
        raise RuntimeError("Indirect entry signature must be i32 (ptr, i32, ptr)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=HERE / "build")
    parser.add_argument("--host-only", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / ("entry-" + uuid.uuid4().hex)
    out.mkdir(parents=True)
    receipt = {
        "status": "IN_PROGRESS", "commands": [], "host_entry_calls_executed": False,
        "pe_code_executed": False, "ps5_code_executed": False, "console_contact": False,
        "source_files": {str(p.relative_to(HERE)): sha(p) for p in [
            HERE / "mb_win_entry.c", HERE / "mb_win_entry.h", HERE / "tests/entry_calls.c",
            HERE / "tests/entry_layout.c", HERE / "verify.py", Path(__file__).resolve()]},
    }

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, argv, env=None, allow_failure=False):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (label + ".stdout")).write_text(result.stdout)
        (out / (label + ".stderr")).write_text(result.stderr)
        receipt["commands"].append({"label": label, "argv": argv, "exit": result.returncode})
        save()
        if result.returncode and not allow_failure:
            raise RuntimeError(label + " failed; see " + str(out / (label + ".stderr")))
        return result

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffreestanding", "-fno-builtin"]
        if platform.system() == "Darwin":
            host_arch, runner = ["-arch", "x86_64"], ["/usr/bin/arch", "-x86_64"]
        elif platform.machine().lower() in ("x86_64", "amd64"):
            host_arch, runner = [], []
        else:
            raise RuntimeError("Actual x86_64 execution is required")
        run("entry-host-compile", [args.host_cc, *host_arch, *flags, HERE / "mb_win_entry.c",
            HERE / "tests/entry_calls.c", "-o", out / "test-entry"])
        receipt["host_result"] = run("entry-host-calls", [*runner, out / "test-entry"]).stdout.strip()
        receipt["host_entry_calls_executed"] = True
        if platform.system() == "Darwin":
            rejected = run("entry-wrong-architecture", [args.host_cc, "-arch", "arm64", *flags,
                "-c", HERE / "mb_win_entry.c", "-o", out / "wrong-arch.o"], allow_failure=True)
            if rejected.returncode == 0 or "Windows entry bridge requires an AMD64 target" not in rejected.stderr:
                raise RuntimeError("Entry architecture guard did not reject ARM64")
            receipt["wrong_architecture_rejected"] = True
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            native_flags = [*flags, "-fPIC", "-march=x86-64"]
            bridge_flags = [*native_flags, "--target=x86_64-unknown-freebsd9", "-fshort-wchar"]
            run("entry-native-layout", [compiler, *native_flags, "-S", "-emit-llvm",
                HERE / "tests/entry_layout.c", "-o", out / "native-layout.ll"], env)
            run("entry-bridge-layout", [compiler, *bridge_flags, "-S", "-emit-llvm",
                HERE / "tests/entry_layout.c", "-o", out / "bridge-layout.ll"], env)
            native_ir = (out / "native-layout.ll").read_text()
            bridge_ir = (out / "bridge-layout.ll").read_text()
            expected = [8, 8, 4, 4, 8, 4]
            if data_layout(native_ir) != data_layout(bridge_ir) or layout_values(native_ir) != expected or layout_values(bridge_ir) != expected:
                raise RuntimeError("Frontend entry layouts disagree")
            run("entry-native-consumer", [compiler, *native_flags, "-c",
                HERE / "tests/entry_layout.c", "-o", out / "entry-consumer.o"], env)
            ir_path, obj = out / "mb_win_entry.ll", out / "mb_win_entry.o"
            run("entry-bridge-ir", [compiler, *bridge_flags, "-S", "-emit-llvm", "-MD", "-MF",
                out / "mb_win_entry.d", HERE / "mb_win_entry.c", "-o", ir_path], env)
            ir = ir_path.read_text()
            if data_layout(ir) != data_layout(native_ir):
                raise RuntimeError("Bridge IR layout differs from PS5")
            check_indirect_call(ir)
            run("entry-ps5-ir-backend", [compiler, "-x", "ir", "-O2", "-fPIC", "-Werror",
                "-Wno-override-module", "-c", ir_path, "-o", obj], env)
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise RuntimeError("Expected little-endian AMD64 ELF ET_REL")
            undefined = run("entry-native-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj]).stdout.strip()
            if undefined:
                raise RuntimeError("Unexpected entry bridge dependencies: " + undefined)
            run("entry-native-disassembly", [llvm / "llvm-objdump", "-d", obj])
            run("entry-native-header", [llvm / "llvm-readobj", "--file-headers", "--relocations", obj])
            receipt["native"] = {
                "build_route": "FreeBSD9 C frontend -> unedited win64cc indirect-call IR -> PS5 LLVM backend",
                "data_layout": data_layout(ir), "scalar_layout": expected,
                "object_sha256": sha(obj), "object_bytes": len(data), "undefined_symbols": [],
                "ir_sha256": sha(ir_path), "compiler_wrapper_sha256": sha(compiler),
                "clang_sha256": sha(llvm / "clang"),
            }
            receipt["native_dependencies"] = {}
            for word in (out / "mb_win_entry.d").read_text().replace("\\\n", " ").split()[1:]:
                dep = Path(word)
                if dep.is_file():
                    receipt["native_dependencies"][str(dep)] = sha(dep)
        receipt["status"] = "ENTRY_HOST_ABI_AND_PS5_IR_OBJECT_PASSED" if not args.host_only else "ENTRY_HOST_ABI_PASSED"
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}, indent=2))
    except Exception as error:
        receipt["status"], receipt["error"] = "FAILED", str(error)
        save()
        raise


if __name__ == "__main__":
    main()
