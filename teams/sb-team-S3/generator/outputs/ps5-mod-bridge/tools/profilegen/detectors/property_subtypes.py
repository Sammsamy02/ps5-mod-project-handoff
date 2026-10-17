"""Members of the property subtypes (``FStructProperty::Struct``, ``FArrayProperty::Inner``, ``FEnumProperty::UnderlyingProp``, ``FBoolProperty``).

Each ``FProperty`` subtype overrides some of ``FProperty``'s virtuals (copy, destroy, initialise, alignment, import/export
text, serialise ...), and those overrides are the code that reads the subtype's own members.  A subtype's vtable is the one
its ``ConstructFn`` installs (``mov [obj], vtable`` right after the allocation); the *overridden* slots are the ones that differ
from ``FProperty``'s own vtable.  The qword/byte loads through ``this`` at or beyond ``sizeof(FProperty)`` in those functions
are exactly the subtype's members:

* ``FStructProperty`` -- one qword (``Struct``, a ``UScriptStruct*``);
* ``FArrayProperty``  -- one qword (``Inner``) and one byte (``ArrayFlags``);
* ``FEnumProperty``   -- two qwords: ``UnderlyingProp`` then ``Enum`` (UE declares them in that order; the first is also the one
  most overrides forward to);
* ``FBoolProperty``   -- four consecutive bytes: ``FieldSize``, ``ByteOffset``, ``ByteMask``, ``FieldMask``.  The initialise override
  (``*(Dest + ByteOffset) &= ~FieldMask``) uses one as the index of the byte it clears and the other as the mask, which confirms
  those two independently of the order.

Everything is ``medium`` (one channel, and the order of equally-sized members is UE's declaration order); ``ByteOffset`` and
``FieldMask`` reach ``high`` through the second channel.
"""
from __future__ import annotations

from collections import Counter
from typing import Dict, List, Optional, Tuple

from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..vtables import find_runs
from ._book import book, decide
from ._flow import Flow

# alignof of the numeric property element types (engine-defined: int8 byte, int32, float, int64, double)
ALIGNMENTS = [("FByteProperty", 1), ("FIntProperty", 4), ("FFloatProperty", 4), ("FInt64Property", 8), ("FDoubleProperty", 8)]
CHANNEL = "property_subtypes.overridden_virtuals"
METHOD = "property_subtypes.overridden_virtuals"
USAGE = "property_subtypes.bool_initialise"


def _book_value(ctx: Context, path: str) -> Optional[int]:
    obs = book(ctx).obs.get(path)
    if obs:
        value, conf, _m, _e, _c = decide(obs)
        if value is not None and conf != LOW:
            return int(value)
    env = ctx.get(path)
    if env is not None and env.confidence != LOW and isinstance(env.value, int):
        return int(env.value)
    return None


def _vtable(ctx: Context, cfn: int) -> Optional[int]:
    malloc = ctx.facts.get("fmemory_malloc")
    st, en = ctx.index.funcs.span(cfn)
    flow = Flow(ctx.index.decoder, ctx.index.decoder.decode_function(st, min(en, st + 0x300)), frame_args=False)
    obj = None
    for c in flow.calls():
        if malloc is not None and c.target == malloc:
            obj = ("ret", c.addr, malloc)
            break
    vts = [e.value[1] for e in flow.stores(size=8) if e.base == obj and e.disp == 0 and e.value and e.value[0] == "addr"]
    return vts[-1] if vts else None


def const_return(ctx: Context, f: int) -> Optional[int]:
    """The constant a trivial function returns in eax (``mov eax, N; ret``, optionally framed), else None."""
    from capstone import x86 as xo
    value = None
    for i in ctx.index.decoder.decode(f, 48, 8):
        mn, ops = i.mnemonic, i.operands
        if mn in ("push", "pop", "nop", "endbr64"):
            continue
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG:
            continue                                                  # mov rbp, rsp
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_IMM:
            value = int(ops[1].imm)
            continue
        if mn == "xor" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and ops[0].reg == ops[1].reg:
            value = 0
            continue
        if mn == "ret":
            return value
        return None
    return None


