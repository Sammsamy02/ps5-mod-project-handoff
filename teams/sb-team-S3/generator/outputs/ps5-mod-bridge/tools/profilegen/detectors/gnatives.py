"""``GNatives`` -- the table of native script functions (``FNativeFuncPtr GNatives[256]``).

Two independent facts find the same global:

1. **The script VM dispatches through it.**  ``FFrame::Step`` (inlined into every ``exec*`` function) reads the next
   bytecode and calls the function pointer at that index::

       B = *Code++;   GNatives[B](Context, *this, Result);
       movzx eax, byte [code] ; lea rcx,[rip+GNatives] ; call [rcx + rax*8]

   The only zero-initialised (BSS) global that is the base of many ``call [base + index*8]`` sites whose index is a
   freshly loaded byte is ``GNatives``;
2. **Static initialisation fills it.**  Every ``IMPLEMENT_VM_FUNCTION(EX_x, execX)`` registers its function with
   ``GNatives[EX_x] = &UObject::execX``, so the same global is the target of many absolute stores ``mov [G + 8*k], reg``
   for many different ``k`` within 256 entries (a byte index).

The function that contains the most such dispatch sites and has by far the most callers is ``FFrame::Step`` itself.
"""
from __future__ import annotations

import re
from collections import Counter
from typing import List, Tuple

from ..index import KIND_LEA, KIND_STORE
from ..model import HIGH, MEDIUM, Context, DetectorUnavailable, Hex
from ..x86util import canon64

MIN_SITES = 8
MIN_STEP_CALLERS = 100
ENTRIES = 256
_SCALED_CALL = re.compile(rb"\xff[\x14\x54\x94][\xc0-\xff]", re.S)          # call qword [base + index*8 (+ disp)]


def bss_ranges(ctx: Context) -> List[Tuple[int, int]]:
    return [(s.vaddr + s.filesz, s.vaddr + s.memsz) for s in ctx.elf.loads if s.writable and s.memsz > s.filesz]


def dispatch_sites(ctx: Context, g: int) -> List[int]:
    from capstone import x86 as xo
    dec = ctx.index.decoder
    out = []
    for site in ctx.index.xrefs.refs_to(g, [KIND_LEA]):
        if not _SCALED_CALL.search(ctx.elf.read_upto(site, 48)):
            continue
        ins = dec.decode(site, 96, 10)
        if len(ins) < 3 or not ins[0].operands or ins[0].operands[0].type != xo.X86_OP_REG:
            continue
        base = canon64(dec.reg_name(ins[0].operands[0].reg))
        loaded = set()
        for i in ins[1:]:
            ops = i.operands
            if i.mnemonic in ("movzx", "mov") and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM and ops[1].size == 1:
                loaded.add(canon64(dec.reg_name(ops[0].reg)))
            if i.mnemonic == "call" and ops and ops[0].type == xo.X86_OP_MEM:
                m = ops[0].mem
                if m.scale == 8 and m.disp == 0 and m.index and m.base and canon64(dec.reg_name(m.base)) == base and \
                        canon64(dec.reg_name(m.index)) in loaded:
                    out.append(site)
                    break
            if ops and ops[0].type == xo.X86_OP_REG and canon64(dec.reg_name(ops[0].reg)) == base and i.mnemonic not in ("cmp", "test", "call"):
                break                       # the base register was overwritten
    return out


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    idx = ctx.index
    counts: Counter = Counter()
    for lo, hi in bss_ranges(ctx):
        for target, _site, kind, _reg in idx.xrefs.iter_range(lo, hi):
            if kind == KIND_LEA:
                counts[target] += 1
    cands = [t for t, c in counts.items() if c >= MIN_SITES]
    scored = []
    for t in cands:
        sites = dispatch_sites(ctx, t)
        if len(sites) >= MIN_SITES:
            scored.append((len(sites), t, sites))
    if not scored:
        ctx.fail("symbols.gnatives", "no zero-initialised global is the base of %d or more 'call [base + index*8]' sites with a byte index" % MIN_SITES)
        return
    scored.sort(reverse=True)
    n, g, sites = scored[0]
    runner = scored[1][0] if len(scored) > 1 else 0
    if runner * 2 > n:
        ctx.fail("symbols.gnatives", "two globals compete: 0x%x (%d sites) and 0x%x (%d sites)" % (g, n, scored[1][1], runner))
        return
    stores = sorted({t - g for t, _s, k, _r in idx.xrefs.iter_range(g, g + ENTRIES * 8) if k == KIND_STORE and (t - g) % 8 == 0})
    ev = ["%d `call [G + index*8]` sites whose index is a freshly loaded byte use the lea of 0x%x as base; runner-up: %d" % (n, g, runner)]
    conf = MEDIUM
    if len(stores) >= 32:
        ev.append("%d distinct absolute stores `mov [G + 8*k], reg` (k up to 0x%x) fill it from static initialisers" % (len(stores), stores[-1] // 8))
        conf = HIGH
    funcs = Counter(idx.funcs.containing(s) for s in sites)
    step = None
    best_callers = 0
    for f in funcs:
        if f is None:
            continue
        st, en = idx.funcs.span(f)
        n_callers = len(idx.calls.callers(f)) if en - st <= 0x60 else 0
        if n_callers >= MIN_STEP_CALLERS and n_callers > best_callers:
            step, best_callers = f, n_callers
    ctx.put("symbols.gnatives.label", Hex(g), conf, "gnatives.dispatch_table", ev)
    ctx.put("symbols.gnatives.entries", Hex(ENTRIES), MEDIUM, "gnatives.byte_index", ["the opcode is a byte, so the table is indexed 0..255"])
    if step is not None:
        ctx.put("symbols.fframe_step.label", Hex(step), MEDIUM, "gnatives.dispatch_table",
                ["small function with %d direct callers that dispatches through GNatives (FFrame::Step)" % len(idx.calls.callers(step))])
    ctx.evidence_files["gnatives"] = {"candidates": [{"global": hex(t), "sites": c} for c, t, _s in scored[:5]],
                                       "absolute_store_slots": len(stores), "step": hex(step) if step else None}
