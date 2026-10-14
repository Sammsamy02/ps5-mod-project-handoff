#!/usr/bin/env python3
"""Assemble checked source/text continuation inputs; never build or publish code."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
SPIKE = ".superpowers/native-dependency-spike/"
FNAME = SPIKE + "ue4ss-native-fname-conv-bootstrap-1/"
CHILD = SPIKE + "native-cppmod-child-lifecycle-1/"
SYNC = SPIKE + "native-canonical-container-sync-1/"
COMPOSE = SPIKE + "native-coherent-fname-runtime-1/compose.py"
BASE_PUBLICATION = "outputs/2026-10-03-native-cpp-loader/PUBLICATION.json"
BASE = {
    "repository": "Sammsamy02/ps5",
    "branch": "codex/runtime-handoff-2026-10-03",
    "commit": "6b3cce37807adc9c75af19f138de764a814d13c6",
    "remote_directory": "handoff/native-cpp-loader-2026-10-03",
    "archive_name": "native-cpp-loader-source.zip",
    "archive_sha256": "267cbacf438a9c4a896c7801e1a6babe43e1846574f99d652befac1fcffa8061",
    "requires_prior_handoffs": True,
}
MANIFESTS = {
    FNAME + "export-files.json": "b7d0a9c8137d803e8b0ec9be7884bdc7188a2b03ac797ceba4962ca3f7cd7e15",
    CHILD + "export-files.json": "ebad4b301946d6a7e3363b8be67fa5eb3c1152ffb95adff59309c52554ea81b0",
}
FIXED = {
    SYNC + "receipt.json": "89c0d55a2bdf99c3b31f05d185f772d2f236b20d22163012d9a5241d1948b3f2",
    COMPOSE: "988701879d2d04c44e9193cd4cc6dd4cbdfb6789c88b363074c929b1dae1862b",
    SPIKE + "native-coherent-fname-runtime-1/README.md": "8b730f7b790467df52a0dd2b0d5bf6593c0147e5975cf68c7fd6ded551fa036a",
}
TEXT_SUFFIXES = {
    ".c", ".h", ".cpp", ".hpp", ".cs", ".csproj", ".props", ".config",
    ".py", ".md", ".json", ".ini", ".patch", ".txt", ".log", ".stdout",
    ".stderr", ".ll", ".s", ".rs", ".toml", ".lock", ".sh",
}
MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_TOTAL_BYTES = 32 * 1024 * 1024


def fail(message):
    raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(name):
    if not isinstance(name, str) or not name or "\\" in name:
        fail(f"invalid repository-relative path: {name!r}")
    p = PurePosixPath(name)
    if p.is_absolute() or str(p) != name or any(x in (".", "..") for x in p.parts):
        fail(f"noncanonical repository-relative path: {name}")
    if any(ord(c) < 32 or ord(c) == 127 for c in name):
        fail(f"control byte in path: {name!r}")
    lowered = name.lower()
    if any(x in lowered for x in ("nexus-test", "native-observations/", "/dotnet-home/", "/.git/")):
        fail(f"excluded input: {name}")
    if any(x.lower() in ("secrets", "credentials", ".ssh", ".aws", ".env", "obj", "bin") for x in p.parts):
        fail(f"excluded input component: {name}")
    if p.suffix not in TEXT_SUFFIXES and p.name not in ("LICENSE", "SharpProspero-LICENSE"):
        fail(f"not an allowed source/text file: {name}")
    q = ROOT
    for part in p.parts:
        q = q / part
        if q.is_symlink():
            fail(f"symlink input is not allowed: {name}")
    if not q.is_file() or not q.resolve().is_relative_to(ROOT):
        fail(f"missing or external input: {name}")
    return q


def checked(name, expected=None, expected_bytes=None):
    path = safe_path(name)
    if path.stat().st_size > MAX_FILE_BYTES:
        fail(f"input exceeds compact-source limit: {name}")
    data = path.read_bytes()
    if len(data) > MAX_FILE_BYTES or b"\x00" in data:
        fail(f"nontext or oversized input: {name}")
    data.decode("utf-8")
    actual = sha(data)
    if expected is not None and actual != expected:
        fail(f"reviewed input changed: {name}; expected {expected}, got {actual}")
    if expected_bytes is not None and len(data) != expected_bytes:
        fail(f"input byte count changed: {name}")
    return {"path": name, "bytes": len(data), "sha256": actual}, data


def pinned(value):
    name, separator, expected = value.rpartition("=")
    if not separator or not re.fullmatch(r"[0-9a-f]{64}", expected):
        fail("expected repository-relative PATH=SHA256")
    return name, expected


def assemble(args):
    if args.final and not args.activation_allowlist:
        fail("final packaging requires the root-supplied --activation-allowlist PATH=SHA256")
    pub_row, pub_data = checked(BASE_PUBLICATION)
    pub = json.loads(pub_data)
    for key in ("repository", "branch", "commit", "remote_directory"):
        if pub.get(key) != BASE[key]:
            fail(f"prior handoff identity mismatch: {key}")
    prior_archive = [r for r in pub["files"] if r["name"] == BASE["archive_name"]]
    if len(prior_archive) != 1 or prior_archive[0]["sha256"] != BASE["archive_sha256"]:
        fail("prior handoff archive identity mismatch")

    files, content, origins = {}, {}, {}

    def add(name, expected=None, size=None, origin="fixed"):
        row, data = checked(name, expected, size)
        if name in files and files[name] != row:
            fail(f"conflicting input pins: {name}")
        files[name], content[name] = row, data
        origins.setdefault(name, set()).add(origin)
        if sum(r["bytes"] for r in files.values()) > MAX_TOTAL_BYTES:
            fail("combined source/text input exceeds 32 MiB limit")
        return data

    def add_manifest(name, expected):
        doc = json.loads(add(name, expected, origin="allowlist"))
        rows = doc.get("files")
        if not isinstance(rows, list) or not rows:
            fail(f"allowlist must contain a nonempty files array: {name}")
        for row in rows:
            if not isinstance(row, dict) or not isinstance(row.get("bytes"), int) or row["bytes"] < 0:
                fail(f"allowlist requires path/bytes/sha256 records: {name}")
            if not re.fullmatch(r"[0-9a-f]{64}", row.get("sha256", "")):
                fail(f"invalid source hash in allowlist: {name}")
            add(row["path"], row["sha256"], row["bytes"], name)

    add(BASE_PUBLICATION, pub_row["sha256"], pub_row["bytes"], "dependency identity")
    for name, expected in MANIFESTS.items():
        add_manifest(name, expected)
    for name, expected in FIXED.items():
        add(name, expected)
    sync = json.loads(content[SYNC + "receipt.json"])
    for row in sync["files"]:
        add(row["path"], row["sha256"], origin=SYNC + "receipt.json")
    if args.activation_allowlist:
        add_manifest(*pinned(args.activation_allowlist))
    for value in args.extra_file:
        add(*pinned(value), origin="explicit extra file")
    for name in ("build_archive.py", "README.md"):
        add((OUT / name).relative_to(ROOT).as_posix(), origin="packaging recipe")
    missing = [] if args.activation_allowlist else ["native activation allowlist"]
    manifest = {
        "schema": 1,
        "assembly": "final" if args.final else "preview",
        "scope": "Explicit source, recipes and compact text evidence; no build artifacts or game captures.",
        "dependency": {**BASE, "publication": pub_row},
        "missing_components": missing,
        "input_allowlists": [{"path": p, "sha256": files[p]["sha256"]}
                             for p in sorted(files) if "allowlist" in origins[p]],
        "activation_allowlist_argument": args.activation_allowlist,
        "extra_file_arguments": args.extra_file,
        "files": [files[p] for p in sorted(files)],
        "build_or_target_execution": False,
        "publication_performed": False,
        "checkpoint_modified": False,
    }
    return manifest, content


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--activation-allowlist", metavar="PATH=SHA256")
    parser.add_argument("--extra-file", action="append", default=[], metavar="PATH=SHA256")
    parser.add_argument("--final", action="store_true", help="assemble zip; requires activation allowlist")
    parser.add_argument("--output", help="new output directory below this recipe directory")
    args = parser.parse_args()
    manifest, content = assemble(args)
    destination = OUT / (args.output or ("build/final" if args.final else "build/preview"))
    if not destination.resolve().is_relative_to(OUT) or destination.resolve() == OUT:
        fail("output must be a new subdirectory of this recipe directory")
    if destination.exists():
        fail(f"output already exists; use a new --output directory: {destination}")
    # Recheck every source after assembly before writing any package output.
    for row in manifest["files"]:
        checked(row["path"], row["sha256"], row["bytes"])
    destination.mkdir(parents=True)
    manifest_data = (json.dumps(manifest, indent=2) + "\n").encode()
    (destination / "MANIFEST.json").write_bytes(manifest_data)
    summary = {"assembly": manifest["assembly"], "source_files": len(content),
               "source_bytes": sum(map(len, content.values())),
               "missing_components": manifest["missing_components"]}
    if args.final:
        archive_path = destination / "native-plugin-activation-source.zip"
        entries = {**content, "NATIVE-PLUGIN-ACTIVATION-MANIFEST.json": manifest_data}
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name, data in sorted(entries.items()):
                info = zipfile.ZipInfo(name, (2026, 10, 3, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                z.writestr(info, data)
        with zipfile.ZipFile(archive_path) as z:
            if z.testzip() is not None or z.namelist() != sorted(entries):
                fail("archive CRC or member-list verification failed")
            for name, data in entries.items():
                if z.read(name) != data:
                    fail(f"archive content verification failed: {name}")
        archive = archive_path.read_bytes()
        summary.update(archive_bytes=len(archive), archive_sha256=sha(archive),
                       archive_crc_and_all_file_hashes_verified=True)
        (destination / "ARCHIVE.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({**summary, "output": str(destination)}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(f"packaging refused: {error}", file=sys.stderr)
        sys.exit(1)
