#!/usr/bin/env python3
"""Focused host verifier for the bounded imported-package name reader."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parent
REPO = Path(__file__).resolve().parents[4]
TESTS = ROOT / "tests"
PROFILE = TESTS / "package_imports_actual_header_profile.json"
EVIDENCE = REPO / ".superpowers/native-observations/package-imports-host-run-1"
HASH_VERSION = 0xC1640000
MAX_HEADER_BYTES = 262144


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def independent_decode(data: bytes) -> dict:
    require(52 <= len(data) <= MAX_HEADER_BYTES, "header span outside focused bounds")
    logical_header = struct.unpack_from("<I", data, 4)[0]
    require(logical_header <= len(data), "captured span omits header tail")
    hash_off, import_off, export_off = struct.unpack_from("<iii", data, 24)
    names_off = struct.unpack_from("<i", data, 48)[0]
    require(0 <= hash_off <= import_off <= export_off <= names_off <= logical_header,
            "unexpected source table offset order")
    count = struct.unpack_from("<I", data, names_off)[0]
    cursor = names_off + 4
    names = []
    declared_bytes = 0
    hash_version = 0
    used = 0
    if count:
        declared_bytes, hash_version = struct.unpack_from("<IQ", data, cursor)
        require(hash_version == HASH_VERSION, "unsupported actual name hash version")
        cursor += 12
        hashes = list(struct.unpack_from("<" + "Q" * count, data, cursor))
        cursor += 8 * count
        headers = list(struct.unpack_from(">" + "H" * count, data, cursor))
        cursor += 2 * count
        sizes = [((h & 0x7FFF) * 2 if h & 0x8000 else h) for h in headers]
        strings_start = cursor
        used = sum(sizes)
        require(used <= MAX_HEADER_BYTES, "name bytes exceed independent bound")
        require(strings_start + used + 4 * count == logical_header,
                "name batch and signed numbers do not exactly consume HeaderSize")
        for i, (encoded, byte_count) in enumerate(zip(headers, sizes)):
            raw = data[cursor:cursor + byte_count]
            names.append({
                "hash": f"{hashes[i]:016x}",
                "encoding": 2 if encoded & 0x8000 else 1,
                "bytes": byte_count,
                "units": (encoded & 0x7FFF) if encoded & 0x8000 else 0,
                "number": struct.unpack_from("<i", data, strings_start + used + 4 * i)[0],
                "raw_hex": raw.hex(),
            })
            cursor += byte_count
        package_numbers = [n["number"] for n in names]
    else:
        require(names_off + 4 == logical_header, "empty imported-name container has trailing bytes")
        package_numbers = []
    require(cursor + 4 * count == logical_header, "table terminator mismatch")
    require(export_off <= logical_header and (export_off - import_off) % 8 == 0,
            "import reference span is not whole object indices")
    require((import_off - hash_off) % 8 == 0, "public hash span is not whole hashes")
    hashes = struct.unpack_from("<" + "Q" * ((import_off - hash_off) // 8), data, hash_off)
    refs = struct.unpack_from("<" + "Q" * ((export_off - import_off) // 8), data, import_off)
    bindings = []
    for i, raw in enumerate(refs):
        kind = raw >> 62
        if kind != 2:
            continue
        value = raw & ((1 << 62) - 1)
        name_index = value >> 32
        hash_index = raw & 0xFFFFFFFF
        require(name_index < len(names), "package reference name index outside imported-name table")
        require(hash_index < len(hashes), "package reference public-hash index outside hash table")
        bindings.append({
            "import_index": i,
            "name_index": name_index,
            "hash_index": hash_index,
            "object_index": f"{raw:016x}",
            "public_hash": f"{hashes[hash_index]:016x}",
        })
    return {
        "header_bytes": logical_header,
        "table_offset": names_off,
        "table_bytes": logical_header - names_off,
        "name_count": count,
        "name_bytes_used": used,
        "declared_string_bytes_raw": declared_bytes,
        "name_hash_version_raw": f"{hash_version:016x}",
        "names": names,
        "binding_count": len(bindings),
        "bindings": bindings,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=EVIDENCE)
    parser.add_argument("--actual-profile", type=Path, default=PROFILE)
    args = parser.parse_args()
    started = time.monotonic()
    repo = REPO
    evidence = args.output_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    profile = json.loads(args.actual_profile.resolve().read_text(encoding="utf-8"))
    require(profile["schema_version"] == 1, "unsupported local profile schema")
    require(isinstance(profile.get("samples"), list) and 1 <= len(profile["samples"]) <= 16,
            "actual profile sample count exceeds focused bound")
    compiler = shutil.which(os.environ.get("CC", "cc"))
    require(compiler is not None, "host C compiler unavailable")
    compiler_path = Path(compiler).resolve()
    compiler_version = subprocess.run([compiler, "--version"], capture_output=True, text=True,
                                      timeout=10, check=True).stdout.splitlines()[0]
    sources = [
        ROOT / "summary.c",
        ROOT / "import_map.c",
        ROOT / "package_imports.c",
        ROOT / "tests/package_imports_test.c",
    ]
    caps_source = TESTS / "header_caps_test.c"
    inputs = [*sources, caps_source, ROOT / "summary.h", ROOT / "import_map.h",
              ROOT / "package_imports.h", Path(__file__).resolve(), args.actual_profile.resolve()]
    before = {str(path): sha256(path.read_bytes()) for path in inputs}
    commands = []
    output = evidence / "package_imports_test"
    argv = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-I", str(ROOT), *map(str, sources), "-o", str(output)]
    build = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    (evidence / "compile.stdout").write_text(build.stdout, encoding="utf-8")
    (evidence / "compile.stderr").write_text(build.stderr, encoding="utf-8")
    commands.append({"argv": argv, "exit": build.returncode,
                     "stdout": "compile.stdout", "stderr": "compile.stderr"})
    require(build.returncode == 0, f"focused host compile failed; see {evidence}/compile.stderr")
    require(not build.stderr, "strict host compile emitted diagnostics")
    suite = subprocess.run([str(output)], capture_output=True, text=True, timeout=15)
    (evidence / "synthetic.stdout").write_text(suite.stdout, encoding="utf-8")
    (evidence / "synthetic.stderr").write_text(suite.stderr, encoding="utf-8")
    commands.append({"argv": [str(output)], "exit": suite.returncode,
                     "stdout": "synthetic.stdout", "stderr": "synthetic.stderr"})
    require(suite.returncode == 0, "focused synthetic C controls failed")
    require(not suite.stderr, "focused synthetic controls emitted diagnostics")

    caps_output = evidence / "header_caps_test"
    caps_argv = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 "-I", str(ROOT), *map(str, sources[:3]), str(caps_source), "-o", str(caps_output)]
    caps_build = subprocess.run(caps_argv, capture_output=True, text=True, timeout=30)
    (evidence / "caps-compile.stdout").write_text(caps_build.stdout, encoding="utf-8")
    (evidence / "caps-compile.stderr").write_text(caps_build.stderr, encoding="utf-8")
    commands.append({"argv": caps_argv, "exit": caps_build.returncode,
                     "stdout": "caps-compile.stdout", "stderr": "caps-compile.stderr"})
    require(caps_build.returncode == 0, "focused header-cap compile failed")
    require(not caps_build.stderr, "strict header-cap compile emitted diagnostics")
    caps = subprocess.run([str(caps_output)], capture_output=True, text=True, timeout=15)
    (evidence / "caps.stdout").write_text(caps.stdout, encoding="utf-8")
    (evidence / "caps.stderr").write_text(caps.stderr, encoding="utf-8")
    commands.append({"argv": [str(caps_output)], "exit": caps.returncode,
                     "stdout": "caps.stdout", "stderr": "caps.stderr"})
    require(caps.returncode == 0, "focused header-cap controls failed")
    require(not caps.stderr, "focused header-cap controls emitted diagnostics")

    sample_receipts = []
    oracle_checks = 0
    for index, sample in enumerate(profile["samples"]):
        path = repo / sample["path"]
        data = path.read_bytes()
        require(len(data) == sample["bytes"], f"sample length changed: {sample['id']}")
        require(52 <= len(data) <= MAX_HEADER_BYTES, "sample exceeds shared header ceiling")
        require(sha256(data) == sample["sha256"], f"sample digest changed: {sample['id']}")
        before[str(path)] = sha256(data)
        expected = independent_decode(data)
        require(expected["header_bytes"] == sample["header_bytes"], "profile HeaderSize mismatch")
        for key in ("table_offset", "table_bytes", "name_count", "name_bytes_used", "binding_count"):
            require(expected[key] == sample[key], f"{sample['id']} profile {key} mismatch")
        actual = subprocess.run([str(output), "--actual", str(path), str(sample["logical_bytes"])],
                                capture_output=True, text=True, timeout=15)
        stdout_name, stderr_name = f"actual-{index}.stdout", f"actual-{index}.stderr"
        (evidence / stdout_name).write_text(actual.stdout, encoding="utf-8")
        (evidence / stderr_name).write_text(actual.stderr, encoding="utf-8")
        commands.append({"argv": [str(output), "--actual", str(path), str(sample["logical_bytes"])],
                         "exit": actual.returncode, "stdout": stdout_name, "stderr": stderr_name})
        require(actual.returncode == 0, f"C reader rejected retained {sample['id']}: {actual.stderr[:300]}")
        require(not actual.stderr, "actual header reader emitted diagnostics")
        require(len(actual.stdout.encode("utf-8")) <= 2 * 1024 * 1024, "C actual result exceeds output bound")
        c_result = json.loads(actual.stdout)
        require(c_result == expected, f"C reader/oracle mismatch: {sample['id']}")
        oracle_checks += 1
        sample_receipts.append({"id": sample["id"], "path": sample["path"], "bytes": len(data),
                                "sha256": sha256(data), **{key: expected[key] for key in
                                ("header_bytes", "table_offset", "table_bytes", "name_count",
                                 "name_bytes_used", "binding_count")}})

    receipt = {
        "schema_version": 1,
        "scope": "focused bounded parser proof; no package decoding, FPackageId resolution, store join, runtime/cook/platform/install claim",
        "compiler": {"path": str(compiler_path), "sha256": sha256(compiler_path.read_bytes()),
                     "version": compiler_version},
        "argv": argv,
        "synthetic_result": suite.stdout.strip(),
        "header_caps_result": caps.stdout.strip(),
        "caps_argv": caps_argv,
        "input_sha256": before,
        "commands": commands,
        "inputs_unchanged": all(sha256(Path(path).read_bytes()) == digest for path, digest in before.items()),
        "actual_oracle_samples": oracle_checks,
        "samples": sample_receipts,
        "source_pins": profile["source_pins"],
        "elapsed_seconds": round(time.monotonic() - started, 3),
    }
    require(receipt["inputs_unchanged"], "input changed during focused proof")
    (evidence / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"synthetic": suite.stdout.strip(), "independent_actual_samples": oracle_checks,
        "evidence": str(evidence / "receipt.json")}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"package-imports verification failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
