"""``UClass::CreateDefaultObject``: ``ClassDefaultObject``, ``SuperStruct``, and the ``FField`` chain it walks.

``UClass`` has a virtual ``CreateDefaultObject`` that builds the class default object (the "CDO") once::

    if (ClassDefaultObject == nullptr) {
        UClass* ParentClass = GetSuperClass();
        if (ParentClass) { ParentClass->GetDefaultObject(); }                    // read ParentClass->ClassDefaultObject
        ClassDefaultObject = StaticAllocateObject(this, GetOuter(), NAME_None,
                                RF_Public | RF_ClassDefaultObject | RF_ArchetypeObject, ...);   // flags 0x31
        for (FField* F = ChildProperties; F; F = F->Next) if (F->GetClass()->CastFlags & mask) ...
    }
    return ClassDefaultObject;

The virtual lives in the ``UClass`` vtable, which the class-registration helper installs (``uclass_vtable`` fact).
Among the vtable's functions it is the one that passes ``RF_Public | RF_ClassDefaultObject | RF_ArchetypeObject``
(the engine constants 0x1 | 0x10 | 0x20) as the fourth argument of a call whose result is stored back into a
field of ``this`` that the function also compares with zero.  That field is ``ClassDefaultObject``.  The
pointer loaded from ``this`` and then read at the same field (the parent's CDO) is ``SuperStruct``.  The loop over
the child fields gives a second channel for ``ChildProperties`` and ``FField::Next``, and reads two levels of
indirection (``field->ClassPrivate->CastFlags``) that give ``FField::ClassPrivate`` and ``FFieldClass::CastFlags``.
"""
from __future__ import annotations

from typing import Dict, List, Optional, Tuple

from ..model import MEDIUM, Context, DetectorUnavailable, Hex
from ..vtables import find_runs
from ._book import book
from ._flow import Flow

THIS = ("arg", 0)
RF_CDO_FLAGS = 0x1 | 0x10 | 0x20          # RF_Public | RF_ClassDefaultObject | RF_ArchetypeObject (engine constants)
CHANNEL = "class_default_object.flow"
METHOD = "class_default_object.create_default_object"


def vtable_functions(ctx: Context, vptr: int, limit: int = 240) -> List[Tuple[int, int]]:
    """(slot, function) for the vtable whose slot 0 is at ``vptr``."""
    for r in find_runs(ctx.elf, 16):
        if r.offset == vptr:
            seen = set()
            out = []
            for k, f in enumerate(r.slots[:limit]):
                if f not in seen:
                    seen.add(f)
                    out.append((k, f))
            return out
    return []


def peel_this(tag) -> Optional[Tuple[int, int, List[int]]]:
    links: List[int] = []
    t = tag
    while t and t[0] == "ld" and t[1] and t[1][0] == "ld":
        links.append(t[2])
        t = t[1]
    if t and t[0] == "ld" and t[1] == THIS and t[3] == 8:
        return t[2], len(links), list(reversed(links))
    return None


