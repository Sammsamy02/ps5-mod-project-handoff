"""``GUObjectArray`` (the global ``FUObjectArray``) and the layout of ``FUObjectArray`` / ``TUObjectArray``.

Locating the global
-------------------
``UObjectArray.cpp`` is the source file of the ``FUObjectArray`` member functions, and its
``__FILE__`` string plus a few fatal-log message texts survive in shipping builds.  The functions
that reference those strings are therefore members of ``FUObjectArray``; the writable global that
*most of them* take the address of (to pass ``this`` on, or to reach a sibling) is the singleton.
It is then cross-checked by popularity (the singleton has hundreds to thousands of references)
and by the ``IndexToObject`` access idiom below.

Layout
------
Only offsets that code shows are reported as measured.  Two sources of evidence:

* the **IndexToObject idiom** over every reference site of the global (each ``lea r,[rip+G]`` is
  followed by loads through ``r``)::

      mov  rX, [G + Objects]            ; chunk table pointer
      shr  rI, 16                        ; chunk index
      mov  rY, [rX + rI*8]               ; chunk
      lea  rZ, [rI + rI*2] ; shl rZ, 3   ; * sizeof(FUObjectItem) = 24
      cmp  dword [G + NumElements], idx  ; bounds check, 0x14 above Objects

  gives ``ObjObjects`` / ``TUObjectArray.Objects``, ``NumElements``, the chunk shift and the
  item stride;
* the **member functions** found above, which receive ``this = G`` in ``rdi``: every
  ``[this + disp]`` access is classified (u32 compare, byte test, TArray data/Num/Max triples
  indexed by 4 or 8, ...).  The trailing members follow from them: the last byte member is
  ``bShouldRecycleObjectIndices`` (tested before an ``int32`` is pushed on the free list),
  ``PrimarySerialNumber`` is the ``int32`` before it, and the structure ends at the next global.

Fields that are not observed directly but follow from observed neighbours by UE's declaration
order (for example ``PreAllocatedObjects`` between ``Objects`` and ``MaxElements``) are marked
``medium`` with that reasoning.  Nothing is copied from another platform's layout: the sizes of
the mutexes between the members differ per platform, which is exactly why a Windows template is
wrong for PS5.
"""
from __future__ import annotations

from collections import Counter, defaultdict
from typing import Dict, List, Optional, Set, Tuple

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..x86util import Regs, field_accesses

ANCHOR_FILE_FRAGMENT = "UObjectArray.cpp"
ANCHOR_TEXT = [
    "Unable to add more objects to disregard for GC pool",
    "Maximum number of UObjects",
    "but another object",                 # "Attempting to add %s at index %d but another object (0x%016llx) exists"
    "points to a different object",       # "Removing object (...) at index %d but the index points to a different object"
]

# FChunkedFixedUObjectArray member order (UE 4.2x .. 5.x): pointer, pointer, int32 x4.
TUOBJECTARRAY_SCHEMA = [("Objects", 0x0, 8), ("PreAllocatedObjects", 0x8, 8), ("MaxElements", 0x10, 4),
                        ("NumElements", 0x14, 4), ("MaxChunks", 0x18, 4), ("NumChunks", 0x1C, 4)]
TUOBJECTARRAY_SIZE = 0x20


def _anchor_functions(ctx: Context) -> Dict[int, Set[str]]:
    """function start -> which anchor strings it references."""
    idx = ctx.index
    out: Dict[int, Set[str]] = defaultdict(set)
    texts: List[Tuple[str, int]] = []
    for va, _txt in idx.strings.find_ascii_containing(ANCHOR_FILE_FRAGMENT):
        texts.append(("file", va))
    for va, _txt in idx.strings.find_utf16_containing(ANCHOR_FILE_FRAGMENT):
        texts.append(("file16", va))
    for frag in ANCHOR_TEXT:
        for va, _txt in idx.strings.find_utf16_containing(frag):
            texts.append((frag, va))
    for label, va in texts:
        for site in idx.xrefs.refs_to(va, [KIND_LEA]):
            f = idx.funcs.containing(site)
            if f is not None:
                out[f].add(label)
    return out


