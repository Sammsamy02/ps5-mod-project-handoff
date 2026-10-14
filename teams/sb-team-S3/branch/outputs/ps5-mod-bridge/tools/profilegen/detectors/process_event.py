"""``UObject::ProcessEvent`` -- its vtable slot and function address.

ProcessEvent is virtual and not overridden by most UObject subclasses, so the same function
address sits at the *same slot index* in thousands of vtables.  Among the functions that are shared
by many vtables at one slot, ProcessEvent is recognised by what it does (the shape is the same in
every UE4/UE5 build):

* it tests ``FUNC_Native`` (``FunctionFlags & 0x400``) on its second argument, the ``UFunction*``
  (``test byte [rsi+F+1], 4`` / ``test dword [rsi+F], 0x400`` / ``bt dword [rsi+F], 10``);
* when native it makes two *virtual* calls through ``this``' vtable -- ``GetFunctionCallspace``
  and ``CallRemoteFunction``, the next two virtuals after ProcessEvent in ``UObject``
  (``call [vt + slot*8 + 8]`` and ``call [vt + slot*8 + 16]``);
* it is a large function (it builds a stack frame for the script call).

The slot index is the position within the vtable run (see ``vtables.py``); the byte offset the
UE4SS ``VTableLayout.ini`` wants is ``slot * 8``.  Because a PS5 (Itanium ABI) vtable starts with
*two* destructor slots (complete + deleting) instead of one, every virtual after the destructor
sits 8 bytes further than in a Win64 template; measuring the slot from the image makes that
irrelevant.
"""
from __future__ import annotations

from collections import Counter
from typing import List, Tuple

from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..vtables import find_runs
from ..x86util import CALLER_SAVED, Regs

MIN_RUN = 32
FUNC_NATIVE = 0x400


def analyse_function(ctx: Context, f: int, slot: int) -> dict:
    from capstone import x86 as xo
    dec = ctx.index.decoder
    regs = Regs(dec)
    st, en = ctx.index.funcs.span(f)
    ins = dec.decode_function(st, min(en, st + 0x1800))
    fn_alias = {"rsi"}                 # UFunction* (second argument)
    this_alias = {"rdi"}
    vt_regs = set()
    native_tests: List[Tuple[int, str, int]] = []
    vcalls: List[int] = []
    for i in ins:
        ops = i.operands
        mn = i.mnemonic
        if mn in ("test", "bt") and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[1].type == xo.X86_OP_IMM and \
                ops[0].mem.base and regs.name(ops[0].mem.base) in fn_alias and ops[0].mem.index == 0:
            sz, imm, d = ops[0].size, ops[1].imm, ops[0].mem.disp
            if mn == "test" and sz == 1 and imm == (FUNC_NATIVE >> 8):
                native_tests.append((d - 1, "test byte [fn+%d], 4" % d, i.address))
            elif mn == "test" and sz in (2, 4, 8) and (imm & 0xFFFFFFFF) == FUNC_NATIVE:
                native_tests.append((d, "test [fn+%d], 0x400" % d, i.address))
            elif mn == "bt" and imm == 10 and sz in (4, 8):
                native_tests.append((d, "bt [fn+%d], 10" % d, i.address))
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM and \
                ops[1].mem.base and regs.name(ops[1].mem.base) in this_alias and ops[1].mem.disp == 0 and ops[1].mem.index == 0:
            vt_regs.add(regs.name(ops[0].reg))
        elif mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG:
            src, dst = regs.name(ops[1].reg), regs.name(ops[0].reg)
            if src in fn_alias:
                fn_alias.add(dst)
            elif dst in fn_alias:
                fn_alias.discard(dst)
            if src in this_alias:
                this_alias.add(dst)
            elif dst in this_alias:
                this_alias.discard(dst)
        elif ops and ops[0].type == xo.X86_OP_REG:
            w = regs.name(ops[0].reg)
            if mn not in ("cmp", "test"):
                fn_alias.discard(w)
                this_alias.discard(w)
                if mn != "mov" or ops[1].type != xo.X86_OP_MEM:
                    vt_regs.discard(w)
        if mn == "call":
            if ops and ops[0].type == xo.X86_OP_MEM and ops[0].mem.index == 0 and ops[0].mem.base and \
                    regs.name(ops[0].mem.base) in vt_regs:
                vcalls.append(ops[0].mem.disp)
            for r in CALLER_SAVED:
                fn_alias.discard(r)
                this_alias.discard(r)
    own = slot * 8
    adjacent = sorted(d for d in set(vcalls) if own < d <= own + 0x10)
    return {"native_tests": native_tests, "vcalls": vcalls, "adjacent": adjacent, "size": en - st,
            "score": (4 if native_tests else 0) + 3 * min(len(adjacent), 2) + (1 if en - st >= 0xC0 else 0)}


