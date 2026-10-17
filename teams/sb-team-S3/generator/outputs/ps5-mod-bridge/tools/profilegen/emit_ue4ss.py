"""Render the UE4SS files from the profile document (never from detector internals).

* ``VTableLayout.ini``        ``[NativeLayout]`` identity header + ``[UObject] ProcessEvent=0x..``
* ``MemberVariableLayout.ini`` identity header + ``[FUObjectArray]`` / ``[TUObjectArray]``
* ``UE4SS-settings.ini``      a *fragment*: ``[EngineVersionOverride]`` with the per-game keys

Rules (the retained PS5 parser's, and the gated assembler's):

* LF line endings, one trailing newline, one blank line between sections, hex offsets as
  ``0x`` + upper-case digits, only whole-line ``;`` comments, no section with only comments.
* A file is written only when the identity header is complete; a class only when *every* one of its
  members was derived with at least ``medium`` confidence (a half-filled class would let the
  runtime trust offsets that were never measured).
* The settings fragment pins ``DebugBuild`` / ``Stats`` only when that value is ``high``;
  otherwise the key is omitted and UE4SS keeps detecting.

The *extended* layout (what ``emit`` writes; ``render_*(doc)`` without ``extended`` renders only the original
classes, which is what the staged-pair comparison uses) adds the reflection classes the layout detectors measure
(``UObjectBase``, ``FUObjectItem``, ``UField``, ``UStruct``, ``UClass``, ``UFunction``, ``FField``, ``FFieldClass``,
``FProperty`` and the property subtypes) and the vtable slots ``[UEngine] Tick`` / ``[FProperty] ...``.  For these:

* only members with confidence >= medium are written, and only for a class whose ``UEP_TotalSize`` is itself >= medium
  (the port refuses a class without a positive size, and any member at or beyond it);
* only keys the port's binding accepts (``data/native_layout_schema.json``, generated from its ``NativeLayoutSchema.inc``)
  and only canonical names (an alias next to its canonical key is refused as a duplicate);
* everything left out (low confidence, unresolved) is listed in whole-line ``;`` comments at the end of the file with the
  reason, and stays in ``runtime-profile.json``.
"""
from __future__ import annotations

import hashlib
import json
import os
from typing import Any, Dict, List, Optional, Tuple

_RANK = {"low": 0, "medium": 1, "high": 2}
MIN_CONFIDENCE = "medium"

FUOBJECTARRAY_FIELDS = ["ObjFirstGCIndex", "ObjLastNonGCIndex", "MaxObjectsNotConsideredByGC", "OpenForDisregardForGC",
                        "ObjObjects", "UObjectCreateListeners", "UObjectDeleteListeners", "PrimarySerialNumber",
                        "bShouldRecycleObjectIndices", "UEP_TotalSize"]
TUOBJECTARRAY_FIELDS = ["Objects", "PreAllocatedObjects", "MaxElements", "NumElements", "MaxChunks", "NumChunks",
                        "UEP_TotalSize"]


EXTENDED_CLASSES = ["UObjectBase", "FUObjectItem", "UField", "UStruct", "UClass", "UFunction", "FField", "FFieldClass", "FProperty",
                    "FBoolProperty", "FStructProperty", "FArrayProperty", "FEnumProperty", "FByteProperty", "FClassProperty",
                    "FDelegateProperty", "FMapProperty", "FSetProperty", "FInterfaceProperty", "FObjectPropertyBase",
                    "FMulticastDelegateProperty", "FSoftClassProperty", "FFieldPathProperty", "UScriptStruct", "UEnum"]
EXTENDED_VTABLES = [("UEngine", "Tick"), ("FProperty", "DestroyValueInternal"), ("FProperty", "InitializeValueInternal"),
                    ("FProperty", "GetMinAlignment")]
_SCHEMA_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "native_layout_schema.json")
_schema_cache: Optional[Dict[str, Any]] = None


def native_schema() -> Dict[str, Any]:
    """The member names the port's binding accepts (names only; generated from its NativeLayoutSchema.inc)."""
    global _schema_cache
    if _schema_cache is None:
        with open(_SCHEMA_PATH, encoding="utf-8") as fh:
            _schema_cache = json.load(fh)
    return _schema_cache


def leaf(doc: Dict[str, Any], path: str) -> Optional[Dict[str, Any]]:
    node: Any = doc
    for part in path.split("."):
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node if isinstance(node, dict) and "value" in node and "confidence" in node else None