def _find_global(ctx: Context, anchors: Dict[int, Set[str]]) -> Optional[dict]:
    from capstone import x86 as xo
    from capstone import x86_const as xc
    idx = ctx.index
    dec = idx.decoder
    elf = ctx.elf
    votes: Dict[int, Set[int]] = defaultdict(set)
    for f in anchors:
        st, en = idx.funcs.span(f)
        for i in dec.decode_function(st, min(en, st + 0x1200)):
            if i.mnemonic == "lea" and len(i.operands) == 2 and i.operands[1].type == xo.X86_OP_MEM and \
                    i.operands[1].mem.base == xc.X86_REG_RIP:
                a = i.address + i.size + i.operands[1].mem.disp
                if elf.is_writable(a):
                    votes[a].add(f)
    if not votes:
        return None
    ranked = sorted(votes, key=lambda a: (-len(votes[a]), a))
    top = ranked[0]
    return {"address": top, "functions": sorted(votes[top]), "runner_up": len(votes[ranked[1]]) if len(ranked) > 1 else 0,
            "ranked": [(a, len(votes[a])) for a in ranked[:4]]}


def _idiom_scan(ctx: Context, g: int, window: int = 160) -> dict:
    """Decode a window after every ``lea r,[rip+g]`` and collect IndexToObject facts."""
    from capstone import x86 as xo
    idx = ctx.index
    dec = idx.decoder
    regs = Regs(dec)
    objects: Counter = Counter()          # disp of chunk-table loads
    nums: Counter = Counter()             # disp of 4-byte compares
    shifts: Counter = Counter()
    strides: Counter = Counter()
    access_hist: Dict[Tuple[int, int, str], int] = Counter()
    pair: Counter = Counter()             # (Objects disp, NumElements disp - Objects disp)
    sites = idx.xrefs.refs_to(g, [KIND_LEA])
    for s in sites:
        ins = dec.decode(s, window)
        if not ins:
            continue
        first = ins[0]
        if first.mnemonic != "lea" or first.operands[0].type != xo.X86_OP_REG:
            continue
        alias = {regs.name(first.operands[0].reg)}
        acc = field_accesses(dec, regs, ins[1:], alias)
        for a in acc:
            if 0 <= a.disp < 0x200 and not a.indexed:
                access_hist[(a.disp, a.size, a.mnemonic)] += 1
        # IndexToObject idiom, anchored on an 8-byte chunk-table load
        loads = [a for a in acc if a.size == 8 and a.reads and not a.indexed and a.mnemonic == "mov"]
        ins_by_addr = {i.address: k for k, i in enumerate(ins)}
        for a in loads:
            k = ins_by_addr.get(a.addr)
            if k is None:
                continue
            tail = ins[k + 1:k + 12]
            has_shift = None
            has_chunk = False
            dest = ins[k].operands[0].reg if ins[k].operands and ins[k].operands[0].type == xo.X86_OP_REG else None
            for t in tail:
                if t.mnemonic in ("shr", "sar") and len(t.operands) == 2 and t.operands[1].type == xo.X86_OP_IMM:
                    has_shift = t.operands[1].imm
                if t.mnemonic == "mov" and len(t.operands) == 2 and t.operands[1].type == xo.X86_OP_MEM and \
                        t.operands[1].mem.base == dest and t.operands[1].mem.index != 0 and t.operands[1].mem.scale == 8:
                    has_chunk = True
            if has_shift is not None and has_chunk:
                objects[a.disp] += 1
                shifts[has_shift] += 1
                for t in tail:
                    if t.mnemonic == "lea" and len(t.operands) == 2 and t.operands[1].type == xo.X86_OP_MEM and \
                            t.operands[1].mem.index == t.operands[1].mem.base and t.operands[1].mem.scale == 2:
                        strides[3 * 8] += 1          # lea r,[r+r*2] ... followed by shl 3 (checked below)
                        break
                for t in tail:
                    if t.mnemonic == "imul" and len(t.operands) == 3 and t.operands[2].type == xo.X86_OP_IMM:
                        strides[t.operands[2].imm] += 1
                # NumElements: a 4-byte compare through the same alias just before the load
                for c in acc:
                    if c.size == 4 and c.mnemonic == "cmp" and not c.indexed and 0 < c.disp - a.disp <= 0x40 and \
                            ins_by_addr.get(c.addr, 1 << 30) < k:
                        pair[(a.disp, c.disp - a.disp)] += 1
                        nums[c.disp] += 1
    return {"sites": len(sites), "objects": objects, "nums": nums, "shifts": shifts, "strides": strides,
            "pairs": pair, "hist": access_hist}


