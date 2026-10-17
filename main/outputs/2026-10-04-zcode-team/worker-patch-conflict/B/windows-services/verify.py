#!/usr/bin/env python3
"""Run local Microsoft x64 ABI calls and compile the same services for PS5 via LLVM IR."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SERVICES = ("memcpy", "memmove", "memset", "memcmp", "strlen", "strcmp",
            "wcslen", "wcscmp", "wcsncmp", "wcscpy",
            "memchr", "strchr", "strrchr", "strstr")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def data_layout(ir):
    return re.search(r'^target datalayout = "([^"]+)"$', ir, re.M).group(1)


def probe_layout(ir):
    line = next(line for line in ir.splitlines() if line.startswith("@mb_win_crt_layout ="))
    return list(map(int, re.findall(r"i64 (\d+)", line)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=HERE / "build")
    parser.add_argument("--host-only", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    receipt = {
        "status": "IN_PROGRESS", "commands": [], "host_abi_calls_executed": False,
        "pe_code_executed": False, "ps5_code_executed": False, "console_contact": False,
        "source_files": {str(p.relative_to(HERE)): sha(p) for p in
                         [HERE / "mb_win_crt.c", HERE / "mb_win_crt.h",
                          HERE / "mb_win_crt_bind.h", HERE / "tests/test_win_crt.c",
                          HERE / "tests/abi_probe.c", HERE / "tests/check_nonvolatile.S",
                          Path(__file__).resolve()]},
        "pe_contract_sha256": sha(HERE / "../windows-image/pe_image.h"),
    }

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, argv, env=None, expect_failure=False, allow_failure=False):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (label + ".stdout")).write_text(result.stdout)
        (out / (label + ".stderr")).write_text(result.stderr)
        receipt["commands"].append({"label": label, "argv": argv, "exit": result.returncode})
        save()
        if expect_failure:
            if result.returncode == 0:
                raise RuntimeError(label + " unexpectedly accepted the invalid architecture")
        elif result.returncode and not allow_failure:
            raise RuntimeError(label + " failed; see " + str(out / (label + ".stderr")))
        return result

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffreestanding", "-fno-builtin"]
        if platform.system() == "Darwin":
            host_arch = ["-arch", "x86_64"]
            runner = ["/usr/bin/arch", "-x86_64"]
        elif platform.machine().lower() in ("x86_64", "amd64"):
            host_arch, runner = [], []
        else:
            raise RuntimeError("Actual x86_64 execution is required; on macOS Rosetta may provide it")
        run("host-compile", [args.host_cc, *host_arch, *flags, HERE / "mb_win_crt.c",
                             HERE / "tests/test_win_crt.c", HERE / "tests/check_nonvolatile.S",
                             "-o", out / "test-win-crt"])
        receipt["host_result"] = run("host-call-tests", [*runner, out / "test-win-crt"]).stdout.strip()
        receipt["host_abi_calls_executed"] = True
        # A supported host compiler must reject callable services for ARM64.
        if platform.system() == "Darwin":
            rejection = run("wrong-architecture", [args.host_cc, "-arch", "arm64", *flags,
                "-c", HERE / "mb_win_crt.c", "-o", out / "must-not-exist.o"], expect_failure=True)
            if "Windows CRT services require an AMD64 target" not in rejection.stderr:
                raise RuntimeError("Wrong-architecture failure did not reach the explicit architecture guard")
            receipt["wrong_architecture_rejected"] = True
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            native_flags = [*flags, "-fPIC", "-march=x86-64"]
            bridge_flags = [*native_flags, "--target=x86_64-unknown-freebsd9", "-fshort-wchar"]
            direct = run("direct-ps5-c-frontend", [compiler, *native_flags, "-c",
                HERE / "mb_win_crt.c", "-o", out / "direct-services.o"], env, allow_failure=True)
            if direct.returncode and "'ms_abi' calling convention is not supported for this target" not in direct.stderr:
                raise RuntimeError("Direct C frontend failed for an unexpected reason")
            run("ps5-layout", [compiler, *native_flags, "-S", "-emit-llvm",
                HERE / "tests/abi_probe.c", "-o", out / "ps5-layout.ll"], env)
            run("bridge-layout", [compiler, *bridge_flags, "-S", "-emit-llvm",
                HERE / "tests/abi_probe.c", "-o", out / "bridge-layout.ll"], env)
            native_layout = (out / "ps5-layout.ll").read_text()
            bridge_layout = (out / "bridge-layout.ll").read_text()
            if data_layout(native_layout) != data_layout(bridge_layout):
                raise RuntimeError("Frontend LLVM data layouts disagree")
            if probe_layout(native_layout) != probe_layout(bridge_layout):
                raise RuntimeError("Frontend scalar/import structure layouts disagree")
            expected = [8, 8, 8, 4, 16, 8, 0, 8, 48, 8, 0, 16, 32, 36, 38, 40, 2, 2]
            if probe_layout(native_layout) != expected:
                raise RuntimeError("Unexpected native scalar/import structure layout")
            run("ps5-binder-consumer", [compiler, *native_flags, "-c",
                HERE / "tests/abi_probe.c", "-o", out / "binder-consumer.o"], env)
            ir_path = out / "mb_win_crt.ll"
            run("services-bridge-ir", [compiler, *bridge_flags, "-S", "-emit-llvm",
                "-MD", "-MF", out / "mb_win_crt.d", HERE / "mb_win_crt.c", "-o", ir_path], env)
            ir = ir_path.read_text()
            if data_layout(ir) != data_layout(native_layout):
                raise RuntimeError("Service IR data layout differs from PS5")
            for name in SERVICES:
                if not re.search(r"^define win64cc [^\n]*@mb_win_" + name + r"\(", ir, re.M):
                    raise RuntimeError("Missing explicit win64cc definition for " + name)
            binder = next(line for line in ir.splitlines() if "@mb_win_crt_bind(" in line and line.startswith("define "))
            if "win64cc" in binder:
                raise RuntimeError("Binder must retain the native C calling convention")
            obj = out / "mb_win_crt.o"
            # Target override is intentional and checked above. The IR is unedited:
            # Clang's PS5 C frontend rejects ms_abi; its LLVM backend accepts win64cc.
            run("services-ps5-ir-backend", [compiler, "-x", "ir", "-O2", "-fPIC",
                "-Werror", "-Wno-override-module", "-c", ir_path, "-o", obj], env)
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise RuntimeError("Expected little-endian AMD64 ELF ET_REL")
            undefined = run("native-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj]).stdout.strip()
            if undefined:
                raise RuntimeError("Unexpected service object dependencies: " + undefined)
            run("native-disassembly", [llvm / "llvm-objdump", "-d", obj])
            run("native-header", [llvm / "llvm-readobj", "--file-headers", "--relocations", obj])
            receipt["native"] = {
                "build_route": "FreeBSD9 C frontend with ms_abi -> unedited win64cc IR -> PS5 LLVM backend",
                "direct_ps5_c_frontend_supported": direct.returncode == 0,
                "data_layout": data_layout(ir), "scalar_and_import_layout": expected,
                "object_sha256": sha(obj), "object_bytes": len(data), "undefined_symbols": [],
                "ir_sha256": sha(ir_path), "compiler_wrapper_sha256": sha(compiler),
                "clang_sha256": sha(llvm / "clang"),
            }
            receipt["native_dependencies"] = {}
            for word in (out / "mb_win_crt.d").read_text().replace("\\\n", " ").split()[1:]:
                dep = Path(word)
                if dep.is_file():
                    receipt["native_dependencies"][str(dep)] = sha(dep)
        receipt["status"] = "HOST_ABI_AND_PS5_IR_OBJECT_PASSED" if not args.host_only else "HOST_ABI_PASSED"
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}, indent=2))
    except Exception as error:
        receipt["status"] = "FAILED"
        receipt["error"] = str(error)
        save()
        raise


if __name__ == "__main__":
    main()
