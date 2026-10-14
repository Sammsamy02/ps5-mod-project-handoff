"""``UGameEngine::Tick`` and the ``UEngine::Tick`` vtable slot.

Two engine-generic strings find the same virtual from two sides:

* ``UEngine::Tick`` -- ``virtual void Tick(float, bool) PURE_VIRTUAL(UEngine::Tick,)`` expands to a stub that passes
  the literal ``TEXT("UEngine::Tick")`` and ``"Pure virtual not implemented (%s)"`` to the fatal-error helper.  The
  stub sits in the ``UEngine`` vtable at the Tick slot (and nowhere else);
* ``causeevent=`` -- the console-command handling inside ``UGameEngine::Tick`` (the string patternsleuth's
  ``gameengine_tick`` resolver anchors on).  The function that references it is ``UGameEngine::Tick``, and it overrides
  the same slot in the ``UGameEngine`` vtable.

Vtables are read from the RELA table (``vtables.py``); the slot index is the position in the run and the byte offset
``VTableLayout.ini`` wants is ``slot * 8``.  The two channels agree when the stub and the override are found at the
same slot of two different vtables.
"""
from __future__ import annotations

from typing import List, Optional, Set

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..vtables import find_runs

STUB_TEXT = "UEngine::Tick"
OVERRIDE_ANCHOR = "causeevent="


def _functions_referencing(ctx: Context, vas: List[int]) -> Set[int]:
    out = set()
    for va in vas:
        for site in ctx.index.xrefs.refs_to(va, [KIND_LEA]):
            f = ctx.index.funcs.containing(site)
            if f is not None:
                out.add(f)
    return out


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    idx = ctx.index
    stub_vas = idx.strings.find_utf16(STUB_TEXT)
    stubs = _functions_referencing(ctx, stub_vas)
    anchor = [(va, t) for va, t in idx.strings.find_utf16_containing(OVERRIDE_ANCHOR) if OVERRIDE_ANCHOR in t]
    anchor_sites = []
    for va, _t in anchor:
        anchor_sites.extend(idx.xrefs.refs_to(va, [KIND_LEA]))
    overrides = sorted({idx.funcs.containing(s) for s in anchor_sites if idx.funcs.containing(s) is not None})
    runs = [r for r in find_runs(ctx.elf, 16) if len(r.slots) >= 32]
    stub_hits = {(k, r.offset) for r in runs for k, f in enumerate(r.slots) if f in stubs}
    over_hits = {(k, r.offset) for r in runs for k, f in enumerate(r.slots) if f in overrides}
    stub_slots = {k for k, _o in stub_hits}
    over_slots = {k for k, _o in over_hits}
    if not stubs and not overrides:
        ctx.fail("vtables.UEngine.Tick", "neither the 'UEngine::Tick' pure-virtual literal nor the 'causeevent=' anchor is in the image")
        ctx.fail("symbols.engine_tick", "neither the 'UEngine::Tick' pure-virtual literal nor the 'causeevent=' anchor is in the image")
        return
    ev: List[str] = []
    slot: Optional[int] = None
    disagreed = False
    conf = LOW
    if len(stub_slots) == 1 and len(stub_hits) >= 1:
        k = next(iter(stub_slots))
        ev.append("the pure-virtual stub 0x%x (references the literal %r) is at slot %d (byte offset 0x%x) of the vtable at 0x%x" % (
            sorted(stubs)[0], STUB_TEXT, k, k * 8, sorted(o for _k, o in stub_hits)[0]))
        slot, conf = k, MEDIUM
    if len(over_slots) == 1 and len(over_hits) >= 1:
        k = next(iter(over_slots))
        ev.append("the function that references %r (0x%x) is at slot %d of the vtable at 0x%x" % (
            OVERRIDE_ANCHOR, overrides[0], k, sorted(o for _k, o in over_hits)[0]))
        if slot is None:
            slot, conf = k, MEDIUM
        elif slot == k:
            conf = HIGH
        else:
            ctx.fail("vtables.UEngine.Tick", "the pure-virtual stub is at slot %d but the 'causeevent=' function is at slot %d" % (slot, k))
            slot, disagreed = None, True
    elif len(over_slots) > 1:
        ev.append("the 'causeevent=' function appears at several slots: %s" % sorted(over_slots))
    if slot is not None:
        ctx.put("vtables.UEngine.Tick", Hex(slot * 8), conf, "engine_tick.vtable_slot", ev)
        ctx.put("vtables.UEngine.Tick_slot_index", Hex(slot), conf, "engine_tick.vtable_slot", ev[:1])
    elif not disagreed:
        ctx.fail("vtables.UEngine.Tick", "the UEngine::Tick stub / UGameEngine::Tick override could not be placed at one vtable slot")
    if len(overrides) == 1 and anchor_sites:
        f = overrides[0]
        c = HIGH if (slot is not None and conf == HIGH) else MEDIUM
        ctx.put("symbols.engine_tick.label", Hex(f), c, "engine_tick.anchor_string",
                ["the only function that references the UTF-16 literal %r; nearest function start at or before the reference (0x%x)" % (
                    OVERRIDE_ANCHOR, anchor_sites[0])] + (["it is also installed at slot %d of the vtable at 0x%x" % (
                        sorted(over_hits)[0][0], sorted(over_hits)[0][1])] if over_hits else []))
        ctx.put("symbols.engine_tick.anchor_xref_label", Hex(anchor_sites[0]), HIGH, "strings", ["rip-relative reference to %r" % OVERRIDE_ANCHOR])
        pro = ctx.elf.read(f, 23)
        if pro:
            ctx.put("symbols.engine_tick.prologue_23", pro.hex(), HIGH, "bytes", ["first 23 bytes at the function start"])
    else:
        ctx.fail("symbols.engine_tick", "%d function(s) reference %r (need exactly 1)" % (len(overrides), OVERRIDE_ANCHOR))
    ctx.evidence_files["engine_tick"] = {"stubs": [hex(f) for f in sorted(stubs)], "overrides": [hex(f) for f in overrides],
                                          "stub_slots": sorted(stub_slots), "override_slots": sorted(over_slots)}