def usable(env: Optional[Dict[str, Any]], minimum: str = MIN_CONFIDENCE) -> bool:
    return env is not None and env["value"] is not None and _RANK[env["confidence"]] >= _RANK[minimum]


def _num(env: Dict[str, Any]) -> int:
    v = env["value"]
    return int(v, 16) if isinstance(v, str) else int(v)


def identity_header(doc: Dict[str, Any]) -> Optional[List[str]]:
    needs = ["build.engine.major", "build.engine.minor", "build.non_shipping", "build.stats",
             "build.engine.revision_string", "build.build_id"]
    envs = {p: leaf(doc, p) for p in needs}
    if not all(usable(e) for e in envs.values()):
        return None
    return [
        "[NativeLayout]",
        "Format=1",
        "Target=prospero-x86_64-sysv",
        "PointerSize=8",
        "EngineMajor=%d" % _num(envs["build.engine.major"]),
        "EngineMinor=%d" % _num(envs["build.engine.minor"]),
        "NonShipping=%d" % (1 if envs["build.non_shipping"]["value"] else 0),
        "Stats=%d" % (1 if envs["build.stats"]["value"] else 0),
        "EngineRevision=%s" % envs["build.engine.revision_string"]["value"],
        "BuildId=%s" % envs["build.build_id"]["value"],
    ]


def _join(lines: List[str]) -> str:
    return "\n".join(lines) + "\n"


def _short(text: str, limit: int = 130) -> str:
    text = " ".join(str(text).split())
    return text if len(text) <= limit else text[:limit - 3] + "..."


def _unresolved_index(doc: Dict[str, Any]) -> Dict[str, str]:
    return {u["id"]: u["why"] for u in doc.get("unresolved", [])}


def _omitted_comments(items: List[Tuple[str, str]], width: int = 118) -> List[str]:
    """Whole-line comments: keys grouped by reason, wrapped."""
    groups: Dict[str, List[str]] = {}
    for key, why in items:
        groups.setdefault(why, []).append(key)
    lines: List[str] = []
    for why, keys in groups.items():
        head = "; %s:" % why
        cur = head
        for k in keys:
            if len(cur) + len(k) + 2 > width and cur != head:
                lines.append(cur)
                cur = ";   "
            cur += " " + k + ","
        lines.append(cur.rstrip(","))
    return lines


def render_vtable_layout(doc: Dict[str, Any], extended: bool = False) -> Optional[str]:
    head = identity_header(doc)
    if head is None:
        return None
    lines = list(head)
    pe = leaf(doc, "vtables.UObject.ProcessEvent")
    if usable(pe):
        lines += ["", "[UObject]", "ProcessEvent=0x%X" % _num(pe)]
    if extended:
        accepted = set(native_schema().get("vtable_classes", []))
        why_not = _unresolved_index(doc)
        by_class: Dict[str, List[Tuple[str, int]]] = {}
        omitted: List[Tuple[str, str]] = []
        for cls, key in EXTENDED_VTABLES:
            env = leaf(doc, "vtables.%s.%s" % (cls, key))
            if cls not in accepted:
                omitted.append(("%s.%s" % (cls, key), "the port has no vtable section for this class"))
            elif usable(env) and _num(env) % 8 == 0:
                by_class.setdefault(cls, []).append((key, _num(env)))
            elif env is not None:
                omitted.append(("%s.%s" % (cls, key), "confidence %s (%s)" % (env["confidence"], env["method"])))
            else:
                omitted.append(("%s.%s" % (cls, key), "unresolved: " + _short(why_not.get("vtables.%s.%s" % (cls, key), "not derived"))))
        for cls in sorted(by_class):
            lines += ["", "[%s]" % cls] + ["%s=0x%X" % (k, v) for k, v in by_class[cls]]
        if omitted:
            lines += [""] + ["; vtable entries not written (kept in runtime-profile.json when measured):"] + _omitted_comments(omitted)
    return _join(lines)


def _class_block(doc: Dict[str, Any], cls: str, fields: List[str]) -> Optional[List[str]]:
    envs = [(f, leaf(doc, "structs.%s.%s" % (cls, f))) for f in fields]
    if not all(usable(e) for _f, e in envs):
        return None
    return ["", "[%s]" % cls] + ["%s=0x%X" % (f, _num(e)) for f, e in envs]


