#!/usr/bin/env python3
"""Offline build and independent-fixture checks for the read-only PAK library."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

HERE = Path(__file__).resolve().parent
FIXTURES = HERE / "tests/fixtures"
PROJECT_SOURCES = [HERE / name for name in (
    "pak_reader.c", "pak_entry_pipeline.c", "pak_entry_decoder.c",
    "pak_directory_parser.c", "bounded_zlib.c", "pak_payload.c")]


def require(condition, detail):
    # Verification must survive python -O / inherited PYTHONOPTIMIZE.
    if not condition:
        raise RuntimeError(detail)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mbedtls-source", type=Path, required=True)
    parser.add_argument("--zlib-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=HERE.parent / "build/pak")
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--ps5-sdk", type=Path)
    parser.add_argument("--llvm-config", type=Path)
    parser.add_argument("--resource-dir", type=Path)
    args = parser.parse_args()
    if args.ps5_sdk and not (args.llvm_config and args.resource_dir):
        parser.error("PS5 compilation requires --llvm-config and --resource-dir")
    if not args.ps5_sdk and (args.llvm_config or args.resource_dir):
        parser.error("target tool options require --ps5-sdk")
    deps = {"mbedtls": args.mbedtls_source.resolve(), "zlib": args.zlib_source.resolve()}
    lock = json.loads((HERE / "dependency-lock.json").read_text())
    for kind, root in deps.items():
        for record in lock[kind]["files"]:
            path = root / record["path"]
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
                raise SystemExit("Missing or mismatched pinned dependency: " + str(path))
    provenance = json.loads((FIXTURES / "provenance.json").read_text())
    for record in provenance["records"]:
        data = (FIXTURES / record["local"]).read_bytes()
        require(len(data) == record["bytes"], "Fixture length mismatch: " + record["local"])
        require(hashlib.sha256(data).hexdigest() == record["sha256"], "Fixture digest mismatch: " + record["local"])

    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    cc = args.cc
    common = ["-std=c11", "-O2", "-fno-strict-aliasing", "-Wall", "-Wextra",
              "-Wpedantic", "-Werror", "-DZ_SOLO", "-DNO_GZIP", "-DZLIB_CONST",
              '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"',
              "-I" + str(HERE), "-I" + str(deps["zlib"]),
              "-I" + str(deps["mbedtls"] / "include"),
              "-I" + str(deps["mbedtls"] / "library")]
    if args.ps5_sdk:
        cc = str(args.ps5_sdk.resolve() / "bin/prospero-clang")
        env["LLVM_CONFIG"] = str(args.llvm_config.resolve())
        common += ["-resource-dir", str(args.resource_dir.resolve())]
    sources = PROJECT_SOURCES + [deps["zlib"] / name for name in (
        "inflate.c", "inftrees.c", "inffast.c", "adler32.c", "zutil.c")]
    sources += [deps["mbedtls"] / "library" / name for name in ("sha1.c", "platform_util.c")]
    commands = []

    def run(argv, expected=0):
        proc = subprocess.run([str(a) for a in argv], env=env, capture_output=True, timeout=30)
        commands.append(dict(argv=[str(a) for a in argv], returncode=proc.returncode,
                             stderr=proc.stderr.decode(errors="replace")))
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        require(proc.returncode == expected, commands[-1])
        return proc.stdout

    target = "native" if args.ps5_sdk else "host"
    objects = []
    compiler_identity = run([cc, "--version"]).decode(errors="replace").strip()
    for source in sources:
        obj = out / (source.stem + ".o")
        run([cc, *common, "-c", source, "-o", obj])
        objects.append(obj)
    probes = {}
    for kind in ("metadata", "payload"):
        binary = out / (kind + ("_probe.elf" if args.ps5_sdk else "_probe"))
        run([cc, *common, HERE / "tests" / (kind + "_probe.c"), *objects,
             *(["-Wl,-z,now"] if args.ps5_sdk else []), "-o", binary])
        probes[kind] = binary

    results = []
    if not args.ps5_sdk:
        for archive in ("pack_v11.pak", "pack_v11_compress.pak"):
            metadata = json.loads(run([probes["metadata"], FIXTURES / archive]))
            require(metadata["stage"] == "complete" and metadata["directory_parse_complete"], "Incomplete fixture metadata")
            require(metadata["primary_sha1_checked"] and metadata["primary_sha1_matches"], "Primary digest unchecked")
            require(metadata["file_count"] == 4 and len(metadata["entries"]) == 4, "Fixture entry count mismatch")
            for entry in metadata["entries"]:
                actual = run([probes["payload"], FIXTURES / archive, str(entry["location"])])
                original = (FIXTURES / entry["name"]).read_bytes()
                require(actual == original, (archive, entry["name"], len(actual), len(original)))
                results.append(dict(archive=archive, name=entry["name"], codec=entry["method"],
                                    bytes=len(actual), matches_independent_original=True))
        # A stale digest must reject metadata and publish no payload bytes.
        corrupt = bytearray((FIXTURES / "pack_v11.pak").read_bytes())
        corrupt[len(corrupt) - 221 + 41] ^= 1
        path = out / "primary-hash-mismatch.pak"
        path.write_bytes(corrupt)
        rejected_output = run([probes["payload"], path, "0"], expected=2)
        require(rejected_output == b"", "Rejected payload published bytes")
    receipt = dict(result="PASS", target=target, target_executed=False, compiler_identity=compiler_identity,
                   compile_link_commands=len(commands), payload_matches=results,
                   installation_supported=False, game_acceptance=False,
                   artifacts={k: dict(path=str(v), bytes=v.stat().st_size) for k, v in probes.items()})
    (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(dict(result="PASS", target=target, independent_payload_matches=len(results),
                          target_executed=False, installation_supported=False)))


if __name__ == "__main__":
    main()
