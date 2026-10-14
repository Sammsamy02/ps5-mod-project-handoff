#!/usr/bin/env python3
"""Read-only DLL intake against a source-pinned loader capability snapshot.

The result is an offline refusal or a request for loader preflight, never a claim
that a DLL can execute or produce gameplay behavior on PS5.
"""

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any, Dict, List

from pe_analyzer import analyze_file


FEATURES = (
    "native_x64_pe32_plus", "metadata_integrity", "managed_clr", "real_static_tls",
    "tls_callback_table_pointer", "direct_gs_access", "delay_import_directory", "rwx_partition",
    "windows_import_abi", "dynamic_loadlibrary", "target_binding",
    "console_behavior",
)
STATES = {"preflight_only", "refused", "unverified"}
SOURCE_PIN_PATHS = {
    "loader/src/pe_loader.cpp", "loader/include/pe_loader.h",
    "loader/src/pe_types.h", "loader/src/win_api_shim.cpp",
    "loader/src/win_api_shim.h",
}


def validate_source_pins(pins: Dict[str, str]) -> None:
    if not isinstance(pins, dict) or set(pins) != SOURCE_PIN_PATHS:
        raise ValueError("loader capability registry has missing or extra source pins")
    if any(not isinstance(value, str) or len(value) != 64 or
           any(char not in "0123456789abcdef" for char in value)
           for value in pins.values()):
        raise ValueError("loader capability registry has invalid source pin hash")


def load_registry(path: Path) -> Dict[str, Any]:
    registry = json.loads(path.read_text())
    if registry.get("schema_version") != 1:
        raise ValueError("unsupported loader capability registry schema")
    entries = registry.get("features")
    if not isinstance(entries, dict) or set(entries) != set(FEATURES):
        raise ValueError("loader capability registry has missing or extra features")
    if any(value not in STATES for value in entries.values()):
        raise ValueError("loader capability registry has an unknown feature state")
    validate_source_pins(registry.get("source_pins"))
    options = registry.get("loader_options")
    if options != {"page_size": 16384, "allow_rwx_partition": False,
                   "allow_no_relocs": False}:
        raise ValueError("loader capability registry has unsupported loader options")
    return registry


def verify_source_pins(registry: Dict[str, Any], loader_root: Path) -> None:
    validate_source_pins(registry.get("source_pins"))
    root = loader_root.resolve()
    for relative, expected in registry["source_pins"].items():
        path = (root / relative).resolve()
        if root not in path.parents or not path.is_file() or \
                hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError("loader source pin missing or mismatched: " + relative)


