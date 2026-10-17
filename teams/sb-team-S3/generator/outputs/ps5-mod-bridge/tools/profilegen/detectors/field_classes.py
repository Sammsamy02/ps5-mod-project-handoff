"""``FFieldClass`` / ``FField`` / ``FProperty``: the property object model.

Every property type has a ``StaticClass()`` that constructs a static ``FFieldClass`` once::

    new (&Static) FFieldClass(L"FBoolProperty", Id, CastFlags, SuperClass, &ConstructFn);

so the UTF-16 names ``FProperty`` ``FBoolProperty`` ``FIntProperty`` ... are loaded into ``rsi`` right before a
call whose target the different names agree on: ``FFieldClass::FFieldClass(this, Name, Id, CastFlags, SuperClass,
ConstructFn)``.  Its body stores the arguments, which gives the layout of ``FFieldClass`` (``Name`` is the result of
the ``FName`` constructor, ``Id`` / ``CastFlags`` / ``SuperClass`` / ``ConstructFn`` the arguments; the zero-initialised
members between them are named by UE's declaration order and kept ``low``).

``ConstructFn`` is ``new T(Owner, Name, Flags)``: it allocates ``sizeof(T)`` bytes (``FMemory::Malloc(size, align)`` with a
constant), which is the size of every property class, and calls the constructors.  The ``FField`` constructor
``FField(FFieldVariant Owner, const FName& Name, EObjectFlags Flags)`` is recognised by its stores (``*Owner``,
``*Name``, the flags dword and a zero ``Next``); the inline ``FProperty`` constructor initialises ``ArrayDim = 1,
ElementSize = 0`` with one 8-byte immediate store, which also fixes the order of the two.

``sizeof(FField)`` follows from the Itanium C++ ABI: the first data member of a derived class of a non-POD base is
placed at the base's *data size*, and ``FProperty`` declares ``ArrayDim`` first.  So when ``ArrayDim`` sits exactly at
the end of ``FlagsPrivate`` the base ends there and its size is that offset rounded up to its alignment.
"""
from __future__ import annotations

from collections import Counter
from typing import Dict, List, Optional, Tuple

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ._book import book
from ._flow import Flow

NAMES = ["FProperty", "FBoolProperty", "FIntProperty", "FStrProperty", "FNameProperty", "FObjectProperty", "FStructProperty",
         "FArrayProperty", "FFloatProperty", "FByteProperty", "FEnumProperty", "FTextProperty", "FMapProperty", "FSetProperty",
         "FDelegateProperty", "FClassProperty", "FInt64Property", "FDoubleProperty", "FField"]
SIZED = ["FProperty", "FBoolProperty", "FStructProperty", "FArrayProperty", "FEnumProperty", "FByteProperty", "FClassProperty",
         "FDelegateProperty", "FMapProperty", "FSetProperty"]
CHANNEL = "field_classes.constructor"
METHOD = "field_classes.constructor_stores"


def registrations(ctx: Context) -> List[dict]:
    idx = ctx.index
    dec = idx.decoder
    out = []
    for name in NAMES:
        addrs = idx.strings.find_utf16(name) or idx.strings.find_utf16(name, anywhere=True)
        done = False
        for va in addrs[:6]:
            for site in idx.xrefs.refs_to(va, [KIND_LEA])[:6]:
                f = idx.funcs.containing(site)
                if f is None:
                    continue
                st, en = idx.funcs.span(f)
                flow = Flow(dec, dec.decode_function(st, min(en, st + 0x200)), frame_args=False)
                for c in flow.calls():
                    if c.args.get("rsi") == ("addr", va) and isinstance(c.target, int):
                        out.append({"name": name, "function": f, "target": c.target, "call": c.addr, "args": dict(c.args), "literal": va})
                        done = True
                        break
                if done:
                    break
            if done:
                break
    return out


