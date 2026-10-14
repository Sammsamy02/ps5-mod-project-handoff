"""Resolve the layout book: infer the members that fit exactly between measured neighbours, then write the envelopes.

Order of work:

1. every class in ``SCOPE`` gets all the keys the port accepts for it registered as *expected*, so a key nobody measured is
   reported as unresolved with a reason instead of silently missing;
2. **declared-order fit.**  Some members are not touched by any analysed code but sit in a gap between two members that were
   measured (at ``medium`` or better), and the gap is exactly as large as the members UE declares there.  ``Children`` lies
   between ``SuperStruct`` and ``ChildProperties``; ``PropertyLink`` and ``RefLink`` between the end of ``Script`` and
   ``DestructorLink``; ``NumParms`` right after ``FunctionFlags`` (two bytes before ``ParmsSize``); ``RPCId`` /
   ``RPCResponseId`` between ``ReturnValueOffset`` and ``FirstPropertyToInit``; ``RepIndex`` after ``PropertyFlags``;
   ``PropertyLinkNext`` / ``NextRef`` between ``Offset_Internal`` and ``DestructorLinkNext``; ``RepNotifyFunc`` after
   ``PostConstructLinkNext`` when that ends the class; ``UField::Next`` is the single member by which ``UField`` outgrows
   ``UObject``.  They need UE's declaration order *and* an exact fit, so they are ``medium`` and say so; a member that has
   only one measured neighbour (``PostConstructLink`` after ``DestructorLink``) stays ``low``;
3. ``_book.resolve`` writes one envelope per observed path and one unresolved entry per expected path that stayed empty.
"""
from __future__ import annotations

from typing import Optional

from ..model import HIGH, LOW, MEDIUM, Context
from . import _book
from ._book import book, decide

# classes whose accepted keys are all reported (measured or unresolved)
SCOPE = ["UObjectBase", "FUObjectItem", "UField", "UStruct", "UClass", "UFunction", "FField", "FFieldClass", "FProperty",
         "FBoolProperty", "FStructProperty", "FArrayProperty", "FEnumProperty"]
SKIP_KEYS = {"UnqiueNameIndexCounter"}               # the port's own misspelling of UniqueNameIndexCounter
_RANK = {LOW: 0, MEDIUM: 1, HIGH: 2}

DEFAULT_REASON = "not read by any analysed function and not bounded by measured neighbours; no template value is substituted"
CHANNEL = "layout_merge.declared_order"
METHOD = "layout_merge.declared_order_fit"


def _decided(ctx: Context, path: str) -> Optional[int]:
    """The value of ``path`` when it is known at medium or better: the book's decision, else an envelope an earlier detector wrote."""
    obs = book(ctx).obs.get(path)
    if obs:
        value, conf, _method, _evidence, _conflicts = decide(obs)
        return int(value) if (isinstance(value, int) and _RANK[conf] >= _RANK[MEDIUM]) else None
    env = ctx.get(path)
    if env is not None and _RANK[env.confidence] >= _RANK[MEDIUM] and isinstance(env.value, int):
        return int(env.value)
    return None


