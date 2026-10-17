"""``FName::ToString`` -- the two overloads that turn an ``FName`` into an ``FString``.

Both resolve the name through the global ``FNamePool`` (the pool address and the ``Blocks`` offset come from the
``fname_pool`` detector)::

    entry  = Blocks[id >> 16] + (id & 0xFFFF) * 2;  header = *(uint16*)entry;  length = header >> 6;  wide = header & 1
    // NAME_NO_NUMBER: copy the characters; otherwise also append '_' + Number - 1

and differ in the calling convention the compiler gives them (SysV / Itanium):

* ``void FName::ToString(FString& Out) const``  -- ``rdi`` = ``const FName*``, ``rsi`` = ``FString*`` (the form UE4SS binds);
* ``FString FName::ToString() const``           -- the hidden return slot comes *first*: ``rdi`` = ``FString*`` (returned in ``rax``),
  ``rsi`` = ``const FName*``.  When ``Number`` is non-zero it forwards to the first form with the two arguments swapped.

Among the (few dozen) functions that take the address of the pool, the by-value form is the one that

1. decodes a pool entry (``shr 16`` + scaled load at ``Blocks`` + header ``shr 6``),
2. reads ``[arg1 + 4]`` (``Number``), returns its own ``rdi`` argument, and
3. calls another pool reader with ``rdi`` / ``rsi`` swapped -- that callee is the out-parameter form,

and it has by far the most callers.  The fatal-message path of ``FUObjectArray::AllocateUObjectIndex``
(``Object->GetFName().ToString()``) calls it, which is an independent confirmation.
"""
from __future__ import annotations

from typing import List

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ._flow import Flow
from . import uobject_array


def pool_readers(ctx: Context, pool: int, blocks: int) -> List[int]:
    """Functions that take the pool's address and index its ``Blocks`` array: ``mov r, [base + index*8 + Blocks]``."""
    from capstone import x86 as xo
    idx = ctx.index
    dec = idx.decoder
    fs = set()
    for site in idx.xrefs.refs_to(pool, [KIND_LEA]):
        f = idx.funcs.containing(site)
        if f is not None:
            fs.add(f)
    out = []
    for f in sorted(fs):
        st, en = idx.funcs.span(f)
        for i in dec.decode_function(st, min(en, st + 0x600)):
            if i.mnemonic == "mov" and len(i.operands) == 2 and i.operands[1].type == xo.X86_OP_MEM:
                m = i.operands[1].mem
                if m.scale == 8 and m.index and m.disp == blocks and i.operands[1].size == 8:
                    out.append(f)
                    break
    return out


def shape(ctx: Context, f: int) -> dict:
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(f)
    flow = Flow(dec, dec.decode_function(st, min(en, st + 0x600)))
    ev = flow.events
    def reads(arg: int, disp: int) -> bool:
        return any(e.base == ("arg", arg) and e.disp == disp and e.kind in ("load", "cmp", "test") for e in ev)
    returns_sret = any(e.kind == "ret" and e.value == ("arg", 0) for e in ev)
    swapped = [c for c in flow.calls() if isinstance(c.target, int) and c.args.get("rdi") == ("arg", 1) and c.args.get("rsi") == ("arg", 0)]
    return {"function": f, "span": en - st, "by_value": reads(1, 0) and reads(1, 4) and returns_sret,
            "out_param": reads(0, 0) and reads(0, 4) and not returns_sret, "swapped_calls": [c.target for c in swapped]}


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    pool = ctx.facts.get("fname_pool")
    blocks = ctx.val("structs.FNamePool.blocks")
    if pool is None or blocks is None:
        ctx.fail("symbols.fname_to_string", "the FNamePool address / Blocks offset is not known (fname_pool did not resolve)")
        return
    idx = ctx.index
    readers = pool_readers(ctx, int(pool), int(blocks))
    shapes = [shape(ctx, f) for f in readers]
    by_value = sorted((s for s in shapes if s["by_value"]), key=lambda s: -len(idx.calls.callers(s["function"])))
    out_param = {s["function"]: s for s in shapes if s["out_param"]}
    if not by_value:
        ctx.fail("symbols.fname_to_string", "none of the %d functions that decode FNamePool entries has the FString-returning shape" % len(readers))
        return
    best = by_value[0]
    n_best = len(idx.calls.callers(best["function"]))
    n_next = len(idx.calls.callers(by_value[1]["function"])) if len(by_value) > 1 else 0
    pair = [t for t in best["swapped_calls"] if t in out_param]
    anchors = uobject_array._anchor_functions(ctx)
    from_cluster = [s for s in idx.calls.callers(best["function"]) if idx.funcs.containing(s) in anchors]
    ev_a = ["decodes FNamePool entries, reads Number at [this+4], returns its own rdi argument (hidden return slot)",
            "%d direct callers (next most-called candidate: %d)" % (n_best, n_next)]
    if from_cluster:
        ev_a.append("called from the FUObjectArray member 0x%x (the fatal-message path prints Object->GetFName().ToString())" % idx.funcs.containing(from_cluster[0]))
    conf_a = HIGH if (pair and from_cluster and n_best >= 20 * max(1, n_next)) else (MEDIUM if n_best > 2 * max(1, n_next) else LOW)
    ctx.put("symbols.fname_to_string_value.label", Hex(best["function"]), conf_a, "fname_tostring.pool_reader_by_value", ev_a)
    ctx.put("symbols.fname_to_string_value.abi", "sysv-amd64; rdi = FString* (hidden return slot, returned in rax), rsi = const FName*", conf_a,
            "fname_tostring.argument_flow", ["rsi supplies [+0] (ComparisonIndex) and [+4] (Number); rdi is returned unchanged"])
    if len(pair) == 1:
        o = pair[0]
        ev_b = ["called by 0x%x with rdi and rsi swapped (the Number != 0 path of the by-value form)" % best["function"],
                "decodes FNamePool entries and reads [this] / [this+4] of its first argument; writes through its second argument (the FString)"]
        conf_b = conf_a
        ctx.put("symbols.fname_to_string.label", Hex(o), conf_b, "fname_tostring.pool_reader_out_param", ev_b)
        ctx.put("symbols.fname_to_string.abi", "sysv-amd64; rdi = const FName*, rsi = FString* (output)", conf_b, "fname_tostring.argument_flow",
                ["rdi supplies [+0] and [+4]; rsi is the FString that receives the characters"])
    else:
        ctx.fail("symbols.fname_to_string", "the by-value form 0x%x does not forward to exactly one out-parameter reader (found %d)" % (
            best["function"], len(pair)))
    ctx.evidence_files["fname_tostring"] = {"readers": [hex(f) for f in readers], "by_value": [hex(s["function"]) for s in by_value],
                                             "out_param": [hex(f) for f in out_param], "callers": {hex(s["function"]): len(idx.calls.callers(s["function"])) for s in by_value}}
