"""``StaticConstructObject_Internal`` -- the engine's object factory.

Method: UE's ``NewObject<T>()`` template contains

    if (Name == NAME_None)
        FObjectInitializer::AssertIfInConstructor(Outer, TEXT("NewObject with empty name can't be used to create default subobjects ..."));
    FStaticConstructObjectParameters Params(Class);
    Params.Outer = Outer; ...
    return static_cast<T*>(StaticConstructObject_Internal(Params));

so the long message literal is referenced from *every* ``NewObject`` instantiation (thousands of
sites), and shortly after each reference the code calls the ``FStaticConstructObjectParameters``
constructor and then ``StaticConstructObject_Internal`` with the *same* stack address in ``rdi``.

For each reference the instructions up to the end of the window are decoded; consecutive direct
calls that receive the same stack-allocated struct (``lea reg,[rbp-X]`` copied into ``rdi``) form a
pair ``(params_ctor, static_construct)``.  The pair seen in the most windows names both functions;
the second is the factory.  Sanity checks: it must be a large frame-pointer function with many
callers, and the dominant pair must clearly beat the runner-up.
"""
from __future__ import annotations

from collections import Counter
from typing import Dict, List, Optional, Tuple

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..x86util import CALLER_SAVED, Regs

ANCHOR_FRAGMENT = "NewObject with empty name"
WINDOW = 220


def first_call(ctx: Context, site: int) -> Optional[int]:
    """Target of the first direct call after the literal reference (the AssertIfInConstructor call)."""
    from capstone import x86 as xo
    for i in ctx.index.decoder.decode(site, 48):
        if i.mnemonic == "call":
            return i.operands[0].imm if i.operands and i.operands[0].type == xo.X86_OP_IMM else None
        if i.mnemonic in ("ret", "jmp"):
            return None
    return None


def call_pairs(ctx: Context, site: int) -> List[Tuple[int, int]]:
    """Consecutive direct calls in the window that share a stack-struct ``rdi`` argument."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    dec = ctx.index.decoder
    regs = Regs(dec)
    ins = dec.decode(site, WINDOW)
    origin: Dict[str, tuple] = {}
    calls: List[Tuple[int, Optional[tuple]]] = []
    for i in ins:
        ops = i.operands
        mn = i.mnemonic
        if mn == "ret":
            break
        if mn == "call":
            if ops and ops[0].type == xo.X86_OP_IMM:
                calls.append((ops[0].imm, origin.get("rdi")))
            for r in CALLER_SAVED:
                origin.pop(r, None)
            continue
        if mn == "lea" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM and \
                ops[1].mem.base in (xc.X86_REG_RBP, xc.X86_REG_RSP) and ops[1].mem.index == 0:
            origin[regs.name(ops[0].reg)] = ("stack", regs.name(ops[1].mem.base), ops[1].mem.disp)
            continue
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and ops[0].size == 8:
            src = origin.get(regs.name(ops[1].reg))
            if src is not None:
                origin[regs.name(ops[0].reg)] = src
            else:
                origin.pop(regs.name(ops[0].reg), None)
            continue
        for w in regs.written(i):
            origin.pop(w, None)
    pairs = []
    for (t0, o0), (t1, o1) in zip(calls, calls[1:]):
        if o0 is not None and o0 == o1 and o0[0] == "stack":
            pairs.append((t0, t1))
    return pairs


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    idx = ctx.index
    lits = idx.strings.find_utf16_containing(ANCHOR_FRAGMENT)
    if not lits:
        ctx.fail("symbols.static_construct_object_internal", "the NewObject empty-name message literal is not in the image")
        return
    sites: List[int] = []
    for va, _t in lits:
        sites.extend(idx.xrefs.refs_to(va, [KIND_LEA]))
    sites = sorted(set(sites))
    pair_count: Counter = Counter()
    first_calls: Counter = Counter()
    windows_with_pair = 0
    for s in sites:
        fc = first_call(ctx, s)
        if fc is not None:
            first_calls[fc] += 1
        ps = call_pairs(ctx, s)
        if ps:
            windows_with_pair += 1
        for p in set(ps):
            pair_count[p] += 1
    if not pair_count:
        ctx.fail("symbols.static_construct_object_internal",
                 "no window after the %d literal references shows two calls sharing a stack-struct argument" % len(sites))
        return
    ranked = pair_count.most_common(3)
    (ctor, scoi), n_best = ranked[0]
    n_second = ranked[1][1] if len(ranked) > 1 else 0
    funcs = idx.funcs
    is_start = funcs.is_start(scoi)
    st, en = funcs.span(scoi)
    prologue = ctx.elf.read(scoi, 16)
    callers = len(idx.calls.callers(scoi))
    big = (en - st) >= 0x100
    pushes = 0
    if prologue:
        # push r15/r14/r13/r12/rbx after the frame setup
        pushes = sum(1 for k in range(4, 16) if prologue[k:k + 1] == b"\x41" and prologue[k + 1:k + 2] in b"\x57\x56\x55\x54") + \
            (1 if b"\x53" in prologue[4:16] else 0)
    ev = ["%d references to the literal %s at vaddr(s) %s" % (
        len(sites), ANCHOR_FRAGMENT, ", ".join("0x%x" % v for v, _ in lits)),
        "%d windows contain two consecutive calls sharing a stack-struct rdi argument" % windows_with_pair,
        "dominant pair (0x%x, 0x%x) in %d windows; runner-up pair in %d" % (ctor, scoi, n_best, n_second),
        "second member 0x%x: function start=%s, span 0x%x bytes, %d callee-saved pushes, %d direct callers, prologue %s" % (
            scoi, is_start, en - st, pushes, callers, prologue.hex() if prologue else "?")]
    conf = HIGH if (n_best >= 20 and n_best > 2 * n_second and is_start and (big or pushes >= 4) and callers >= 20) else \
        (MEDIUM if n_best >= 5 and n_best > n_second and is_start else LOW)
    ctx.put("symbols.static_construct_object_internal.label", Hex(scoi), conf, "static_construct.new_object_pair", ev)
    ctx.put("symbols.sco_params_ctor.label", Hex(ctor), conf, "static_construct.new_object_pair",
            ["first member of the dominant pair: receives the same stack struct, one call earlier (%d windows)" % n_best])
    ctx.put("symbols.static_construct_object_internal.anchor_string_label", Hex(lits[0][0]), HIGH, "strings",
            ["UTF-16 literal %r" % lits[0][1][:60]])
    if first_calls:
        fc, n = first_calls.most_common(1)[0]
        ctx.put("symbols.assert_if_in_constructor.label", Hex(fc), MEDIUM, "static_construct.new_object_pair",
                ["first call after the literal reference in %d of %d windows (the assertion that the literal is the message of)" % (
                    n, len(sites))])
    if prologue:
        ctx.put("symbols.static_construct_object_internal.prologue_16", prologue.hex(), HIGH, "bytes",
                ["first 16 bytes at the function start"])
    ctx.facts["static_construct_object"] = scoi
    ctx.evidence_files["static_construct"] = {
        "pairs": [{"ctor": hex(a), "scoi": hex(b), "windows": n} for (a, b), n in ranked],
        "references": len(sites), "windows_with_pair": windows_with_pair}
