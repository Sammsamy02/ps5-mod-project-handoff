"""``UObjectBase`` (``ObjectFlags``, ``InternalIndex``, ``ClassPrivate``, ``NamePrivate``, ``OuterPrivate``) and ``FUObjectItem``.

``UObjectBase::UObjectBase(UClass* InClass, EObjectFlags InFlags, EInternalObjectFlags InInternalFlags,
UObject* InOuter, FName InName, ...)`` registers the new object with the global array::

    ClassPrivate (InClass), ObjectFlags (InFlags), OuterPrivate (InOuter), NamePrivate (InName),
    InternalIndex = INDEX_NONE;
    GUObjectArray.AllocateUObjectIndex(this, InInternalFlags, ...);

``FUObjectArray::AllocateUObjectIndex(UObjectBase* Object, EInternalObjectFlags, int32 Index, int32 Serial)``
is a member of the ``FUObjectArray`` cluster (the one function that carries *all* of the fatal messages
"Maximum number of UObjects", "Unable to add more objects to disregard for GC pool" and "but another
object ... exists").  Every function that calls it with its own ``this`` as ``rsi`` is a constructor of
the object base.  The data-flow walk (``_flow.py``) tags the incoming arguments; a store of argument 1
(``InClass``) is ``ClassPrivate``, of argument 2 (``InFlags``) ``ObjectFlags``, of argument 4
(``InOuter``) ``OuterPrivate``, of argument 5 (``InName``) ``NamePrivate``, and the constant -1 is
``InternalIndex``.  A second constructor variant (the one that builds the name from a ``TCHAR*``) is
independent corroboration: it stores argument 1 at the same offset and stores *the value the FName
constructor wrote* at ``NamePrivate``.  ``AllocateUObjectIndex`` itself stores the index into the object
it is given (``InternalIndex``) and fills the ``FUObjectItem`` (``Object``, atomic ``Flags``,
``SerialNumber``).

``UObjectBase`` is complete at the end of ``OuterPrivate``; when ``sizeof(UObject)`` (the registration
of the class named ``Object``) is exactly that offset, ``UObject`` adds no member and the total is the
same number.
"""
from __future__ import annotations

from collections import Counter
from typing import Dict, List, Optional, Tuple

from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ._book import book
from ._flow import Flow
from . import uobject_array

UOBJECTBASE_MEMBERS = ["ObjectFlags", "InternalIndex_Private", "ClassPrivate", "NamePrivate", "OuterPrivate"]
FUOBJECTITEM_MEMBERS = ["Object", "Flags", "SerialNumber"]


def find_allocate_index(ctx: Context) -> Optional[Tuple[int, List[str]]]:
    anchors = uobject_array._anchor_functions(ctx)
    best = None
    for f, labels in anchors.items():
        if "Unable to add more objects to disregard for GC pool" not in labels:
            continue
        if best is None or len(labels) > len(best[1]):
            best = (f, sorted(labels))
    return best


def constructor_candidates(ctx: Context, alloc: int) -> List[dict]:
    """Functions that call ``alloc`` with their own ``this`` as the object: their stores into ``this``."""
    idx = ctx.index
    dec = idx.decoder
    fname_ctor = ctx.facts.get("fname_ctor")
    out = []
    seen = set()
    for site in idx.calls.callers(alloc):
        f = idx.funcs.containing(site)
        if f is None or f in seen:
            continue
        seen.add(f)
        st, en = idx.funcs.span(f)
        flow = Flow(dec, dec.decode_function(st, min(en, st + 0x800)))
        call = next((c for c in flow.calls() if c.addr == site), None)
        if call is None or call.args.get("rsi") != ("arg", 0):
            continue
        this = ("arg", 0)
        roles: Dict[str, Tuple[int, int]] = {}
        for e in flow.stores():
            if e.base != this or e.value is None:
                continue
            v = e.value
            if e.size == 8 and v == ("arg", 1):
                roles.setdefault("ClassPrivate", (e.disp, e.addr))
            elif e.size == 4 and v == ("arg", 2):
                roles.setdefault("ObjectFlags", (e.disp, e.addr))
            elif e.size == 8 and v == ("arg", 4):
                roles.setdefault("OuterPrivate", (e.disp, e.addr))
            elif e.size == 8 and v == ("arg", 5):
                roles.setdefault("NamePrivate", (e.disp, e.addr))
            elif e.size == 4 and v == ("imm", 0xFFFFFFFF):
                roles.setdefault("InternalIndex_Private", (e.disp, e.addr))
            elif e.size == 8 and v[0] == "out" and fname_ctor and v[2] == fname_ctor:
                roles.setdefault("NamePrivate_fname", (e.disp, e.addr))
        out.append({"function": f, "call": site, "roles": roles,
                    "extent": max([e.disp + e.size for e in flow.stores() if e.base == this and e.size <= 8] or [0])})
    return out


