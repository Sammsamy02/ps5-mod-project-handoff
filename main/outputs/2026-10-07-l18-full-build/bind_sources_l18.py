#!/usr/bin/env python3
"""Prepare or apply l18 source bindings. --apply requires the parent GO record."""
import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from prep_guard import OWN, require_owned

REQUIRED_LAYOUT = {
    "runtime_owner": "runtime_owner.cpp",
    "initializer": "UnrealInitializer.cpp",
    "bridge": "runtime_start_bridge_r14_diag_spanpub.cpp",
    "CopyinStartup.hpp": "CopyinStartup.hpp",
    "CopyinSink.hpp": "CopyinSink.hpp",
    "CodeImageCache.hpp": "CodeImageCache.hpp",
    "CodeImageSha256.hpp": "CodeImageSha256.hpp",
    "CodeSignatureProfile.hpp": "CodeSignatureProfile.hpp",
    "runtime_owner.hpp": "runtime_owner.hpp",
    "shadow_copy.cpp": "shadow_copy.cpp",
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, value):
    path = require_owned(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def validate_selection(data):
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
        raise ValueError("source selection schema must be integer 1")
    records = data.get("selected_sources")
    if not isinstance(records, dict) or not REQUIRED_LAYOUT.keys() <= records.keys():
        raise ValueError("source selection lacks required l18 roles/headers")
    parents, paths, names = set(), set(), set()
    for role, record in records.items():
        if not isinstance(role, str) or not isinstance(record, dict):
            raise ValueError("invalid source selection record")
        path_value, digest = record.get("path"), record.get("sha256")
        if not isinstance(path_value, str) or not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("incomplete source pin: " + role)
        path = Path(path_value)
        if not path.is_absolute() or not path.is_file() or path.is_symlink():
            raise ValueError("source must be an absolute regular file: " + role)
        expected = REQUIRED_LAYOUT.get(role, role)
        if path.name != expected:
            raise ValueError("source role basename mismatch: " + role)
        if path.resolve() in paths or path.name in names:
            raise ValueError("duplicate source selection path/basename")
        parents.add(path.parent.resolve()); paths.add(path.resolve()); names.add(path.name)
    if len(parents) != 1:
        raise ValueError("selected unit/header topology must use one source directory")
    source_dir = next(iter(parents))
    inventory = {p.name for p in source_dir.iterdir() if p.is_file() and p.suffix in {".cpp", ".hpp", ".h"}}
    if inventory != names:
        raise ValueError("source selection must cover the complete unit/header inventory")
    for role, record in records.items():
        if sha(record["path"]) != record["sha256"]:
            raise ValueError("source pin drift: " + role)
    return records


def layout_plan(directory):
    """No hashes or snapshots are taken from an evolving source tree."""
    directory = Path(directory)
    names = sorted(p.name for p in directory.iterdir() if p.is_file() and p.suffix in {".cpp", ".hpp", ".h"})
    return {"status": "PLANNED-BINDINGS-ONLY", "source_directory": str(directory.resolve()),
            "required_layout": REQUIRED_LAYOUT, "unit_header_names": names, "hashes_frozen": False,
            "source_copy_performed": False, "readiness_changed": False,
            "apply_requires": "source owner freeze + parent review + explicit parent GO record"}


def check_pin(record, label):
    if not isinstance(record, dict) or not isinstance(record.get("path"), str) or not isinstance(record.get("sha256"), str):
        raise ValueError("missing reviewed provenance pin: " + label)
    path = Path(record["path"])
    if not path.is_file() or sha(path) != record["sha256"]:
        raise ValueError("reviewed provenance pin drift: " + label)
    return path


def apply_bindings(selection_path, go_path):
    selection_path, go_path = Path(selection_path), Path(go_path)
    data, go = json.loads(selection_path.read_text()), json.loads(go_path.read_text())
    prep = json.loads((OWN / "PREP-MANIFEST.json").read_text())
    if (go.get("parent_reported_xom_audit") is not True
            or go.get("source_manifest_sha256") != sha(selection_path)
            or go.get("xom_audit_sha256") != prep["xom_audit_receipt"]["sha256"]
            or go.get("upload_authorized") is not False):
        raise ValueError("HOLD: parent GO must bind the final source selection and reported XOM audit")
    for role in ("frozen_source_review", "host_receipt", "native_receipt", "source_delta", "parent_initializer_test", "parent_native_pins", "settings_provenance_addendum"):
        check_pin(go.get(role), role)
    check_pin(prep["xom_audit_receipt"], "XOM audit")
    records = validate_selection(data)
    provenance = data.get("provenance")
    if not isinstance(provenance, dict):
        raise ValueError("source selection lacks frozen source/test provenance")
    for role, record in provenance.items():
        if isinstance(record, dict) and "path" in record:
            check_pin(record, role)
    required_provenance = {"source_delta", "HOST-ALL.json", "NATIVE-FOCUS.json"}
    if not required_provenance <= provenance.keys():
        raise ValueError("source selection lacks source-delta/host/native receipt pins")
    if (OWN / "units").exists():
        raise ValueError("source bindings already exist; preserving history")
    # Copy only after all original source/review/provenance checks pass.
    units = require_owned(OWN / "units")
    units.mkdir()
    selected = {}
    for role, record in sorted(records.items()):
        source = Path(record["path"]); target = units / source.name
        content = source.read_bytes()
        if hashlib.sha256(content).hexdigest() != record["sha256"]:
            raise ValueError("source changed during snapshot: " + role)
        target.write_bytes(content)
        selected[role] = {"path": str(target), "sha256": record["sha256"]}
    inputs = require_owned(OWN / "inputs/source-freeze"); inputs.mkdir()
    shutil.copyfile(selection_path, inputs / "SOURCE-SELECTION.original.json")
    shutil.copyfile(go_path, inputs / "PARENT-BUILD-GO.original.json")
    evidence = {}
    for role, record in sorted(provenance.items()):
        if isinstance(record, dict) and "path" in record:
            destination = inputs / (role + "-" + Path(record["path"]).name)
            shutil.copyfile(record["path"], destination)
            evidence[role] = {"path": str(destination), "sha256": record["sha256"], "original_path": record["path"]}
    for role, record in go.items():
        if isinstance(record, dict) and "path" in record and "sha256" in record:
            check_pin(record, "parent " + role)
            destination = inputs / ("parent-" + role + "-" + Path(record["path"]).name)
            shutil.copyfile(record["path"], destination)
            evidence["parent-" + role] = {"path": str(destination), "sha256": record["sha256"], "original_path": record["path"]}
    bound = {"schema": 1, "selected_sources": selected, "original_sources": records,
             "original_selection": {"path": str(selection_path), "sha256": sha(selection_path)},
             "provenance": evidence, "scope": "byte-identical private source snapshot; host/native receipts are not engine evidence"}
    manifest = OWN / "SOURCE-SELECTION.bound.json"; save(manifest, bound)
    manifest_sha = sha(manifest)
    edition = json.loads((OWN / "EDITIONS-l18.json").read_text())
    edition["bridge_cli"] = {"file": selected["bridge"]["path"], "sha256": selected["bridge"]["sha256"], "provenance": str(manifest)}
    edition["compose"]["initializer_armed"] = {"dir": str(units), "selection_status": "L18-FROZEN-PRIVATE-SNAPSHOT"}
    edition["l18_scope"] = {"status": "ADMITTED-FROZEN-SOURCE-MANIFEST", "source_manifest_sha256": manifest_sha,
                            "selected_sources": selected, "source_snapshot": "byte-identical", "upload": "HOLD"}
    edition["admissions"]["initializer_fold"] = {"status": "ADMITTED-V4ARMED-FOLD", "files": {r["path"]: r["sha256"] for r in selected.values()}, "evidence": str(manifest)}
    # Historical source pins remain visible, plus the complete new selection.
    known = {r["file"] for r in edition["units"]}
    for role, record in selected.items():
        if record["path"] not in known:
            edition["units"].append({"file": record["path"], "sha256": record["sha256"], "what": "L18 frozen selected " + role})
    private_tools = {str(path): sha(path) for path in sorted(OWN.glob("*.py"))}
    for item in edition["units"]:
        if item["file"] in private_tools:
            item["sha256"] = private_tools.pop(item["file"])
    for path, digest in private_tools.items():
        edition["units"].append({"file": path, "sha256": digest, "what": "Private l18 pipeline/tool"})
    save(OWN / "EDITIONS-l18.json", edition)
    change = json.loads((OWN / "integration/change-manifest.json").read_text())
    changed_units = change["changed_units"]
    for unit_name, role, kind in (("runtime_owner", "runtime_owner", "replaced-bootstrap"), ("unreal-unrealinitializer", "initializer", "replaced-affected"), ("shadow-copy", "shadow_copy.cpp", "extra")):
        row = next((r for r in changed_units if r["unit"] == unit_name), None)
        if row is None:
            row = {"unit": unit_name, "kind": kind}; changed_units.append(row)
        row.update(source=selected[role]["path"], sha256=selected[role]["sha256"],
                   sha256_after={selected[role]["path"]: selected[role]["sha256"]},
                   receipt=str(OWN / "receipts/l11-replace-receipt.json"), justification="L18 frozen cleanup + authenticated disk/XOM code verification")
    bridge = next(r for r in change["packaging_units"] if r["unit"] == "lifecycle/runtime_start_bridge.cpp")
    bridge.update(source=selected["bridge"]["path"], sha256=selected["bridge"]["sha256"], justification="L18 frozen lifecycle/cache bridge and exact internal providers")
    changed_headers = {r["rel"]: r for r in change["changed_headers_staged"]}
    for record in selected.values():
        path = Path(record["path"])
        if path.suffix in {".h", ".hpp"}:
            changed_headers[path.name] = {"rel": path.name, "source": str(path), "sha256": record["sha256"], "justification": "L18 coherent selected helper/header snapshot"}
    change["changed_headers_staged"] = list(changed_headers.values())
    change.update(manifest_id="l18-cleanup-xom-code-verification", expected_next_launch="UPLOAD HOLD: local artifacts only; runtime/XOM/hook/menu evidence remains unproved")
    change["l18_preparation"] = {"source_selection": str(manifest), "source_selection_sha256": manifest_sha, "parent_go": str(go_path), "upload": "HOLD"}
    save(OWN / "integration/change-manifest.json", change)
    prep.update(readiness="READY", source_manifest={"path": str(manifest), "sha256": manifest_sha}, parent_go=str(go_path))
    save(OWN / "PREP-MANIFEST.json", prep)
    save(OWN / "BUILD-GO.json", {**go, "source_manifest_sha256": manifest_sha,
                                "approved_original_source_manifest_sha256": sha(selection_path),
                                "xom_audit_sha256": prep["xom_audit_receipt"]["sha256"],
                                "parent_go_original": {"path": str(inputs / "PARENT-BUILD-GO.original.json"), "sha256": sha(inputs / "PARENT-BUILD-GO.original.json")},
                                "local_full_build_only": True, "upload": "HOLD"})
    return {"status": "BOUND-FOR-APPROVED-LOCAL-BUILD", "selected_count": len(selected), "source_manifest": str(manifest), "sha256": manifest_sha, "build_run": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path)
    parser.add_argument("--selection", type=Path)
    parser.add_argument("--go", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    if args.apply:
        if args.selection is None or args.go is None:
            parser.error("--apply requires final --selection and parent --go records")
        result = apply_bindings(args.selection, args.go)
    elif args.layout is not None:
        result = layout_plan(args.layout)
        save(OWN / "BINDING-PLAN.json", result)
    else:
        parser.error("use --layout for an unfrozen plan, or --apply after parent GO")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