def dtor_prefix_evidence(runs) -> Tuple[int, int, int]:
    """(long runs, runs whose slots 0 and 1 are two distinct nearby functions, runs where they are equal)."""
    long_runs = [r for r in runs if len(r.slots) >= MIN_RUN]
    pairs = sum(1 for r in long_runs if r.slots[0] != r.slots[1] and abs(r.slots[1] - r.slots[0]) <= 0x100)
    same = sum(1 for r in long_runs if r.slots[0] == r.slots[1])
    return len(long_runs), pairs, same


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    runs = [r for r in find_runs(ctx.elf, 16)]
    long_runs = [r for r in runs if len(r.slots) >= MIN_RUN]
    if not long_runs:
        ctx.fail("vtables.UObject.ProcessEvent", "no vtable runs found in the RELA table")
        return
    counts: Counter = Counter()
    for r in long_runs:
        for k, fn in enumerate(r.slots[:256]):
            counts[(k, fn)] += 1
    threshold = max(8, len(long_runs) // 40)
    cands = sorted(((k, fn, c) for (k, fn), c in counts.items() if c >= threshold), key=lambda t: -t[2])
    scored = []
    for k, fn, c in cands[:400]:
        a = analyse_function(ctx, fn, k)
        if a["native_tests"]:
            scored.append((a["score"], c, k, fn, a))
    if not scored:
        ctx.fail("vtables.UObject.ProcessEvent", "no widely shared vtable function tests FUNC_Native on its second argument")
        return
    scored.sort(key=lambda t: (-t[0], -t[1]))
    score, c, slot, fn, a = scored[0]
    runner = scored[1] if len(scored) > 1 else None
    ev = ["%d of %d vtable runs (>= %d slots) hold function 0x%x at slot %d (byte offset 0x%x)" % (
        c, len(long_runs), MIN_RUN, fn, slot, slot * 8),
        "it tests FUNC_Native on its second argument: %s at 0x%x" % (a["native_tests"][0][1], a["native_tests"][0][2]),
        "virtual calls through this' vtable at byte offsets %s (adjacent virtuals: %s)" % (
            ", ".join("0x%x" % d for d in sorted(set(a["vcalls"]))[:8]), ", ".join("0x%x" % d for d in a["adjacent"]) or "none"),
        "function span 0x%x bytes" % a["size"]]
    if runner:
        ev.append("runner-up candidate: 0x%x at slot %d in %d runs, score %d vs %d" % (runner[3], runner[2], runner[1], runner[0], score))
    unique = runner is None or runner[0] < score
    if score >= 8 and unique and c >= 20 and c * 10 >= len(long_runs):
        conf = HIGH
    elif score >= 7 and unique:
        conf = MEDIUM
    else:
        conf = LOW
    ctx.put("vtables.UObject.ProcessEvent", Hex(slot * 8), conf, "process_event.shared_vtable_function", ev)
    ctx.put("vtables.UObject.ProcessEvent_slot_index", Hex(slot), conf, "process_event.shared_vtable_function", ev[:1])
    ctx.put("symbols.process_event.label", Hex(fn), conf, "process_event.shared_vtable_function", ev)
    pro = ctx.elf.read(fn, 24)
    if pro:
        ctx.put("symbols.process_event.prologue_24", pro.hex(), HIGH, "bytes", ["first 24 bytes at the function start"])
    if len(a["adjacent"]) == 2:
        ctx.put("vtables.UObject.GetFunctionCallspace", Hex(a["adjacent"][0]), LOW, "process_event.adjacent_virtual_calls",
                ["first of the two virtual calls ProcessEvent makes after the FUNC_Native test"])
        ctx.put("vtables.UObject.CallRemoteFunction", Hex(a["adjacent"][1]), LOW, "process_event.adjacent_virtual_calls",
                ["second of the two virtual calls ProcessEvent makes after the FUNC_Native test"])
    d, kind, addr = a["native_tests"][0]
    ctx.put("structs.UFunction.FunctionFlags", Hex(d), MEDIUM, "process_event.native_test",
            ["%s at 0x%x: FUNC_Native (0x400) is bit 10 of FunctionFlags, so FunctionFlags starts at +0x%x" % (kind, addr, d)])
    n_long, pairs, same = dtor_prefix_evidence(long_runs)
    if n_long:
        # The Itanium two-destructor prefix is a layout the image has to show: most long runs must start with two
        # distinct, nearby functions (complete + deleting destructor).  A Win64-style single destructor, or runs
        # whose first two slots are unrelated, leave the claim unresolved rather than asserting 2.
        fact = "%d of %d vtable runs start with two distinct functions within 0x100 bytes of each other" % (pairs, n_long)
        if pairs * 2 >= n_long:
            ctx.put("vtables.UObject.dtor_prefix_slots", Hex(2), MEDIUM, "process_event.itanium_destructor_pair",
                    ["Itanium C++ ABI (clang/Prospero): complete and deleting destructors occupy vtable slots 0 and 1", fact])
        else:
            ctx.fail("vtables.UObject.dtor_prefix_slots",
                     "%s: not the Itanium two-destructor prefix (%d runs repeat one function in slots 0 and 1); the "
                     "ProcessEvent slot above is measured from the run start and does not depend on it" % (fact, same))
    ctx.facts["process_event"] = (slot, fn)
    ctx.evidence_files["process_event"] = {
        "runs": len(runs), "long_runs": len(long_runs), "candidates": [
            {"slot": k, "function": hex(fn_), "runs": c_, "score": s_, "native_tests": [(d_, kk, hex(ad)) for d_, kk, ad in aa["native_tests"]],
             "vcalls": [hex(x) for x in sorted(set(aa["vcalls"]))]} for s_, c_, k, fn_, aa in scored[:6]]}
