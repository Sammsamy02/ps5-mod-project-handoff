"""``GMalloc`` (the global ``FMalloc*``), ``FMemory::Malloc`` / ``FMemory::Free`` and the FMalloc vtable slots.

Method:

* ``FMemory::Malloc`` is the callee of ``FNamePool::Init``'s block allocation (``fname_pool``
  detector).  Its body is a tiny dispatcher

      lea  r14, [rip + G]          ; or: mov rdi, [rip + G]
      mov  rdi, [r14]              ; FMalloc* GMalloc  (null test -> lazy create path)
      mov  rax, [rdi]              ; vtable
      call [rax + d_malloc]        ; virtual Malloc(size, alignment)

  so ``G`` is the variable the engine itself dereferences and ``d_malloc`` is the vtable byte
  offset of ``FMalloc::Malloc``.
* ``FMemory::Free`` is the same shape over the *same* global with a different vtable offset and a
  single argument; of the candidates the most-called one is ``Free``.
* ``GMalloc`` as reported here is ``G``: what ``FMemory::Malloc`` and ``FMemory::Free`` actually load.
* The lazy-create function that stores into ``G`` is also decoded: the value it stores comes from a
  one-instruction accessor that returns the raw (undecorated) allocator pointer held in a second
  global, reported as ``gmalloc_base_slot``.  That is the variable the engine's bootstrap code
  fills once; it is *not* what ``FMemory::Malloc`` reads.
* ``build.allocator_family`` is the most frequent ``FMalloc<Name>`` identifier among UTF-16 strings.
"""
from __future__ import annotations

import re
from collections import Counter, defaultdict
from typing import Dict, List, Optional, Tuple

from ..index import KIND_LEA
from ..model import HIGH, MEDIUM, Context, DetectorUnavailable, Hex

_FAMILY = re.compile(rb"F\x00M\x00a\x00l\x00l\x00o\x00c\x00((?:[A-Za-z]\x00)+(?:[0-9]\x00)?)", re.S)


def analyse_dispatch(ctx: Context, start: int, limit: int = 0x180) -> Optional[dict]:
    """First ``global -> vtable -> call [vt+d]`` dispatch in the function at ``start``."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    dec = ctx.index.decoder
    ins = dec.decode(start, limit)
    glob: Dict[str, Tuple[int, str]] = {}        # reg -> (G, 'addr'|'value'): reg holds &G or *G
    vt: Dict[str, int] = {}                      # reg -> G whose vtable it holds
    written_args: List[str] = []
    arg_regs = {"rsi": "rsi", "esi": "rsi", "rdx": "rdx", "edx": "rdx", "rcx": "rcx", "ecx": "rcx"}
    for i in ins:
        ops = i.operands
        mn = i.mnemonic
        if mn in ("ret", "jmp") and not glob:
            break
        if mn == "lea" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM and \
                ops[1].mem.base == xc.X86_REG_RIP:
            glob[dec.reg_name(ops[0].reg)] = (i.address + i.size + ops[1].mem.disp, "addr")
            continue
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM:
            m = ops[1].mem
            dst = dec.reg_name(ops[0].reg)
            if m.base == xc.X86_REG_RIP:
                glob[dst] = (i.address + i.size + m.disp, "value")
                continue
            base = dec.reg_name(m.base) if m.base else ""
            if base in glob and glob[base][1] == "addr" and m.index == 0 and m.disp == 0:
                glob[dst] = (glob[base][0], "value")
                continue
            if base in glob and glob[base][1] == "value" and m.index == 0 and m.disp == 0:
                vt[dst] = glob[base][0]
                continue
        if mn in ("call", "jmp") and len(ops) == 1 and ops[0].type == xo.X86_OP_MEM and ops[0].mem.index == 0:
            base = dec.reg_name(ops[0].mem.base) if ops[0].mem.base else ""
            if base in vt:
                return {"global": vt[base], "disp": ops[0].mem.disp, "args": sorted(set(written_args)),
                        "kind": "lea" if any(v[1] == "addr" for v in glob.values()) else "load", "call_site": i.address}
        if ops and ops[0].type == xo.X86_OP_REG:
            nm = dec.reg_name(ops[0].reg)
            if nm in arg_regs and mn in ("mov", "xor", "lea", "movzx", "movsxd"):
                written_args.append(arg_regs[nm])
    return None


def find_free(ctx: Context, g: int, malloc: int, malloc_disp: int) -> Optional[dict]:
    """The most-called one-argument dispatcher over ``g`` with a vtable offset other than Malloc's."""
    funcs = ctx.index.funcs
    best = None
    seen = set()
    for site in ctx.index.xrefs.refs_to(g, [KIND_LEA]):
        f = funcs.containing(site)
        if f is None or f in seen or f == malloc:
            continue
        seen.add(f)
        st, en = funcs.span(f)
        if en - st > 0x100:
            continue
        d = analyse_dispatch(ctx, f)
        if not d or d["global"] != g or d["disp"] == malloc_disp or "rdx" in d["args"] or "rcx" in d["args"]:
            continue
        callers = len(ctx.index.calls.callers(f))
        cand = {"function": f, "disp": d["disp"], "callers": callers, "size": en - st}
        if best is None or callers > best["callers"]:
            best = cand
    return best