def make_dossier(report: Dict[str, Any], registry: Dict[str, Any],
                 source_pins_verified: bool = False) -> Dict[str, Any]:
    features = registry["features"]
    blockers: List[Dict[str, str]] = []
    def block(feature: str, observation: str) -> None:
        blockers.append({"feature": feature, "observation": observation,
                         "capability": features.get(feature, "unverified")})

    if report.get("classification") != "native-x64" or report.get("format") != "PE32+":
        block("native_x64_pe32_plus", report.get("classification", "unknown"))
    directories = {row.get("index"): row for row in report.get("data_directories", [])}
    clr_dir = directories.get(14, {})
    if report.get("managed") or clr_dir.get("rva") or clr_dir.get("size"):
        block("managed_clr", "CLR descriptor present")
    if report.get("optional", {}).get("number_of_rva_and_sizes", 0) <= 9:
        block("metadata_integrity", "loader requires more than nine PE data directories")
    optional = report.get("optional", {})
    image_size = optional.get("size_of_image", 0)
    if image_size == 0 or image_size > 512 * 1024 * 1024:
        block("metadata_integrity", "SizeOfImage is zero or exceeds loader's 512 MiB cap")
    entry_rva = optional.get("address_of_entry_point", 0)
    if entry_rva:
        executable = False
        for section in report.get("sections", []):
            exec_bytes = section["raw_size"]
            if section["virtual_size"] and section["virtual_size"] < exec_bytes:
                exec_bytes = section["virtual_size"]
            if section["executable"] and section["virtual_address"] <= entry_rva < \
                    section["virtual_address"] + exec_bytes:
                executable = True
                break
        if not executable:
            block("metadata_integrity", "entry point is outside file-backed executable section")
    if report.get("warnings") or report.get("debug", {}).get("status") == "malformed":
        block("metadata_integrity", "PE inspection produced warnings or malformed debug data")

    tls = report.get("tls", {})
    if tls.get("status") == "malformed":
        block("real_static_tls", "malformed TLS directory: " + tls.get("reason", "unknown"))
    elif tls.get("status") == "real" and features["real_static_tls"] != "preflight_only":
        block("real_static_tls", "initialized=%s, zero-fill=%s, callbacks=%s" % (
            tls.get("initialized_bytes", 0), tls.get("zero_fill_bytes", 0),
            tls.get("callback_count", 0)))
    if tls.get("callback_table_present") and features["tls_callback_table_pointer"] != "preflight_only":
        block("tls_callback_table_pointer", "callback table pointer present")
    gs_access = report.get("gs_access", {"status": "not_scanned"})
    if gs_access.get("status") != "scanned" and report.get("classification") == "native-x64":
        block("direct_gs_access", "GS instruction audit unavailable: " +
              gs_access.get("reason", gs_access.get("status", "unknown")))
    elif gs_access.get("candidate_count", 0):
        block("direct_gs_access", "%d candidate direct GS memory operands" %
              gs_access["candidate_count"])

    delay_dir = directories.get(13, {})
    if (delay_dir.get("rva") or delay_dir.get("size")) and \
            features["delay_import_directory"] != "preflight_only":
        block("delay_import_directory", "delay import directory present")

    options = registry["loader_options"]
    if not options["allow_rwx_partition"]:
        page_size = options["page_size"]
        page_flags: Dict[int, set] = {}
        for section in report.get("sections", []):
            start = section["virtual_address"]
            extent = section["virtual_size"] or section["raw_size"]
            if not extent:
                continue
            if start + extent > report.get("optional", {}).get("size_of_image", 0):
                block("metadata_integrity", "section extends beyond SizeOfImage")
                continue
            for page in range(start // page_size, (start + extent - 1) // page_size + 1):
                flags = page_flags.setdefault(page, set())
                if section["executable"]:
                    flags.add("X")
                if section["writable"]:
                    flags.add("W")
        for page, flags in sorted(page_flags.items()):
            if flags == {"W", "X"}:
                block("rwx_partition", "16 KiB page at RVA 0x%X needs write and execute" %
                      (page * page_size))

    return {
        "file": report.get("file"),
        "sha256": report.get("sha256"),
        "registry_snapshot": registry.get("snapshot"),
        "registry_source_pins": registry["source_pins"],
        "registry_source_pins_verified": source_pins_verified,
        "registry_loader_options": options,
        "offline_gate": "blocked" if blockers else "requires_loader_preflight",
        "blockers": blockers,
        "observed": {
            "classification": report.get("classification"),
            "tls": tls,
            "gs_access": gs_access,
            "debug": report.get("debug"),
            "proxy_hint": report.get("ue4ss", {}).get("looks_like_proxy_dll"),
            "import_dlls": [row.get("dll") for row in report.get("imports", [])],
            "delay_import_dlls": [row.get("dll") for row in report.get("delay_imports", [])],
        },
        "unverified_gates": ["loader_preflight", "relocation_and_slide",
                             "gs_access_scan_completeness", "loader_runtime_gs_enforcement",
                             "windows_import_abi", "dynamic_loadlibrary",
                             "target_binding", "console_behavior"],
        "warnings": report.get("warnings", []),
        "diagnostics": report.get("diagnostics", []),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", help="Windows PE DLLs to inspect without execution")
    parser.add_argument("--registry", type=Path,
                        default=Path(__file__).with_name("loader_capabilities.json"))
    parser.add_argument("--loader-root", type=Path,
                        help="verify registry hashes against this loader checkout")
    parser.add_argument("--out", type=Path, help="write JSON to this file")
    args = parser.parse_args()
    registry = load_registry(args.registry)
    if args.loader_root:
        verify_source_pins(registry, args.loader_root)
    dossiers = [make_dossier(analyze_file(path)[1], registry,
                             source_pins_verified=bool(args.loader_root)) for path in args.files]
    payload = dossiers[0] if len(dossiers) == 1 else dossiers
    result = json.dumps(payload, indent=2) + "\n"
    if args.out:
        args.out.write_text(result)
    else:
        print(result, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
