"""Whole selected artifact and exact helper/provider checks; no engine claim."""
import hashlib
import json
import subprocess
from pathlib import Path
from prep_guard import OWN, require_owned

LAYOUT_PROVIDER = "_ZN2RC8Platform13NativeLayouts26GetStagedNativeLayoutFactsEv"
CACHE_PROVIDERS = {"bridge_code_cache_context", "bridge_code_cache_signature"}
OWNER_PROVIDERS = {"bridge_copyin_initializer_enter", "bridge_copyin_initializer_leave", "bridge_copyin_initializer_complete"}
ALL_NEW_PROVIDERS = CACHE_PROVIDERS | OWNER_PROVIDERS
REQUIRED_BRIDGE_TYPES = {name: "T" for name in ALL_NEW_PROVIDERS}
CONSUMERS = {
    "runtime_owner": {"bridge_copyin_initializer_enter", "bridge_copyin_initializer_leave"},
    "unreal-unrealinitializer": {"bridge_copyin_initializer_complete", *CACHE_PROVIDERS, LAYOUT_PROVIDER},
}
CACHE_HEADERS = {"CodeImageCache.hpp", "CodeImageSha256.hpp", "CopyinStartup.hpp", "CopyinSink.hpp"}
ROOT = OWN.parents[1]
LLVM = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def resolved(path):
    path = Path(path)
    return (path if path.is_absolute() else ROOT / path).resolve()


def source_records():
    prep = json.loads((OWN / "PREP-MANIFEST.json").read_text())
    return json.loads(Path(prep["source_manifest"]["path"]).read_text())["selected_sources"]


def helper_dependencies(dependency_file, records, required=frozenset(), stage_dir=None):
    """A matching basename/hash at an old include path is still source drift."""
    expected = {Path(row["path"]).name: row for row in records.values()
                if Path(row["path"]).suffix in {".h", ".hpp"}}
    deps = {resolved(token) for token in Path(dependency_file).read_text().replace("\\\n", " ").split()[1:]}
    found = {}
    for path in deps:
        row = expected.get(path.name)
        if row is None:
            continue
        intended = (Path(stage_dir) / path.name).resolve() if stage_dir else resolved(row["path"])
        if path != intended:
            raise ValueError("prior-source helper include drift: " + str(path))
        if sha(path) != row["sha256"]:
            raise ValueError("selected helper hash drift: " + str(path))
        found[path.name] = {"path": str(path), "sha256": row["sha256"]}
    if not required <= found.keys():
        raise ValueError("missing selected helper dependencies: " + str(sorted(required - found.keys())))
    return dict(sorted(found.items()))


def nm(path, defined=False):
    argv = [str(LLVM / "llvm-nm"), "--extern-only", "--defined-only" if defined else "--undefined-only", "--format=posix", str(path)]
    result = subprocess.run(argv, capture_output=True, text=True)
    if result.returncode:
        raise ValueError("llvm-nm failed: " + result.stderr[-500:])
    return {line.split()[0]: line.split()[1] for line in result.stdout.splitlines() if len(line.split()) >= 2}


