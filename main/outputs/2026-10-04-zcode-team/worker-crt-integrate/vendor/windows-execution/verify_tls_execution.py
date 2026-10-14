#!/usr/bin/env python3
"""Execute the owned dynamic-TLS PE on x86_64; build its typed bridge with the PS5 IR backend."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SERVICES = HERE.parent / "windows-services"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def data_layout(ir):
    return next(line for line in ir.splitlines() if line.startswith("target datalayout ="))


def layout_values(ir):
    line = next(line for line in ir.splitlines() if line.startswith("@mb_tls_bridge_layout ="))
    return list(map(int, re.findall(r"i64 (\d+)", line)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--lld-link", type=Path, default=ROOT / ".superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/tls-pe-runtime-1")
    parser.add_argument("--host-only", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / "pe_module.c", HERE / "pe_module.h", HERE / "tests/tls_fixture.c",
        HERE / "tests/run_tls_fixture.c", HERE / "tests/kernel_tls.def", HERE / "tests/crt_errno.def",
        HERE / "tests/crt_heap_string.def", Path(__file__).resolve(),
        HERE.parent / "windows-image/pe_image.c", HERE.parent / "windows-image/pe_image.h"]
    sources += [SERVICES / name for name in ["mb_win_crt.c", "mb_win_crt.h", "mb_win_crt_bind.h",
        "mb_win_heap.c", "mb_win_heap.h", "mb_win_heap_bridge.c", "mb_win_heap_bridge.h",
        "mb_win_threads.c", "mb_win_threads.h", "mb_win_tls_bridge.c", "mb_win_tls_bridge.h", "tests/test_tls_bridge.c", "tests/tls_bridge_layout.c"]]
    receipt = {"status": "IN_PROGRESS", "commands": [],
        "source_files": {str(path.relative_to(ROOT)): sha(path) for path in sources},
        "downloaded_dll_executed": False, "console_contact": False, "ps5_execution": False,
        "red_phase": "Stub TLS bridge failed first successful begin assertion before implementation"}

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
        service_c = [SERVICES / name for name in ["mb_win_crt.c", "mb_win_heap.c", "mb_win_heap_bridge.c", "mb_win_tls_bridge.c", "mb_win_threads.c"]]
        run("compile-bridge-session-checks", [args.host_cc, "-arch", "x86_64", *flags, "-pthread",
            *service_c, SERVICES / "tests/test_tls_bridge.c", "-o", out / "test-bridge"])
        receipt["bridge_checks"] = run("bridge-session-checks", ["/usr/bin/arch", "-x86_64", out / "test-bridge"]).strip()
        libs = []
        for name, definition in [("tls", "kernel_tls.def"), ("errno", "crt_errno.def"), ("heap_string", "crt_heap_string.def")]:
            lib = out / (name + ".lib")
            run(name + "-import-library", [args.lld_link, "/lib", "/def:" + str(HERE / "tests" / definition),
                "/machine:x64", "/out:" + str(lib)])
            libs.append(lib)
        run("compile-owned-tls-pe", [args.llvm_bin / "clang", "--target=x86_64-pc-windows-msvc", *flags,
            "-fno-stack-protector", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
            "-c", HERE / "tests/tls_fixture.c", "-o", out / "fixture.obj"])
        run("link-owned-tls-pe", [args.lld_link, "/dll", "/noentry", "/nodefaultlib", "/align:16384",
            "/dynamicbase", "/nxcompat", "/timestamp:0", "/out:" + str(out / "tls_fixture.dll"),
            out / "fixture.obj", *libs])
        run("independent-pe-read", [args.llvm_bin / "llvm-readobj", "--file-headers", "--sections",
            "--coff-imports", "--coff-exports", "--coff-basereloc", out / "tls_fixture.dll"])
        run("compile-host-tls-runtime", [args.host_cc, "-arch", "x86_64", *flags, "-pthread",
            HERE.parent / "windows-image/pe_image.c", *service_c,
            HERE / "pe_module.c", HERE / "tests/run_tls_fixture.c", "-o", out / "run-tls"])
        result = json.loads(run("execute-owned-tls-pe", ["/usr/bin/arch", "-x86_64", out / "run-tls", out / "tls_fixture.dll"]))
        expected = {"owned_pe_executed": True, "tls_imports_called": 6, "composed_imports_called": 2,
            "thread_values_isolated": True, "reuse_clears_existing_thread": True, "last_error_effects_passed": True,
            "cleanup_pin_lifetime_passed": True, "unloaded": True, "console_contact": False}
        if result != expected:
            raise RuntimeError("Unexpected TLS fixture result: " + repr(result))
        receipt["result"] = result
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            native_flags = [*flags, "-O2", "-fPIC", "-march=x86-64"]
            bridge_flags = [*native_flags, "--target=x86_64-unknown-freebsd9", "-fshort-wchar"]
            probe = SERVICES / "tests/tls_bridge_layout.c"
            run("native-layout", [compiler, *native_flags, "-S", "-emit-llvm", probe, "-o", out / "native-layout.ll"], env)
            run("bridge-layout", [compiler, *bridge_flags, "-S", "-emit-llvm", probe, "-o", out / "bridge-layout.ll"], env)
            native_ir = (out / "native-layout.ll").read_text()
            bridge_ir = (out / "bridge-layout.ll").read_text()
            layouts = layout_values(native_ir)
            if data_layout(native_ir) != data_layout(bridge_ir) or layouts != layout_values(bridge_ir) or len(layouts) != 24 or layouts[:4] != [8, 8, 4, 4]:
                raise RuntimeError("Native PS5 and bridge frontend ABI layouts disagree")
            run("native-header-consumer", [compiler, *native_flags, "-c", probe, "-o", out / "header-consumer.o"], env)
            ir_path, obj = out / "mb_win_tls_bridge.ll", out / "mb_win_tls_bridge.o"
            run("bridge-ir", [compiler, *bridge_flags, "-S", "-emit-llvm", "-MD", "-MF", out / "bridge.d",
                SERVICES / "mb_win_tls_bridge.c", "-o", ir_path], env)
            ir = ir_path.read_text()
            if data_layout(ir) != data_layout(native_ir):
                raise RuntimeError("Bridge IR data layout differs from native PS5")
            definitions = [line for line in ir.splitlines() if line.startswith("define ")]
            expected_thunks = {"tls_get_last_error": ("i32", []), "tls_set_last_error": ("void", ["i32"]),
                "tls_alloc": ("i32", []), "tls_free": ("i32", ["i32"]),
                "tls_get_value": ("ptr", ["i32"]), "tls_set_value": ("i32", ["i32", "ptr"])}
            for name, (result_type, parameters) in expected_thunks.items():
                line = next(line for line in definitions if "@" + name + "(" in line)
                if "win64cc" not in line or not re.search(r"\b" + result_type + r"(?: [a-z_]+)* @" + name + r"\(", line):
                    raise RuntimeError("Missing typed Win64 thunk: " + name)
                args_text = line.split("(" ,1)[1].split(")",1)[0]
                actual = re.findall(r"(?:^|, )(ptr|i64|i32)\b", args_text)
                if actual != parameters:
                    raise RuntimeError("Wrong Win64 thunk parameters: " + name)
            if sum("win64cc" in line for line in definitions) != 6:
                raise RuntimeError("Expected exactly six Microsoft ABI definitions")
            for name in ["mb_win_tls_bridge_begin", "mb_win_tls_bridge_end", "mb_win_tls_bridge_bind", "mb_win_runtime_bind"]:
                if "win64cc" in next(line for line in definitions if "@" + name + "(" in line):
                    raise RuntimeError("Public control/binder must keep native ABI")
            run("ps5-ir-backend", [compiler, "-x", "ir", "-O2", "-fPIC", "-Werror", "-Wno-override-module", "-c", ir_path, "-o", obj], env)
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise RuntimeError("Expected AMD64 ELF64LE ET_REL")
            undefined = run("native-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", obj])
            names = sorted(line.split()[0] for line in undefined.splitlines() if line.strip())
            allowed = sorted(["mb_win_process_bind", "mb_win_threads_acquire", "mb_win_threads_release_pin",
                "mb_win_threads_last_error", "mb_win_threads_tls_alloc", "mb_win_threads_tls_free",
                "mb_win_threads_tls_get", "mb_win_threads_tls_set"])
            if names != allowed:
                raise RuntimeError("Unexpected bridge dependencies: " + repr(names))
            run("native-disassembly", [llvm / "llvm-objdump", "-d", obj])
            run("native-headers", [llvm / "llvm-readobj", "--file-headers", "--relocations", obj])
            receipt["native"] = {"build_route": "FreeBSD9 C frontend -> unedited win64cc IR -> PS5 LLVM backend",
                "data_layout": data_layout(ir), "structure_layout": layouts, "object_sha256": sha(obj),
                "object_bytes": len(data), "undefined_symbols": names, "ir_sha256": sha(ir_path),
                "compiler_wrapper_sha256": sha(compiler), "clang_sha256": sha(llvm / "clang")}
            receipt["native_dependencies"] = {}
            for word in (out / "bridge.d").read_text().replace("\\\n", " ").split()[1:]:
                dep = Path(word)
                if dep.is_file():
                    receipt["native_dependencies"][str(dep)] = sha(dep)
        for name, digest in receipt["source_files"].items():
            if sha(ROOT / name) != digest:
                raise RuntimeError("Source changed during verification: " + name)
        receipt.update(status="OWNED_TLS_PE_HOST_AND_PS5_BRIDGE_OBJECT_PASSED" if not args.host_only else "OWNED_TLS_PE_HOST_PASSED",
            fixture={"bytes": (out / "tls_fixture.dll").stat().st_size, "sha256": sha(out / "tls_fixture.dll")},
            host_runner_sha256=sha(out / "run-tls"),
            scope="Owned no-entry PE invokes six Win64 dynamic TLS/last-error services plus composed errno/strlen; actual local host pthread execution only; PS5 bridge object is compile/ABI evidence")
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}))
    except Exception as error:
        receipt.update(status="FAILED", error=str(error))
        save()
        raise


if __name__ == "__main__":
    main()
