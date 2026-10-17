#!/usr/bin/env python3
"""Check pinned, already provisioned tools and sources; never download/install."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
import pathlib
import platform
import sys

ROOT = pathlib.Path(__file__).resolve().parent

def verify_files(root, files):
    root = root.resolve()
    for item in files:
        relative = pathlib.Path(item["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("dependency path escapes project: " + str(relative))
        path = root / relative
        if not path.resolve().is_relative_to(root):
            raise ValueError("dependency symlink escapes project: " + str(relative))
        if not path.is_file(): raise ValueError("missing local dependency: " + str(relative))
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""): digest.update(chunk)
        if digest.hexdigest() != item["sha256"]:
            raise ValueError("dependency hash mismatch: " + str(relative))
    return len(files)

def main():
    try:
        manifest = json.loads((ROOT / "third_party/manifest.json").read_text())
        if platform.system() != "Darwin" or platform.machine() != "arm64":
            raise ValueError("this pinned toolchain requires Darwin arm64")
        count = verify_files(ROOT, manifest["checked_files"])
    except (OSError, ValueError, KeyError) as error:
        print("local dependencies rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps({"checked_files": count, "host": platform.system(), "architecture": platform.machine(), "network_contact": False}))
    return 0

if __name__ == "__main__": sys.exit(main())