def infer(ctx: Context) -> None:
    b = book(ctx)
    v = lambda cls, key: _decided(ctx, "structs.%s.%s" % (cls, key))

    def add(cls: str, key: str, value: int, why: str, cap: str = MEDIUM) -> None:
        path = "structs.%s.%s" % (cls, key)
        if path in b.obs and any(o.channel == CHANNEL for o in b.obs[path]):
            return
        b.observe(path, value, CHANNEL, METHOD, [why], cap)

    sup, child_props = v("UStruct", "SuperStruct"), v("UStruct", "ChildProperties")
    if sup is not None and child_props is not None and child_props - sup == 16:
        add("UStruct", "Children", sup + 8, "SuperStruct is at +0x%x and ChildProperties at +0x%x: the 8-byte slot between them is the "
            "one member UE declares there (Children)" % (sup, child_props))
    script, dtor = v("UStruct", "Script"), v("UStruct", "DestructorLink")
    if script is not None and dtor is not None and dtor - (script + 16) == 16:
        add("UStruct", "PropertyLink", script + 16, "Script (a 16-byte TArray) ends at +0x%x and DestructorLink is at +0x%x: the two 8-byte "
            "slots between them are PropertyLink and RefLink, in declaration order" % (script + 16, dtor))
        add("UStruct", "RefLink", script + 24, "second of the two slots between the end of Script and DestructorLink")
    if dtor is not None:
        add("UStruct", "PostConstructLink", dtor + 8, "declared directly after DestructorLink (no measured member on the far side to bound it)", LOW)
    flags, parms, ret, first = (v("UFunction", "FunctionFlags"), v("UFunction", "ParmsSize"), v("UFunction", "ReturnValueOffset"),
                                v("UFunction", "FirstPropertyToInit"))
    if flags is not None and parms is not None and parms == flags + 6:
        add("UFunction", "NumParms", flags + 4, "FunctionFlags (4 bytes) ends at +0x%x and ParmsSize (2-byte aligned) is at +0x%x: the byte "
            "between them, declared right after FunctionFlags, is NumParms" % (flags + 4, parms))
    if ret is not None and first is not None and first - (ret + 2) == 6:
        add("UFunction", "RPCId", ret + 2, "ReturnValueOffset ends at +0x%x and FirstPropertyToInit is at +0x%x: two uint16 (RPCId, "
            "RPCResponseId) and alignment padding fill the gap" % (ret + 2, first))
        add("UFunction", "RPCResponseId", ret + 4, "second uint16 after ReturnValueOffset")
    fl, serial, item_total = v("FUObjectItem", "Flags"), v("FUObjectItem", "SerialNumber"), v("FUObjectItem", "UEP_TotalSize")
    if fl is not None and serial is not None and serial - (fl + 4) == 4:
        add("FUObjectItem", "ClusterRootIndex", fl + 4, "Flags (4 bytes) ends at +0x%x and SerialNumber is at +0x%x: the dword between them, "
            "declared after Flags, is ClusterRootIndex" % (fl + 4, serial))
    if serial is not None and item_total is not None and item_total - (serial + 4) == 4:
        add("FUObjectItem", "RefCount", serial + 4, "the last 4 bytes of the item, after SerialNumber, hold RefCount")
    obj_total, field_total = v("UObjectBase", "UEP_TotalSize"), v("UField", "UEP_TotalSize")
    if obj_total is not None and field_total is not None and field_total - obj_total == 8:
        add("UField", "Next", obj_total, "UField outgrows UObject by exactly 8 bytes: the one member UField declares (Next) follows the base")
    pflags, off, dnext, pcnext, total = (v("FProperty", "PropertyFlags"), v("FProperty", "Offset_Internal"), v("FProperty", "DestructorLinkNext"),
                                         v("FProperty", "PostConstructLinkNext"), v("FProperty", "UEP_TotalSize"))
    if pflags is not None and off is not None and off - (pflags + 8) == 4:
        add("FProperty", "RepIndex", pflags + 8, "the 4 bytes between PropertyFlags and Offset_Internal hold RepIndex (uint16) and the "
            "replication condition byte, in declaration order")
    if off is not None and dnext is not None and dnext - (off + 4) == 16:
        add("FProperty", "PropertyLinkNext", off + 4, "Offset_Internal ends at +0x%x and DestructorLinkNext is at +0x%x: the two 8-byte slots "
            "between them are PropertyLinkNext and NextRef, in declaration order" % (off + 4, dnext))
        add("FProperty", "NextRef", off + 12, "second of the two slots between Offset_Internal and DestructorLinkNext")
    if pcnext is not None and total is not None and total - (pcnext + 8) == 8:
        add("FProperty", "RepNotifyFunc", pcnext + 8, "the last 8 bytes of the class, after PostConstructLinkNext, hold RepNotifyFunc")


def detect(ctx: Context) -> None:
    from .. import emit_ue4ss
    b = book(ctx)
    schema = emit_ue4ss.native_schema()["classes"]
    for cls in SCOPE:
        spec = schema.get(cls)
        if spec is None:
            continue
        keys = [k for k in spec["keys"] if k not in SKIP_KEYS]
        b.expect(["structs.%s.%s" % (cls, k) for k in keys], DEFAULT_REASON)
    # virtual slots the port binds that no detector here identifies (reasons are specific, not the generic one)
    b.expect(["vtables.FProperty.CopyValuesInternal"], "its callers are thin wrappers around a plain-memcpy path; no analysed code identifies the slot")
    b.expect(["vtables.UEngine.LoadMap"], "UEngine::LoadMap has no pure-virtual stub string to anchor it (only UEngine::Tick has one in this engine)")
    b.expect(["vtables.AGameModeBase.InitGameState"], "no string or call-sequence anchor for AGameModeBase::InitGameState is analysed")
    infer(ctx)
    _book.resolve(ctx)
