"""``FNamePool`` -- the global name table, its field offsets, and ``FMemory::Malloc``.

Method:

1. **Find ``FNamePool::Init``.**  UE hard-codes a table of default names (``None``,
   ``ByteProperty``, ``IntProperty``, ... 200+ of them) and registers them in the pool
   constructor, one ``lea rsi,[ascii literal]; mov edx,len; call Store`` per name.  The function
   that references the largest number of *distinct* default-name literals is ``Init``.
2. **Read the layout out of Init.**
     memset(pool + A, 0, S)          -> A = ``CurrentBlock`` offset; the cleared span covers
                                        CurrentBlock, CurrentByteCursor and ``Blocks[]``
     block = FMemory::Malloc(B, 2);  -> B = block size; ``mov [pool + D], rax`` -> D = ``Blocks``
     id = Store(pool, "None", 4);    -> ``mov [pool + N], eax`` -> N = ``NoneId``
     next literal "ByteProperty"     -> following store gives ``BytePropertyId``
     lea r,[pool + H]; add r, 0x40.. -> hash bucket table at H with that stride
   ``Malloc`` is the callee of the block allocation, i.e. ``FMemory::Malloc``.
3. **Find the pool address.**  Init takes the pool as ``this`` (rdi): the pool is the global
   address loaded into rdi (``lea rdi,[rip+X]``) at its call sites.
4. **Cross-check** with the block-growth function (the other caller of ``Malloc(B, 2)``): it must
   read ``CurrentBlock`` and ``CurrentByteCursor`` as consecutive u32s and store the new block
   at ``Blocks[CurrentBlock]``; and with the readers of the pool (``id >> 16`` selects the block,
   ``word [block + off*2]`` is the entry header).
"""
from __future__ import annotations

import struct
from collections import Counter, defaultdict
from typing import Dict, List, Optional, Tuple

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex

DEFAULT_NAME_LITERALS = [
    "None", "ByteProperty", "IntProperty", "BoolProperty", "FloatProperty", "ObjectProperty", "NameProperty",
    "DelegateProperty", "DoubleProperty", "ArrayProperty", "StructProperty", "VectorProperty", "RotatorProperty",
    "StrProperty", "TextProperty", "InterfaceProperty", "MulticastDelegateProperty", "WeakObjectProperty",
    "LazyObjectProperty", "SoftObjectProperty", "UInt64Property", "UInt32Property", "UInt16Property",
    "Int64Property", "Int16Property", "Int8Property", "MapProperty", "SetProperty", "EnumProperty",
    "Class", "Package", "Object", "Function", "Struct", "Enum", "Default__Object",
]
MIN_DISTINCT = 8


def find_init(ctx: Context) -> Optional[dict]:
    st = ctx.index.strings
    xr = ctx.index.xrefs
    funcs = ctx.index.funcs
    hits: Dict[int, set] = defaultdict(set)
    sites_of: Dict[str, List[int]] = {}
    for name in DEFAULT_NAME_LITERALS:
        for va in st.find_ascii(name):
            for site in xr.refs_to(va, [KIND_LEA]):
                f = funcs.containing(site)
                if f is not None:
                    hits[f].add(name)
                    sites_of.setdefault(name, []).append(site)
    if not hits:
        return None
    ranked = sorted(hits, key=lambda f: (-len(hits[f]), f))
    best = ranked[0]
    runner = len(hits[ranked[1]]) if len(ranked) > 1 else 0
    if len(hits[best]) < MIN_DISTINCT:
        return None
    return {"func": best, "distinct": len(hits[best]), "runner_up": runner, "literals": sorted(hits[best])}


def _reg(dec, rid) -> str:
    return dec.reg_name(rid)