def base_allocator_slot(ctx: Context, g: int) -> Optional[dict]:
    """Decode the writers of ``g``: ``call A; ... mov [g], rax`` and read the global A returns."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    dec = ctx.index.decoder
    funcs = ctx.index.funcs
    seen = set()
    for site in ctx.index.xrefs.refs_to(g, [KIND_LEA]):
        f = funcs.containing(site)
        if f is None or f in seen:
            continue
        seen.add(f)
        st, en = funcs.span(f)
        if en - st > 0x600:
            continue
        ins = dec.decode_function(st, en)
        addr_regs: Dict[str, int] = {}
        last_call = None
        rax_from_call = False
        for i in ins:
            ops = i.operands
            if i.mnemonic == "call" and ops and ops[0].type == xo.X86_OP_IMM:
                last_call, rax_from_call = ops[0].imm, True
                continue
            if i.mnemonic == "lea" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM \
                    and ops[1].mem.base == xc.X86_REG_RIP:
                addr_regs[dec.reg_name(ops[0].reg)] = i.address + i.size + ops[1].mem.disp
                continue
            if i.mnemonic == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[0].size == 8 and \
                    ops[1].type == xo.X86_OP_REG and dec.reg_name(ops[1].reg) == "rax" and rax_from_call and \
                    ops[0].mem.base and addr_regs.get(dec.reg_name(ops[0].mem.base)) == g and ops[0].mem.disp == 0:
                # the stored value is the result of `last_call`: is that a one-load accessor?
                acc = dec.decode(last_call, 24)
                for a in acc[:4]:
                    if a.mnemonic == "mov" and len(a.operands) == 2 and a.operands[1].type == xo.X86_OP_MEM and \
                            a.operands[1].mem.base == xc.X86_REG_RIP:
                        return {"writer": f, "accessor": last_call, "slot": a.address + a.size + a.operands[1].mem.disp,
                                "store_site": i.address}
                return None
            if ops and ops[0].type == xo.X86_OP_REG and dec.reg_name(ops[0].reg) == "rax":
                rax_from_call = False
    return None


def bootstrap_of_slot(ctx: Context, slot: int) -> Optional[dict]:
    """The single store of the base-allocator pointer and the construction next to it."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    from ..index import KIND_STORE
    dec = ctx.index.decoder
    stores = ctx.index.xrefs.refs_to(slot, [KIND_STORE])
    if len(stores) != 1:
        return {"stores": len(stores)}
    f = ctx.index.funcs.containing(stores[0])
    ins = dec.decode(max(stores[0] - 24, f or 0), 40)
    inst_va = ctor = None
    for k, i in enumerate(ins):
        if i.address >= stores[0]:
            break
        if i.mnemonic == "lea" and len(i.operands) == 2 and i.operands[1].type == xo.X86_OP_MEM and \
                i.operands[1].mem.base == xc.X86_REG_RIP:
            inst_va = i.address + i.size + i.operands[1].mem.disp
        if i.mnemonic == "call" and i.operands and i.operands[0].type == xo.X86_OP_IMM:
            ctor = i.operands[0].imm
    return {"stores": 1, "store_site": stores[0], "factory": f, "instance": inst_va, "ctor": ctor}


