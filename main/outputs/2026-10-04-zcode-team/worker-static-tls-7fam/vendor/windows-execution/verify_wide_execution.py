#!/usr/bin/env python3
"""Build and execute only the owned UTF-16 CRT DLL fixture on a local x86_64 Mac.

No downloaded DLL or console execution. Target object compilation is separate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-cc", default="clang")
    parser.add_argument("--llvm-bin", type=Path, default=ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin")
    parser.add_argument("--lld-link", type=Path, default=ROOT / ".superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link")
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/wide-pe-runtime-1")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / "pe_module.c", HERE / "pe_module.h", HERE / "tests/wide_fixture.c",
        HERE / "tests/run_wide_fixture.c", HERE / "tests/crt_wide.def", Path(__file__).resolve(),
        HERE.parent / "windows-services/mb_win_crt.c", HERE.parent / "windows-services/mb_win_crt.h",
        HERE.parent / "windows-services/mb_win_crt_bind.h", HERE.parent / "windows-image/pe_image.c",
        HERE.parent / "windows-image/pe_image.h"]
    receipt = {"status": "IN_PROGRESS", "commands": [],
        "source_files": {str(path.relative_to(ROOT)): sha(path) for path in sources},
        "downloaded_dll_executed": False, "console_contact": False, "ps5_execution": False}

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(label, command):
        command = list(map(str, command))
        completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=60)
        (out / (label + ".stdout")).write_text(completed.stdout)
        (out / (label + ".stderr")).write_text(completed.stderr)
        receipt["commands"].append({"label": label, "argv": command, "exit": completed.returncode})
        save()
        if completed.returncode:
            raise RuntimeError(f"{label} failed: {out / (label + '.stderr')}")
        return completed.stdout

    try:
        run("wide-import-library", [args.lld_link, "/lib", "/def:" + str(HERE / "tests/crt_wide.def"),
            "/machine:x64", "/out:" + str(out / "wide.lib")])
        run("compile-owned-wide-pe", [args.llvm_bin / "clang", "--target=x86_64-pc-windows-msvc",
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-ffreestanding", "-fno-builtin",
            "-fno-stack-protector", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
            "-c", HERE / "tests/wide_fixture.c", "-o", out / "fixture.obj"])
        run("link-owned-wide-pe", [args.lld_link, "/dll", "/noentry", "/nodefaultlib", "/align:16384",
            "/dynamicbase", "/nxcompat", "/timestamp:0", "/out:" + str(out / "wide_fixture.dll"),
            out / "fixture.obj", out / "wide.lib"])
        # 16 KiB section alignment deliberately matches the host protection granule.
        run("independent-pe-read", [args.llvm_bin / "llvm-readobj", "--file-headers", "--sections",
            "--coff-imports", "--coff-exports", "--coff-basereloc", out / "wide_fixture.dll"])
        run("compile-host-wide-runtime", [args.host_cc, "-arch", "x86_64", "-std=c11",
            "-Wall", "-Wextra", "-Werror", "-O1", "-ffreestanding", "-fno-builtin",
            HERE.parent / "windows-image/pe_image.c", HERE.parent / "windows-services/mb_win_crt.c",
            HERE / "pe_module.c", HERE / "tests/run_wide_fixture.c", "-o", out / "run-wide"])
        result = json.loads(run("execute-owned-wide-pe", ["/usr/bin/arch", "-x86_64",
            out / "run-wide", out / "wide_fixture.dll"]))
        expected = {"owned_pe_executed": True, "wide_imports_called": 4, "copied_units": 5,
            "copy": [0x0041, 0xd83d, 0xde80, 0x8000, 0xffff, 0],
            "guarded_bounds_passed": True, "unloaded": True, "console_contact": False}
        if result != expected:
            raise RuntimeError("Unexpected wide fixture result: " + repr(result))
        for name, digest in receipt["source_files"].items():
            if sha(ROOT / name) != digest:
                raise RuntimeError("Source changed during verification: " + name)
        receipt.update(status="OWNED_WIDE_PE_HOST_EXECUTION_PASSED", result=result,
            fixture={"bytes": (out / "wide_fixture.dll").stat().st_size, "sha256": sha(out / "wide_fixture.dll")},
            host_runner_sha256=sha(out / "run-wide"),
            scope="Owned no-entry PE invokes four Microsoft-ABI UTF-16 services, including guarded count bounds; local host execution only")
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}))
    except Exception as error:
        receipt.update(status="FAILED", error=str(error))
        save()
        raise


if __name__ == "__main__":
    main()