def analyse_constructor(ctx: Context, cf: int) -> dict:
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(cf)
    flow = Flow(dec, dec.decode_function(st, min(en, st + 0x300)))
    this = ("arg", 0)
    fname_ctor = ctx.facts.get("fname_ctor")
    roles: Dict[str, Tuple[int, int]] = {}
    zeros: List[Tuple[int, int, int]] = []
    extent = 0
    for e in flow.stores():
        if e.base != this or e.value is None:
            continue
        extent = max(extent, e.disp + e.size)
        v = e.value
        if e.size == 8 and v == ("arg", 2):
            roles["Id"] = (e.disp, e.addr)
        elif e.size == 8 and v == ("arg", 3):
            roles["CastFlags"] = (e.disp, e.addr)
        elif e.size == 8 and v == ("arg", 4):
            roles["SuperClass"] = (e.disp, e.addr)
        elif e.size == 8 and v == ("arg", 5):
            roles["ConstructFn"] = (e.disp, e.addr)
        elif e.size == 8 and v[0] == "out" and fname_ctor and v[2] == fname_ctor:
            roles["FName"] = (e.disp, e.addr)
        elif v == ("imm", 0) and e.size in (4, 8):
            zeros.append((e.disp, e.size, e.addr))
    positional: Dict[str, Tuple[int, int]] = {}
    if all(k in roles for k in ("CastFlags", "SuperClass", "ConstructFn")):
        c, s, k = roles["CastFlags"][0], roles["SuperClass"][0], roles["ConstructFn"][0]
        for d, sz, a in sorted(set(zeros)):
            if sz == 4 and c < d < s:
                positional.setdefault("ClassFlags", (d, a))
            elif sz == 8 and s < d < k:
                positional.setdefault("DefaultObject", (d, a))
            elif sz == 4 and d > k:
                positional.setdefault("UniqueNameIndexCounter", (d, a))
    return {"roles": roles, "positional": positional, "extent": extent}


def field_constructor(ctx: Context, construct_fns: List[int]) -> Optional[dict]:
    """The ``FField(Owner, Name, Flags)`` constructor among the callees of the property ``ConstructFn`` functions."""
    idx = ctx.index
    dec = idx.decoder
    seen = set()
    for cfn in construct_fns:
        st, en = idx.funcs.span(cfn)
        flow = Flow(dec, dec.decode_function(st, min(en, st + 0x300)), frame_args=False)
        for c in flow.calls():
            t = c.target
            if not isinstance(t, int) or t in seen:
                continue
            seen.add(t)
            s2, e2 = idx.funcs.span(t)
            if e2 - s2 > 0x200:
                continue
            fl = Flow(dec, dec.decode_function(s2, e2))
            this = ("arg", 0)
            owner = [e for e in fl.stores(size=8) if e.base == this and e.value == ("ld", ("arg", 1), 0, 8)]
            name = [e for e in fl.stores(size=8) if e.base == this and e.value == ("ld", ("arg", 2), 0, 8)]
            flags = [e for e in fl.stores(size=4) if e.base == this and e.value == ("arg", 3)]
            if len(owner) == 1 and len(name) == 1 and len(flags) == 1:
                nxt = [e for e in fl.stores(size=8) if e.base == this and e.value == ("imm", 0) and owner[0].disp < e.disp < name[0].disp]
                return {"function": t, "owner": owner[0], "name": name[0], "flags": flags[0], "next": nxt[0] if len(nxt) == 1 else None,
                        "found_from": cfn}
    return None