def allocator_family(ctx: Context) -> Optional[Tuple[str, Counter, List[int]]]:
    elf = ctx.elf
    tokens: Counter = Counter()
    where: Dict[str, List[int]] = defaultdict(list)
    for seg in elf.loads:
        if seg.executable:
            continue
        for m in _FAMILY.finditer(elf.data, seg.offset, seg.offset + seg.filesz):
            tok = "FMalloc" + m.group(1).decode("utf-16-le")
            tokens[tok] += 1
            where[tok].append(seg.vaddr + m.start() - seg.offset)
    if not tokens:
        return None
    best = tokens.most_common(1)[0][0]
    return best, tokens, where[best]


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    fam = allocator_family(ctx)
    if fam:
        name, tokens, vas = fam
        ctx.put("build.allocator_family", name, MEDIUM, "gmalloc.family_literals",
                ["UTF-16 identifiers starting FMalloc: " + ", ".join("%s x%d" % kv for kv in tokens.most_common(5)),
                 "most frequent at vaddrs " + ", ".join("0x%x" % v for v in vas[:4])])
    malloc = ctx.facts.get("fmemory_malloc")
    if malloc is None:
        ctx.fail("symbols.gmalloc", "FMemory::Malloc was not found (needs the fname_pool detector)")
        return
    d = analyse_dispatch(ctx, malloc)
    if d is None:
        ctx.fail("symbols.gmalloc", "FMemory::Malloc at 0x%x is not a global -> vtable -> call dispatcher" % malloc)
        return
    g = d["global"]
    funcs = ctx.index.funcs
    callers = len(ctx.index.calls.callers(malloc))
    ctx.put("symbols.fmemory_malloc.label", Hex(malloc), HIGH, "gmalloc.dispatch_shape",
            ["callee of FNamePool::Init's Malloc(block, align) call",
             "body is a %s-loaded global -> vtable -> `call [vt+0x%x]` dispatcher with args %s" % (d["kind"], d["disp"], "/".join(d["args"])),
             "%d direct call sites" % callers])
    free = find_free(ctx, g, malloc, d["disp"])
    ev = ["FMemory::Malloc at 0x%x loads the allocator pointer from 0x%x (%s) and calls vtable+0x%x" % (malloc, g, d["kind"], d["disp"])]
    conf = MEDIUM
    if free:
        ev.append("FMemory::Free at 0x%x (%d direct call sites) loads the same variable and calls vtable+0x%x" % (
            free["function"], free["callers"], free["disp"]))
        conf = HIGH
        ctx.put("symbols.fmemory_free.label", Hex(free["function"]), HIGH, "gmalloc.dispatch_shape",
                ["one-argument dispatcher over the same global as FMemory::Malloc, vtable+0x%x, most-called of %s" % (
                    free["disp"], "the dispatchers found"), "%d direct call sites" % free["callers"]])
        ctx.put("vtables.FMalloc.Free", Hex(free["disp"]), HIGH, "gmalloc.dispatch_shape",
                ["`call [vtable + 0x%x]` in FMemory::Free (slot %d with 8-byte entries)" % (free["disp"], free["disp"] // 8)])
        ctx.facts["fmemory_free"] = free["function"]
    else:
        ctx.fail("symbols.fmemory_free", "no one-argument dispatcher over 0x%x with a different vtable offset" % g)
    ctx.put("vtables.FMalloc.Malloc", Hex(d["disp"]), HIGH if free else MEDIUM, "gmalloc.dispatch_shape",
            ["`call [vtable + 0x%x]` in FMemory::Malloc (slot %d with 8-byte entries)" % (d["disp"], d["disp"] // 8)])
    n_lea = ctx.index.xrefs.count_to(g)
    ev.append("%d rip-relative references to the variable image-wide" % n_lea)
    for name in ("symbols.gmalloc.label", "symbols.gmalloc_effective_ptr.label"):
        ctx.put(name, Hex(g), conf, "gmalloc.dispatch_shape", ev)
    ctx.facts["gmalloc"] = g
    base = base_allocator_slot(ctx, g)
    if base:
        boot = bootstrap_of_slot(ctx, base["slot"])
        e2 = ["the function that fills 0x%x (at 0x%x) stores the result of accessor 0x%x" % (g, base["writer"], base["accessor"]),
              "the accessor returns the contents of 0x%x" % base["slot"]]
        if boot and boot.get("stores") == 1:
            e2.append("exactly one store to 0x%x image-wide, in factory 0x%x (site 0x%x)" % (
                base["slot"], boot["factory"] or 0, boot["store_site"]))
        ctx.put("symbols.gmalloc_base_slot.label", Hex(base["slot"]), MEDIUM, "gmalloc.lazy_create_accessor", e2)
        if boot and boot.get("stores") == 1:
            ctx.put("symbols.gmalloc_bootstrap_store_site.label", Hex(boot["store_site"]), MEDIUM,
                    "gmalloc.lazy_create_accessor", ["the only store of the base allocator pointer"])
            if boot.get("factory"):
                ctx.put("symbols.gmalloc_factory.label", Hex(boot["factory"]), MEDIUM, "gmalloc.lazy_create_accessor",
                        ["function containing the single store"])
            if boot.get("instance"):
                ctx.put("symbols.gmalloc_instance.label", Hex(boot["instance"]), MEDIUM, "gmalloc.lazy_create_accessor",
                        ["address loaded just before the constructor call that precedes the store"])
            if boot.get("ctor"):
                ctx.put("symbols.gmalloc_instance_ctor.label", Hex(boot["ctor"]), MEDIUM, "gmalloc.lazy_create_accessor",
                        ["last call before the store"])
    ctx.evidence_files["gmalloc"] = {"dispatch": d, "free": free, "base": base}
