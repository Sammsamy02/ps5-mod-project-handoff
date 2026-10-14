#!/usr/bin/env python3
"""Build owned provider/consumer PE DLLs and execute dependency lifetime checks.

Local x86_64 Mac execution only; no downloaded DLLs or console contact.
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
    parser.add_argument("--output", type=Path, default=ROOT / ".superpowers/native-dependency-spike/dependency-pe-runtime-1")
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE / "pe_dependencies.c", HERE / "pe_dependencies.h", HERE / "pe_module.c", HERE / "pe_module.h",
        HERE / "tests/dependency_provider.c", HERE / "tests/dependency_consumer.c", HERE / "tests/dependency_runner.c",
        HERE / "tests/dependency_missing.def", Path(__file__).resolve(),
        HERE.parent / "windows-image/pe_image.c", HERE.parent / "windows-image/pe_image.h"]
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
        compile_flags = ["--target=x86_64-pc-windows-msvc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-O1", "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables"]
        link_flags = ["/dll", "/noentry", "/nodefaultlib", "/align:16384", "/dynamicbase", "/nxcompat", "/timestamp:0"]
        run("compile-provider", [args.llvm_bin / "clang", *compile_flags, "-c",
            HERE / "tests/dependency_provider.c", "-o", out / "provider.obj"])
        run("link-provider", [args.lld_link, *link_flags, "/out:" + str(out / "DependencyProvider.dll"),
            "/implib:" + str(out / "provider.lib"), out / "provider.obj"])
        run("compile-consumer", [args.llvm_bin / "clang", *compile_flags, "-c",
            HERE / "tests/dependency_consumer.c", "-o", out / "consumer.obj"])
        run("link-consumer", [args.lld_link, *link_flags, "/out:" + str(out / "DependencyConsumer.dll"),
            out / "consumer.obj", out / "provider.lib"])
        run("missing-import-library", [args.lld_link, "/lib", "/machine:x64",
            "/def:" + str(HERE / "tests/dependency_missing.def"), "/out:" + str(out / "missing.lib")])
        run("compile-missing-consumer", [args.llvm_bin / "clang", *compile_flags, "-DDEPENDENCY_MISSING_IMPORT",
            "-c", HERE / "tests/dependency_consumer.c", "-o", out / "missing.obj"])
        run("link-missing-consumer", [args.lld_link, *link_flags, "/out:" + str(out / "DependencyMissing.dll"),
            out / "missing.obj", out / "provider.lib", out / "missing.lib"])
        # 16 KiB sections match the explicit local protection granule.
        for name in ("DependencyProvider.dll", "DependencyConsumer.dll", "DependencyMissing.dll"):
            run("read-" + name, [args.llvm_bin / "llvm-readobj", "--file-headers", "--sections",
                "--coff-imports", "--coff-exports", "--coff-basereloc", out / name])
        run("compile-host-dependencies", [args.host_cc, "-arch", "x86_64", "-std=c11",
            "-Wall", "-Wextra", "-Werror", "-O1", "-ffreestanding", "-fno-builtin",
            HERE.parent / "windows-image/pe_image.c", HERE / "pe_module.c", HERE / "pe_dependencies.c",
            HERE / "tests/dependency_runner.c", "-o", out / "run-dependencies"])
        result = json.loads(run("execute-owned-dependencies", ["/usr/bin/arch", "-x86_64", out / "run-dependencies",
            out / "DependencyProvider.dll", out / "DependencyConsumer.dll", out / "DependencyMissing.dll"]))
        expected = {"owned_dependency_pe_executed": True, "consumer_result": 89,
            "busy_pin_retained": True, "failed_release_pin_retained": True,
            "failed_load_pins_rolled_back": True, "failed_load_cleanup_pin_retained": True,
            "all_unloaded": True, "console_contact": False}
        if result != expected:
            raise RuntimeError("Unexpected dependency fixture result: " + repr(result))
        for name, digest in receipt["source_files"].items():
            if sha(ROOT / name) != digest:
                raise RuntimeError("Source changed during verification: " + name)
        receipt.update(status="OWNED_DEPENDENCY_PE_HOST_EXECUTION_PASSED", result=result,
            fixtures={name: {"bytes": (out / name).stat().st_size, "sha256": sha(out / name)}
                for name in ("DependencyProvider.dll", "DependencyConsumer.dll", "DependencyMissing.dll")},
            host_runner_sha256=sha(out / "run-dependencies"),
            scope="Owned no-entry PE consumer calls provider through its IAT; dependency pins survive busy/failed cleanup and roll back after failed load; local host execution only")
        save()
        print(json.dumps({"status": receipt["status"], "receipt": str(out / "receipt.json")}))
    except Exception as error:
        receipt.update(status="FAILED", error=str(error))
        save()
        raise


if __name__ == "__main__":
    main()
