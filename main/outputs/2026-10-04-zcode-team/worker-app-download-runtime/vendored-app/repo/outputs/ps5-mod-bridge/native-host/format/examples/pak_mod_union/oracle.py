#!/usr/bin/env python3
"""Compare a base PAK plus one mod PAK with a derived PAK by decoded content."""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


HASH_RE = re.compile(r"^[0-9a-f]{64}$")
ENTRY_RE = re.compile(r"^(\d+) file entries$", re.MULTILINE)
VERSION_RE = re.compile(r"^version: (.+)$", re.MULTILINE)
MOUNT_RE = re.compile(r"^mount point: (.*)$", re.MULTILINE)
# Empty means repak emits the complete virtual mount point plus archive path.
# These are format identities only; they are never filesystem destinations.
STRIP_PREFIX = ""
COMMAND_TIMEOUT_SECONDS = 300
MAX_REPORT_MISMATCHES = 100


class OracleError(RuntimeError):
    pass


def checked_regular(path: str, label: str) -> Path:
    p = Path(path).resolve(strict=True)
    st = p.stat()
    if not p.is_file() or not os.path.isfile(p) or not st.st_size:
        raise OracleError(f"{label} is not a nonempty regular file")
    return p


def run(argv: list[str]) -> tuple[str, float]:
    started = time.monotonic()
    try:
        proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, encoding="utf-8", errors="strict",
                              timeout=COMMAND_TIMEOUT_SECONDS, check=False)
    except subprocess.TimeoutExpired as exc:
        raise OracleError(f"command timed out after {COMMAND_TIMEOUT_SECONDS}s: {argv[1]}") from exc
    elapsed = time.monotonic() - started
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip().replace("\n", " ")[:1000]
        raise OracleError(f"{argv[1]} failed with exit {proc.returncode}: {detail}")
    return proc.stdout, elapsed


def inspect_info(repak: Path, pak: Path, commands: list[dict]) -> dict:
    argv = [str(repak), "info", str(pak)]
    stdout, elapsed = run(argv)
    commands.append({"argv": argv, "elapsed_seconds": round(elapsed, 6), "exit": 0})
    entries = ENTRY_RE.search(stdout)
    version = VERSION_RE.search(stdout)
    mount = MOUNT_RE.search(stdout)
    if not entries or not version or not mount:
        raise OracleError(f"unrecognized `repak info` output for {pak.name}")
    return {"version": version.group(1), "mount_point": mount.group(1),
            "declared_entry_count": int(entries.group(1)), "bytes": pak.stat().st_size}


def decoded_hashes(repak: Path, pak: Path, commands: list[dict]) -> dict[str, str]:
    argv = [str(repak), "hash-list", "--strip-prefix", STRIP_PREFIX, str(pak)]
    stdout, elapsed = run(argv)
    commands.append({"argv": argv, "elapsed_seconds": round(elapsed, 6), "exit": 0})
    result: dict[str, str] = {}
    for line_no, line in enumerate(stdout.splitlines(), 1):
        digest, sep, virtual_path = line.partition(" ")
        if not sep or not HASH_RE.fullmatch(digest) or not virtual_path:
            raise OracleError(f"malformed hash-list row {line_no} for {pak.name}")
        if virtual_path in result:
            raise OracleError(f"duplicate decoded virtual path in hash-list: {virtual_path}")
        # Keep virtual paths opaque and exact. In particular, leading ../ mount
        # components are engine metadata, not paths to validate or materialize.
        result[virtual_path] = digest
    return result


def inspect_pak(repak: Path, pak: Path, label: str, commands: list[dict]) -> tuple[dict, dict[str, str]]:
    info = inspect_info(repak, pak, commands)
    hashes = decoded_hashes(repak, pak, commands)
    if len(hashes) != info["declared_entry_count"]:
        raise OracleError(f"repak exposed {len(hashes)} unique decoded paths but `info` declares "
                          f"{info['declared_entry_count']} rows for {label}; refusing duplicate/collapsed paths")
    return info, hashes


def diff_maps(expected: dict[str, str], actual: dict[str, str]) -> list[dict]:
    mismatches = []
    for path in sorted(expected.keys() | actual.keys()):
        want, got = expected.get(path), actual.get(path)
        if want != got:
            mismatches.append({"path": path, "expected_sha256": want, "actual_sha256": got})
    return mismatches


def write_receipt(path: Path, report: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")


def main(argv: list[str]) -> int:
    if len(argv) != 5:
        print("usage: oracle.py REPAK BASE_PAK MOD_PAK OUTPUT_PAK RECEIPT.json", file=sys.stderr)
        return 2
    repak_arg, base_arg, mod_arg, output_arg, receipt_arg = argv
    commands: list[dict] = []
    report: dict = {
        "schema_version": 1,
        "status": "error",
        "method": "exact virtual-path union; SHA-256 of repak-decoded entry bytes",
        "path_comparison": "case-sensitive complete virtual mount-plus-entry paths, unchanged",
        "strip_prefix": STRIP_PREFIX,
        "commands": commands,
        "runtime_or_install_claimed": False,
    }
    receipt = Path(receipt_arg).expanduser().resolve(strict=False)
    try:
        repak = checked_regular(repak_arg, "repak executable")
        if not os.access(repak, os.X_OK):
            raise OracleError("repak is not executable")
        base = checked_regular(base_arg, "base PAK")
        mod = checked_regular(mod_arg, "mod PAK")
        output = checked_regular(output_arg, "output PAK")
        all_inputs = [repak, base, mod, output]
        if len({str(p) for p in all_inputs}) != len(all_inputs):
            raise OracleError("repak/base/mod/output paths must be distinct")
        if receipt in all_inputs:
            raise OracleError("receipt path aliases an input")
        report["inputs"] = {"base": str(base), "mod": str(mod), "output": str(output),
                            "repak": str(repak)}
        base_info, base_map = inspect_pak(repak, base, "base", commands)
        mod_info, mod_map = inspect_pak(repak, mod, "mod", commands)
        output_info, output_map = inspect_pak(repak, output, "output", commands)
        expected = dict(base_map)
        expected.update(mod_map)
        mismatches = diff_maps(expected, output_map)
        overrides = sorted(base_map.keys() & mod_map.keys())
        report.update({
            "status": "pass" if not mismatches else "mismatch",
            "base": {**base_info, "decoded_path_count": len(base_map)},
            "mod": {**mod_info, "decoded_path_count": len(mod_map)},
            "output": {**output_info, "decoded_path_count": len(output_map)},
            "expected_union_path_count": len(expected),
            "overridden_paths": [{"path": p, "base_sha256": base_map[p],
                                  "mod_sha256": mod_map[p], "changed": base_map[p] != mod_map[p]}
                                 for p in overrides],
            "added_path_count": len(mod_map.keys() - base_map.keys()),
            "preserved_base_path_count": len(base_map.keys() - mod_map.keys()),
            "mismatch_count": len(mismatches),
            "mismatches": mismatches[:MAX_REPORT_MISMATCHES],
            "mismatches_truncated": len(mismatches) > MAX_REPORT_MISMATCHES,
            "runtime_or_install_claimed": False,
        })
    except Exception as exc:
        report["error"] = str(exc)
    try:
        write_receipt(receipt, report)
    except FileExistsError:
        print(json.dumps({"status": "error", "error": "receipt already exists; refusing overwrite"}))
        return 2
    except OSError as exc:
        print(json.dumps({"status": "error", "error": f"cannot write receipt: {exc}"}))
        return 2
    print(json.dumps({"status": report["status"], "receipt": str(receipt),
                      "mismatch_count": report.get("mismatch_count")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