def _member_profile(ctx: Context, funcs: List[int]) -> dict:
    """Classify the ``[this + disp]`` accesses in the member functions (this = rdi at entry)."""
    dec = ctx.index.decoder
    regs = Regs(dec)
    per_disp: Dict[int, Counter] = defaultdict(Counter)
    byte_tests: Counter = Counter()
    atomics: Counter = Counter()
    for f in funcs:
        st, en = ctx.index.funcs.span(f)
        ins = dec.decode_function(st, min(en, st + 0x1200))
        acc = field_accesses(dec, regs, ins, {"rdi"}, stop_at_ret=False)
        for a in acc:
            if a.disp < 0 or a.disp > 0x400:
                continue
            per_disp[a.disp][(a.size, a.mnemonic, "w" if a.writes else "r")] += 1
            if a.size == 1 and a.mnemonic in ("cmp", "test", "movzx", "mov"):
                byte_tests[a.disp] += 1
            if a.lock:
                atomics[a.disp] += 1
    return {"per_disp": per_disp, "byte_tests": byte_tests, "atomics": atomics}


def _tarray_triples(ctx: Context, funcs: List[int]) -> Dict[int, int]:
    """disp -> element scale for TArray-shaped members: a pointer load at d plus an Num read at d+8."""
    from capstone import x86 as xo
    dec = ctx.index.decoder
    regs = Regs(dec)
    out: Dict[int, int] = {}
    for f in funcs:
        st, en = ctx.index.funcs.span(f)
        ins = dec.decode_function(st, min(en, st + 0x1200))
        acc = field_accesses(dec, regs, ins, {"rdi"}, stop_at_ret=False)
        by_disp = {a.disp for a in acc}
        ptr_loads = [a for a in acc if a.size == 8 and a.reads and a.mnemonic == "mov"]
        for a in ptr_loads:
            if (a.disp + 8) in by_disp:
                # who indexes the loaded pointer?  find the destination register's later [reg+idx*scale] use
                k = next((n for n, i in enumerate(ins) if i.address == a.addr), None)
                if k is None:
                    continue
                dest = ins[k].operands[0].reg if ins[k].operands[0].type == xo.X86_OP_REG else None
                for t in ins[k + 1:k + 24]:
                    for op in t.operands:
                        if op.type == xo.X86_OP_MEM and op.mem.base == dest and op.mem.index != 0 and op.mem.scale in (4, 8):
                            out.setdefault(a.disp, op.mem.scale)
        # element push/pop on a TArray<int32>: `mov [reg + idx*4], r32` after loading the data pointer
    return out


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    anchors = _anchor_functions(ctx)
    if not anchors:
        ctx.fail("symbols.object_array", "no function references the UObjectArray.cpp source string or its fatal messages")
        return
    found = _find_global(ctx, anchors)
    if found is None:
        ctx.fail("symbols.object_array", "no writable global is referenced by the UObjectArray.cpp functions")
        return
    g = found["address"]
    refs = ctx.index.xrefs.count_to(g)
    n_f = len(found["functions"])
    kinds_seen = sorted({lbl for f in found["functions"] for lbl in anchors[f]})
    ev = ["%d of the %d functions that reference UObjectArray.cpp strings take the address of 0x%x (runner-up: %d)" % (
        n_f, len(anchors), g, found["runner_up"]),
        "anchor strings seen: %s" % ", ".join(kinds_seen),
        "%d rip-relative references to it image-wide" % refs]
    idiom = _idiom_scan(ctx, g)
    idiom_sites = sum(idiom["objects"].values())
    if idiom_sites:
        ev.append("independent behaviour check: %d of its %d reference sites show the IndexToObject idiom "
                  "(chunk table load, index >> %d, [table + chunk*8])" % (
                      idiom_sites, idiom["sites"], idiom["shifts"].most_common(1)[0][0]))
    cluster_ok = n_f >= 3 and n_f > found["runner_up"] and refs >= 100
    conf = HIGH if (cluster_ok and idiom_sites >= 5) else (MEDIUM if (n_f >= 2 or idiom_sites >= 5) else LOW)
    ctx.put("symbols.object_array.label", Hex(g), conf, "uobject_array.member_function_cluster", ev)
    ctx.facts["guobjectarray"] = g
    # ---- layout --------------------------------------------------------------------------
    if not idiom["objects"]:
        derive_layout(ctx, g, idiom, {"per_disp": {}, "byte_tests": Counter(), "atomics": Counter()}, {})
        return
    t_off = idiom["objects"].most_common(1)[0][0]
    members = sorted(anchors)           # every function of UObjectArray.cpp that has a source-path/fatal string
    mp = _member_profile(ctx, members)
    triples = _tarray_triples(ctx, members)
    layout = derive_layout(ctx, g, idiom, mp, triples)
    ctx.evidence_files["uobject_array"] = {
        "anchor_functions": {hex(f): sorted(v) for f, v in anchors.items()}, "member_functions": [hex(f) for f in members],
        "idiom": {k: ({hex(a) if isinstance(a, int) else str(a): b for a, b in v.items()} if hasattr(v, "items") else v)
                  for k, v in idiom.items() if k != "hist"},
        "tarrays": {hex(k): v for k, v in triples.items()},
        "layout": {k: (hex(v) if isinstance(v, int) else v) for k, v in layout.items() if not isinstance(v, (dict, list))},
    }


