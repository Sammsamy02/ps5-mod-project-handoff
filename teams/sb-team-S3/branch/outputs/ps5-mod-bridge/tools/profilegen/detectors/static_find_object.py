"""``StaticFindObject(UClass*, UObject* Outer, const TCHAR* Name, bool ExactClass)``.

UE keeps *fatal* log messages in shipping builds, and the object lookup functions each carry one:
``"Illegal call to StaticFindObject() while serializing object data or garbage collecting!"``
(older engines) or ``"Illegal call to StaticFindObjectFast() while serializing object data!"`` /
``"... while garbage collecting!"`` (UE 5.x).  Every function that references such a message is a
member of the ``StaticFind*`` family.  They are told apart by what they do with their arguments:

* the **string** variant takes ``const TCHAR*`` in ``rdx`` and walks it as UTF-16 (the inline
  ``wcslen`` loop and the name split), the **FName** variants never touch ``rdx`` as memory;
* ``StaticFindObject`` compares its ``Outer`` argument (``rsi``) with ``-1``
  (``ANY_PACKAGE``) while the fast variants do not;
* it carries *both* fatal messages;
* it is the larger one, with its own stack buffer for the split name.

The candidate with the most of these traits is reported; the traits found are the evidence.
There is no signature for this function in UE4SS (it implements the lookup itself over
``FUObjectArray``); the address is informational.
"""
from __future__ import annotations

from collections import defaultdict
from typing import Dict, Set

from ..index import KIND_LEA
from ..model import LOW, MEDIUM, Context, DetectorUnavailable, Hex
from ..x86util import CALLER_SAVED, Regs

ANCHOR_FRAGMENT = "Illegal call to StaticFindObject"


def anchor_functions(ctx: Context) -> Dict[int, Set[int]]:
    out: Dict[int, Set[int]] = defaultdict(set)
    for va, text in ctx.index.strings.find_utf16_containing(ANCHOR_FRAGMENT):
        for site in ctx.index.xrefs.refs_to(va, [KIND_LEA]):
            f = ctx.index.funcs.containing(site)
            if f is not None:
                out[f].add(va)
    return out


def analyse(ctx: Context, f: int) -> dict:
    from capstone import x86 as xo
    dec = ctx.index.decoder
    regs = Regs(dec)
    st, en = ctx.index.funcs.span(f)
    ins = dec.decode_function(st, min(en, st + 0x1400))
    tag: Dict[str, str] = {"rdi": "class", "rsi": "outer", "rdx": "name", "rcx": "exact"}
    name_reads = 0
    outer_cmp = 0
    class_forwarded = 0
    exact_forwarded = 0
    for i in ins:
        ops = i.operands
        mn = i.mnemonic
        for op in ops:
            if op.type == xo.X86_OP_MEM and op.mem.base and tag.get(regs.name(op.mem.base)) == "name" and op.size == 2:
                name_reads += 1
        if mn == "cmp" and len(ops) == 2 and ops[1].type == xo.X86_OP_IMM and ops[1].imm in (-1, 0xFFFFFFFFFFFFFFFF) and \
                ops[0].type == xo.X86_OP_REG and tag.get(regs.name(ops[0].reg)) == "outer":
            outer_cmp += 1
        if mn == "call":
            if tag.get("rdi") == "class":
                class_forwarded += 1
            if tag.get("rcx") == "exact":
                exact_forwarded += 1
            for r in CALLER_SAVED:
                tag.pop(r, None)
            continue
        written = regs.written(i)
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and ops[0].size >= 4:
            src, dst = regs.name(ops[1].reg), regs.name(ops[0].reg)
            written = written - {dst}
            if src in tag:
                tag[dst] = tag[src]
            else:
                tag.pop(dst, None)
        for w in written:
            tag.pop(w, None)
    return {"size": en - st, "name_reads": name_reads, "outer_cmp": outer_cmp, "class_forwarded": class_forwarded,
            "exact_forwarded": exact_forwarded}


def detect(ctx: Context) -> None:
    if ctx.index.decoder is None:
        raise DetectorUnavailable("capstone is not installed")
    anchors = anchor_functions(ctx)
    if not anchors:
        ctx.fail("symbols.static_find_object", "no function references an 'Illegal call to StaticFindObject...' message")
        return
    all_msgs = set()
    for v in anchors.values():
        all_msgs |= v
    scored = []
    for f, msgs in anchors.items():
        a = analyse(ctx, f)
        score = (3 if a["name_reads"] >= 2 else 0) + (2 if a["outer_cmp"] else 0) + \
                (1 if len(msgs) == len(all_msgs) and len(all_msgs) > 1 else 0) + (1 if 0x100 <= a["size"] <= 0x1200 else 0)
        callers = len(ctx.index.calls.callers(f))
        scored.append((score, callers, f, a, msgs))
    scored.sort(key=lambda t: (-t[0], -t[1], t[2]))
    score, callers, f, a, msgs = scored[0]
    runner = scored[1] if len(scored) > 1 else None
    ev = ["%d functions reference 'Illegal call to StaticFindObject...' fatal messages (%d distinct messages)" % (
        len(anchors), len(all_msgs)),
        "0x%x: %d UTF-16 reads through its third argument, rsi compared with -1 %d time(s), "
        "carries %d of %d messages, span 0x%x bytes, %d direct callers" % (
            f, a["name_reads"], a["outer_cmp"], len(msgs), len(all_msgs), a["size"], callers)]
    if runner:
        ev.append("runner-up 0x%x scores %d (name reads %d, ANY_PACKAGE compares %d)" % (
            runner[2], runner[0], runner[3]["name_reads"], runner[3]["outer_cmp"]))
    unique = runner is None or runner[0] < score
    # one structural method (several traits of the same function), no independent second channel:
    # at best "medium" by the confidence rules
    conf = MEDIUM if (score >= 7 and unique) else LOW
    ctx.put("symbols.static_find_object.label", Hex(f), conf, "static_find_object.fatal_message_family", ev)
    abi_parts = ["rdi=UClass*"]
    abi_ev = []
    if a["outer_cmp"]:
        abi_parts.append("rsi=UObject* outer (-1 = any package)")
        abi_ev.append("rsi compared with -1")
    if a["name_reads"] >= 2:
        abi_parts.append("rdx=const char16_t* name")
        abi_ev.append("rdx dereferenced as UTF-16 %d times" % a["name_reads"])
    if a["exact_forwarded"]:
        abi_parts.append("ecx=bExactClass")
        abi_ev.append("ecx forwarded unchanged to a call")
    if len(abi_parts) > 2:
        ctx.put("symbols.static_find_object.abi", "sysv-amd64; " + "; ".join(abi_parts), MEDIUM if conf != LOW else LOW,
                "static_find_object.argument_flow", abi_ev + ["rdi forwarded to a call %d time(s)" % a["class_forwarded"]])
    ctx.facts["static_find_object"] = f
    ctx.evidence_files["static_find_object"] = {
        "candidates": [{"function": hex(f_), "score": s, "callers": c, **aa} for s, c, f_, aa, _m in scored]}