def analyse(ctx: Context, f: int) -> Optional[dict]:
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(f)
    if en - st < 0x100:
        return None
    flow = Flow(dec, dec.decode_function(st, min(en, st + 0x900)))
    ev = flow.events
    calls = [c for c in flow.calls() if c.args.get("rcx") == ("imm", RF_CDO_FLAGS)]
    if not calls:
        return None
    store = None
    for c in calls:
        want = ("ret", c.addr, c.target)
        for e in ev:
            if e.kind == "store" and e.base == THIS and e.size == 8 and e.value == want:
                store = (e.disp, e.addr, c.addr)
                break
        if store:
            break
    if store is None:
        return None
    d = store[0]
    out: Dict[str, object] = {"function": f, "cdo": d, "store": store[1], "alloc_call": store[2]}
    out["zero_test"] = any(e.kind == "cmp" and e.base == THIS and e.disp == d and e.size == 8 and e.value == ("imm", 0) for e in ev)
    # SuperStruct: a pointer loaded from this whose own field d is read
    cands = {e.base[2] for e in ev if e.kind == "load" and e.disp == d and e.size == 8 and e.base and e.base[0] == "ld"
             and e.base[1] == THIS and e.base[3] == 8}
    if len(cands) == 1:
        out["super"] = next(iter(cands))
    # the child-field loop: chain elements are ``this->head`` followed through a link, and each element's
    # class pointer is dereferenced again (``field->ClassPrivate->CastFlags``)
    heads: Dict[int, dict] = {}
    for e in ev:
        pk = peel_this(e.base) if e.base else None
        if pk is None:
            continue
        head, depth, links = pk
        h = heads.setdefault(head, {"links": {}, "classes": {}})
        if depth >= 1:
            h["links"][links[0]] = h["links"].get(links[0], 0) + 1
    for head, h in heads.items():
        if not h["links"]:
            continue
        nxt = max(h["links"].items(), key=lambda kv: kv[1])[0]
        # the cast-flags qword is the one a large mask is applied to: and / andn / or with a 64-bit constant
        for e in ev:
            pk = peel_this(e.base) if e.base else None
            if pk is not None and pk[0] == head and len(pk[2]) == 2 and pk[2][0] == nxt and e.kind == "load" and e.size == 8 and e.disp:
                loaded = ("ld", e.base, e.disp, 8)
                if any(m.kind == "mask" and m.base == loaded for m in ev):
                    h["classes"][(pk[2][1], e.disp)] = h["classes"].get((pk[2][1], e.disp), 0) + 1
        h["next"] = nxt
    walkers = {k: v for k, v in heads.items() if v["links"] and v["classes"]}
    if len(walkers) == 1:
        head, h = next(iter(walkers.items()))
        out["child_head"] = head
        out["next"] = h["next"]
        (cls_off, cast_off), _n = max(h["classes"].items(), key=lambda kv: kv[1])
        out["class_private"], out["cast_flags"] = cls_off, cast_off
    return out


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    b.expect(["structs.UClass.ClassDefaultObject", "structs.UStruct.SuperStruct", "structs.FField.ClassPrivate", "structs.FFieldClass.CastFlags"],
             "UClass::CreateDefaultObject could not be identified in the UClass vtable (no function passes RF_Public|RF_ClassDefaultObject|"
             "RF_ArchetypeObject and stores the result back into this)")
    vptr = ctx.facts.get("uclass_vtable")
    if vptr is None:
        return
    funcs = vtable_functions(ctx, vptr)
    hits = []
    for slot, f in funcs:
        a = analyse(ctx, f)
        if a is not None:
            a["slot"] = slot
            hits.append(a)
    if len(hits) != 1:
        b.why_not("structs.UClass.ClassDefaultObject", "%d function(s) of the UClass vtable look like CreateDefaultObject (need exactly 1)" % len(hits))
        return
    a = hits[0]
    ev0 = "UClass vtable slot %d (+0x%x), function 0x%x: calls with RF_Public|RF_ClassDefaultObject|RF_ArchetypeObject (0x31) and stores " \
          "the result at this+0x%x (0x%x)" % (a["slot"], a["slot"] * 8, a["function"], a["cdo"], a["store"])
    b.observe("structs.UClass.ClassDefaultObject", a["cdo"], CHANNEL, METHOD,
              [ev0] + (["the same field is compared with zero first"] if a["zero_test"] else []), MEDIUM)
    ctx.put("vtables.UClass.CreateDefaultObject", Hex(a["slot"] * 8), MEDIUM, METHOD, [ev0])
    ctx.put("symbols.create_default_object.label", Hex(a["function"]), MEDIUM, METHOD, [ev0])
    if "super" in a:
        b.observe("structs.UStruct.SuperStruct", a["super"], CHANNEL, METHOD,
                  ["pointer loaded from this+0x%x has its own field +0x%x (ClassDefaultObject) read: the parent class's default object" % (
                      a["super"], a["cdo"])], MEDIUM)
    if "child_head" in a:
        b.observe("structs.UStruct.ChildProperties", a["child_head"], CHANNEL, METHOD,
                  ["loop over the field chain starting at this+0x%x" % a["child_head"]], MEDIUM)
        b.observe("structs.FField.Next", a["next"], CHANNEL, METHOD, ["the loop advances through +0x%x" % a["next"]], MEDIUM)
        b.observe("structs.FField.ClassPrivate", a["class_private"], CHANNEL, METHOD,
                  ["each field's +0x%x is dereferenced again (field->ClassPrivate->CastFlags)" % a["class_private"]], MEDIUM)
        b.observe("structs.FFieldClass.CastFlags", a["cast_flags"], CHANNEL, METHOD,
                  ["field->ClassPrivate is read at +0x%x and tested against a mask of cast bits" % a["cast_flags"]], MEDIUM)
    ctx.evidence_files["class_default_object"] = {k: (hex(v) if isinstance(v, int) else v) for k, v in a.items()}
