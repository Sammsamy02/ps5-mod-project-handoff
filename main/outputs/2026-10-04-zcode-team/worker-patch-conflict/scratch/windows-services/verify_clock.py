#!/usr/bin/env python3
"""Execute the independent QPC/QPF service on x86_64; build native PS5 objects.

Host controls use both injected failure/boundary samples and the real host clock.
The separate native provider is compiled directly; Win64 thunks retain their ABI
through unedited LLVM IR. No existing binder or candidate is rebuilt or edited.
"""
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
    line = next(x for x in ir.splitlines() if x.startswith("@mb_win_clock_layout ="))
    return list(map(int, re.findall(r"i64 (\d+)", line)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--sdk", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/windows-clock-1")
    parser.add_argument("--host-only", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / "mb_win_clock.c", HERE / "mb_win_clock.h", HERE / "mb_win_clock_native.c",
               HERE / "tests/test_win_clock.c", HERE / "tests/clock_layout.c", HERE / "mb_win_threads.c",
               HERE / "mb_win_threads.h", HERE / "verify.py", Path(__file__).resolve(),
               HERE.parent / "windows-execution/pe_module.h", HERE.parent / "windows-image/pe_image.h"]
    census = ROOT / ".superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/llvm-reference.json"
    rows = json.loads(census.read_text())
    observed = [{"input_index": n, "import": item} for n, row in enumerate(rows) for item in row["imports"]
                if item["library"] == "KERNEL32.dll" and item["name"] in ("QueryPerformanceCounter", "QueryPerformanceFrequency")]
    receipt = {"status": "IN_PROGRESS", "commands": [], "sources": {str(p.relative_to(ROOT)): sha(p) for p in sources},
               "retained_imports": {"path": str(census.relative_to(ROOT)), "sha256": sha(census), "observations": observed},
               "host_win64_calls_executed": False, "real_host_clock_executed": False,
               "pe_instructions_executed": False, "target_objects_executed": False, "console_contact": False,
               "integrated_into_existing_binders_or_candidate": False}

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, argv, env=None):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (label + ".stdout")).write_text(result.stdout)
        (out / (label + ".stderr")).write_text(result.stderr)
        receipt["commands"].append({"label": label, "argv": argv, "exit": result.returncode})
        save()
        if result.returncode:
            raise RuntimeError(label + " failed; see " + str(out / (label + ".stderr")))
        return result.stdout

    def native_object(label, path, allowed, llvm):
        data = path.read_bytes()
        if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
            raise RuntimeError("Expected AMD64 ELF ET_REL")
        undefined = run(label + "-undefined", [llvm / "llvm-nm", "--undefined-only", "--format=posix", path])
        names = {line.split()[0] for line in undefined.splitlines() if line.split()}
        if names != allowed:
            raise RuntimeError("Unexpected " + label + " imports: " + repr(sorted(names)))
        return {"path": str(path), "sha256": sha(path), "bytes": len(data), "imports": sorted(names)}

    try:
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffreestanding", "-fno-builtin"]
        if platform.system() == "Darwin":
            host_arch, runner = ["-arch", "x86_64"], ["/usr/bin/arch", "-x86_64"]
        elif platform.machine().lower() in ("x86_64", "amd64"):
            host_arch, runner = [], []
        else:
            raise RuntimeError("Actual x86_64 host execution is required")
        units = [HERE / "mb_win_clock.c", HERE / "mb_win_clock_native.c", HERE / "mb_win_threads.c", HERE / "tests/test_win_clock.c"]
        run("fault-compile", [args.host_cc, *host_arch, *flags, "-DMB_WIN_CLOCK_TEST", "-pthread", *units, "-o", out / "fault-clock"])
        receipt["fault_result"] = run("fault-calls", [*runner, out / "fault-clock"]).strip()
        receipt["host_win64_calls_executed"] = True
        run("real-clock-compile", [args.host_cc, *host_arch, *flags, "-pthread", *units, "-o", out / "real-clock"])
        receipt["real_clock_result"] = run("real-clock-calls", [*runner, out / "real-clock"]).strip()
        receipt["real_host_clock_executed"] = True
        if args.sanitize:
            run("sanitized-compile", [args.host_cc, *host_arch, *flags, "-DMB_WIN_CLOCK_TEST", "-pthread",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer", *units, "-o", out / "sanitized-clock"])
            receipt["sanitizer_result"] = run("sanitized-calls", [*runner, out / "sanitized-clock"]).strip()
        if not args.host_only:
            compiler = args.sdk.resolve() / "bin/prospero-clang"
            llvm = args.llvm_bin.resolve()
            env = dict(os.environ, LLVM_CONFIG=str(llvm / "llvm-config"))
            native_flags = [*flags, "-fPIC", "-march=x86-64"]
            bridge_flags = [*native_flags, "--target=x86_64-unknown-freebsd9", "-fshort-wchar"]
            run("native-layout", [compiler, *native_flags, "-S", "-emit-llvm", HERE / "tests/clock_layout.c", "-o", out / "native-layout.ll"], env)
            run("bridge-layout", [compiler, *bridge_flags, "-S", "-emit-llvm", HERE / "tests/clock_layout.c", "-o", out / "bridge-layout.ll"], env)
            native_ir, bridge_layout_ir = (out / "native-layout.ll").read_text(), (out / "bridge-layout.ll").read_text()
            layout = layout_values(native_ir)
            if layout[:5] != [8, 8, 8, 8, 4] or layout != layout_values(bridge_layout_ir) or data_layout(native_ir) != data_layout(bridge_layout_ir):
                raise RuntimeError("PS5 and bridge frontend layouts disagree")
            ir_path, bridge_obj, provider_obj = out / "mb_win_clock.ll", out / "mb_win_clock.o", out / "mb_win_clock_native.o"
            run("bridge-ir", [compiler, *bridge_flags, "-S", "-emit-llvm", "-MD", "-MF", out / "bridge.d",
                HERE / "mb_win_clock.c", "-o", ir_path], env)
            ir = ir_path.read_text()
            definitions = [line for line in ir.splitlines() if line.startswith("define ") and "win64cc" in line]
            if len(definitions) != 2 or not all(re.search(r"\bi32 @query_(counter|frequency)\(ptr [^,)]*\)", line) for line in definitions):
                raise RuntimeError("Expected exactly two Windows BOOL(pointer) definitions")
            if data_layout(ir) != data_layout(native_ir):
                raise RuntimeError("Win64 bridge data layout differs from native target")
            run("ps5-ir-backend", [compiler, "-x", "ir", "-O2", "-fPIC", "-Werror", "-Wno-override-module", "-c", ir_path, "-o", bridge_obj], env)
            run("native-provider", [compiler, *native_flags, "-MD", "-MF", out / "provider.d", "-c", HERE / "mb_win_clock_native.c", "-o", provider_obj], env)
            bridge_info = native_object("bridge", bridge_obj, {"mb_win_clock_read_counter", "mb_win_clock_read_frequency",
                "mb_win_threads_acquire", "mb_win_threads_release_pin", "mb_win_threads_last_error"}, llvm)
            provider_info = native_object("provider", provider_obj, {"clock_gettime"}, llvm)
            run("bridge-disassembly", [llvm / "llvm-objdump", "-dr", bridge_obj])
            run("provider-disassembly", [llvm / "llvm-objdump", "-dr", provider_obj])
            stub = args.sdk.resolve() / "target/lib/libkernel_web.so"
            symbols = run("sdk-provider-symbols", [llvm / "llvm-nm", "-D", "--defined-only", "--format=posix", stub])
            symbol = [line for line in symbols.splitlines() if line.split() and line.split()[0] == "clock_gettime"]
            if len(symbol) != 1:
                raise RuntimeError("SDK stub does not declare clock_gettime exactly once")
            dynamic = run("sdk-provider-soname", [llvm / "llvm-readelf", "--dynamic", stub])
            soname = re.search(r"\(SONAME\).*\[([^]]+)\]", dynamic)
            if not soname:
                raise RuntimeError("SDK provider SONAME unavailable")
            receipt["native"] = {"provider": provider_info, "bridge": bridge_info,
                "provider_method": "direct PS5 C frontend", "bridge_method": "FreeBSD9 frontend -> unedited win64cc IR -> PS5 backend",
                "data_layout": data_layout(ir), "layout_values": layout, "ir_sha256": sha(ir_path),
                "compiler_sha256": sha(compiler), "clang_sha256": sha(llvm / "clang"),
                "clock_provider_declaration": {"path": str(stub), "sha256": sha(stub), "soname": soname.group(1),
                    "symbol": symbol[0], "target_runtime_resolution_proven": False}}
            receipt["native_dependencies"] = {}
            for name in ("bridge.d", "provider.d"):
                for word in (out / name).read_text().replace("\\\n", " ").split()[1:]:
                    p = Path(word)
                    if p.is_file():
                        receipt["native_dependencies"][str(p)] = sha(p)
        if receipt["sources"] != {str(p.relative_to(ROOT)): sha(p) for p in sources}:
            raise RuntimeError("Sources changed during verification")
        receipt["sources_stable"] = True
        receipt["status"] = "CLOCK_HOST_WIN64_AND_NATIVE_OBJECTS_PASSED" if not args.host_only else "CLOCK_HOST_WIN64_PASSED"
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}, indent=2))
    except Exception as error:
        receipt["status"], receipt["error"] = "FAILED", str(error)
        save()
        raise


if __name__ == "__main__":
    main()