def _extended_blocks(doc: Dict[str, Any]) -> Tuple[List[str], int, List[Tuple[str, str]]]:
    """Class blocks for the measured reflection classes, the number of blocks, and (key, reason) for what was left out."""
    schema = native_schema()["classes"]
    why_not = _unresolved_index(doc)
    structs = doc.get("structs", {}) if isinstance(doc.get("structs"), dict) else {}
    lines: List[str] = []
    blocks = 0
    omitted: List[Tuple[str, str]] = []
    for cls in EXTENDED_CLASSES:
        node = structs.get(cls)
        spec = schema.get(cls)
        if not isinstance(node, dict) or spec is None:
            continue
        accepted = [k for k in spec["keys"] if k != "UEP_TotalSize"]
        members: List[Tuple[str, int]] = []
        for key in accepted:
            path = "structs.%s.%s" % (cls, key)
            env = leaf(doc, path)
            if usable(env):
                members.append((key, _num(env)))
            elif env is not None:
                omitted.append((path[len("structs."):], "confidence %s (%s)" % (env["confidence"], env["method"])))
            elif path in why_not:
                omitted.append((path[len("structs."):], "unresolved: " + _short(why_not[path])))
        total_env = leaf(doc, "structs.%s.UEP_TotalSize" % cls)
        if not members:
            continue
        if not usable(total_env):
            omitted.append(("%s.*" % cls, "the class is not written: UEP_TotalSize is %s" % (
                "confidence " + total_env["confidence"] if total_env is not None else "unresolved")))
            continue
        total = _num(total_env)
        keep = []
        for key, off in members:
            if 0 <= off < total:
                keep.append((key, off))
            else:
                omitted.append(("%s.%s" % (cls, key), "offset 0x%X is outside the class size 0x%X" % (off, total)))
        if not keep:
            continue
        lines += ["", "[%s]" % cls] + ["%s=0x%X" % (k, v) for k, v in keep] + ["UEP_TotalSize=0x%X" % total]
        blocks += 1
    return lines, blocks, omitted


def render_member_layout(doc: Dict[str, Any], extended: bool = False) -> Optional[str]:
    head = identity_header(doc)
    if head is None:
        return None
    lines = list(head)
    blocks = 0
    for cls, fields in (("FUObjectArray", FUOBJECTARRAY_FIELDS), ("TUObjectArray", TUOBJECTARRAY_FIELDS)):
        b = _class_block(doc, cls, fields)
        if b:
            lines += b
            blocks += 1
    if extended:
        more, n, omitted = _extended_blocks(doc)
        lines += more
        blocks += n
        if omitted:
            lines += ["", "; members not written: confidence below medium or unresolved (runtime-profile.json keeps the measurements and the reasons)."]
            lines += _omitted_comments(omitted)
            lines += ["; every other key the port accepts for these classes was not analysed by this generator version."]
    return _join(lines) if blocks else None


def render_settings_fragment(doc: Dict[str, Any]) -> Optional[str]:
    major, minor = leaf(doc, "build.engine.major"), leaf(doc, "build.engine.minor")
    if not (usable(major) and usable(minor)):
        return None
    lines = [
        "; Per-game UE4SS settings generated by profilegen (fragment: merge into the PS5 policy file).",
        "; Only keys the executable itself answers are written; everything else keeps its default.",
        "",
        "[EngineVersionOverride]",
        "MajorVersion=%d" % _num(major),
        "MinorVersion=%d" % _num(minor),
    ]
    cfg = leaf(doc, "build.configuration")
    if usable(cfg, "high"):
        lines.append("DebugBuild=%s" % {"shipping": "false", "development": "true", "test": "test"}[cfg["value"]])
    st = leaf(doc, "build.stats")
    if usable(st, "high"):
        lines.append("Stats=%s" % ("true" if st["value"] else "false"))
    return _join(lines)


def emit(doc: Dict[str, Any], outdir: str) -> List[Dict[str, Any]]:
    """Write the files under ``outdir/ue4ss`` and return the ``emit[]`` records."""
    files = (("ue4ss/VTableLayout.ini", render_vtable_layout(doc, extended=True)),
             ("ue4ss/MemberVariableLayout.ini", render_member_layout(doc, extended=True)),
             ("ue4ss/UE4SS-settings.ini", render_settings_fragment(doc)))
    out = []
    for rel, text in files:
        if text is None:
            continue
        path = os.path.join(outdir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        data = text.encode("ascii")
        with open(path, "wb") as fh:
            fh.write(data)
        out.append({"path": rel, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return out
