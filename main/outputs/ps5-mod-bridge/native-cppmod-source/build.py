#!/usr/bin/env python3
"""Compile a genuine C++ mod and audit its native host ABI, entirely locally.

This produces relocatable target objects and a source/object audit package. It
does not invent a host import table, link a runnable PRX, or contact a console.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import uuid
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SPIKE = ROOT / ".superpowers/native-dependency-spike"
PORT = SPIKE / "ue4ss-unreal-native-build-1/port"

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-receipt", type=Path, default=SPIKE / "native-coherent-runtime-1/coherent-full-1/cppmod.command.json")
    parser.add_argument("--host-runtime", type=Path, default=SPIKE / "native-coherent-runtime-1/composition-1/coherent-runtime.o")
    parser.add_argument("--output", type=Path, default=SPIKE / "ue4ss-native-cppmod-source-1")
    args = parser.parse_args()
    recorded = json.loads(args.compile_receipt.read_text())
    if recorded["exit"] != 0:
        parser.error("compile receipt must record a successful native CppMod compilation")
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, LLVM_CONFIG=recorded["LLVM_CONFIG"])
    llvm = Path(recorded["LLVM_CONFIG"]).parent
    commands = []

    def run(label, command):
        command = [str(value) for value in command]
        result = subprocess.run(command, env=env, cwd=ROOT, capture_output=True, text=True)
        (out / (label + ".stdout")).write_text(result.stdout)
        (out / (label + ".stderr")).write_text(result.stderr)
        commands.append({"label": label, "argv": command, "exit": result.returncode})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if result.returncode:
            raise SystemExit(f"{label} failed: {out / (label + '.stderr')}")
        return result.stdout

    def compile_object(label, source, plugin=False):
        command = list(recorded["argv"])
        command[command.index(recorded["source"])] = str(source)
        obj = out / (label + ".o")
        dep = out / (label + ".d")
        command[command.index("-o") + 1] = str(obj)
        if "-MF" in command:
            command[command.index("-MF") + 1] = str(dep)
        else:
            command += ["-MD", "-MF", str(dep)]
        command[1:1] = ["-I", str(PORT / "ue4ss/include")]
        if plugin:
            command = [item for item in command if item != "-DRC_UE4SS_EXPORTS"]
            # ELF visibility replaces Windows DLL attributes, without changing
            # the genuine CppUserModBase declaration or its vtable layout.
            command += ['-DRC_UE4SS_API=__attribute__((visibility("default")))']
        run(label, command)
        return obj, dep

    mod, plugin_dep = compile_object("lifecycle-mod", HERE / "lifecycle_mod.cpp", True)
    child, child_dep = compile_object("child-runtime", HERE / "child_runtime.cpp", True)
    plugin=out/"lifecycle-plugin.o"
    run("combine-plugin",[llvm/"ld.lld","-r",mod,child,"-o",plugin])
    base_source = PORT / "ue4ss/src/Mod/CppUserModBase.cpp"
    original_base = SPIKE / "ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/src/Mod/CppUserModBase.cpp"
    base, base_dep = compile_object("genuine-base", base_source)
    run("module-elf", [llvm / "llvm-readelf", "-h", "-s", plugin])

    def symbols(label, path, flag):
        text = run(label, [llvm / "llvm-nm", "--extern-only", flag, "--format=posix", path])
        return {line.split()[0] for line in text.splitlines() if line.strip()}

    plugin_defs = symbols("plugin-definitions", plugin, "--defined-only")
    plugin_undefined = symbols("plugin-imports", plugin, "--undefined-only")
    base_defs = symbols("base-definitions", base, "--defined-only")
    base_undefined = symbols("base-imports", base, "--undefined-only")
    host_defs = symbols("host-definitions", args.host_runtime, "--defined-only")
    host_undefined = symbols("host-imports", args.host_runtime, "--undefined-only")
    exports = {"start_mod", "uninstall_mod", "modbridge_cppmod_counters", "module_start", "module_stop"}
    if not exports.issubset(plugin_defs):
        raise SystemExit("real mod lifecycle exports were not emitted")
    required_base = {symbol for symbol in plugin_undefined if "CppUserModBase" in symbol}
    if not required_base or not required_base.issubset(base_defs):
        raise SystemExit("genuine base object does not provide the derived mod's base ABI")
    extra_unresolved = sorted(base_undefined - host_defs - host_undefined)
    if extra_unresolved:
        raise SystemExit("genuine base introduces unaudited imports: " + repr(extra_unresolved))

    dependencies = {}
    for dep in (plugin_dep, child_dep, base_dep):
        for path in dep.read_text().replace("\\\n", " ").split()[1:]:
            if Path(path).is_file():
                dependencies[path] = sha(path)
    (out / "dependency-hashes.json").write_text(json.dumps(dependencies, indent=2) + "\n")
    audit = {
        "status": "NATIVE_CPP_MOD_SOURCE_COMPILED_AND_ABI_AUDITED",
        "target": "x86_64-sie-ps5 / __PROSPERO__",
        "plugin_object": str(plugin), "plugin_sha256": sha(plugin),
        "child_runtime_object": str(child), "child_runtime_sha256": sha(child),
        "genuine_base_object": str(base), "genuine_base_sha256": sha(base),
        "base_source": str(base_source), "base_source_sha256": sha(base_source),
        "authoritative_base_source": str(original_base), "authoritative_base_sha256": sha(original_base),
        "base_native_change": "register_tab refuses unsupported GUI before retaining a tab",
        "compile_receipt": str(args.compile_receipt.resolve()), "compile_receipt_sha256": sha(args.compile_receipt),
        "host_runtime": str(args.host_runtime.resolve()), "host_runtime_sha256": sha(args.host_runtime),
        "lifecycle_exports": sorted(exports), "plugin_undefined_symbols": sorted(plugin_undefined),
        "base_symbols_required_by_mod": sorted(required_base),
        "base_symbols_missing_from_supplied_host": sorted(required_base - host_defs),
        "required_base_symbols_provided_by_genuine_tu": True,
        "base_new_unresolved_beyond_supplied_host": extra_unresolved,
        "remaining_imports_with_genuine_base_and_host": sorted(plugin_undefined - base_defs - host_defs),
        "host_export_import_table_proven": False, "runnable_prx": False,
        "native_execution": False, "game_bindings_used": False, "console_contact": False,
    }
    (out / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")
    package = out / "native-cppmod-source-audit.zip"
    with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in (HERE / "lifecycle_mod.cpp", HERE/"child_runtime.cpp", HERE/"child_runtime.h", HERE / "README.md", plugin, out / "audit.json"):
            archive.write(path, path.name)
    print(json.dumps({"audit": str(out / "audit.json"), "package": str(package),
        "required_base_symbols_provided": True, "runnable_prx": False}, indent=2))

if __name__ == "__main__":
    main()
