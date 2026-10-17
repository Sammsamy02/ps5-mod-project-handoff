"""Assemble the profile document (``modbridge.runtime-profile/1``) from a finished detection context.

Rules enforced here, not in the detectors:

* every leaf of the data sections is an envelope ``{value, confidence, method, evidence}``;
* ``symbols.<name>.runtime`` is always ``label + load.base`` and carries the weaker of the two
  confidences (an address is only as good as the base it is shifted by);
* ``status`` is ``"generated"`` and ``verification`` is ``null``: this tool never claims a console
  verification, and refuses to write a document that does.
"""
from __future__ import annotations

import hashlib
import json
import os
import time
from typing import Any, Dict, List, Optional

from .model import Context, Envelope, Hex, weakest

SCHEMA = "modbridge.runtime-profile/1"
from . import __version__

GENERATOR = {"name": "profilegen", "version": __version__}
DIGEST_SECTIONS = ("binding", "elf", "load", "build", "symbols", "structs", "vtables", "ue4ss")
DATA_SECTIONS = DIGEST_SECTIONS + ("version_map",)


def _nest(values: Dict[str, Envelope]) -> Dict[str, Any]:
    root: Dict[str, Any] = {}
    for path in values:
        node = root
        parts = path.split(".")
        for p in parts[:-1]:
            nxt = node.get(p)
            if nxt is None:
                nxt = node[p] = {}
            elif "value" in nxt and "confidence" in nxt:          # leaf in the way: should not happen
                raise ValueError("path %s runs through the leaf %s" % (path, p))
            node = nxt
        if parts[-1] in node:
            raise ValueError("path %s is both a leaf and a section" % path)
        node[parts[-1]] = values[path].to_json()
    return root


def add_runtime_addresses(ctx: Context) -> None:
    base = ctx.get("load.base")
    if base is None:
        return
    for path in [p for p in list(ctx.values) if p.startswith("symbols.") and p.endswith(".label")]:
        env = ctx.values[path]
        if not isinstance(env.value, int):
            continue
        rt = path[:-len("label")] + "runtime"
        ctx.put(rt, Hex(int(env.value) + int(base.value)), weakest(env.confidence, base.confidence), "label + load.base",
                ["label 0x%x + load.base 0x%x (%s)" % (int(env.value), int(base.value), base.method)])


def version_map(ctx: Context) -> None:
    title = ctx.val("binding.title_id")
    version = ctx.val("binding.content_version")
    directory = ctx.options.get("game_profiles_dir")
    if not title or not version:
        return
    row = None
    path = None
    if directory:
        cand = os.path.join(directory, "%s.json" % title)
        if os.path.isfile(cand):
            path = cand
            try:
                with open(cand, encoding="utf-8") as fh:
                    data = json.load(fh)
            except (OSError, ValueError):
                data = {}
            for b in data.get("builds", []):
                if b.get("console_content_version") == version:
                    row = b
    if row is not None:
        ctx.put("version_map.pc_build", row.get("pc_build"), "high" if row.get("pc_build") else "low", "game-profile.builds",
                ["row for console_content_version %s in %s" % (version, os.path.basename(path))])
        if row.get("pc_build") is None:
            ctx.fail("version_map.pc_build", "the game profile has no PC build recorded for content version %s" % version,
                     "upstream-source")
    else:
        ctx.fail("version_map.pc_build", "no game-profile row for %s content version %s" % (title, version), "upstream-source")


def canonical(obj: Any) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def profile_digest(doc: Dict[str, Any]) -> str:
    body = {k: doc[k] for k in DIGEST_SECTIONS if k in doc}
    return hashlib.sha256(canonical(body).encode("ascii")).hexdigest()


def assemble(ctx: Context, emitted: Optional[List[dict]] = None, utc: Optional[str] = None) -> Dict[str, Any]:
    add_runtime_addresses(ctx)
    version_map(ctx)
    nested = _nest(ctx.values)
    elf = ctx.elf
    title = ctx.val("binding.title_id", "unknown")
    version = ctx.val("binding.content_version", "unknown")
    sha = ctx.val("binding.executable.sha256", "")
    doc: Dict[str, Any] = {
        "schema": SCHEMA,
        "profile_id": "%s/%s/%s" % (title, version, sha[:8]),
        "status": "generated",
        "profile_digest": None,
        "verification": None,
        "generated": {
            "generator": dict(GENERATOR),
            "utc": utc or time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "inputs": [{"role": "executable", "bytes": elf.size, "sha256": sha}] + (
                [{"role": "param.json", "bytes": int(ctx.val("binding.param_json.bytes", 0)),
                  "sha256": ctx.val("binding.param_json.sha256")}] if ctx.get("binding.param_json.sha256") else []),
            "capabilities": {"capstone": ctx.index.decoder is not None},
        },
    }
    for section in DATA_SECTIONS:
        if section in nested:
            doc[section] = nested[section]
    extra = [k for k in nested if k not in DATA_SECTIONS]
    if extra:
        raise ValueError("detector wrote outside the data sections: %s" % extra)
    doc["emit"] = list(emitted or [])
    doc["unresolved"] = [u.to_json() for u in sorted(ctx.unresolved, key=lambda u: u.id)]
    doc["profile_digest"] = profile_digest(doc)
    if doc["status"] != "generated" or doc["verification"] is not None:        # never an assert: must survive -O
        raise ValueError("profilegen only ever produces status 'generated' with no verification block")
    return doc


def confidence_counts(doc: Dict[str, Any]) -> Dict[str, int]:
    counts = {"high": 0, "medium": 0, "low": 0}

    def walk(node):
        if isinstance(node, dict):
            if "confidence" in node and "value" in node and "method" in node:
                counts[node["confidence"]] += 1
            else:
                for v in node.values():
                    walk(v)
    for s in DATA_SECTIONS:
        if s in doc:
            walk(doc[s])
    return counts


def iter_envelopes(doc: Dict[str, Any]):
    """Yield (dotted path, envelope dict) for every leaf of the data sections."""
    def walk(node, path):
        if isinstance(node, dict):
            if "confidence" in node and "value" in node and "method" in node:
                yield ".".join(path), node
            else:
                for k, v in node.items():
                    yield from walk(v, path + [k])
    for s in DATA_SECTIONS:
        if s in doc:
            yield from walk(doc[s], [s])
