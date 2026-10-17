#!/usr/bin/env python3
"""Validate and emit the three named, immutable native app assets."""
import argparse
import hashlib
import os
import pathlib
import re
import stat


ASSET_NAMES = ("index.html", "app.js", "style.css")
MAX_ASSET_BYTES = 256 * 1024
_HTML_SCRIPT = re.compile(br"<script\b[^>]*\bsrc\s*=\s*[\"']/app\.js[\"']", re.IGNORECASE)
_HTML_STYLE = re.compile(br"<link\b[^>]*\bhref\s*=\s*[\"']/style\.css[\"']", re.IGNORECASE)


def _read_regular_file(root, name):
    if pathlib.PurePosixPath(name).name != name or name not in ASSET_NAMES:
        raise ValueError("unknown asset name")
    path = root / name
    try:
        before = path.lstat()
    except OSError as error:
        raise ValueError("missing asset") from error
    if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
        raise ValueError("asset is not a regular non-symlink file")
    if before.st_size <= 0 or before.st_size > MAX_ASSET_BYTES:
        raise ValueError("asset size is outside bounds")
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise ValueError("asset open failed") from error
    try:
        opened = os.fstat(descriptor)
        if (not stat.S_ISREG(opened.st_mode) or opened.st_dev != before.st_dev or
                opened.st_ino != before.st_ino or opened.st_size != before.st_size):
            raise ValueError("asset changed during open")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            data = stream.read(MAX_ASSET_BYTES + 1)
    finally:
        os.close(descriptor)
    if len(data) != before.st_size or len(data) > MAX_ASSET_BYTES:
        raise ValueError("asset changed or exceeded bound while reading")
    try:
        data.decode("utf-8", "strict")
    except UnicodeError as error:
        raise ValueError("asset is not UTF-8") from error
    if b"\x00" in data:
        raise ValueError("asset contains NUL")
    return data


def inspect_assets(asset_root):
    """Read only the exact three named files and validate a same-origin index."""
    root = pathlib.Path(asset_root)
    if root.is_symlink() or not root.is_dir():
        raise ValueError("asset root is missing or is a symlink")
    assets = {name: _read_regular_file(root, name) for name in ASSET_NAMES}
    if sum(map(len, assets.values())) > MAX_ASSET_BYTES:
        raise ValueError("asset bundle exceeds total bound")
    index = assets["index.html"]
    if not _HTML_SCRIPT.search(index) or not _HTML_STYLE.search(index):
        raise ValueError("index must reference absolute /app.js and /style.css")
    return assets


def asset_identity(assets):
    if type(assets) is not dict or tuple(assets) != ASSET_NAMES:
        raise ValueError("asset set does not match the native allowlist")
    return {
        name: {"size": len(assets[name]), "sha256": hashlib.sha256(assets[name]).hexdigest()}
        for name in ASSET_NAMES
    }


def write_c_assets(assets, destination):
    """Emit exact bytes as static const arrays; the runtime never opens asset files."""
    identity = asset_identity(assets)
    destination = pathlib.Path(destination)
    lines = ['#include "http.h"', ""]
    for index, name in enumerate(ASSET_NAMES):
        symbol = ("index_html", "app_js", "style_css")[index]
        content = assets[name]
        lines.append(f"static const unsigned char web_{symbol}_bytes[] = {{")
        for offset in range(0, len(content), 16):
            block = content[offset:offset + 16]
            lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in block) + ",")
        lines.append("};")
        lines.append(f"const struct http_static_asset http_asset_{symbol} = {{")
        lines.append(f"    web_{symbol}_bytes, sizeof(web_{symbol}_bytes)")
        lines.append("};")
        lines.append("")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines), encoding="ascii")
    return identity


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-dir", type=pathlib.Path, required=True)
    parser.add_argument("--output-c", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)
    assets = inspect_assets(args.asset_dir)
    identity = write_c_assets(assets, args.output_c)
    print(identity)


if __name__ == "__main__":
    main()