def derive_layout(ctx: Context, g: int, idiom: dict, mp: dict, triples: Dict[int, int]) -> dict:
    out: dict = {}
    objects = idiom["objects"]
    if not objects:
        ctx.fail("structs.TUObjectArray.Objects", "no IndexToObject idiom (chunk table load + shift + [x+i*8]) around the global")
        return out
    t_off, t_count = objects.most_common(1)[0]
    shift, shift_n = idiom["shifts"].most_common(1)[0]
    pair_best = idiom["pairs"].most_common(1)[0] if idiom["pairs"] else None
    ev_obj = ["%d of %d reference sites load the chunk table at G+0x%x, shift the index right by %d and index [table + i*8]" % (
        t_count, idiom["sites"], t_off, shift)]
    ctx.put("structs.FUObjectArray.ObjObjects", Hex(t_off), HIGH, "uobject_array.index_to_object_idiom", ev_obj)
    ctx.put("structs.TUObjectArray.Objects", Hex(0), HIGH, "uobject_array.index_to_object_idiom",
            ev_obj + ["TUObjectArray starts at ObjObjects, so Objects is its first member"])
    ctx.put("structs.TUObjectArray.elements_per_chunk", Hex(1 << shift), HIGH, "uobject_array.index_to_object_idiom",
            ["chunk index = id >> %d in %d sites" % (shift, shift_n)])
    out["objects"] = t_off
    strides = idiom["strides"]
    if strides:
        st, n = strides.most_common(1)[0]
        ctx.put("structs.FUObjectItem.stride", Hex(st), MEDIUM, "uobject_array.index_to_object_idiom",
                ["item address = chunk + (id & 0xffff) * %d (lea r,[r+r*2] + shl 3 / imul) in %d sites" % (st, n)])
    # NumElements: the compare offset that sits 0x14 above Objects -- UE's declared member order
    num_rel = None
    if pair_best:
        (obj_d, delta), n_pair = pair_best
        num_rel = delta
    nums_abs = (t_off + num_rel) if num_rel is not None else None
    ev_num = []
    if nums_abs is not None:
        ev_num.append("%d sites compare a u32 at G+0x%x against the index before loading the chunk table at G+0x%x "
                      "(distance 0x%x)" % (pair_best[1], nums_abs, t_off, num_rel))
    sch = {name: (off, size) for name, off, size in TUOBJECTARRAY_SCHEMA}
    num_decl = sch["NumElements"][0]
    # The member names below come from the FChunkedFixedUObjectArray declaration order.  That order is only trusted when the
    # bounds-check idiom measured NumElements at its declared distance.  Not measured: the schema stays, capped at low.
    # Measured somewhere else: the shape is a different one, so the schema-derived members are not reported at all.
    shape_ok = num_rel is not None and num_rel == num_decl
    shape_differs = num_rel is not None and num_rel != num_decl
    cap = (lambda c: c) if shape_ok else (lambda c: LOW)
    out["shape_ok"], out["shape_differs"] = shape_ok, shape_differs
    # which schema members are seen directly in the member functions?
    per_disp = mp["per_disp"]
    seen = lambda d, size=None: d in per_disp and (size is None or any(k[0] == size for k in per_disp[d]))
    why_differs = ("TUObjectArray is not laid out like FChunkedFixedUObjectArray: the bounds check compares a u32 at "
                   "ObjObjects+0x%x, not +0x%x" % (num_rel or 0, num_decl))
    for name, off, size in TUOBJECTARRAY_SCHEMA:
        absd = t_off + off
        if name == "Objects":
            continue
        if shape_differs:
            if name == "NumElements":
                ctx.put("structs.TUObjectArray.NumElements", Hex(num_rel), MEDIUM, "uobject_array.index_to_object_idiom",
                        ev_num + ["distance differs from the declared member order (+0x%x); the other members are not derived" % num_decl])
            else:
                ctx.fail("structs.TUObjectArray." + name, why_differs)
            continue
        if name == "NumElements":
            if shape_ok and nums_abs is not None:
                ctx.put("structs.TUObjectArray.NumElements", Hex(off), HIGH, "uobject_array.index_to_object_idiom",
                        ev_num + ["distance matches the declared member order (Objects, PreAllocatedObjects, MaxElements, NumElements)"])
            else:
                ctx.put("structs.TUObjectArray.NumElements", Hex(off), LOW, "uobject_array.schema",
                        ["declared order puts NumElements at +0x%x; the bounds-check idiom did not confirm it" % off])
            continue
        if seen(absd, size):
            ctx.put("structs.TUObjectArray." + name, Hex(off), cap(HIGH), "uobject_array.member_access",
                    ["member functions access %d-byte field at G+0x%x (= ObjObjects 0x%x + 0x%x): %s" % (
                        size, absd, t_off, off, ", ".join("%s%s x%d" % (m, "" if rw == "r" else "(w)", c)
                                                         for (sz, m, rw), c in per_disp[absd].most_common(3)))] +
                    ([] if shape_ok else ["member order not confirmed by a bounds-check idiom: the name follows the declaration only"]))
        else:
            ctx.put("structs.TUObjectArray." + name, Hex(off), cap(MEDIUM), "uobject_array.schema",
                    ["not touched by the member functions; follows from the observed Objects (+0x0) and NumElements (+0x14) "
                     "by UE's declared member order"])
    # end of TUObjectArray = first member after it that code touches
    after = sorted(d for d in per_disp if d >= t_off + TUOBJECTARRAY_SIZE)
    if shape_differs:
        ctx.fail("structs.TUObjectArray.UEP_TotalSize", why_differs)
    else:
        ctx.put("structs.TUObjectArray.UEP_TotalSize", Hex(TUOBJECTARRAY_SIZE), cap(MEDIUM), "uobject_array.schema",
                ["six members (2 pointers + 4 int32) = 0x20" + (
                    "; the next member the code touches is at G+0x%x" % after[0] if after else "")])
    # ---- FUObjectArray prefix --------------------------------------------------------------
    prefix = [("ObjFirstGCIndex", 0x0, 4), ("ObjLastNonGCIndex", 0x4, 4), ("MaxObjectsNotConsideredByGC", 0x8, 4),
              ("OpenForDisregardForGC", 0xC, 1)]
    for name, off, size in prefix:
        if seen(off, size):
            ctx.put("structs.FUObjectArray." + name, Hex(off), HIGH, "uobject_array.member_access",
                    ["member functions access a %d-byte field at G+0x%x: %s" % (size, off, ", ".join(
                        "%s x%d" % (m, c) for (sz, m, rw), c in per_disp[off].most_common(3) if sz == size))])
        else:
            ctx.put("structs.FUObjectArray." + name, Hex(off), LOW, "uobject_array.schema",
                    ["not accessed by the member functions; follows UE's declared order (int32, int32, int32, bool)"])
    # ---- tail -----------------------------------------------------------------------------
    if shape_differs:
        # the tail starts where TUObjectArray ends, which is unknown for a different shape
        for name in ("UObjectCreateListeners", "UObjectDeleteListeners", "PrimarySerialNumber", "bShouldRecycleObjectIndices",
                     "UEP_TotalSize"):
            ctx.fail("structs.FUObjectArray." + name, why_differs)
        return out
    byte_tests = mp["byte_tests"]
    cand_bytes = [d for d in byte_tests if d >= t_off + TUOBJECTARRAY_SIZE]
    arrays = sorted(d for d, scale in triples.items() if d >= t_off + TUOBJECTARRAY_SIZE)
    ptr_arrays = sorted(d for d, scale in triples.items() if scale == 8 and d >= t_off + TUOBJECTARRAY_SIZE)
    if len(ptr_arrays) >= 2:
        create, delete = ptr_arrays[0], ptr_arrays[1]
        why = ["TArray-shaped members (pointer at +d, count at +d+8, elements indexed by 8) at G+%s; the lower is the create "
               "list, the higher the delete list (UE declares UObjectCreateListeners first)" % ", ".join("0x%x" % d for d in ptr_arrays)]
        ctx.put("structs.FUObjectArray.UObjectCreateListeners", Hex(create), cap(MEDIUM), "uobject_array.tarray_members", why)
        ctx.put("structs.FUObjectArray.UObjectDeleteListeners", Hex(delete), cap(MEDIUM), "uobject_array.tarray_members", why)
        out["create"], out["delete"] = create, delete
    else:
        ctx.fail("structs.FUObjectArray.UObjectCreateListeners", "fewer than two pointer-TArray members found in the member functions")
    if cand_bytes:
        bflag = max(cand_bytes)
        serial = bflag - 4
        ev_b = ["the last byte-sized member the member functions touch is at G+0x%x (`cmp byte [this+0x%x], ...`, x%d)" % (
            bflag, bflag, byte_tests[bflag])]
        nxt = [t for t, _s, _k, _r in ctx.index.xrefs.iter_range(g + bflag + 1, g + 0x400)]
        next_global = min(nxt) if nxt else None
        size = (bflag + 1 + 7) & ~7
        ctx.put("structs.FUObjectArray.bShouldRecycleObjectIndices", Hex(bflag), cap(MEDIUM), "uobject_array.member_access", ev_b)
        ctx.put("structs.FUObjectArray.PrimarySerialNumber", Hex(serial), cap(MEDIUM), "uobject_array.schema_order",
                ["an int32 counter directly precedes bShouldRecycleObjectIndices in UE's declaration (no member between them)"])
        ev_s = ["rounded up to 8 from the end of the last member (G+0x%x + 1)" % bflag]
        conf_s = MEDIUM
        if next_global is not None:
            ev_s.append("the next referenced global starts at G+0x%x" % (next_global - g))
            if next_global - g == size:
                conf_s = HIGH
        ctx.put("structs.FUObjectArray.UEP_TotalSize", Hex(size), cap(conf_s), "uobject_array.end_of_struct", ev_s)
        out.update({"recycle": bflag, "serial": serial, "size": size})
    else:
        ctx.fail("structs.FUObjectArray.bShouldRecycleObjectIndices", "no byte-sized member access beyond TUObjectArray in the member functions")
    return out