def analyse_init(ctx: Context, init: int) -> dict:
    """Decode Init and pull out the layout facts."""
    from capstone import x86 as xo
    dec = ctx.index.decoder
    funcs = ctx.index.funcs
    start, end = funcs.span(init)
    ins = dec.decode_function(start, end)
    out: Dict[str, object] = {"instructions": len(ins)}
    calls = Counter()
    for i in ins:
        if i.mnemonic == "call" and i.operands and i.operands[0].type == xo.X86_OP_IMM:
            calls[i.operands[0].imm] += 1
    store_fn, store_calls = (calls.most_common(1)[0] if calls else (None, 0))
    out["store_function"], out["store_calls"] = store_fn, store_calls
    imm_in = lambda i, n: i.operands[n].type == xo.X86_OP_IMM
    pool_regs = {"rdi"}                  # registers holding the incoming pool pointer
    memset = None
    block_alloc = None
    none_id = None
    after_none = False
    byte_prop_pending = False
    named_stores: List[Tuple[str, int]] = []
    last_lit: Optional[str] = None
    lea_hash: Dict[str, int] = {}
    hash_base = hash_stride = None
    last_edi = last_esi = last_edx = None
    n = len(ins)
    for k, i in enumerate(ins):
        mn = i.mnemonic
        ops = i.operands
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and \
                _reg(dec, ops[1].reg) in pool_regs:
            pool_regs.add(_reg(dec, ops[0].reg))
        # argument immediates feeding the next call
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and imm_in(i, 1):
            nm = _reg(dec, ops[0].reg)
            if nm == "edi":
                last_edi = ops[1].imm & 0xFFFFFFFF
            elif nm == "esi":
                last_esi = ops[1].imm & 0xFFFFFFFF
            elif nm == "edx":
                last_edx = ops[1].imm & 0xFFFFFFFF
        if mn == "lea" and len(ops) == 2 and ops[1].type == xo.X86_OP_MEM:
            m = ops[1].mem
            if _reg(dec, m.base) in pool_regs and m.index == 0:
                lea_hash[_reg(dec, ops[0].reg)] = m.disp
                if _reg(dec, ops[0].reg) == "rdi" and memset is None:
                    memset = {"lea_site": i.address, "offset": m.disp}
        if mn == "add" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and imm_in(i, 1):
            nm = _reg(dec, ops[0].reg)
            if nm in lea_hash and ops[1].imm in (0x20, 0x40, 0x80, 0x100) and hash_base is None and lea_hash[nm] > 0x100:
                hash_base, hash_stride = lea_hash[nm], ops[1].imm
        if mn == "call" and ops and ops[0].type == xo.X86_OP_IMM:
            tgt = ops[0].imm
            if memset is not None and memset.get("size") is None and last_edx is not None and tgt != store_fn:
                memset["size"] = last_edx
                memset["call"] = tgt
            if block_alloc is None and last_edi is not None and last_edi >= 0x1000 and last_esi is not None \
                    and last_esi <= 16 and tgt != store_fn:
                # the next store of rax into [pool + D] is the Blocks[0] pointer
                for j in ins[k + 1:k + 12]:
                    if j.mnemonic == "mov" and len(j.operands) == 2 and j.operands[0].type == xo.X86_OP_MEM and \
                            j.operands[0].size == 8 and _reg(dec, j.operands[0].mem.base) in pool_regs and \
                            j.operands[1].type == xo.X86_OP_REG and _reg(dec, j.operands[1].reg) == "rax":
                        block_alloc = {"size": last_edi, "align": last_esi, "malloc": tgt,
                                       "store_offset": j.operands[0].mem.disp, "call_site": i.address}
                        break
            last_edi = last_esi = last_edx = None
    # literal -> store -> `mov [pool+N], eax`
    st = ctx.index.strings
    lit_va = {}
    for name in ("None", "ByteProperty"):
        for va in st.find_ascii(name):
            lit_va.setdefault(name, set()).add(va)
    xr = ctx.index.xrefs
    site_lit: Dict[int, str] = {}
    for name, vas in lit_va.items():
        for va in vas:
            for s in xr.refs_to(va, [KIND_LEA]):
                if start <= s < end:
                    site_lit[s] = name
    ids: Dict[str, int] = {}
    for k, i in enumerate(ins):
        if i.address in site_lit:
            name = site_lit[i.address]
            seen_call = False
            for j in ins[k + 1:k + 16]:
                if j.mnemonic == "call":
                    seen_call = True
                    continue
                if seen_call and j.mnemonic == "mov" and len(j.operands) == 2 and \
                        j.operands[0].type == xo.X86_OP_MEM and j.operands[0].size == 4 and \
                        j.operands[1].type == xo.X86_OP_REG and _reg(dec, j.operands[1].reg) == "eax" and \
                        _reg(dec, j.operands[0].mem.base) in pool_regs:
                    ids.setdefault(name, j.operands[0].mem.disp)
                    break
    out.update({"memset": memset, "block_alloc": block_alloc, "ids": ids,
                "hash": ({"base": hash_base, "stride": hash_stride} if hash_base else None)})
    return out


