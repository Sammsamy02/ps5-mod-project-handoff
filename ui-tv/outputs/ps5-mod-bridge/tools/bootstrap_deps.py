#!/usr/bin/env python3
"""Verify vendored dependency bytes offline; opt in to fetch only missing pins."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.request


def verify(root: Path, fetch_missing: bool = False) -> None:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Unsafe dependency path: {relative}")
        path = root / relative
        if not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Dependency path escapes vendor directory: {relative}")
        expected = entry["sha256"]
        commit = entry["commit"]
        prefix = f'https://raw.githubusercontent.com/{entry["repository"]}/{commit}/'
        url = entry["url"]
        if not re.fullmatch(r"[0-9a-f]{40}", commit) or not url.startswith(prefix):
            raise ValueError(f"Dependency URL is not pinned to its recorded commit: {relative}")
        if not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError(f"Invalid dependency digest: {relative}")
        if not path.exists():
            if not fetch_missing:
                raise ValueError(f"Missing vendored file: {relative}; use --fetch-missing to fetch this pin")
            with urllib.request.urlopen(url, timeout=30) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != expected:
                raise ValueError(f"Downloaded dependency hash mismatch: {relative}")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError(f"Vendored dependency hash mismatch: {relative}")
    print(f'Verified {len(manifest["files"])} pinned dependency files')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch-missing", action="store_true")
    parser.add_argument("--vendor-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / "third_party")
    args = parser.parse_args()
    try:
        verify(args.vendor_root, args.fetch_missing)
    except (OSError, ValueError, KeyError) as error:
        print(f"Dependency verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
