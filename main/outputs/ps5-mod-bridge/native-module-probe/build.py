#!/usr/bin/env python3
"""Build a local log-only PRX with a separately built SharpProspero tool.

This command never connects to a console or invokes a payload loader.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SHARPPROSPERO_BASE_COMMIT = "5dfdb4b0b166e83dc9ae275001afb695f12c7174"


def load_segments(data):
    """Compare the bytes the container carries, not discarded ELF section tables."""
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phsize, phcount = struct.unpack_from("<HH", data, 54)
    if phsize != 56 or phoff + phsize * phcount > len(data):
        raise SystemExit("invalid program-header extent")
    segments = []
    for index in range(phcount):
        kind, flags, offset, address, _, size, memory_size, align = struct.unpack_from(
            "<II6Q", data, phoff + phsize * index)
        if kind == 1:
            if offset + size > len(data):
                raise SystemExit("loadable segment exceeds file")
            segments.append((address, flags, memory_size, align, data[offset:offset + size]))
    return segments


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--llvm-config", type=Path, required=True)
    parser.add_argument("--dotnet", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True,
                        help="sharpprospero-bindgen.dll from pinned source")
    parser.add_argument("--profile", type=Path, default=HERE / "profiles/townfall.json")
    parser.add_argument("--output", type=Path, default=HERE / "build")
    args = parser.parse_args()
    profile = json.loads(args.profile.read_text())
    title = profile["title_id"]
    if not re.fullmatch(r"PPSA[0-9]{5}", title):
        parser.error("profile must identify a native PS5 title")
    for key in ("log_path", "module_path"):
        value = profile[key]
        if (not value.startswith("/data/") or not value.isascii()
                or any(c in value for c in "\r\n\0") or ".." in Path(value).parts):
            parser.error(f"invalid owned /data path: {key}")
    if Path(profile["module_path"]).name != "modbridge_log_probe.prx":
        parser.error("module_path must end in modbridge_log_probe.prx")

    build_id = uuid.uuid4().hex
    out = args.output.resolve() / build_id
    out.mkdir(parents=True, exist_ok=False)
    marker = f"modbridge-log-probe/v2 build={build_id} title={title}"
    lines = {entry: f"{marker} {entry}\n"
             for entry in ("init_array", "module_start")}
    (out / "probe_config.h").write_text(
        "#pragma once\n"
        + "#define MODBRIDGE_PROBE_LOG " + json.dumps(profile["log_path"]) + "\n"
        + "#define MODBRIDGE_PROBE_INIT_LINE " + json.dumps(lines["init_array"]) + "\n"
        + "#define MODBRIDGE_PROBE_START_LINE " + json.dumps(lines["module_start"]) + "\n")
    env = os.environ.copy()
    env["LLVM_CONFIG"] = str(args.llvm_config.resolve())
    env["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1"
    env["DOTNET_NOLOGO"] = "1"
    env["DOTNET_CLI_HOME"] = str(out / "dotnet-home")
    commands = []

    def run(label, command):
        command = [str(x) for x in command]
        result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / f"{label}.log").write_text(result.stdout)
        commands.append({"label": label, "argv": command,
                         "exit_code": result.returncode})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if result.returncode:
            raise SystemExit(f"{label} failed; see {out / (label + '.log')}")

    obj = out / "probe.o"
    module = out / "modbridge_log_probe.prx"
    bare_dir = out / "bare"
    bare_dir.mkdir()
    bare = bare_dir / module.name
    run("compile", [args.sdk.resolve() / "bin/prospero-clang", "-std=c11",
                    "-O2", "-Wall", "-Wextra", "-Werror", "-fPIC",
                    "-fno-stack-protector", "-I", out,
                    "-c", HERE / "probe.c", "-o", obj])
    tool = [args.dotnet.resolve(), args.tool.resolve()]
    run("link", tool + ["link", "--kind", "prx", "--self-contained",
                        "--obj", obj, "--export", "modbridge_probe_result",
                        "--export", "module_start",
                        "--library-init", "module_start",
                        "--publish-name", "modbridge_log_probe",
                        "--out", bare])
    data = bare.read_bytes()
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (0xfe18, 62):
        raise SystemExit("link output is not an x86_64 SCE dynamic-module ELF")
    run("inspect", tool + ["elf", "--file", bare, "--exports"])
    run("module-layout", [args.llvm_config.resolve().parent / "llvm-readelf",
                          "-h", "-l", "-d", "-r", "-s", bare])
    wrapper = args.sdk.resolve() / "samples/install_app/make_fself.py"
    run("wrap", [sys.executable, wrapper, bare, module])
    wrapped = module.read_bytes()
    if wrapped[:4] != bytes.fromhex("4f153d1d"):
        raise SystemExit("SDK wrapper output lacks its expected SELF header")
    run("wrapper-inspect", tool + ["self", "--inspect", "--file", module])
    extracted = out / "wrapper-extracted.elf"
    run("wrapper-extract", tool + ["self", "--extract", "--in", module,
                                   "--out", extracted])
    if load_segments(data) != load_segments(extracted.read_bytes()):
        raise SystemExit("SELF round trip changed loadable segment content")
    (out / "ploader.ini.example").write_text(
        "; Local template only; no 13.60/native-game loader validation.\n"
        f"[{title}]\n{profile['module_path']} = 2\n")
    (out / "expected-log-lines.txt").write_text("".join(lines.values()))
    receipt = {
        "build_id": build_id,
        "profile": profile,
        "module": str(module),
        "module_sha256": hashlib.sha256(wrapped).hexdigest(),
        "module_magic_hex": wrapped[:4].hex(),
        "bare_elf": str(bare),
        "bare_elf_sha256": hashlib.sha256(data).hexdigest(),
        "bare_elf_magic_hex": data[:4].hex(),
        "wrapper_source_sha256": hashlib.sha256(wrapper.read_bytes()).hexdigest(),
        "wrapper_load_segments_round_trip_passed": True,
        "source_sha256": hashlib.sha256((HERE / "probe.c").read_bytes()).hexdigest(),
        "linker_dll_sha256": hashlib.sha256(args.tool.read_bytes()).hexdigest(),
        "toolchain_assembly_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(args.tool.resolve().parent.glob("*.dll"))
        },
        "sharp_prospero_base_commit": SHARPPROSPERO_BASE_COMMIT,
        "format": "SDK fake SELF containing x86_64 ET_SCE_DYNAMIC; not Sony-signed",
        "local_compile_link_inspect_passed": True,
        "console_contact": False,
        "native_game_execution_proven": False,
        "ue4ss_included": False,
        "expected_log_lines": lines,
    }
    (out / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
