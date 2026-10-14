"""Compare a generated profile with an oracle (a JSON file of expected leaf values).

The oracle is *data supplied at run time* -- no expected value lives in this package.  Its keys are
dotted paths into the profile (``symbols.process_event.label``); values are compared as written in
the profile (hex strings for numbers).  Differences are reported, never "fixed": a row is
``match`` (equal), ``differs`` (the generator measured something else), or ``absent`` (the
generator does not produce that key).  An optional notes file adds the reason a difference is
expected.

    python3 -m tools.profilegen.validate --profile DIR/runtime-profile.json --oracle oracle.json [--notes notes.json]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from typing import Any, Dict, List, Optional

from . import emit_ue4ss

# oracle key -> profile key, for the few leaves the generator names differently
ALIASES: Dict[str, str] = {
    "symbols.sco_init.label": "symbols.assert_if_in_constructor.label",
    "symbols.sco_init.runtime": "symbols.assert_if_in_constructor.runtime",
}


def emit_extras(doc: Dict[str, Any]) -> Dict[str, Any]:
    """Values of the oracle's ``emit.*`` / ``ue4ss.settings.*`` keys, computed from the profile document."""
    out: Dict[str, Any] = {}
    head = emit_ue4ss.identity_header(doc)
    if head is not None:
        text = "\n".join(head) + "\n"
        out["emit.NativeLayout.header"] = text
        raw = text.encode("ascii")
        out["emit.VTableLayout.identity_only.bytes"] = "0x%x" % len(raw)
        out["emit.VTableLayout.identity_only.sha256"] = hashlib.sha256(raw).hexdigest()
    vt = emit_ue4ss.render_vtable_layout(doc)
    if vt is not None and "[UObject]" in vt:
        raw = vt.encode("ascii")
        out["emit.VTableLayout.with_process_event.bytes"] = "0x%x" % len(raw)
        out["emit.VTableLayout.with_process_event.sha256"] = hashlib.sha256(raw).hexdigest()
    mv = emit_ue4ss.render_member_layout(doc)
    if mv is not None:
        raw = mv.encode("ascii")
        out["emit.MemberVariableLayout.bytes"] = "0x%x" % len(raw)
        out["emit.MemberVariableLayout.sha256"] = hashlib.sha256(raw).hexdigest()
    st = emit_ue4ss.render_settings_fragment(doc)
    if st is not None:
        for line in st.splitlines():
            if line.startswith("MajorVersion="):
                out["ue4ss.settings.EngineVersionOverride.MajorVersion"] = "0x%x" % int(line.split("=")[1])
            if line.startswith("MinorVersion="):
                out["ue4ss.settings.EngineVersionOverride.MinorVersion"] = "0x%x" % int(line.split("=")[1])
    return out


def _is_env(node: Any) -> bool:
    return isinstance(node, dict) and "value" in node and "confidence" in node and "method" in node


def resolve(doc: Dict[str, Any], path: str) -> Optional[Dict[str, Any]]:
    """The envelope for ``path``; a path may continue *into* an envelope whose value is a dict."""
    path = ALIASES.get(path, path)
    node: Any = doc
    parts = path.split(".")
    for i, part in enumerate(parts):
        if _is_env(node):
            inner = node["value"]
            if isinstance(inner, dict) and part in inner:
                return {"value": inner[part], "confidence": node["confidence"], "method": node["method"],
                        "evidence": node["evidence"]}
            return None
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node if _is_env(node) else None


def _same(a: Any, b: Any) -> bool:
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, str) and isinstance(b, str):
        try:
            if a.lower().startswith(("0x", "-0x")) and b.lower().startswith(("0x", "-0x")):
                return int(a, 16) == int(b, 16)
        except ValueError:
            pass
    return a == b


def compare(doc: Dict[str, Any], oracle: Dict[str, Any], notes: Optional[Dict[str, str]] = None,
            extra: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
    """One row per oracle key.  ``extra`` maps keys the profile cannot hold (``emit.*``) to generated values."""
    notes = notes or {}
    extra = extra or {}
    rows = []
    for key, spec in oracle.items():
        want = spec["value"] if isinstance(spec, dict) and "value" in spec else spec
        row = {"key": key, "oracle": want, "generated": None, "match": None, "confidence": "", "method": "",
               "note": notes.get(key, "")}
        if key in extra:
            got = extra[key]
            row.update(generated=got, match="match" if _same(got, want) else "differs", method="emit_ue4ss")
        else:
            env = resolve(doc, key)
            if env is None:
                row["match"] = "absent"
            else:
                row.update(generated=env["value"], confidence=env["confidence"], method=env["method"],
                           match="match" if _same(env["value"], want) else "differs")
        rows.append(row)
    return rows


def summary(rows: List[Dict[str, Any]]) -> Dict[str, int]:
    out = {"match": 0, "differs": 0, "absent": 0}
    for r in rows:
        out[r["match"]] += 1
    return out


def _cell(v: Any, limit: int = 44) -> str:
    s = "-" if v is None else str(v)
    s = s.replace("|", "\\|").replace("\n", " ")
    return s if len(s) <= limit else s[:limit - 3] + "..."


def render_markdown(rows: List[Dict[str, Any]], only: Optional[List[str]] = None) -> str:
    lines = ["| key | generated | oracle | match | confidence | method | note |", "|---|---|---|---|---|---|---|"]
    for r in rows:
        if only is not None and r["key"] not in only:
            continue
        lines.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            r["key"], _cell(r["generated"]), _cell(r["oracle"]), {"match": "yes", "differs": "NO", "absent": "not produced"}[r["match"]],
            r["confidence"] or "-", _cell(r["method"], 36), _cell(r["note"], 120)))
    return "\n".join(lines) + "\n"


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.profilegen.validate", description=__doc__)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--oracle", required=True)
    ap.add_argument("--notes")
    ap.add_argument("--markdown", action="store_true")
    ns = ap.parse_args(argv)
    def load(path):
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    doc = load(ns.profile)
    oracle = load(ns.oracle)
    notes = load(ns.notes) if ns.notes else None
    rows = compare(doc, oracle, notes, emit_extras(doc))
    if ns.markdown:
        sys.stdout.write(render_markdown(rows))
    else:
        for r in rows:
            print("%-9s %-58s generated=%s oracle=%s" % (r["match"], r["key"], r["generated"], r["oracle"]))
    s = summary(rows)
    print("match %(match)d, differs %(differs)d, not produced %(absent)d" % s, file=sys.stderr)
    return 0 if s["differs"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