def alignment_slot(ctx: Context, runs, cfns) -> Optional[Tuple[int, List[str]]]:
    """The virtual slot whose override returns alignof(T) as a constant for every numeric property type: GetMinAlignment."""
    vts = {}
    for name, _al in ALIGNMENTS:
        if name not in cfns:
            return None
        vt = _vtable(ctx, cfns[name])
        if vt not in runs:
            return None
        vts[name] = runs[vt]
    n = min(len(r.slots) for r in vts.values())
    hits = []
    for k in range(min(n, 96)):
        if all(const_return(ctx, vts[name].slots[k]) == al for name, al in ALIGNMENTS):
            hits.append(k)
    if len(hits) != 1:
        return None
    return hits[0], ["slot %d (+0x%x): the overrides of %s return the constants %s" % (
        hits[0], hits[0] * 8, ", ".join(nm for nm, _a in ALIGNMENTS), ", ".join(str(a) for _n, a in ALIGNMENTS))]


def member_loads(ctx: Context, run, base_run, floor: int, limit: int = 0xA0) -> Dict[Tuple[int, int], int]:
    """(disp, size) -> number of overridden virtuals that load it through ``this`` (displacements in [floor, limit))."""
    idx = ctx.index
    dec = idx.decoder
    cnt: Counter = Counter()
    for f, bf in zip(run.slots, base_run.slots):
        if f == bf:
            continue
        s, e = idx.funcs.span(f)
        if e - s > 0x600:
            continue
        fl = Flow(dec, dec.decode_function(s, e))
        for key in {(ev.disp, ev.size) for ev in fl.events if ev.kind == "load" and ev.base == ("arg", 0) and floor <= ev.disp < limit}:
            cnt[key] += 1
    return dict(cnt)


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    reg = ctx.facts.get("field_class_registrations")
    total = _book_value(ctx, "structs.FProperty.UEP_TotalSize")
    b.expect(["structs.FStructProperty.Struct", "structs.FArrayProperty.Inner", "structs.FArrayProperty.ArrayFlags",
              "structs.FEnumProperty.UnderlyingProp", "structs.FEnumProperty.Enum", "structs.FBoolProperty.FieldSize",
              "structs.FBoolProperty.ByteOffset", "structs.FBoolProperty.ByteMask", "structs.FBoolProperty.FieldMask"],
             "the property subtype virtuals could not be analysed (the FFieldClass registrations / sizeof(FProperty) were not resolved)")
    if reg is None or total is None:
        return
    cfns = reg["construct_fns"]
    if "FProperty" not in cfns:
        return
    runs = {r.offset: r for r in find_runs(ctx.elf, 16)}
    base_vt = _vtable(ctx, cfns["FProperty"])
    if base_vt not in runs:
        return
    base_run = runs[base_vt]
    found: Dict[str, Dict[Tuple[int, int], int]] = {}
    for name in ("FStructProperty", "FArrayProperty", "FEnumProperty", "FBoolProperty"):
        if name not in cfns:
            continue
        vt = _vtable(ctx, cfns[name])
        if vt in runs:
            found[name] = member_loads(ctx, runs[vt], base_run, total)

    def ev(name: str, key: Tuple[int, int], what: str) -> List[str]:
        return ["%s overrides %d FProperty virtual(s) that load a %d-byte member at +0x%x through this (%s)" % (
            name, found[name][key], key[1], key[0], what)]

    st = found.get("FStructProperty", {})
    q = sorted(k for k in st if k[1] == 8)
    if len(st) == 1 and len(q) == 1:
        b.observe("structs.FStructProperty.Struct", q[0][0], CHANNEL, METHOD, ev("FStructProperty", q[0], "its only member: the UScriptStruct"), MEDIUM)
    ar = found.get("FArrayProperty", {})
    q = sorted(k for k in ar if k[1] == 8)
    by = sorted(k for k in ar if k[1] == 1)
    if len(q) == 1:
        b.observe("structs.FArrayProperty.Inner", q[0][0], CHANNEL, METHOD, ev("FArrayProperty", q[0], "the inner property"), MEDIUM)
    if len(by) == 1 and len(ar) == 2:
        b.observe("structs.FArrayProperty.ArrayFlags", by[0][0], CHANNEL, METHOD, ev("FArrayProperty", by[0], "its only byte member"), MEDIUM)
    en = found.get("FEnumProperty", {})
    q = sorted(k for k in en if k[1] == 8)
    if len(q) == 2 and len(en) == 2:
        lo, hi = q
        b.observe("structs.FEnumProperty.UnderlyingProp", lo[0], CHANNEL, METHOD,
                  ev("FEnumProperty", lo, "the lower of its two pointer members; UE declares UnderlyingProp first") +
                  ["%d overrides read it against %d for the other" % (en[lo], en[hi])], MEDIUM)
        b.observe("structs.FEnumProperty.Enum", hi[0], CHANNEL, METHOD,
                  ev("FEnumProperty", hi, "the higher of its two pointer members: the UEnum"), MEDIUM)
    bo = found.get("FBoolProperty", {})
    bytes_ = sorted(k for k in bo if k[1] == 1)
    if len(bytes_) == 4 and len(bo) == 4 and [k[0] for k in bytes_] == list(range(total, total + 4)):
        names = ["FieldSize", "ByteOffset", "ByteMask", "FieldMask"]
        for (d, _s), n in zip(bytes_, names):
            b.observe("structs.FBoolProperty." + n, d, CHANNEL, METHOD,
                      ev("FBoolProperty", (d, 1), "four consecutive bytes after the FProperty base: FieldSize, ByteOffset, ByteMask, FieldMask"), MEDIUM)
        # second channel: the override that clears the bit uses ByteOffset as an index and FieldMask as the mask
        vt = _vtable(ctx, cfns["FBoolProperty"])
        init_slot = _book_value(ctx, "vtables.FProperty.InitializeValueInternal")
        if vt in runs and init_slot is not None and init_slot // 8 < len(runs[vt].slots):
            f = runs[vt].slots[init_slot // 8]
            s, e = ctx.index.funcs.span(f)
            fl = Flow(ctx.index.decoder, ctx.index.decoder.decode_function(s, min(e, s + 0x100)))
            for x in fl.events:
                if x.kind == "idxmem" and x.value and x.value[0] == "ld" and x.value[1] == ("arg", 0) and x.value[3] == 1 and \
                        x.target and x.target[0] == "ld" and x.target[1] == ("arg", 0) and x.target[3] == 1:
                    why = "InitializeValue override 0x%x clears a bit: byte [Dest + [this+0x%x]] &= ~[this+0x%x]" % (f, x.value[2], x.target[2])
                    b.observe("structs.FBoolProperty.ByteOffset", x.value[2], USAGE, "property_subtypes.bool_initialise", [why], MEDIUM)
                    b.observe("structs.FBoolProperty.FieldMask", x.target[2], USAGE, "property_subtypes.bool_initialise", [why], MEDIUM)
                    break
    # GetMinAlignment: found from the numeric types, then read in the struct / enum overrides (Struct->..., UnderlyingProp->...)
    al = alignment_slot(ctx, runs, cfns)
    if al is not None:
        slot, why = al
        ctx.put("vtables.FProperty.GetMinAlignment", Hex(slot * 8), HIGH, "property_subtypes.alignment_constants", why)
        for name, path in (("FStructProperty", "structs.FStructProperty.Struct"), ("FEnumProperty", "structs.FEnumProperty.UnderlyingProp")):
            if name not in cfns:
                continue
            vt = _vtable(ctx, cfns[name])
            if vt not in runs or slot >= len(runs[vt].slots):
                continue
            f = runs[vt].slots[slot]
            s, e = ctx.index.funcs.span(f)
            fl = Flow(ctx.index.decoder, ctx.index.decoder.decode_function(s, min(e, s + 0x200)))
            loads = sorted({(x.disp, x.size) for x in fl.events if x.kind == "load" and x.base == ("arg", 0) and x.disp >= total})
            if len(loads) == 1 and loads[0][1] == 8:
                what = "GetMinAlignment override 0x%x loads the only member (+0x%x) and forwards to it" % (f, loads[0][0])
                b.observe(path, loads[0][0], "property_subtypes.get_min_alignment", "property_subtypes.get_min_alignment", [what], MEDIUM)
    ctx.evidence_files["property_subtypes"] = {n: {"%s/%d" % (hex(k[0]), k[1]): c for k, c in v.items()} for n, v in found.items()}