def pool_from_callers(ctx: Context, init: int) -> dict:
    """The pool address, read from how Init's callers set up rdi, plus the init-flag byte."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    dec = ctx.index.decoder
    funcs = ctx.index.funcs
    elf = ctx.elf
    cands: Counter = Counter()
    flags: Counter = Counter()
    where: Dict[int, List[int]] = defaultdict(list)
    for site in ctx.index.calls.callers(init, calls_only=True):
        f = funcs.containing(site)
        if f is None:
            continue
        ins = dec.decode(f, site - f + 8)
        regs: Dict[str, int] = {}
        flag_va = None
        for i in ins:
            if i.address > site:
                break
            ops = i.operands
            if i.mnemonic == "lea" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and \
                    ops[1].type == xo.X86_OP_MEM and ops[1].mem.base == xc.X86_REG_RIP:
                regs[_reg(dec, ops[0].reg)] = i.address + i.size + ops[1].mem.disp
            elif i.mnemonic == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG:
                v = regs.get(_reg(dec, ops[1].reg))
                if v is not None:
                    regs[_reg(dec, ops[0].reg)] = v
                else:
                    regs.pop(_reg(dec, ops[0].reg), None)
            elif i.mnemonic == "cmp" and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[0].size == 1 and \
                    ops[0].mem.base == xc.X86_REG_RIP and ops[1].type == xo.X86_OP_IMM and ops[1].imm == 0:
                flag_va = i.address + i.size + ops[0].mem.disp
            elif ops and ops[0].type == xo.X86_OP_REG:
                regs.pop(_reg(dec, ops[0].reg), None)
        pool = regs.get("rdi")
        if pool is not None and elf.is_mapped(pool):
            cands[pool] += 1
            where[pool].append(site)
            if flag_va is not None:
                flags[flag_va] += 1
    return {"candidates": cands, "sites": where, "flags": flags}


def entry_facts(ctx: Context, pool: int, blocks_off: int) -> dict:
    """How readers of the pool decode a name id: block shift, entry stride, header length shift."""
    from capstone import x86 as xo
    dec = ctx.index.decoder
    funcs = ctx.index.funcs
    xr = ctx.index.xrefs
    shifts: Counter = Counter()
    strides: Counter = Counter()
    len_shifts: Counter = Counter()
    sites = 0
    seen_funcs = set()
    for site in xr.refs_to(pool, [KIND_LEA]):
        f = funcs.containing(site)
        if f is None or f in seen_funcs:
            continue
        seen_funcs.add(f)
        st, en = funcs.span(f)
        ins = dec.decode_function(st, min(en, st + 700))
        last_shr = None
        word_dest = None
        for i in ins:
            ops = i.operands
            if i.mnemonic == "shr" and len(ops) == 2 and ops[1].type == xo.X86_OP_IMM and ops[1].imm in (12, 14, 16, 18):
                last_shr = ops[1].imm
            if i.mnemonic == "mov" and len(ops) == 2 and ops[1].type == xo.X86_OP_MEM and ops[1].mem.disp == blocks_off \
                    and ops[1].mem.scale == 8 and ops[1].mem.index != 0 and last_shr is not None:
                shifts[last_shr] += 1
                sites += 1
            if i.mnemonic == "movzx" and len(ops) == 2 and ops[1].type == xo.X86_OP_MEM and ops[1].size == 2 and \
                    ops[1].mem.index != 0 and ops[1].mem.scale in (2, 1, 4):
                strides[ops[1].mem.scale] += 1
                word_dest = _reg(dec, ops[0].reg)
            if i.mnemonic == "shr" and len(ops) == 2 and ops[1].type == xo.X86_OP_IMM and ops[1].imm in (5, 6, 7) and word_dest \
                    and word_dest[:2] != "xx":
                len_shifts[ops[1].imm] += 1
    return {"block_shift": shifts, "entry_scale": strides, "len_shift": len_shifts, "functions": len(seen_funcs),
            "block_reads": sites}


def growth_function(ctx: Context, init: int, alloc: dict) -> Optional[dict]:
    """The other caller of Malloc(B, align): decode it for CurrentBlock / CurrentByteCursor / Blocks."""
    from capstone import x86 as xo
    dec = ctx.index.decoder
    funcs = ctx.index.funcs
    found = []
    seen = set()
    needle = b"\xbf" + struct.pack("<I", alloc["size"])           # mov edi, <block size>
    for site in ctx.index.calls.callers(alloc["malloc"], calls_only=True):
        near = ctx.elf.read(site - 14, 14)                         # cheap pre-filter before decoding the function
        if near is None or needle not in near:
            continue
        f = funcs.containing(site)
        if f is None or f == init or f in seen:
            continue
        seen.add(f)
        st, en = funcs.span(f)
        body = dec.decode_function(st, en)
        last_edi = None
        calls_alloc = False
        for i in body:
            if i.mnemonic == "mov" and len(i.operands) == 2 and i.operands[0].type == xo.X86_OP_REG and \
                    _reg(dec, i.operands[0].reg) == "edi" and i.operands[1].type == xo.X86_OP_IMM:
                last_edi = i.operands[1].imm & 0xFFFFFFFF
            if i.mnemonic == "call" and i.operands and i.operands[0].type == xo.X86_OP_IMM and \
                    i.operands[0].imm == alloc["malloc"] and last_edi == alloc["size"]:
                calls_alloc = True
        if not calls_alloc:
            continue
        cur_block = cursor = blocks = None
        loads = []
        for k, i in enumerate(body):
            ops = i.operands
            if i.mnemonic == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM and \
                    ops[1].size == 4 and ops[1].mem.index == 0:
                loads.append(ops[1].mem.disp)
            if i.mnemonic == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[0].size == 4 and \
                    ops[0].mem.index == 0 and ops[1].type == xo.X86_OP_REG:
                nxt = body[k + 1] if k + 1 < len(body) else None
                if nxt is not None and nxt.mnemonic == "mov" and len(nxt.operands) == 2 and \
                        nxt.operands[0].type == xo.X86_OP_MEM and nxt.operands[0].size == 4 and \
                        nxt.operands[1].type == xo.X86_OP_IMM and nxt.operands[1].imm == 0 and \
                        nxt.operands[0].mem.disp == ops[0].mem.disp + 4:
                    cur_block, cursor = ops[0].mem.disp, nxt.operands[0].mem.disp
            if i.mnemonic == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[0].size == 8 and \
                    ops[0].mem.scale == 8 and ops[0].mem.index != 0 and ops[1].type == xo.X86_OP_REG and \
                    _reg(dec, ops[1].reg) == "rax":
                blocks = ops[0].mem.disp
        if cur_block is not None:
            found.append({"function": f, "current_block": cur_block, "cursor": cursor, "blocks": blocks,
                          "reads": sorted(set(loads)), "size": en - st})
    if not found:
        return None
    # the dedicated reserve function is the smallest one; the others have it inlined
    best = min(found, key=lambda c: (c["size"], c["function"]))
    best["others"] = [c["function"] for c in found if c is not best]
    return best


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    found = find_init(ctx)
    if found is None:
        ctx.fail("symbols.name_pool", "no function references %d or more distinct default-name literals" % MIN_DISTINCT)
        return
    init = found["func"]
    layout = analyse_init(ctx, init)
    ev_init = ["function 0x%x references %d distinct default-name literals (runner-up function: %d)" % (
        init, found["distinct"], found["runner_up"]),
        "it calls one store function 0x%x %d times (one call per default name)" % (
            layout["store_function"] or 0, layout["store_calls"])]
    # The name cluster alone does not make a function FNamePool::Init: the memset + Malloc(block, align) layout shape has
    # to be in it too.  When the default names are registered across several functions the biggest registrar may be
    # none of them, so without the shape everything derived from the cluster is capped at low.
    shaped = bool(layout["memset"] and layout["block_alloc"] and layout["memset"].get("size"))
    cap = (lambda c: c) if shaped else (lambda c: LOW)
    if not shaped:
        ev_init.append("the function does not show the memset + Malloc(block, align) layout shape of FNamePool::Init, so it "
                       "may only be one of several functions that register the default names")
    ctx.put("symbols.fnamepool_init.label", Hex(init),
            cap(HIGH if found["distinct"] >= 2 * max(found["runner_up"], 1) else MEDIUM),
            "fname_pool.default_name_cluster", ev_init)
    if layout["store_function"]:
        ctx.put("symbols.fname_default_name_store.label", Hex(layout["store_function"]), cap(MEDIUM),
                "fname_pool.default_name_cluster", ["target of %d calls inside Init, one per default name" % layout["store_calls"]])
        ctx.put("structs.FNamePool.default_name_count", Hex(layout["store_calls"]), cap(MEDIUM), "fname_pool.default_name_cluster",
                ["calls to the store function inside Init"])
    # ---- pool address --------------------------------------------------------------------
    pc = pool_from_callers(ctx, init)
    if not pc["candidates"]:
        ctx.fail("symbols.name_pool", "could not read the pool address from the callers of Init at 0x%x" % init)
        return
    ranked = pc["candidates"].most_common()
    pool = ranked[0][0]
    refs = ctx.index.xrefs.count_to(pool)
    ref_funcs = sorted({ctx.index.funcs.containing(s) for s in ctx.index.xrefs.refs_to(pool)})
    span = (ref_funcs[0], ref_funcs[-1]) if ref_funcs else (0, 0)
    ev = ["rdi = lea [rip+0x%x] at %d call site(s) of Init 0x%x: %s" % (
        pool, ranked[0][1], init, ", ".join("0x%x" % s for s in pc["sites"][pool][:6])),
        "%d rip-relative references to the pool from %d functions spanning 0x%x..0x%x (one tight cluster)" % (
            refs, len(ref_funcs), span[0], span[1])]
    if len(ranked) > 1:
        ev.append("other candidates: " + ", ".join("0x%x x%d" % kv for kv in ranked[1:4]))
    ma = layout["memset"]
    ba = layout["block_alloc"]
    if not (ma and ba and ma.get("size")):
        ctx.fail("structs.FNamePool.blocks", "Init does not show the memset + Malloc(block, align) layout shape")
        conf = LOW
        cur_off = blocks_off = None
    else:
        cur_off = ma["offset"]
        blocks_off = ba["store_offset"]
        max_blocks = (ma["size"] - (blocks_off - cur_off)) // 8
        ev.append("Init clears 0x%x bytes at pool+0x%x then stores Blocks[0] at pool+0x%x" % (ma["size"], cur_off, blocks_off))
        conf = HIGH if (blocks_off - cur_off == 8 and max_blocks in (1 << 12, 1 << 13, 1 << 14, 1 << 15)) else MEDIUM
    label_conf = LOW if conf == LOW else (HIGH if (conf == HIGH and refs >= 20) else MEDIUM)
    ctx.put("symbols.name_pool.label", Hex(pool), label_conf, "fname_pool.init_callers", ev)
    ctx.facts["fname_pool"] = pool
    if blocks_off is None:
        return
    grow = growth_function(ctx, init, ba)
    ctx.put("structs.FNamePool.current_block", Hex(cur_off), HIGH if grow and grow["current_block"] == cur_off else MEDIUM,
            "fname_pool.init_memset", ["start of the memset in Init: pool+0x%x" % cur_off] +
            (["block-growth function 0x%x increments the u32 at pool+0x%x" % (grow["function"], grow["current_block"])] if grow else []))
    cursor_off = grow["cursor"] if grow else cur_off + 4
    ctx.put("structs.FNamePool.current_byte_offset", Hex(cursor_off), HIGH if grow else LOW,
            "fname_pool.growth_function" if grow else "fname_pool.schema_order",
            ["block-growth function 0x%x zeroes the u32 at pool+0x%x right after CurrentBlock++" % (grow["function"], grow["cursor"])]
            if grow else ["no growth function found; assumed to follow CurrentBlock (UE source order)"])
    if grow:
        ctx.put("symbols.fnamepool_reserve.label", Hex(grow["function"]), MEDIUM, "fname_pool.growth_function",
                ["the smallest other caller of Malloc(0x%x, %d); reads CurrentBlock/CurrentByteCursor and stores new blocks "
                 "(%d other function(s) have the same growth step inlined)" % (ba["size"], ba["align"], len(grow["others"]))])
    ctx.put("structs.FNamePool.blocks", Hex(blocks_off), HIGH if (not grow or grow["blocks"] in (None, blocks_off)) else MEDIUM,
            "fname_pool.init_block_store",
            ["Init stores the pointer returned by Malloc(0x%x, %d) at pool+0x%x" % (ba["size"], ba["align"], blocks_off)] +
            (["growth function stores new blocks at [pool + CurrentBlock*8 + 0x%x]" % grow["blocks"]] if grow and grow["blocks"] is not None else []))
    ctx.put("structs.FNamePool.block_size", Hex(ba["size"]), HIGH, "fname_pool.init_block_store",
            ["first argument of the block allocation in Init"])
    ctx.put("structs.FNamePool.max_blocks", Hex(max_blocks), MEDIUM, "fname_pool.init_memset",
            ["(memset size 0x%x - (Blocks - CurrentBlock) 0x%x) / 8" % (ma["size"], blocks_off - cur_off)])
    ctx.put("symbols.fmemory_malloc.label", Hex(ba["malloc"]), MEDIUM, "fname_pool.block_allocator",
            ["callee of the Malloc(0x%x, %d) block allocation in FNamePool::Init at 0x%x" % (ba["size"], ba["align"], ba["call_site"])])
    ctx.facts["fmemory_malloc"] = ba["malloc"]
    ids = layout["ids"]
    if "None" in ids:
        ctx.put("structs.FNamePool.none_id", Hex(ids["None"]), HIGH, "fname_pool.init_ids",
                ["Init stores the id returned for the literal \"None\" at pool+0x%x" % ids["None"]])
    if "ByteProperty" in ids:
        ctx.put("structs.FNamePool.byte_property_id", Hex(ids["ByteProperty"]), HIGH, "fname_pool.init_ids",
                ["Init stores the id returned for the literal \"ByteProperty\" at pool+0x%x" % ids["ByteProperty"]])
    h = layout["hash"]
    if h and "None" in ids:
        count = (ids["None"] - h["base"]) // h["stride"]
        ctx.put("structs.FNamePool.hash_buckets", Hex(h["base"]), MEDIUM, "fname_pool.init_hash_loop",
                ["Init loops over lea [pool+0x%x] in steps of 0x%x" % (h["base"], h["stride"])])
        ctx.put("structs.FNamePool.hash_bucket_stride", Hex(h["stride"]), MEDIUM, "fname_pool.init_hash_loop", ["loop step"])
        ctx.put("structs.FNamePool.hash_bucket_count", Hex(count), MEDIUM, "fname_pool.init_hash_loop",
                ["(NoneId offset 0x%x - bucket base 0x%x) / stride" % (ids["None"], h["base"])])
    # ---- init flag ------------------------------------------------------------------------
    if pc["flags"]:
        flag, n = pc["flags"].most_common(1)[0]
        ctx.put("symbols.name_pool_init_flag.label", Hex(flag), MEDIUM, "fname_pool.init_guard",
                ["`cmp byte [rip+0x%x], 0` guards the call to Init at the same site (pool%+#x)" % (flag, flag - pool)])
        ctx.put("structs.FNamePool.init_flag_offset", "%s0x%x" % ("-" if flag < pool else "", abs(flag - pool)), MEDIUM,
                "fname_pool.init_guard", ["guard byte address minus pool address"])
    # ---- readers --------------------------------------------------------------------------
    ef = entry_facts(ctx, pool, blocks_off)
    if ef["block_shift"]:
        shift, n = ef["block_shift"].most_common(1)[0]
        ctx.put("structs.FNamePool.block_offset_bits", Hex(shift), MEDIUM, "fname_pool.reader_decode",
                ["`shr reg, %d` feeds `mov r, [x + idx*8 + 0x%x]` (Blocks[id >> %d]) in %d function(s)" % (shift, blocks_off, shift, n)])
    if ef["entry_scale"]:
        scale, n = ef["entry_scale"].most_common(1)[0]
        ctx.put("structs.FNamePool.entry_stride", Hex(scale), MEDIUM, "fname_pool.reader_decode",
                ["entry header read as `movzx r, word [block + off*%d]` in %d reader instruction(s)" % (scale, n)])
    if ef["len_shift"]:
        ls, n = ef["len_shift"].most_common(1)[0]
        ctx.put("structs.FNameEntry.header", "u16 LE: bit0 wide; length = header >> %d" % ls, LOW, "fname_pool.reader_decode",
                ["`shr` by %d applied to the loaded header word in %d reader(s); bit layout beyond that is the UE5 convention" % (ls, n)])
    ctx.evidence_files["fname_pool"] = {
        "init": init, "literals_in_init": found["literals"], "layout": {
            k: v for k, v in layout.items() if k not in ("ids",)}, "ids": layout["ids"],
        "pool_candidates": {hex(k): v for k, v in pc["candidates"].items()}, "growth": grow, "reader_facts": {
            k: dict(v) if hasattr(v, "items") else v for k, v in ef.items()}}
