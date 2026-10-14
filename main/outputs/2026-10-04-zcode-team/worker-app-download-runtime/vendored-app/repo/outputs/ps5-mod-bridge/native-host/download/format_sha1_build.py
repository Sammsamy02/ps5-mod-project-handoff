#!/usr/bin/env python3
"""Build a separate Mbed TLS 3.6.7 SHA1 provider for format consumers.

The selected curl/Mbed TLS download profile intentionally omits legacy SHA1.
This helper builds only the pinned upstream ``library/sha1.c`` translation
unit under the existing SHA1-only PAK configuration. It never changes or
rebuilds the transport archive. The returned compile context must be used for
PAK SHA1 consumers; the selected TLS context remains separate.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
from collections import Counter

HERE = Path(__file__).resolve().parent
NATIVE_HOST = HERE.parent
PAK = NATIVE_HOST / "pak"
PAK_CONFIG = PAK / "pak_mbedtls_config.h"
PAK_CONFIG_SHA256 = "350a9c1cb6677e9a23d20007b48391817b894796b9d1820391c2efee44d13cbb"
SHA1_SOURCE_SHA256 = "277b7499fe8315a46442c39c762e8768056114e3694215e4e2cadbd1ebfbce12"
SHA1_HEADER_SHA256 = "e644d96d46d44da66e160eb69ad637f13caef1d192e3361616833b342c6a859b"
SHA1_SYMBOLS = {
    "mbedtls_sha1_init", "mbedtls_sha1_free", "mbedtls_sha1_clone",
    "mbedtls_sha1_starts", "mbedtls_sha1_update", "mbedtls_sha1_finish",
    "mbedtls_internal_sha1_process", "mbedtls_sha1",
}
SHA1_CONTEXT_ABI = {
    "size": 92,
    "total_offset": 0,
    "state_offset": 8,
    "buffer_offset": 28,
    "sha1_alt": False,
}


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def _dependency_build():
    spec = importlib.util.spec_from_file_location(
        "modbridge_format_sha1_download_dependencies",
        NATIVE_HOST / "download" / "dependency_build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _crypto_build():
    spec = importlib.util.spec_from_file_location(
        "modbridge_format_sha1_crypto_validator", NATIVE_HOST / "crypto_build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _checked(path, expected, label):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or digest(path) != expected:
        raise ValueError("format SHA1 input identity mismatch: " + label)
    return path.resolve()


def _run(argv, output, commands, env, *, record_output=True):
    result = subprocess.run([str(value) for value in argv], cwd=output, env=env,
                            capture_output=True, text=True, timeout=60)
    entry = {"argv": [str(value) for value in argv], "returncode": result.returncode}
    if record_output:
        entry.update({"stdout": result.stdout, "stderr": result.stderr})
    else:
        stdout, stderr = result.stdout.encode(), result.stderr.encode()
        entry.update({"stdout_bytes": len(stdout),
                      "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
                      "stderr_bytes": len(stderr),
                      "stderr_sha256": hashlib.sha256(stderr).hexdigest()})
    commands.append(entry)
    (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    if result.returncode:
        raise ValueError("format SHA1 command failed; see " + str(output / "commands.json"))
    return result.stdout


def _mode_argv(commands, needle):
    for argv in commands:
        if any(needle in value for value in argv):
            return argv
    raise ValueError("selected Mbed profile lacks its recorded " + needle + " command")


def _symbol_rows(nm_output):
    rows = []
    for line in nm_output.splitlines():
        fields = line.split()
        if len(fields) >= 2:
            symbol = fields[-1]
            normalized = symbol[1:] if symbol.startswith("_") else symbol
            rows.append((normalized, fields[-2]))
    return rows


def _defined_symbol_rows(nm, path, mode, output, commands, env):
    args = [nm, "--extern-only", "--defined-only", path] if mode == "native" else [nm, "-gU", path]
    return [(name, kind) for name, kind in _symbol_rows(
        _run(args, output, commands, env, record_output=False))
            if kind.upper() != "U"]


def _undefined_symbols(nm, path, output, commands, env):
    raw = _run([nm, "-u", path], output, commands, env, record_output=False)
    result = set()
    for line in raw.splitlines():
        fields = line.split()
        if not fields:
            continue
        symbol = fields[-1]
        if symbol.startswith("_mbedtls_"):
            symbol = symbol[1:]
        result.add(symbol)
    return result


def _include_closure(depfile, mode, roots):
    raw = depfile.read_text().replace("\\\n", " ")
    names = shlex.split(raw.split(":", 1)[1])
    entries = []
    for name in dict.fromkeys(names):
        path = Path(name).resolve()
        origin = next((label for root, label in roots if path.is_relative_to(root)), "system")
        if mode == "native" and origin == "system":
            raise ValueError("native format SHA1 include closure contains an unpinned header")
        entries.append({"path": str(path), "origin": origin, "sha256": digest(path)})
    return entries


def _recorded_native_profile(profile, mbed):
    rows = json.loads((profile / "native" / "mbed-commands.json").read_text())
    source_compile = _mode_argv(rows, "/library/sha1.c")
    compiler = Path(source_compile[0]).resolve()
    resource = Path(source_compile[source_compile.index("-resource-dir") + 1]).resolve()
    if ("-nostdinc" not in source_compile or "-isystem" not in source_compile or
            source_compile[source_compile.index("-isystem") + 1] != str(resource / "include") or
            str(mbed / "library" / "common.h") not in source_compile or
            str(profile / "tls_effective.h") not in source_compile):
        raise ValueError("native profile's recorded compilation context is incomplete")
    return compiler, resource, source_compile


def prepare_format_sha1(mode, output_root, profile, *, sdk=None, llvm_bin=None):
    """Build and describe the isolated SHA1 provider for host or native mode.

    The output path must not exist. Native mode takes the same verified SDK and
    LLVM ``bin`` directory used by the selected download profile. No target
    executable is run. ``link_archives`` is ordered provider-first, then the
    exact selected crypto archive that owns ``mbedtls_platform_zeroize``.
    """
    dependency = _dependency_build()
    crypto = _crypto_build()
    if mode not in dependency.MODE_HASHES:
        raise ValueError("format SHA1 mode must be host or native")

    profile = Path(profile).resolve()
    if profile.is_symlink() or not profile.is_dir():
        raise ValueError("selected download profile is unavailable")
    mbed = profile.parent / ("mbedtls-" + dependency.MBED_PIN)
    release = profile.parent / ("mbedtls-" + dependency.MBED_PIN + ".tar.gz")
    if not mbed.is_dir():
        raise ValueError("same-pin Mbed TLS source tree is unavailable")

    selected_crypto = _checked(
        profile / mode / "libmbedcrypto.a",
        dependency.MODE_HASHES[mode]["mbedcrypto"], "selected transport crypto archive")
    config_files = {name: _checked(profile / name, expected, name)
                    for name, expected in dependency.CONFIG_HASHES.items()}
    pak_config = _checked(PAK_CONFIG, PAK_CONFIG_SHA256, "format SHA1-only PAK config")
    mbed_archive_hashes = dependency._verify_vendor(mbed, release, dependency.MBED_RELEASE_SHA)
    if (mbed_archive_hashes.get("library/sha1.c") != SHA1_SOURCE_SHA256 or
            mbed_archive_hashes.get("include/mbedtls/sha1.h") != SHA1_HEADER_SHA256):
        raise ValueError("pinned Mbed TLS SHA1 source/header identity mismatch")

    output = Path(output_root).absolute()
    if output.is_symlink() or output.exists():
        raise ValueError("format SHA1 output directory must be fresh")
    output = output.resolve()
    for tree in (profile, mbed, release, PAK, NATIVE_HOST):
        if output == tree or output.is_relative_to(tree):
            raise ValueError("format SHA1 output must be outside retained/source inputs")

    native_options = []
    selected_tools = {}
    if mode == "host":
        if sdk is not None or llvm_bin is not None:
            raise ValueError("host format SHA1 mode must not receive native tool paths")
        recorded = json.loads((profile / "host" / "mbed-commands.json").read_text())
        source_compile = _mode_argv(recorded, "/library/sha1.c")
        compiler = Path("/usr/bin/clang").resolve()
        if Path(source_compile[0]).resolve() != compiler:
            raise ValueError("host compiler does not match selected Mbed profile")
        if not compiler.is_file():
            raise ValueError("host compiler is unavailable")
        archiver = Path("/usr/bin/ar")
        nm = Path("/usr/bin/nm")
        selected_tools = {"compiler": {"path": str(compiler), "sha256": digest(compiler)},
                          "archiver": {"path": str(archiver), "sha256": digest(archiver)},
                          "symbol_reader": {"path": str(nm), "sha256": digest(nm)}}
        env = os.environ.copy()
        for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                     "CFLAGS", "CPPFLAGS", "LDFLAGS", "LIBS", "CONFIG_SITE",
                     "MBEDTLS_CONFIG_FILE", "MBEDTLS_USER_CONFIG_FILE"):
            env.pop(name, None)
        base_flags = ["-std=c11", "-Os", "-fno-common", "-ffunction-sections",
                      "-fdata-sections", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
    else:
        if sdk is None or llvm_bin is None:
            raise ValueError("native format SHA1 mode requires the pinned SDK and LLVM paths")
        sdk, llvm_bin = Path(sdk).resolve(), Path(llvm_bin).resolve()
        compiler, resource, source_compile = _recorded_native_profile(profile, mbed)
        if compiler != (sdk / "bin/prospero-clang").resolve():
            raise ValueError("native SDK compiler does not match selected profile")
        selected, manifest_sha = crypto._read_llvm_selected_manifest()
        crypto._verify_sdk(sdk, llvm_bin.parents[2] / "sdk.zip")
        for name, expected in crypto.TOOL_HASHES.items():
            tool_path = (llvm_bin / name).resolve()
            if not tool_path.is_relative_to(llvm_bin):
                raise ValueError("LLVM tool link escapes its pinned package")
            _checked(tool_path, expected, "LLVM " + name)
        resource_files = {name[len("lib/clang/21/"):]: value
                          for name, value in selected.items()
                          if name.startswith("lib/clang/21/")}
        crypto._check_tree(resource, resource_files)
        ar_rel = profile.parent / "tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar"
        archiver = _checked(ar_rel, selected["bin/llvm-ar"], "selected LLVM archiver")
        nm = _checked((llvm_bin / "llvm-nm").resolve(),
                      crypto.TOOL_HASHES["llvm-nm"], "LLVM symbol reader")
        llvm_config = _checked((llvm_bin / "llvm-config").resolve(), crypto.TOOL_HASHES["llvm-config"],
                               "LLVM configuration tool")
        env = os.environ.copy()
        for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                     "SDKROOT", "CFLAGS", "CPPFLAGS", "LDFLAGS", "LIBS", "CONFIG_SITE",
                     "MBEDTLS_CONFIG_FILE", "MBEDTLS_USER_CONFIG_FILE"):
            env.pop(name, None)
        env["LLVM_CONFIG"] = str(llvm_config)
        native_options = ["-nostdinc", "-resource-dir", str(resource),
                          "-isystem", str(resource / "include")]
        base_flags = ["-std=c11", "-Os", "-fno-common", "-ffunction-sections",
                      "-fdata-sections", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        selected_tools = {"compiler": {"path": str(compiler), "sha256": digest(compiler)},
                          "archiver": {"path": str(archiver), "sha256": digest(archiver)},
                          "symbol_reader": {"path": str(nm), "sha256": digest(nm)},
                          "llvm_config": {"path": str(llvm_config), "sha256": digest(llvm_config)},
                          "sdk_sha256": crypto.SDK_SHA,
                          "llvm_selected_manifest_sha256": manifest_sha,
                          "resource_dir": str(resource),
                          "resource_files": resource_files}

    output.mkdir(parents=True, exist_ok=False)
    env["ZERO_AR_DATE"] = "1"
    env["SOURCE_DATE_EPOCH"] = "0"
    commands = []
    include_dirs = [PAK, mbed / "include", mbed / "library"]
    flags = [*base_flags, *["-I" + str(path) for path in include_dirs],
             '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"', *native_options]
    source = mbed / "library/sha1.c"
    obj = output / "sha1.o"
    depfile = output / "sha1.d"
    _run([compiler, *flags, "-MD", "-MF", depfile, "-c", source, "-o", obj],
         output, commands, env)

    if mode == "native":
        data = obj.read_bytes()
        if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
            raise ValueError("format SHA1 object is not x86_64 FreeBSD ELF relocatable")

    archive = output / "libmbedformatsha1.a"
    _run([archiver, "rcsD" if mode == "native" else "rcs", archive, obj],
         output, commands, env)
    members = _run([archiver, "t", archive], output, commands, env).splitlines()
    object_members = [name for name in members if name not in
                      ("__.SYMDEF", "__.SYMDEF SORTED")]
    if object_members != ["sha1.o"]:
        raise ValueError("format SHA1 archive must contain only sha1.o")

    provider_rows = _defined_symbol_rows(nm, obj, mode, output, commands, env)
    provider_counts = Counter(name for name, _ in provider_rows)
    provider_exports = set(provider_counts)
    provider_types = {name: kind.upper() for name, kind in provider_rows}
    if (provider_counts != Counter({name: 1 for name in SHA1_SYMBOLS}) or
            set(provider_types.values()) != {"T"}):
        raise ValueError("format SHA1 provider symbol closure mismatch")
    owner_rows = _defined_symbol_rows(nm, selected_crypto, mode, output, commands, env)
    owner_counts = Counter(name for name, _ in owner_rows)
    owner_exports = set(owner_counts)
    if any(name.startswith("mbedtls_sha1") for name in owner_counts):
        raise ValueError("selected transport owner already defines SHA1 symbols")
    zeroize_rows = [kind.upper() for name, kind in owner_rows
                    if name == "mbedtls_platform_zeroize"]
    if len(zeroize_rows) != 1 or zeroize_rows[0] != "T":
        raise ValueError("selected transport owner must provide one shared zeroize implementation")
    undefined = _undefined_symbols(nm, obj, output, commands, env)
    if "mbedtls_platform_zeroize" not in undefined:
        raise ValueError("format SHA1 provider did not retain the shared zeroize dependency")

    # The exact same canonical public context must compile under the format-only
    # and selected transport configurations. The separate test verifier links
    # and runs the real provider against the selected owner.
    abi_source = HERE / "format_sha1_tests/context_abi.c"
    abi_results = {}
    for label, config_name, config_root in (
        ("format_sha1", "pak_mbedtls_config.h", PAK),
        ("transport_tls", "tls_config.h", profile),
    ):
        abi_obj = output / (label + "_abi.o")
        abi_dep = output / (label + "_abi.d")
        abi_flags = [*base_flags, *["-I" + str(path) for path in include_dirs],
                     "-I" + str(config_root),
                     f'-DMBEDTLS_CONFIG_FILE="{config_name}"', *native_options]
        _run([compiler, *abi_flags, "-MD", "-MF", abi_dep, "-c", abi_source,
              "-o", abi_obj], output, commands, env)
        macros = _run([compiler, *abi_flags, "-dM", "-E", "-x", "c", abi_source],
                      output, commands, env, record_output=False)
        definitions = {line.split()[1]: " ".join(line.split()[2:])
                       for line in macros.splitlines() if line.startswith("#define ")}
        if "MBEDTLS_SHA1_ALT" in definitions:
            raise ValueError("Mbed TLS SHA1 alternate context is forbidden")
        if (label == "format_sha1" and "MBEDTLS_SHA1_C" not in definitions) or (
                label == "transport_tls" and "MBEDTLS_SHA1_C" in definitions):
            raise ValueError("format/transport SHA1 feature separation changed")
        if label == "format_sha1" and any(name in definitions for name in (
                "MBEDTLS_PSA_CRYPTO_C", "MBEDTLS_SSL_CLI_C", "MBEDTLS_SSL_TLS_C")):
            raise ValueError("format-only configuration imported transport features")
        abi_results[label] = {
            "configuration_sha256": digest(config_root / config_name),
            "object_sha256": digest(abi_obj),
            "context_abi": SHA1_CONTEXT_ABI,
            "sha1_enabled": "MBEDTLS_SHA1_C" in definitions,
            "sha1_alt": "MBEDTLS_SHA1_ALT" in definitions,
        }

    roots = [(output.resolve(), "format_output"), (PAK.resolve(), "format_config"),
             (profile.resolve(), "selected_transport_profile"),
             (mbed.resolve(), "pinned_mbedtls_source")]
    if mode == "native":
        roots.append((sdk.resolve(), "pinned_ps5_sdk"))
        roots.append((resource.resolve(), "pinned_llvm_resource"))
    closure = dependency._includes(depfile, mode, roots)
    if not any(row["path"].endswith("/mbedtls/sha1.h") for row in closure):
        raise ValueError("format SHA1 include closure omits the pinned public header")
    if not any(row["path"] == str(pak_config) for row in closure):
        raise ValueError("format SHA1 include closure omits the exact PAK config")

    identity = {
        "schema_version": 1,
        "mode": mode,
        "mbedtls_commit": dependency.MBED_PIN,
        "mbedtls_release_sha256": dependency.MBED_RELEASE_SHA,
        "mbedtls_source_sha256": SHA1_SOURCE_SHA256,
        "mbedtls_sha1_header_sha256": SHA1_HEADER_SHA256,
        "vendor_source_file_count": len(mbed_archive_hashes),
        "format_config_path": str(pak_config),
        "format_config_sha256": digest(pak_config),
        "builder_sha256": digest(__file__),
        "context_abi_probe_sha256": digest(HERE / "format_sha1_tests/context_abi.c"),
        "known_vector_driver_sha256": digest(HERE / "format_sha1_tests/driver.c"),
        "transport_configuration_sha256": dependency.CONFIG_HASHES,
        "selected_transport_crypto_archive": str(selected_crypto),
        "selected_transport_crypto_archive_sha256": digest(selected_crypto),
        "provider_archive": str(archive),
        "provider_archive_sha256": digest(archive),
        "provider_members": object_members,
        "provider_sha1_exports": sorted(provider_exports),
        "provider_undefined_symbols": sorted(undefined),
        "transport_sha1_exports": sorted(owner_exports & SHA1_SYMBOLS),
        "transport_zeroize_export_count": sum(
            name == "mbedtls_platform_zeroize" for name in owner_exports),
        "context_abi_checks": abi_results,
        "include_closure": closure,
        "tools": selected_tools,
        "target_executed": False,
        "transport_archive_rebuilt": False,
        "platform_util_rebuilt_or_duplicated": False,
        "link_order": [str(archive), str(selected_crypto)],
    }
    (output / "manifest.json").write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
    format_compile_context = {
        "include_dirs": [str(path) for path in include_dirs],
        "compile_definitions": ['MBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"'],
        "forced_includes": [],
        "native_compile_options": native_options,
        "configuration_path": str(pak_config),
        "configuration_sha256": digest(pak_config),
        "context_abi": SHA1_CONTEXT_ABI,
    }
    transport_compile_context = {
        "include_dirs": [str(profile), str(mbed / "include"), str(mbed / "library")],
        "compile_definitions": ['MBEDTLS_CONFIG_FILE="tls_config.h"'],
        "forced_includes": [str(mbed / "library/common.h"),
                            str(config_files["tls_effective.h"])],
        "configuration_sha256": dependency.CONFIG_HASHES,
    }
    return {
        "archive": str(archive),
        "include_dirs": format_compile_context["include_dirs"],
        "compile_definitions": format_compile_context["compile_definitions"],
        "forced_includes": [],
        "native_compile_options": native_options,
        "link_archives": [str(archive), str(selected_crypto)],
        "compile_context": format_compile_context,
        "transport_crypto_archive": str(selected_crypto),
        "transport_compile_context": transport_compile_context,
        "identity": identity,
        "evidence_manifest": str(output / "manifest.json"),
        "commands": commands,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("host", "native"), required=True)
    parser.add_argument("--profile", type=Path, required=True,
                        help="selected retained curl/Mbed TLS download profile")
    parser.add_argument("--output-dir", type=Path, required=True,
                        help="fresh output directory outside all retained inputs")
    parser.add_argument("--ps5-sdk", type=Path)
    parser.add_argument("--llvm-bin", type=Path)
    args = parser.parse_args()
    try:
        result = prepare_format_sha1(args.mode, args.output_dir, args.profile,
                                     sdk=args.ps5_sdk, llvm_bin=args.llvm_bin)
    except (OSError, ValueError, subprocess.SubprocessError, KeyError, IndexError) as error:
        raise SystemExit(str(error)) from error
    print(json.dumps({"archive": result["archive"],
                      "compile_context": result["compile_context"],
                      "transport_crypto_archive": result["transport_crypto_archive"],
                      "identity": result["identity"],
                      "evidence_manifest": result["evidence_manifest"]},
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