def analyse_allocator(ctx: Context, alloc: int) -> dict:
    """What AllocateUObjectIndex stores into the object it is handed and into the item it fills."""
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(alloc)
    flow = Flow(dec, dec.decode_function(st, min(en, st + 0x900)))
    obj = ("arg", 1)
    index_store = [e for e in flow.stores(size=4) if e.base == obj]
    out: Dict[str, object] = {"index_stores": [(e.disp, e.addr) for e in index_store]}
    # the item: the register that receives a qword store of the Object argument at offset 0
    items = Counter(e.base for e in flow.stores(size=8) if e.disp == 0 and e.value == obj and e.base != obj)
    if items:
        item = items.most_common(1)[0][0]
        group = [e for e in flow.events if e.kind == "store" and e.base == item]
        members: Dict[str, Tuple[int, int]] = {"Object": (0, next(e.addr for e in group if e.disp == 0 and e.value == obj))}
        for e in group:
            if e.size == 4 and e.value == ("arg", 4):
                members.setdefault("SerialNumber", (e.disp, e.addr))
            if e.size == 4 and e.mnemonic.startswith("lock cmpxchg"):
                members.setdefault("Flags", (e.disp, e.addr))
        out["item"] = members
    return out


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    b.expect(["structs.UObjectBase.%s" % m for m in UOBJECTBASE_MEMBERS] + ["structs.UObjectBase.UEP_TotalSize"] +
             ["structs.FUObjectItem.%s" % m for m in FUOBJECTITEM_MEMBERS] + ["structs.FUObjectItem.UEP_TotalSize"],
             "the FUObjectArray allocation member (the function that carries the 'Unable to add more objects to disregard "
             "for GC pool' message) was not found")
    found = find_allocate_index(ctx)
    if found is None:
        return
    alloc, labels = found
    ctx.put("symbols.allocate_uobject_index.label", Hex(alloc), MEDIUM, "uobject_base.fatal_message_family",
            ["the FUObjectArray member that carries all of: %s" % "; ".join(labels)])
    ctors = constructor_candidates(ctx, alloc)
    primary = None
    for c in sorted(ctors, key=lambda c: -len([r for r in c["roles"] if not r.endswith("_fname")])):
        if len([r for r in c["roles"] if not r.endswith("_fname")]) >= 4:
            primary = c
            break
    if primary is None:
        for m in UOBJECTBASE_MEMBERS:
            b.why_not("structs.UObjectBase." + m, "no caller of AllocateUObjectIndex (0x%x) passes its own this and stores the constructor "
                      "arguments (found %d caller function(s))" % (alloc, len(ctors)))
        return
    ev0 = "constructor 0x%x calls AllocateUObjectIndex (0x%x) with its own this" % (primary["function"], alloc)
    names = {"ClassPrivate": "InClass (argument 1)", "ObjectFlags": "InFlags (argument 2)", "OuterPrivate": "InOuter (argument 4)",
             "NamePrivate": "InName (argument 5)", "InternalIndex_Private": "the constant -1 (INDEX_NONE)"}
    for member, (disp, addr) in primary["roles"].items():
        if member in names:
            b.observe("structs.UObjectBase.%s" % member, disp, "uobject_base.constructor", "uobject_base.constructor_stores",
                      [ev0, "stores %s at +0x%x (0x%x)" % (names[member], disp, addr)], MEDIUM)
    # corroboration from the other constructor variants
    for c in ctors:
        if c is primary:
            continue
        for member, (disp, addr) in c["roles"].items():
            target = "NamePrivate" if member == "NamePrivate_fname" else member
            if target in ("ClassPrivate", "NamePrivate", "InternalIndex_Private"):
                what = {"ClassPrivate": "stores its argument 1 (the class)", "NamePrivate": "stores the FName the FName constructor wrote",
                        "InternalIndex_Private": "stores the constant -1"}[target]
                b.observe("structs.UObjectBase.%s" % target, disp, "uobject_base.constructor_variant", "uobject_base.constructor_variants",
                          ["variant constructor 0x%x %s at +0x%x (0x%x)" % (c["function"], what, disp, addr)], MEDIUM)
    # the index store inside the allocator
    alloc_facts = analyse_allocator(ctx, alloc)
    idx_stores = alloc_facts.get("index_stores", [])
    if len(idx_stores) == 1:
        d, a = idx_stores[0]
        b.observe("structs.UObjectBase.InternalIndex_Private", d, "uobject_base.allocator", "uobject_base.allocator_store",
                  ["AllocateUObjectIndex (0x%x) stores the allocated index into the object at +0x%x (0x%x)" % (alloc, d, a)], MEDIUM)
    item = alloc_facts.get("item")
    if item:
        for m, (d, a) in item.items():
            b.observe("structs.FUObjectItem.%s" % m, d, "uobject_base.allocator", "uobject_base.allocator_stores",
                      ["AllocateUObjectIndex (0x%x) fills the item at +0x%x (0x%x)" % (alloc, d, a)], MEDIUM)
    stride = ctx.get("structs.FUObjectItem.stride")
    if stride is not None:
        b.observe("structs.FUObjectItem.UEP_TotalSize", int(stride.value), "uobject_array.stride", stride.method,
                  ["sizeof(FUObjectItem) is the array stride; " + (stride.evidence[0] if stride.evidence else "")], stride.confidence)
    # the end of the members, compared with sizeof(UObject)
    end = primary["extent"]
    end8 = (end + 7) & ~7
    uobject = ctx.facts.get("uobject_size")
    if uobject is not None and end8 == uobject:
        b.observe("structs.UObjectBase.UEP_TotalSize", uobject, "uobject_base.extent_equals_uobject", "uobject_base.extent",
                  ["the last member ends at +0x%x and sizeof(UObject) is 0x%x, so UObject adds nothing to UObjectBase" % (end, uobject)],
                  HIGH, True)
    elif uobject is not None:
        b.observe("structs.UObjectBase.UEP_TotalSize", end8, "uobject_base.extent", "uobject_base.extent",
                  ["lower bound: the last member ends at +0x%x but sizeof(UObject) is 0x%x, so UObject may add members" % (end, uobject)], LOW)
    ctx.evidence_files["uobject_base"] = {
        "allocate_index": hex(alloc), "labels": labels, "constructors": [
            {"function": hex(c["function"]), "roles": {k: [hex(v[0]), hex(v[1])] for k, v in c["roles"].items()}} for c in ctors],
        "allocator": {k: v for k, v in alloc_facts.items() if k != "item"},
        "item": {k: [hex(v[0]), hex(v[1])] for k, v in (item or {}).items()}}
