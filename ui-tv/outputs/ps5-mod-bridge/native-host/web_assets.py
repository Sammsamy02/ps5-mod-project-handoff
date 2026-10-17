#!/usr/bin/env python3
"""Validate UI sources and emit the same three immutable native app assets."""
import argparse
import hashlib
from html.parser import HTMLParser
import os
import pathlib
import re
import stat


ASSET_NAMES = ("index.html", "app.js", "style.css")
SCRIPT_NAMES = ("controller-keyboard.js", "config.js", "installer-service.js",
                "installer-mock.js", "installer-ui.js", "app.js")
MAX_ASSET_BYTES = 256 * 1024
_HTML_STYLE = re.compile(br"<link\b[^>]*\bhref\s*=\s*[\"']/style\.css[\"']", re.IGNORECASE)


def _read_regular_file(root, name):
    if pathlib.PurePosixPath(name).name != name or name not in ASSET_NAMES + SCRIPT_NAMES:
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


class _Scripts(HTMLParser):
    """Record exact script spans; only ordinary same-origin source scripts bundle."""
    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.source = source
        self.line_offsets = [0] + [match.end() for match in re.finditer("\n", source)]
        self.active = None
        self.scripts = []

    def source_offset(self):
        line, column = self.getpos()
        return self.line_offsets[line - 1] + column

    def handle_starttag(self, tag, attrs):
        if tag != "script":
            return
        names = [name for name, _value in attrs]
        attributes = dict(attrs)
        source = attributes.get("src")
        if (self.active is not None or len(set(names)) != len(names) or
                set(names) - {"src", "defer"} or
                source not in {"/" + name for name in SCRIPT_NAMES}):
            raise ValueError("index contains an unknown or nonordinary script")
        self.active = (self.source_offset(), source[1:])

    def handle_startendtag(self, tag, attrs):
        if tag == "script":
            raise ValueError("script must have an explicit closing tag")

    def handle_data(self, data):
        if self.active is not None and data.strip():
            raise ValueError("inline script content is not permitted")

    def handle_endtag(self, tag):
        if tag != "script":
            return
        if self.active is None:
            raise ValueError("script closing tag has no source")
        closing = re.match(r"</script\s*>", self.source[self.source_offset():], re.IGNORECASE)
        if closing is None:
            raise ValueError("invalid script closing tag")
        start, name = self.active
        self.scripts.append((start, self.source_offset() + closing.end(), name))
        self.active = None


def _bundle_scripts(root, assets):
    source = assets["index.html"].decode("utf-8")
    parser = _Scripts(source)
    parser.feed(source)
    parser.close()
    names = [name for _start, _end, name in parser.scripts]
    if parser.active is not None or names.count("app.js") != 1 or len(set(names)) != len(names):
        raise ValueError("index must reference each source once, including /app.js")
    if len(names) == 1:
        return assets
    scripts = [assets[name] if name == "app.js" else _read_regular_file(root, name) for name in names]
    # A separator also prevents a trailing // comment or expression from
    # changing the meaning of the following independently authored module.
    bundle = b"\n;\n".join(scripts)
    for start, end, name in reversed(parser.scripts):
        if name != "app.js":
            source = source[:start] + source[end:]
    return {"index.html": source.encode("utf-8"), "app.js": bundle, "style.css": assets["style.css"]}


def inspect_assets(asset_root):
    """Read known referenced sources; preserve the exact native three-asset ABI."""
    root = pathlib.Path(asset_root)
    if root.is_symlink() or not root.is_dir():
        raise ValueError("asset root is missing or is a symlink")
    assets = {name: _read_regular_file(root, name) for name in ASSET_NAMES}
    if not _HTML_STYLE.search(assets["index.html"]):
        raise ValueError("index must reference absolute /style.css")
    assets = _bundle_scripts(root, assets)
    if sum(map(len, assets.values())) > MAX_ASSET_BYTES:
        raise ValueError("asset bundle exceeds total bound")
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
