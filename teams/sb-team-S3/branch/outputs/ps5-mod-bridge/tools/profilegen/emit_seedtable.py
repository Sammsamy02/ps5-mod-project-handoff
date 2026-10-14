"""Render ``SeedTable.h`` (SEED-TABLE-FORMAT.md v2) from a runtime profile document.

The runtime may read ``runtime-profile.json`` directly or keep compiling a generated ``SeedTable.h``;
this module supports the second option without deciding between them.  The header has the layout of
the one ``gen_seed_table.py`` writes, in the same order:

    kGenerated, kBaseObserved, kFNamePoolLabel, kHeapLo, kHeapHi,
    [GMalloc validator support constants], struct SlotSeed,
    kGUObjectArray (slot 0, data), kFNameCtorWchar (slot 2, code), kGMalloc (slot 3, data),
    kStaticConstructObject (slot 4, code), kRequiredSetComplete, kImageSha256

Everything is read from the profile (``symbols.*`` labels, ``load.base``, ``binding.executable.sha256``);
nothing is remembered from any game.  Rules:

* A slot is ``present`` only when its label is resolved at ``medium`` or better; otherwise it is written
  absent (``false``, label 0, empty derivation) with the reason in a comment, and ``kRequiredSetComplete``
  is false.  The runtime's reaction to an incomplete table is its own (a loud refusal in the v2 policy).
* GMalloc has two candidate variables in the profile: ``symbols.gmalloc`` (what ``FMemory::Malloc`` and
  ``FMemory::Free`` load, ``dispatch``) and ``symbols.gmalloc_base_slot`` (the raw allocator pointer the
  lazy-create function fetches, ``base_slot``).  The default is the generator's own value (``dispatch``);
  ``--seed-gmalloc`` picks either explicitly.  The choice and the other value are written as a comment
  in the header and returned in ``notes``; the emitter never falls back from one to the other.
* ``kHeapLo``/``kHeapHi`` are named by the v2 format but the profile has no heap span (the envelope is
  retired): by default they are emitted as an unbounded range with a comment, ``--seed-heap-span LO:HI``
  supplies a range and ``omit`` drops both constants.
* The derivation string of a slot is the compact JSON of ``{anchor_bytes, rip_arithmetic, uniqueness,
  method, confidence, source}`` taken from the profile's method/evidence, escaped as a C++ string literal
  (backslash first, then the double quote), so the compiled string is exactly that JSON text.
* No status or verification of the profile is copied, and a profile string that would put a
  console-verification marker into the header makes the emitter refuse.

    python3 -m tools.profilegen.emit_seedtable --profile DIR/runtime-profile.json --out SeedTable.h
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import textwrap
from typing import Any, Dict, List, Optional, Tuple

from .emit_ue4ss import leaf

MIN_CONFIDENCE = "medium"
_RANK = {"low": 0, "medium": 1, "high": 2}
NAMESPACE = "RC::Unreal::UnrealInitializer::SeedTable"
GMALLOC_CHOICES = ("dispatch", "base_slot")
HEAP_UNBOUNDED = (0, 0xFFFFFFFFFFFFFFFF)
# (profile key, ps_scan slot id, kind, C++ constant, symbols section path); gmalloc is resolved through the choice
SLOTS: Tuple[Tuple[str, int, str, str, Optional[str]], ...] = (
    ("g_uobject_array", 0, "data", "kGUObjectArray", "symbols.object_array"),
    ("fname_ctor_wchar", 2, "code", "kFNameCtorWchar", "symbols.fname_ctor_wchar"),
    ("gmalloc", 3, "data", "kGMalloc", None),
    ("static_construct_object_internal", 4, "code", "kStaticConstructObject", "symbols.static_construct_object_internal"),
)
GMALLOC_SOURCES = {"dispatch": "symbols.gmalloc", "base_slot": "symbols.gmalloc_base_slot"}
PROLOGUE_KEYS = ("prologue_32", "prologue_24", "prologue_16")
# a header must never carry a console-verification claim; profile text that would is refused, not rewritten
FORBIDDEN_MARKER = re.compile(r"console[-_ ]?verified|verification", re.IGNORECASE)


class SeedTableRefused(ValueError):
    """The header cannot be produced (a required fact is missing, an option is unusable, or a marker would be written)."""


class SeedOptions:
    """Choices the caller makes explicitly; each default is stated in the header and in ``notes``."""

    def __init__(self, gmalloc: Optional[str] = None, heap_span: Optional[str] = None) -> None:
        if gmalloc is not None and gmalloc not in GMALLOC_CHOICES:
            raise SeedTableRefused("--seed-gmalloc must be one of %s, not %r" % ("|".join(GMALLOC_CHOICES), gmalloc))
        self.gmalloc_explicit = gmalloc is not None
        self.gmalloc = gmalloc or "dispatch"
        self.heap = parse_heap_span(heap_span)


def parse_heap_span(text: Optional[str]):
    """``None``/``unbounded`` -> HEAP_UNBOUNDED, ``omit`` -> None (drop the constants), ``LO:HI`` -> (lo, hi)."""
    if text is None or text == "unbounded":
        return HEAP_UNBOUNDED
    if text == "omit":
        return None
    m = re.fullmatch(r"(0x[0-9a-fA-F]+):(0x[0-9a-fA-F]+)", text.strip())
    if not m:
        raise SeedTableRefused("--seed-heap-span must be unbounded, omit or LO:HI (hex), not %r" % text)
    lo, hi = int(m.group(1), 16), int(m.group(2), 16)
    if lo >= hi:
        raise SeedTableRefused("--seed-heap-span: lo must be below hi")
    return (lo, hi)


class SlotModel:
    def __init__(self, name: str, slot_id: int, kind: str, const: str) -> None:
        self.name, self.slot_id, self.kind, self.const = name, slot_id, kind, const
        self.present = False
        self.label = 0
        self.derivation: Optional[Dict[str, str]] = None
        self.source: Optional[str] = None
        self.reason: Optional[str] = None
        self.comments: List[str] = []


class SeedTable:
    """The facts of one header, ready to render."""

    def __init__(self) -> None:
        self.profile_id = ""
        self.digest = ""
        self.base = 0
        self.base_comment = ""
        self.pool_label = 0
        self.sha256 = ""
        self.heap: Optional[Tuple[int, int]] = HEAP_UNBOUNDED
        self.heap_comment = ""
        self.instance: Optional[Tuple[int, str]] = None        # (label, trailing comment)
        self.vtable: Optional[Tuple[int, str]] = None
        self.support_notes: List[str] = []
        self.slots: List[SlotModel] = []
        self.notes: List[str] = []                              # every decision the emitter made, for stderr and docs

    @property
    def complete(self) -> bool:
        return all(s.present for s in self.slots)


# ---------------------------------------------------------------------------------------------
def _number(env: Dict[str, Any]) -> int:
    """The unsigned 64-bit number in a profile envelope (a ``0x`` string or an integer); anything else is a refusal."""
    v = env["value"]
    try:
        n = int(v, 16) if isinstance(v, str) else int(v)
    except (TypeError, ValueError):
        raise SeedTableRefused("profile value %r is not a number" % (v,))
    if isinstance(v, bool) or not 0 <= n < (1 << 64):
        raise SeedTableRefused("profile value %r is not an unsigned 64-bit number" % (v,))
    return n


def _usable(env: Optional[Dict[str, Any]]) -> bool:
    return (env is not None and env.get("value") is not None and env.get("confidence") in _RANK
            and _RANK[env["confidence"]] >= _RANK[MIN_CONFIDENCE])


def _why_unusable(env: Optional[Dict[str, Any]], path: str) -> str:
    if env is None:
        return "%s is not in the profile" % path
    if env.get("value") is None:
        return "%s has no value" % path
    return "%s is only %s confidence (the floor is %s)" % (path, env.get("confidence"), MIN_CONFIDENCE)


def _hex(n: int) -> str:
    return "0x%x" % n


def _segment_note(doc: Dict[str, Any], label: int) -> str:
    """``phdr7 BSS`` / ``phdr3 RELRO``: which program header holds ``label`` (from the profile's elf.segments)."""
    segs = (doc.get("elf") or {}).get("segments") or {}
    load = None
    relro = False
    bss = False
    for key, env in segs.items():
        v = env.get("value") if isinstance(env, dict) else None
        if not isinstance(v, dict):
            continue
        try:
            va, filesz, memsz = int(v["p_vaddr"], 16), int(v["p_filesz"], 16), int(v["p_memsz"], 16)
        except (KeyError, ValueError, TypeError):
            continue
        if not (va <= label < va + memsz):
            continue
        if v.get("type") == "LOAD" and load is None:
            load = key
            bss = label >= va + filesz
        elif v.get("type") == "GNU_RELRO":
            relro = True
    if load is None:
        return ""
    return " (phdr%s%s%s)" % (load[2:] if load.startswith("ph") else load, " BSS" if bss else "", " RELRO" if relro else "")


def _runtime_text(doc: Dict[str, Any], label: int) -> str:
    base = leaf(doc, "load.base")
    if base is None or base.get("value") is None:
        return "runtime = linked %s + base" % _hex(label)
    b = _number(base)
    return "runtime %s = linked %s + base %s (%s, %s)" % (_hex(label + b), _hex(label), _hex(b), base["method"], base["confidence"])


def derivation_for(doc: Dict[str, Any], section: str, kind: str, label_env: Dict[str, Any]) -> Dict[str, str]:
    """``{anchor_bytes, rip_arithmetic, uniqueness, method, confidence, source}`` from the profile's own words."""
    label = _number(label_env)
    anchor = None
    if kind == "code":
        for key in PROLOGUE_KEYS:
            env = leaf(doc, "%s.%s" % (section, key))
            if env is not None and isinstance(env.get("value"), str):
                anchor = env["value"]
                break
    if anchor is None:
        anchor = ("no instruction bytes in the profile for this slot; located by %s" % label_env["method"]
                  if kind == "code" else "data symbol (no instruction bytes); located by %s" % label_env["method"])
    evidence = [e if isinstance(e, str) else json.dumps(e, sort_keys=True) for e in label_env.get("evidence", [])]
    return {
        "anchor_bytes": anchor,
        "rip_arithmetic": _runtime_text(doc, label),
        "uniqueness": "; ".join(evidence) if evidence else "no evidence recorded in the profile",
        "method": label_env["method"],
        "confidence": label_env["confidence"],
        "source": "%s.label" % section,
    }


def c_literal_body(text: str) -> str:
    """The inside of a C++ ``"..."`` literal whose value is exactly ``text`` (backslash first, then the quote)."""
    return text.replace("\\", "\\\\").replace('"', '\\"')


def derivation_literal(derivation: Dict[str, str]) -> str:
    return c_literal_body(json.dumps(derivation, separators=(",", ":")))


def _guard_text(*texts: str) -> None:
    for t in texts:
        m = FORBIDDEN_MARKER.search(t)
        if m:
            raise SeedTableRefused("profile text would put %r into SeedTable.h; the header never carries a "
                                   "console-verification marker" % m.group(0))


# ---------------------------------------------------------------------------------------------
def build(doc: Dict[str, Any], options: Optional[SeedOptions] = None) -> SeedTable:
    """Collect the facts of the header from a profile document (no text yet)."""
    opts = options or SeedOptions()
    t = SeedTable()
    t.profile_id = str(doc.get("profile_id", ""))
    t.digest = str(doc.get("profile_digest") or "")

    base = leaf(doc, "load.base")
    pool = leaf(doc, "symbols.name_pool.label")
    sha = leaf(doc, "binding.executable.sha256")
    missing = []
    if not _usable(base):
        missing.append(_why_unusable(base, "load.base"))
    if not _usable(pool):
        missing.append(_why_unusable(pool, "symbols.name_pool.label"))
    if sha is None or not isinstance(sha.get("value"), str) or not re.fullmatch(r"[0-9a-f]{64}", sha["value"]):
        missing.append("binding.executable.sha256 is missing or not a sha256")
    if missing:
        raise SeedTableRefused("SeedTable.h needs the load base, the FNamePool label and the executable hash: " + "; ".join(missing))
    t.base, t.pool_label, t.sha256 = _number(base), _number(pool), sha["value"]
    t.base_comment = ("kBaseObserved is load.base from the profile (%s, %s); the runtime compares it with the live image base"
                      % (base["confidence"], base["method"]))
    if base["method"] != "load.cli_override":
        t.base_comment += (": it is a platform prediction, not read from the file")
    t.notes.append("kBaseObserved = %s from load.base (%s, %s)" % (_hex(t.base), base["confidence"], base["method"]))

    # heap envelope ----------------------------------------------------------------------------
    t.heap = opts.heap
    if t.heap is None:
        t.heap_comment = ("kHeapLo/kHeapHi are not emitted (--seed-heap-span omit): the heap envelope is retired and the "
                          "profile carries no heap span")
        t.notes.append("kHeapLo/kHeapHi omitted (--seed-heap-span omit)")
    elif t.heap == HEAP_UNBOUNDED:
        t.heap_comment = ("kHeapLo/kHeapHi: the heap envelope is RETIRED and the profile carries no heap span, but format v2 names "
                          "both constants, so they are emitted as an UNBOUNDED range (any non-zero Blocks[0] passes); "
                          "pass --seed-heap-span LO:HI to supply a range or omit to drop them")
        t.notes.append("kHeapLo/kHeapHi emitted as the unbounded range 0x0..0xffffffffffffffff (profile has no heap span)")
    else:
        t.heap_comment = ("kHeapLo/kHeapHi were supplied on the command line (--seed-heap-span); the profile carries "
                          "no heap span")
        t.notes.append("kHeapLo/kHeapHi = %s..%s supplied by the caller" % (_hex(t.heap[0]), _hex(t.heap[1])))

    # GMalloc support constants ------------------------------------------------------------------
    family = leaf(doc, "build.allocator_family")
    fam = family["value"] if family and isinstance(family.get("value"), str) else "allocator"
    for attr, section, what in (("instance", "symbols.gmalloc_instance", "global instance"),
                                ("vtable", "symbols.gmalloc_vtable", "vtable")):
        env = leaf(doc, section + ".label")
        const = "kGMalloc%sLabel" % attr.capitalize()
        if _usable(env):
            label = _number(env)
            setattr(t, attr, (label, "%s %s%s" % (fam, what, _segment_note(doc, label))))
            t.notes.append("%s = %s from %s.label (%s)" % (const, _hex(label), section, env["confidence"]))
        else:
            t.support_notes.append("%s is not emitted: %s" % (const, _why_unusable(env, section + ".label")))
            t.notes.append("%s not emitted: %s" % (const, _why_unusable(env, section + ".label")))

    # slots --------------------------------------------------------------------------------------
    for name, slot_id, kind, const, section in SLOTS:
        s = SlotModel(name, slot_id, kind, const)
        if name == "gmalloc":
            _gmalloc_slot(doc, s, opts, t)
        else:
            env = leaf(doc, section + ".label")
            if _usable(env):
                s.present, s.label, s.source = True, _number(env), section
                s.derivation = derivation_for(doc, section, kind, env)
            else:
                s.reason = _why_unusable(env, section + ".label")
                s.comments.append("slot %d (%s) is absent: %s" % (slot_id, name, s.reason))
        t.slots.append(s)
        if not s.present:
            t.notes.append("slot %d %s ABSENT: %s" % (slot_id, name, s.reason))
    t.notes.append("kRequiredSetComplete = %s (%d of %d required slots present)" % (
        str(t.complete).lower(), sum(1 for s in t.slots if s.present), len(t.slots)))

    texts = [t.profile_id, t.digest, t.base_comment, t.heap_comment] + t.support_notes
    texts += [x[1] for x in (t.instance, t.vtable) if x is not None]
    for s in t.slots:
        texts += s.comments + list((s.derivation or {}).values())
    _guard_text(*texts)
    return t


def _gmalloc_slot(doc: Dict[str, Any], s: SlotModel, opts: SeedOptions, t: SeedTable) -> None:
    chosen = opts.gmalloc
    other = "base_slot" if chosen == "dispatch" else "dispatch"
    csec, osec = GMALLOC_SOURCES[chosen], GMALLOC_SOURCES[other]
    cenv, oenv = leaf(doc, csec + ".label"), leaf(doc, osec + ".label")
    how = "--seed-gmalloc %s%s" % (chosen, "" if opts.gmalloc_explicit else " (the default: the generator's own value)")
    if not _usable(cenv):
        reason = _why_unusable(cenv, csec + ".label")
        if opts.gmalloc_explicit:
            raise SeedTableRefused("--seed-gmalloc %s: %s; the emitter does not substitute the other variable" % (chosen, reason))
        s.reason = reason
        s.comments.append("slot 3 (gmalloc) is absent: %s; the other variable (%s) is NOT used in its place" % (reason, osec))
        return
    s.present, s.label, s.source = True, _number(cenv), csec
    s.derivation = derivation_for(doc, csec, "data", cenv)
    if oenv is not None and oenv.get("value") is not None:
        ovalue = _number(oenv)
        if ovalue == s.label:
            s.comments.append("kGMalloc: %s; symbols.gmalloc and symbols.gmalloc_base_slot name the same variable" % how)
            t.notes.append("gmalloc: %s -> %s; the two candidate variables agree" % (how, _hex(s.label)))
        else:
            s.comments.append("kGMalloc: %s -> %s from %s." % (how, _hex(s.label), csec))
            s.comments.append("Not seeded: %s = %s (%s, %s) -- the other candidate, picked with --seed-gmalloc %s. "
                              "Neither value has been observed on the console; that test decides."
                              % (osec, _hex(ovalue), oenv["confidence"], oenv["method"], other))
            t.notes.append("gmalloc: %s -> %s; NOT seeded: %s = %s" % (how, _hex(s.label), osec, _hex(ovalue)))
            if chosen == "dispatch":
                s.comments.append("The V1 support constant below (kGMallocInstanceLabel) was written for the base_slot variable; "
                                  "the dispatch variable can hold a wrapper over that instance, so V1 may not hold under this choice.")
    else:
        s.comments.append("kGMalloc: %s -> %s from %s; the other candidate (%s) is not in the profile." % (how, _hex(s.label), csec, osec))
        t.notes.append("gmalloc: %s -> %s; the other candidate %s is not in the profile" % (how, _hex(s.label), osec))


# ---------------------------------------------------------------------------------------------
def one_line(text: str) -> str:
    """``text`` made safe for a ``//`` comment: printable ASCII only (no newline can end the comment early) and no trailing
    backslash (which would splice the next line into the comment)."""
    clean = "".join(c if " " <= c <= "~" else "?" for c in str(text))
    return clean + " " if clean.endswith("\\") else clean


def _comment(pad: str, text: str) -> List[str]:
    """``text`` as ``//`` lines no wider than ~120 columns."""
    return [pad + "// " + one_line(line) for line in textwrap.wrap(one_line(text), width=112, break_long_words=False,
                                                                     break_on_hyphens=False)]


def render(t: SeedTable) -> str:
    """The header text (UTF-8, LF, one trailing newline)."""
    guard = "RC_SEED_TABLE_H_GENERATED_%x_" % (t.base ^ t.pool_label)
    pad = "    "
    out: List[str] = [
        "// SeedTable.h — GENERATED by tools/profilegen (emit_seedtable.py) from runtime-profile.json.",
        "// Format contract: SEED-TABLE-FORMAT.md v2 (worker-initializer-integration).",
        "// Profile %s, digest %s." % (one_line(t.profile_id), one_line(t.digest[:16])),
        "#ifndef " + guard,
        "#define " + guard,
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace " + NAMESPACE,
        "{",
        pad + "inline constexpr bool kGenerated = true;",
    ]
    out += _comment(pad, t.base_comment + ".")
    out += [
        pad + "inline constexpr uint64_t kBaseObserved = %s;" % _hex(t.base),
        pad + "inline constexpr uint64_t kFNamePoolLabel = %s;" % _hex(t.pool_label),
    ]
    out += _comment(pad, t.heap_comment + ".")
    if t.heap is not None:
        out += [pad + "inline constexpr uint64_t kHeapLo = %s;" % _hex(t.heap[0]),
                pad + "inline constexpr uint64_t kHeapHi = %s;" % _hex(t.heap[1])]
    out += [
        pad + "// GMalloc validator support constants (V1/V2) — base-relative labels, same translation_rule as the slots:",
        pad + "// runtime = base + label. NOT seeds: they are never bound; they only gate slot 3's arming (V1: [GMalloc]",
        pad + "// equals the derived instance; V2: the instance vptr equals the derived vtable).",
    ]
    consts = []
    if t.instance is not None:
        consts.append(("kGMallocInstanceLabel", t.instance))
    if t.vtable is not None:
        consts.append(("kGMallocVtableLabel", t.vtable))
    heads = [pad + "inline constexpr uint64_t %s = %s;" % (n, _hex(v[0])) for n, v in consts]
    width = max((len(h) for h in heads), default=0)
    for h, (_n, v) in zip(heads, consts):
        out.append(h.ljust(width) + " // " + one_line(v[1]))
    for line in t.support_notes:
        out += _comment(pad, line + ".")
    out += [
        pad + "struct SlotSeed",
        pad + "{",
        pad + "    bool present;",
        pad + "    bool data_kind;",
        pad + "    uint64_t label;",
        pad + "    const char* name;",
        pad + "    const char* derivation;",
        pad + "};",
    ]
    for s in t.slots:
        for c in s.comments:
            out += _comment(pad, c)
        out.append(pad + "inline constexpr SlotSeed %s{ %s, %s, %s, \"%s\", \"%s\" }; // slot %d (%s)" % (
            s.const, str(s.present).lower(), str(s.kind == "data").lower(), _hex(s.label), s.name,
            derivation_literal(s.derivation) if s.derivation else "", s.slot_id, s.kind))
    out += [
        pad + "inline constexpr bool kRequiredSetComplete = %s;" % str(t.complete).lower(),
        pad + "inline constexpr std::string_view kImageSha256 = \"%s\";" % t.sha256,
        "}",
        "#endif",
        "",
    ]
    return "\n".join(out)


def render_profile(doc: Dict[str, Any], options: Optional[SeedOptions] = None) -> Tuple[str, SeedTable]:
    t = build(doc, options)
    return render(t), t


def write_text(text: str, path: str) -> Dict[str, Any]:
    """Write an already rendered header (UTF-8, LF); returns ``{path, bytes, sha256}``."""
    data = text.encode("utf-8")
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(data)
    return {"path": path, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def write(doc: Dict[str, Any], path: str, options: Optional[SeedOptions] = None) -> Tuple[Dict[str, Any], SeedTable]:
    """Render and write the header to ``path``; returns ``({path, bytes, sha256}, model)``.  Nothing is written on refusal."""
    text, t = render_profile(doc, options)
    return write_text(text, path), t


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.profilegen.emit_seedtable", description=__doc__.split("\n")[0])
    ap.add_argument("--profile", required=True, help="runtime-profile.json")
    ap.add_argument("--out", required=True, help="where to write SeedTable.h")
    ap.add_argument("--seed-gmalloc", choices=GMALLOC_CHOICES,
                    help="which GMalloc variable slot 3 seeds: dispatch (default; what FMemory::Malloc/Free load) or base_slot")
    ap.add_argument("--seed-heap-span", help="unbounded (default), omit, or LO:HI (hex) for kHeapLo/kHeapHi")
    ns = ap.parse_args(argv)
    try:
        with open(ns.profile, encoding="utf-8") as fh:
            doc = json.load(fh)
        record, t = write(doc, ns.out, SeedOptions(ns.seed_gmalloc, ns.seed_heap_span))
    except SeedTableRefused as exc:
        print("emit_seedtable: refused: %s" % exc, file=sys.stderr)
        return 4
    except (OSError, ValueError) as exc:
        print("emit_seedtable: %s" % exc, file=sys.stderr)
        return 2
    print("emit_seedtable: wrote %s (%d bytes, sha256 %s)" % (ns.out, record["bytes"], record["sha256"][:16]))
    for n in t.notes:
        print("  " + n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
