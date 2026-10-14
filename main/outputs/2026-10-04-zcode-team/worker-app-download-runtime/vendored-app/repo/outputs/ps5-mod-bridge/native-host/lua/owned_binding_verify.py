#!/usr/bin/env python3
"""Focused real-Lua bindings proof. Native artifacts are never executed."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys


LUA_HEADERS = {
    "lua.h": "048c6574ba5e8d225d47b66e83b479832ce353881f263ecf8389247442921f6c",
    "lauxlib.h": "2d9e18e577a6062646268e7f7228264c28bd9b972c186e1af81b8357179c0ab4",
    "luaconf.h": "3e8e572899bc4fe1be486cbf195c91abc54ab96caea735c870c982a6fc00af80",
    "lualib.h": "4ea0f67d3e467f5d6a327616e22a3c2da8e03d47a3c3660916275f2842f521ea",
}
# Retained Lua5.4.7/UE4SS LuaRaw archives, with owned recursive pthread glue.
# They remain unmodified and are not rebuilt by this verifier.
LUA_LIBRARIES = {
    "host": "1369e0acc509a1bf65a5ab98efd55951455b087b25e13cdff36844949957747c",
    "native": "92382c6f96ee4ed57bbccd0dd7b85a8ebcacf21f550eefe6ec8965570c355620",
}
OLD_BOOL_SHA256 = "98934306a31b43d464878999fda8548bcb3791bdeb2839b1047a9fa428ef801f"
PUBLIC_APIS = {
    "lba_install", "lba_descriptor_valid", "lba_install_with_bool",
    "lba_bool_descriptor_valid", "lba_bool_apply", "lba_install_with_refs",
    "lba_ref_descriptor_valid",
}
ALLOWED_IMPORTS = {
    "__stderrp", "abort", "clock_gettime", "fmod", "fprintf", "free", "frexp",
    "ldexp", "longjmp", "memcmp", "memcpy", "memset", "pow", "printf",
    "pthread_mutex_init", "pthread_mutex_lock", "pthread_mutex_unlock",
    "pthread_mutexattr_destroy", "pthread_mutexattr_init", "pthread_mutexattr_settype",
    "pthread_once", "realloc", "setjmp", "snprintf", "strchr", "strcmp",
    "strcoll", "strcpy", "strlen", "strpbrk", "strspn", "strtod", "time",
}


def identity(path):
    path = Path(path).resolve(strict=True)
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest()}


def require_identity(path, digest):
    result = identity(path)
    if result["sha256"] != digest:
        raise ValueError(f"pinned input differs: {path}")
    return result


def main():
    source_root = Path(__file__).resolve().parent
    workspace = source_root.parents[3]
    evidence_root = workspace / ".superpowers/native-dependency-spike/owned-lua-reference-binding-1"
    tests = source_root / "owned_binding_tests"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("red", "host", "native"))
    parser.add_argument("--lua-include", type=Path, required=True)
    parser.add_argument("--lua-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--red-source", type=Path,
                        help="immutable prior Bool adapter; required for behavioral RED")
    parser.add_argument("--sanitize", action="store_true",
                        help="ASan/UBSan for current host adapter/tests; Lua archive is unsanitized")
    parser.add_argument("--ps5-sdk", type=Path)
    parser.add_argument("--llvm-config", type=Path)
    parser.add_argument("--resource-dir", type=Path)
    args = parser.parse_args()
    if args.sanitize and args.mode != "host":
        parser.error("--sanitize is supported only in host mode")
    if bool(args.red_source) != (args.mode == "red"):
        parser.error("--red-source is required exactly in red mode")
    native_paths = (args.ps5_sdk, args.llvm_config, args.resource_dir)
    if args.mode == "native" and not all(native_paths):
        parser.error("native requires --ps5-sdk, --llvm-config and --resource-dir")
    if args.mode != "native" and any(native_paths):
        parser.error("SDK/toolchain arguments are supported only in native mode")

    include = args.lua_include.resolve(strict=True)
    library = args.lua_library.resolve(strict=True)
    out = args.output_dir.resolve()
    if not out.is_relative_to(evidence_root.resolve()) or out == evidence_root.resolve():
        raise ValueError("output must be a fresh child of the owned ignored evidence directory")
    pinned = [require_identity(include / name, sha) for name, sha in LUA_HEADERS.items()]
    pinned.append(require_identity(library, LUA_LIBRARIES["native" if args.mode == "native" else "host"]))
    binding = source_root / "owned_binding.c"
    header = source_root / "owned_binding.h"
    drivers = {"reference": tests / "driver.c", "legacy-bool": tests / "legacy_bool_driver.c"}
    tracked_inputs = [Path(__file__), header, tests / "vm_gate.h", *drivers.values()]
    if args.mode == "red":
        old = args.red_source.resolve(strict=True)
        pinned.append(require_identity(old, OLD_BOOL_SHA256))
        tracked_inputs.append(tests / "red_ref_stub.c")
    else:
        tracked_inputs.append(binding)
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH", "SDKROOT"):
        env.pop(name, None)
    receipt = {
        "mode": args.mode, "passed": False, "commands": [],
        "target_executed": False, "network_used": False, "console_connected": False,
        "engine_provider_available": False, "runtime_or_installation_claim": False,
        "lua_version": "5.4.7", "lua_archive_rebuilt": False,
        "sanitizer": {"adapter_and_tests": args.sanitize, "lua_archive": False,
                      "leak_sanitizer": args.sanitize and sys.platform != "darwin",
                      "limit": ("retained LuaRaw archive is uninstrumented; " +
                                ("Apple ASan has no LeakSanitizer; bounded Lua allocator close is checked"
                                 if sys.platform == "darwin" else "leak detection requested"))
                               if args.sanitize else "not selected"},
        "input_before": [identity(p) for p in tracked_inputs] + pinned,
    }

    def save():
        (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def run(argv, label, expected=0):
        completed = subprocess.run([str(v) for v in argv], env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=60)
        (out / (label + ".stdout")).write_bytes(completed.stdout)
        (out / (label + ".stderr")).write_bytes(completed.stderr)
        receipt["commands"].append({"argv": [str(v) for v in argv], "exit": completed.returncode,
                                    "stdout": label + ".stdout", "stderr": label + ".stderr"})
        save()
        if completed.returncode != expected:
            raise ValueError(f"{label}: exit {completed.returncode}, expected {expected}; see {out}")
        if label.startswith(("compile", "link")) and completed.stderr:
            raise ValueError(f"{label}: unexpected compiler/linker diagnostics; see {out}")
        return completed.stdout.decode("utf-8", errors="strict")

    save()
    try:
        if args.mode == "red":
            original = old.read_bytes()
            anchor = b'#include "lua_binding.h"'
            if original.count(anchor) != 1:
                raise ValueError("prior Bool include differs from the frozen source")
            derived = original.replace(anchor, b'#include "owned_binding.h"')
            binding = out / "prior-bool-adapter.c"
            binding.write_bytes(derived)
            receipt["red_baseline"] = {
                "parent": identity(old), "derived": identity(binding),
                "transformation": 'only include "lua_binding.h" -> "owned_binding.h"',
                "stub": identity(tests / "red_ref_stub.c"),
            }

        flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 "-ffunction-sections", "-fdata-sections", "-pthread", "-I", str(include),
                 "-I", str(source_root), "-I", str(tests)]
        compiler = Path("/usr/bin/clang")
        layout = None
        if args.mode == "native":
            sdk = args.ps5_sdk.resolve(strict=True)
            llvm_config = args.llvm_config.resolve(strict=True)
            resource = args.resource_dir.resolve(strict=True)
            compiler = sdk / "bin/prospero-clang"
            env["LLVM_CONFIG"] = str(llvm_config)
            flags += ["-nostdinc", "-resource-dir", str(resource), "-isystem", str(resource / "include")]
            helper = source_root.parent / "archive/link_layout.py"
            tracked_inputs += [helper, compiler, llvm_config]
            receipt["input_before"] += [identity(p) for p in (helper, compiler, llvm_config)]
            spec = importlib.util.spec_from_file_location("owned_lua_link_layout", helper)
            layout = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(layout)
            receipt["link_profile"] = layout.write_linker_script(sdk, out)
            llvm_bin = llvm_config.parent
            for name in ("llvm-nm", "llvm-readobj"):
                tracked_inputs.append(llvm_bin / name)
                receipt["input_before"].append(identity(llvm_bin / name))
        if args.sanitize:
            flags += ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            env["ASAN_OPTIONS"] = "detect_leaks=" + ("0" if sys.platform == "darwin" else "1") + ":halt_on_error=1"
            env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            receipt["sanitizer"]["runtime_options"] = {
                "ASAN_OPTIONS": env["ASAN_OPTIONS"], "UBSAN_OPTIONS": env["UBSAN_OPTIONS"]}
        receipt["compiler"] = identity(compiler)
        run([compiler, "--version"], "compiler-version")
        adapter_obj = out / "owned_binding.o"
        run([compiler, *flags, "-c", binding, "-o", adapter_obj], "compile-adapter")
        common = [adapter_obj]
        if args.mode == "red":
            stub_obj = out / "red_ref_stub.o"
            run([compiler, *flags, "-c", tests / "red_ref_stub.c", "-o", stub_obj], "compile-ref-stub")
            common.append(stub_obj)
        receipt["executables"] = {}
        receipt["behavior"] = {}
        for name, source in drivers.items():
            obj = out / (name + ".o")
            run([compiler, *flags, "-c", source, "-o", obj], "compile-" + name)
            executable = out / (name + (".elf" if args.mode == "native" else ""))
            if args.mode == "native":
                linker = ["-Wl,--gc-sections", "-Wl,--no-undefined", "-Wl,-z,now",
                          "-Wl,-z,max-page-size=0x4000",
                          "-Wl,-T," + receipt["link_profile"]["caller_script"],
                          "-Wl,-Map=" + str(out / (name + ".map"))]
                if name == "reference":
                    linker += ["-Wl,--undefined=" + api for api in sorted(PUBLIC_APIS)]
            else:
                linker = ["-Wl,-dead_strip"]
            run([compiler, *flags, *common, obj, library, *linker, "-o", executable], "link-" + name)
            receipt["executables"][name] = identity(executable)
            if args.mode == "native":
                native = {"dynamic_layout": layout.validate_dynamic_layout(executable)}
                elf_text = run([llvm_bin / "llvm-readobj", "--file-headers", "--program-headers",
                                "--dynamic-table", "--needed-libs", executable], "elf-" + name)
                if "EM_X86_64" not in elf_text or "BIND_NOW" not in elf_text:
                    raise ValueError("native ELF lacks required x86_64/BIND_NOW facts")
                imports_text = run([llvm_bin / "llvm-nm", "--dynamic", "--undefined-only", executable], "imports-" + name)
                imports = {line.split()[-1] for line in imports_text.splitlines() if line.split()}
                if not imports or imports - ALLOWED_IMPORTS:
                    raise ValueError(f"unexpected native imports: {sorted(imports - ALLOWED_IMPORTS)}")
                native["imports"] = sorted(imports)
                native["imports_within_reviewed_membership"] = True
                definitions = run([llvm_bin / "llvm-nm", "--defined-only", executable], "defined-" + name)
                symbols = {line.split()[-1] for line in definitions.splitlines() if line.split()}
                if name == "reference" and not PUBLIC_APIS <= symbols:
                    raise ValueError(f"missing native public APIs: {sorted(PUBLIC_APIS - symbols)}")
                native["public_apis_retained"] = sorted(PUBLIC_APIS & symbols)
                receipt["behavior"][name] = {"executed": False, "native_link": native}
            else:
                expected = 1 if args.mode == "red" and name == "reference" else 0
                text = run([executable], "behavior-" + name, expected=expected)
                if args.mode == "red" and name == "reference":
                    failures = [line for line in text.splitlines() if line.startswith("FAIL ")]
                    if not failures or not any(re.search(r"ref|chain|IsValid|null", line, re.I) for line in failures):
                        raise ValueError("RED must show named absent-reference/validity behavior failures")
                    receipt["behavior"][name] = {"executed": True, "intended_behavioral_failure": True,
                                                 "failures": failures}
                else:
                    if "FAIL " in text or "failures=0" not in text:
                        raise ValueError(f"{name} lacks zero-failure behavior summary")
                    receipt["behavior"][name] = {"executed": True, "passed": True}
                print(text, end="")
        receipt["input_after"] = [identity(p["path"]) for p in receipt["input_before"]]
        receipt["inputs_unchanged"] = receipt["input_before"] == receipt["input_after"]
        if not receipt["inputs_unchanged"]:
            raise ValueError("source/header/library/tool changed during proof")
        receipt["objects"] = [identity(p) for p in sorted(out.glob("*.o"))]
        receipt["passed"] = True
        save()
        print(f"{args.mode}: receipt {out / 'receipt.json'}")
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        receipt["failure"] = str(error)
        save()
        raise


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
