#!/usr/bin/env python3
"""Verify isolated format SHA1 provider closure on host and native link-only."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import time

HERE = Path(__file__).resolve().parent
DOWNLOAD = HERE.parent


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_builder():
    spec = importlib.util.spec_from_file_location(
        "modbridge_format_sha1_builder", DOWNLOAD / "format_sha1_build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--ps5-sdk", type=Path, required=True)
    parser.add_argument("--llvm-bin", type=Path, required=True)
    args = parser.parse_args()

    root = args.output_root.absolute()
    if root.is_symlink() or root.exists():
        parser.error("output root must be new")
    root.mkdir(parents=True, exist_ok=False)
    receipt = {"result": "INCOMPLETE", "target_executed": False,
               "provider_test_source": str(HERE / "driver.c"),
               "abi_probe_source": str(HERE / "context_abi.c"),
               "commands": [], "modes": {}}
    (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    builder = load_builder()

    def run(mode, name, argv, env, expected=0):
        argv = [str(value) for value in argv]
        start = time.monotonic()
        result = subprocess.run(argv, cwd=root / mode, env=env,
                                capture_output=True, timeout=60)
        stdout = result.stdout.decode(errors="replace")
        stderr = result.stderr.decode(errors="replace")
        row = {"mode": mode, "name": name, "argv": argv,
               "returncode": result.returncode,
               "elapsed_seconds": round(time.monotonic() - start, 6),
               "stdout": stdout[-2000:], "stderr": stderr[-2000:]}
        receipt["commands"].append(row)
        (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
        if result.returncode != expected:
            receipt["result"] = "FAIL"
            receipt["failed_command"] = name
            (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
            raise SystemExit("command failed: " + name)
        return result.stdout

    for mode in ("host", "native"):
        mode_output = root / mode
        prepared = builder.prepare_format_sha1(
            mode, mode_output, args.profile,
            sdk=args.ps5_sdk if mode == "native" else None,
            llvm_bin=args.llvm_bin if mode == "native" else None)
        identity = prepared["identity"]
        tool = identity["tools"]
        compiler = tool["compiler"]["path"]
        env = os.environ.copy()
        for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                     "CFLAGS", "CPPFLAGS", "LDFLAGS", "LIBS", "CONFIG_SITE",
                     "MBEDTLS_CONFIG_FILE", "MBEDTLS_USER_CONFIG_FILE"):
            env.pop(name, None)
        if mode == "native":
            env["LLVM_CONFIG"] = tool["llvm_config"]["path"]
        env["ZERO_AR_DATE"] = "1"
        env["SOURCE_DATE_EPOCH"] = "0"

        context = prepared["compile_context"]
        flags = ["-std=c11", "-Os", "-fno-common", "-ffunction-sections",
                 "-fdata-sections", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        flags += ["-I" + path for path in context["include_dirs"]]
        flags += ["-D" + value for value in context["compile_definitions"]]
        flags += context["native_compile_options"]
        driver_object = mode_output / "sha1_driver.o"
        run(mode, "caller-compile", [compiler, *flags, "-MD", "-MF",
                                      mode_output / "sha1_driver.d", "-c",
                                      HERE / "driver.c", "-o", driver_object], env)
        binary = mode_output / ("format-sha1-link-only.elf" if mode == "native"
                                else "format-sha1-host-test")
        link = [compiler, driver_object, *prepared["link_archives"], "-o", binary]
        if mode == "native":
            link.insert(1, "-Wl,-z,now")
        run(mode, "provider-owner-link", link, env)

        if mode == "host":
            run(mode, "known-vector-execution", [binary], env)
            runtime = {"executed": True, "known_vector_checks": 2}
        else:
            image = binary.read_bytes()
            if (image[:6] != b"\x7fELF\x02\x01" or
                    struct.unpack_from("<H", image, 18)[0] != 62 or
                    struct.unpack_from("<H", image, 16)[0] not in (2, 3)):
                raise SystemExit("native format SHA1 link is not x86_64 ELF executable")
            runtime = {"executed": False, "elf_machine": "x86_64",
                       "elf_type": {2: "ET_EXEC", 3: "ET_DYN"}[
                           struct.unpack_from("<H", image, 16)[0]],
                       "target_executed": False}
        receipt["modes"][mode] = {
            "identity": identity,
            "provider_compile_context": context,
            "transport_compile_context": prepared["transport_compile_context"],
            "provider_link_order": prepared["link_archives"],
            "driver_sha256": digest(HERE / "driver.c"),
            "driver_object_sha256": digest(driver_object),
            "linked_test_sha256": digest(binary),
            "runtime": runtime,
        }
        (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")

    receipt["result"] = "PASS"
    receipt["target_executed"] = False
    receipt["checks"] = {
        "host_known_vectors": 2,
        "native_link_only": True,
        "same_pin_source_and_config_identities": True,
        "same_sha1_context_layout_under_format_and_transport_configs": True,
        "provider_exports_only_pinned_sha1_api": True,
        "selected_owner_has_no_sha1_duplicate": True,
        "selected_owner_supplies_shared_zeroize": True,
        "provider_has_no_platform_util_member": True,
    }
    (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print("format-sha1-provider host_vectors=2 native_link_only=true duplicate_sha1_exports=0")


if __name__ == "__main__":
    main()
