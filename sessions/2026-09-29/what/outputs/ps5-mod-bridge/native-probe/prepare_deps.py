#!/usr/bin/env python3
"""Prepare already downloaded official archives inside this probe's build folder.

No downloads or global installation. Hash archives before opening them, validate
every member/link, and publish a selective extraction only after validation.
"""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
import pathlib
import platform
import shutil
import stat
import sys
import tarfile
import tempfile
import zipfile
from verify_dependencies import ROOT, verify_files

PACKAGE = "LLVM-21.1.8-macOS-ARM64"
TOOLS = {"clang", "clang-21", "lld", "ld.lld", "llvm-config", "llvm-readobj", "llvm-readelf", "llvm-objdump", "llvm-nm"}

def safe_name(name):
    path = pathlib.PurePosixPath(name)
    if not name or "\0" in name or "\\" in name or path.is_absolute() or ".." in path.parts or not path.parts:
        raise ValueError("unsafe archive member: " + repr(name))
    return str(path)

def safe_link(name, target):
    safe_name(name)
    if not target or "\0" in target or "\\" in target or pathlib.PurePosixPath(target).is_absolute():
        raise ValueError("unsafe archive link: " + repr(target))
    parts = list(pathlib.PurePosixPath(name).parent.parts)
    for part in pathlib.PurePosixPath(target).parts:
        if part == "..":
            if not parts: raise ValueError("archive link escapes extraction root")
            parts.pop()
        elif part != ".": parts.append(part)
    if not parts or parts[0] != pathlib.PurePosixPath(name).parts[0]:
        raise ValueError("archive link escapes package")
    return safe_name("/".join(parts))

def selected(name):
    path = pathlib.PurePosixPath(name)
    if len(path.parts) < 2 or path.parts[0] != PACKAGE: return False
    return ((len(path.parts) == 3 and path.parts[1] == "bin" and path.name in TOOLS)
            or path.name in ("LICENSE.TXT", "LICENSE", "NOTICE"))

def extract_tools(archive, destination):
    if destination.exists() or destination.is_symlink(): raise ValueError("tool extraction already exists")
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = pathlib.Path(tempfile.mkdtemp(prefix="llvm-stage-", dir=destination.parent))
    links = []
    try:
        with tarfile.open(archive, "r|*") as source:
            for member in source:
                name = safe_name(member.name)
                if member.issym(): safe_link(name, member.linkname)
                elif member.islnk(): safe_name(member.linkname)
                elif not (member.isfile() or member.isdir()): raise ValueError("special archive member is forbidden")
                if not selected(name): continue
                path = staging / name
                if member.issym():
                    links.append((path, member.linkname)); continue
                if member.islnk(): raise ValueError("selected hard link is unsupported")
                if not member.isfile(): continue
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.exists(): raise ValueError("duplicate selected archive member")
                with source.extractfile(member) as incoming, path.open("xb") as outgoing:
                    shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
                path.chmod(0o755 if path.parent.name == "bin" else 0o644)
        for path, target in links:
            resolved = (path.parent / target).resolve()
            if not resolved.is_relative_to(staging.resolve()) or not resolved.is_file():
                raise ValueError("selected symlink target is missing/unsafe")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(target)
        staging.rename(destination)
    finally:
        if staging.exists(): shutil.rmtree(staging)

def verify_sdk(archive, checked):
    expected = {item["path"]: item["sha256"] for item in checked}
    included = {"ps5-payload-sdk/target/include/ps5/payload.h": "third_party/sdk-public-payload.h", "ps5-payload-sdk/ldscripts/elf_x86_64.x": "third_party/elf_x86_64.x"}
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            name = safe_name(member.filename)
            if stat.S_ISLNK(member.external_attr >> 16):
                safe_link(name, source.read(member).decode("utf-8"))
        for name, local in included.items():
            if hashlib.sha256(source.read(name)).hexdigest() != expected[local]:
                raise ValueError("SDK release/source mismatch: " + name)

def main():
    try:
        if platform.system() != "Darwin" or platform.machine() != "arm64":
            raise ValueError("pinned archive requires Darwin arm64")
        if shutil.disk_usage(ROOT).free < 12 * 1024**3:
            raise ValueError("at least 12 GiB free space required for preparation")
        manifest = json.loads((ROOT / "third_party/manifest.json").read_text())
        archives = manifest["archives"]
        archive_files = [{"path": a["local_path"], "sha256": a["sha256"]} for a in archives]
        verify_files(ROOT, archive_files)
        for item in archives:
            if (ROOT / item["local_path"]).stat().st_size != item["size"]:
                raise ValueError("archive length mismatch: " + item["name"])
        verify_sdk(ROOT / archives[0]["local_path"], manifest["checked_files"])
        destination = ROOT / "build/deps/llvm"
        if not destination.exists(): extract_tools(ROOT / archives[1]["local_path"], destination)
        count = verify_files(ROOT, manifest["checked_files"])
    except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile) as error:
        print("local archive preparation failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps({"archives_verified": len(archives), "checked_files": count, "extraction_root": str(destination), "network_contact": False}))
    return 0

if __name__ == "__main__": sys.exit(main())
