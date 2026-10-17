#!/usr/bin/env python3
"""Adapters to sibling modules. Interface-independent fixture helpers live
here now; the actual flow bindings are added once each module's interface is
discovered (see review_static.py output). Unbound flows raise SkipTarget.
"""

import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import ZipKit, module_dir  # noqa: E402


class SkipTarget(Exception):
    """Raised when the required module/binding doesn't exist yet."""


# ---------------------------------------------------------------- fixtures
# (interface-independent — usable by any adapter binding)

def fixture_good_mod(sb):
    z = ZipKit(sb.path)
    return z.normal("goodmod.zip", {
        "mods/GoodMod/init.lua": (
            'local M = {}\nfunction M.onLoad()\n  FindFirstOf("Item")\nend\n'
            'return M\n'),
        "mods/GoodMod/manifest.json": json.dumps({
            "name": "GoodMod", "version": "1.0.0",
            "files": ["init.lua"], "api": ["FindFirstOf"]}),
    })


def fixture_conflicting_mods(sb):
    """Two mods that both claim the same file (classic conflict)."""
    z = ZipKit(sb.path)
    shared = "mods/Shared/overlay.lua"
    a = z.normal("modA.zip", {
        "mods/ModA/init.lua": "-- A\n",
        "mods/ModA/manifest.json": json.dumps(
            {"name": "ModA", "files": ["init.lua", "../Shared/overlay.lua"]}),
        shared: "-- A's overlay\n",
    })
    b = z.normal("modB.zip", {
        "mods/ModB/init.lua": "-- B\n",
        "mods/ModB/manifest.json": json.dumps(
            {"name": "ModB", "files": ["init.lua", "../Shared/overlay.lua"]}),
        shared: "-- B's overlay\n",
    })
    return a, b


def tamper(mod_path, target_root):
    """Tamper an installed mod: flip a byte in every .lua under target_root
    that matches the mod name; if none, append junk to a random file."""
    hits = 0
    for root, _d, files in os.walk(target_root):
        for fn in files:
            p = os.path.join(root, fn)
            if fn.endswith((".lua", ".json")) and os.path.getsize(p) > 0:
                with open(p, "rb") as f:
                    data = bytearray(f.read())
                data[len(data) // 2] ^= 0xFF
                with open(p, "wb") as f:
                    f.write(bytes(data))
                hits += 1
                break  # one file is enough tampering
        if hits:
            break
    if not hits:
        p = os.path.join(target_root, "tamper.marker")
        with open(p, "wb") as f:
            f.write(b"x")
    return hits


# ---------------------------------------------------------------- flows
# bindings filled in after interface discovery

def require_flow():
    f = Flow()
    missing = f.missing()
    if missing:
        raise SkipTarget(
            f"flow bindings not resolved (missing: {missing}); "
            "run discovery once modules exist")
    return f


class Flow:
    def __init__(self):
        self._bind = {}

    def missing(self):
        return [k for k in ("detect", "compat_check", "install", "verify",
                            "uninstall", "verify_clean", "conflict_check")
                if k not in self._bind]

    # each method delegates to a binding, else raises SkipTarget
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        def missing(*a, **kw):
            raise SkipTarget(f"no binding for '{name}'")
        return self._bind.get(name, missing)


def is_conflict(result):
    """Heuristic: does a conflict-check result say 'conflict'?"""
    if result is None:
        return False
    if isinstance(result, bool):
        return result
    if isinstance(result, (dict,)):
        for k in ("conflict", "conflicts", "blocked", "ok", "compatible"):
            if k in result:
                v = result[k]
                if isinstance(v, bool):
                    return v if k in ("conflict", "conflicts", "blocked") \
                        else not v
                if v:
                    return bool(v)
    if isinstance(result, (list, tuple)):
        return len(result) > 0
    return False


def verify_ok(result):
    if isinstance(result, bool):
        return result
    if isinstance(result, dict) and "ok" in result:
        return bool(result["ok"])
    if isinstance(result, (list, tuple)) and len(result) == 2:
        return bool(result[0])
    return bool(result)