def construct_info(ctx: Context, cfn: int) -> dict:
    idx = ctx.index
    dec = idx.decoder
    malloc = ctx.facts.get("fmemory_malloc")
    st, en = idx.funcs.span(cfn)
    flow = Flow(dec, dec.decode_function(st, min(en, st + 0x300)), frame_args=False)
    out: Dict[str, object] = {}
    obj = None
    for c in flow.calls():
        if malloc is not None and c.target == malloc:
            sz = c.args.get("rdi")
            if sz and sz[0] == "imm":
                out["size"] = (int(sz[1]), c.addr)
                obj = ("ret", c.addr, malloc)
            break
    if obj is not None:
        for e in flow.stores(size=8):
            if e.base == obj and e.value == ("imm", 1):
                out["arraydim"] = (e.disp, e.addr)
                break
    return out


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    b.expect(["structs.FFieldClass.%s" % m for m in ("FName", "Id", "CastFlags", "SuperClass", "ConstructFn", "ClassFlags", "DefaultObject",
                                                     "UniqueNameIndexCounter", "UEP_TotalSize")] +
             ["structs.FField.ClassPrivate", "structs.FField.Owner", "structs.FField.NamePrivate", "structs.FField.FlagsPrivate",
              "structs.FField.UEP_TotalSize", "structs.FProperty.UEP_TotalSize"] +
             ["structs.%s.UEP_TotalSize" % n for n in SIZED if n != "FProperty"],
             "the FFieldClass constructor was not located (no call target is shared by the F<Type>Property class-name registrations)")
    regs = registrations(ctx)
    votes = Counter()
    names_for: Dict[int, set] = {}
    for r in regs:
        votes[r["target"]] += 1
        names_for.setdefault(r["target"], set()).add(r["name"])
    if not votes:
        return
    cf, n = votes.most_common(1)[0]
    if n < 4 or len(names_for[cf]) < 4 or (len(votes) > 1 and votes.most_common(2)[1][1] * 2 > n):
        return
    ctx.put("symbols.ffield_class_ctor.label", Hex(cf), HIGH if n >= 8 else MEDIUM, "field_classes.name_registrations",
            ["%d F<Type> class-name literals (%s) are passed to 0x%x with the static descriptor in rdi and Id / CastFlags / SuperClass / ConstructFn "
             "in rdx / rcx / r8 / r9" % (n, ", ".join(sorted(names_for[cf])[:6]), cf)])
    body = analyse_constructor(ctx, cf)
    roles = body["roles"]
    ev0 = "FFieldClass constructor 0x%x" % cf
    for member, (disp, addr) in roles.items():
        b.observe("structs.FFieldClass.%s" % member, disp, CHANNEL, METHOD, ["%s stores %s at +0x%x (0x%x)" % (
            ev0, {"Id": "argument 3 (Id)", "CastFlags": "argument 4 (CastFlags)", "SuperClass": "argument 5 (SuperClass)",
                  "ConstructFn": "argument 6 (ConstructFn)", "FName": "the FName its FName-constructor call built"}[member], disp, addr)], MEDIUM)
    for member, (disp, addr) in body["positional"].items():
        b.observe("structs.FFieldClass.%s" % member, disp, "field_classes.declaration_order", "field_classes.zero_initialiser",
                  ["%s zero-initialises +0x%x (0x%x) between its argument stores; named by UE's declaration order" % (ev0, disp, addr)], LOW)
    # total size: the constructor's extent, and the spacing of the static descriptors
    ext8 = (body["extent"] + 7) & ~7
    statics = sorted({r["args"]["rdi"][1] for r in regs if r["target"] == cf and r["args"].get("rdi") and r["args"]["rdi"][0] == "addr"})
    b.observe("structs.FFieldClass.UEP_TotalSize", ext8, "field_classes.ctor_extent", "field_classes.extent",
              ["the constructor initialises every byte up to +0x%x; the class has 8-byte alignment" % body["extent"]], MEDIUM)
    if len(statics) >= 6:
        gaps = Counter(b2 - a for a, b2 in zip(statics, statics[1:]))
        small = min(gaps)
        if gaps[small] >= 3 and small > 8:
            bound = (small - 1) & ~7
            b.observe("structs.FFieldClass.UEP_TotalSize", bound, "field_classes.static_spacing", "field_classes.spacing",
                      ["%d static descriptors; the closest two are 0x%x apart %d times and each is followed by its 8-byte guard variable, "
                       "so sizeof <= 0x%x" % (len(statics), small, gaps[small], bound)], MEDIUM)
    # property class sizes, the FField constructor, ArrayDim
    cfns = {}
    for r in regs:
        if r["target"] != cf:
            continue
        c9 = r["args"].get("r9")
        if c9 and c9[0] == "addr" and r["name"] not in cfns:
            cfns[r["name"]] = c9[1]
    infos = {name: construct_info(ctx, f) for name, f in cfns.items()}
    ctx.facts["field_class_registrations"] = {"constructor": cf, "construct_fns": dict(cfns)}
    for name in SIZED:
        info = infos.get(name)
        if info and "size" in info:
            size, addr = info["size"]
            b.observe("structs.%s.UEP_TotalSize" % name, size, "field_classes.construct_allocation", "field_classes.allocation_size",
                      ["ConstructFn 0x%x allocates the object with a constant byte count 0x%x (call at 0x%x): sizeof(%s)" % (
                          cfns[name], size, addr, name)], HIGH, True)
    dims = {info["arraydim"][0] for info in infos.values() if "arraydim" in info}
    arraydim = None
    if len(dims) == 1:
        arraydim = next(iter(dims))
        any_info = next(i for i in infos.values() if "arraydim" in i)
        ev = ["inline FProperty constructor stores the 8-byte constant 1 at +0x%x (0x%x): ArrayDim = 1, ElementSize = 0" % (
            arraydim, any_info["arraydim"][1])]
        b.observe("structs.FProperty.ArrayDim", arraydim, "field_classes.inline_constructor", "field_classes.inline_constructor", ev, MEDIUM)
        b.observe("structs.FProperty.ElementSize", arraydim + 4, "field_classes.inline_constructor", "field_classes.inline_constructor", ev, MEDIUM)
    fc = field_constructor(ctx, [f for f in cfns.values()])
    flags_end = None
    if fc:
        ev = "FField constructor 0x%x (from ConstructFn 0x%x)" % (fc["function"], fc["found_from"])
        b.observe("structs.FField.Owner", fc["owner"].disp, "field_classes.field_constructor", "field_classes.field_constructor_stores",
                  ["%s stores *Owner at +0x%x (0x%x)" % (ev, fc["owner"].disp, fc["owner"].addr)], MEDIUM)
        b.observe("structs.FField.NamePrivate", fc["name"].disp, "field_classes.field_constructor", "field_classes.field_constructor_stores",
                  ["%s stores *Name at +0x%x (0x%x)" % (ev, fc["name"].disp, fc["name"].addr)], MEDIUM)
        b.observe("structs.FField.FlagsPrivate", fc["flags"].disp, "field_classes.field_constructor", "field_classes.field_constructor_stores",
                  ["%s stores the flags argument at +0x%x (0x%x)" % (ev, fc["flags"].disp, fc["flags"].addr)], MEDIUM)
        if fc["next"] is not None:
            b.observe("structs.FField.Next", fc["next"].disp, "field_classes.field_constructor", "field_classes.field_constructor_stores",
                      ["%s clears +0x%x between Owner and Name (0x%x)" % (ev, fc["next"].disp, fc["next"].addr)], MEDIUM)
        flags_end = fc["flags"].disp + 4
        if arraydim is not None and arraydim == flags_end:
            b.observe("structs.FField.UEP_TotalSize", (flags_end + 7) & ~7, "field_classes.tail_padding", "field_classes.itanium_tail_padding",
                      ["FProperty's first member (ArrayDim) sits at +0x%x, exactly where FField's data ends (after FlagsPrivate); "
                       "the Itanium ABI places a derived class's first member at the base's data size, so sizeof(FField) is that offset "
                       "rounded up to its 8-byte alignment" % arraydim], MEDIUM)
    ctx.evidence_files["field_classes"] = {
        "constructor": hex(cf), "registrations": [{"name": r["name"], "static": hex(r["args"]["rdi"][1]) if r["args"].get("rdi") and
                                                    r["args"]["rdi"][0] == "addr" else None} for r in regs if r["target"] == cf],
        "construct_fns": {k: hex(v) for k, v in cfns.items()}, "sizes": {k: hex(v["size"][0]) for k, v in infos.items() if "size" in v},
        "field_constructor": hex(fc["function"]) if fc else None}