def whole_selected_gate(run_dir, candidate_dir):
    run_dir, candidate_dir = Path(run_dir), Path(candidate_dir)
    receipt = json.loads((run_dir / "receipt.json").read_text())
    records = source_records()
    selected = receipt["selected_runtime_units"]
    if len(selected) != receipt["runtime_compilation_units"]:
        raise ValueError("selected source count/receipt mismatch")
    rows = {}
    for name, unit in selected.items():
        source, obj = resolved(unit["source"]), resolved(unit["object"])
        if sha(source) != unit["source_sha256"] or sha(obj) != unit["object_sha256"]:
            raise ValueError("whole-selected source/object hash mismatch: " + name)
        rows[name] = {"source": str(source), "source_sha256": unit["source_sha256"], "object": str(obj), "object_sha256": unit["object_sha256"]}
    input_hashes = json.loads((run_dir / "input-hashes.json").read_text())
    input_paths = {str(resolved(path)): digest for path, digest in input_hashes.items()}
    order = receipt["runtime_object_order"]
    if len(order) != len(selected) or len(set(order)) != len(order) or set(order) != selected.keys():
        raise ValueError("whole selected unit/link-order mismatch")
    for name, unit in rows.items():
        if input_paths.get(unit["object"]) != unit["object_sha256"]:
            raise ValueError("selected unit absent from actual composition inputs: " + name)
    for path, digest in input_hashes.items():
        if sha(resolved(path)) != digest:
            raise ValueError("whole composition input hash mismatch: " + path)
    if sha(resolved(receipt["runtime_object"])) != receipt["runtime_sha256"]:
        raise ValueError("composed relocatable hash mismatch")
    consumers = {}
    role_map = {"runtime_owner": "runtime_owner", "unreal-unrealinitializer": "initializer", "shadow-copy": "shadow_copy.cpp", "native-layoutbinding": "NativeLayoutBinding.cpp"}
    for name, role in role_map.items():
        unit = selected[name]
        if resolved(unit["source"]) != resolved(records[role]["path"]) or unit["source_sha256"] != records[role]["sha256"]:
            raise ValueError("l19 selected role/source mismatch: " + role)
        required = (CACHE_HEADERS | {"NativeLayoutBinding.hpp", "BuildFactsChecks.hpp", "RuntimeBuildProfile.hpp"}) if name == "unreal-unrealinitializer" else {"runtime_owner.hpp"} if name == "runtime_owner" else {"NativeLayoutBinding.hpp", "NativeLayoutActiveWidths.hpp"} if name == "native-layoutbinding" else {"shadow_copy.hpp"}
        helpers = helper_dependencies(resolved(unit["dependency_file"]), records, required)
        und = nm(resolved(unit["object"]))
        expected = CONSUMERS.get(name, set())
        if not expected <= und.keys() or any(und[n] != "U" for n in expected):
            raise ValueError("exact l19 provider consumer reference missing: " + name)
        consumers[name] = {"required_strong_undefined": sorted(expected), "helper_dependencies": helpers}
    overlay = json.loads((OWN / "driver/staging/overlay-manifest.json").read_text())
    stage = OWN / "driver/staging/lineage-overlay-2"
    for row in records.values():
        name = Path(row["path"]).name
        if Path(name).suffix not in {".h", ".hpp"}:
            continue
        staged = overlay["headers"].get(name)
        if staged is None or staged["sha256"] != row["sha256"] or sha(stage / name) != row["sha256"]:
            raise ValueError("l19 overlay header binding mismatch: " + name)
    bridge_obj = candidate_dir / "build/runtime_start_bridge.o"
    bridge_defs = nm(bridge_obj, defined=True)
    if any(bridge_defs.get(n) != kind for n, kind in REQUIRED_BRIDGE_TYPES.items()):
        raise ValueError("exact l19 bridge provider definition/type missing")
    bridge_helpers = helper_dependencies(candidate_dir / "build/runtime_start_bridge.d", records,
                                          CACHE_HEADERS | {"CodeSignatureProfile.hpp"}, stage)
    # Verify the new C++ provider independently of existing bridge providers.
    binding_object = resolved(selected["native-layoutbinding"]["object"])
    composed = resolved(receipt["runtime_object"])
    definition_evidence = {}
    for label, obj in (("selected-binding", binding_object), ("composed-runtime", composed)):
        output = subprocess.run([str(LLVM / "llvm-nm"), "--extern-only", "--defined-only", "--format=posix", str(obj)], capture_output=True, text=True)
        if output.returncode:
            raise ValueError("new layout provider nm failed: " + label)
        lines = [line for line in output.stdout.splitlines() if line.split() and line.split()[0] == LAYOUT_PROVIDER]
        if len(lines) != 1 or lines[0].split()[1] != "T":
            raise ValueError("new layout provider needs exactly one strong definition: " + label)
        if LAYOUT_PROVIDER in nm(obj):
            raise ValueError("new layout provider remains undefined: " + label)
        definition_evidence[label] = {"object":str(obj),"sha256":sha(obj),"symbol":LAYOUT_PROVIDER,"definitions":lines,"strong_definition_count":1}
    payload = {"status": "PASS-WHOLE-SELECTED-ARTIFACTS", "scope": "local build artifact/source/provider checks only; engine behavior unproved",
               "selected_count": len(selected), "input_hash_count": len(input_hashes), "selected": rows,
               "consumers": consumers, "bridge_providers": REQUIRED_BRIDGE_TYPES, "bridge_helper_dependencies": bridge_helpers,
               "new_layout_provider":definition_evidence,"engine_execution_proved": False}
    destination = require_owned(OWN / "receipts" / ("whole-selected-" + candidate_dir.name + ".json"))
    destination.write_text(json.dumps(payload, indent=2) + "\n")
    return payload
