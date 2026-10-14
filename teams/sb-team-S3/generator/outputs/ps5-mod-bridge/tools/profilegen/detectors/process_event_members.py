"""Member offsets read by ``UObject::ProcessEvent``: ``UObject``, ``UFunction``, ``UStruct``, ``FField``, ``FProperty``.

``ProcessEvent`` is the one function that touches every layer of the reflection object model in a
fixed, source-defined way.  With ``this`` in ``rdi`` and the ``UFunction*`` in ``rsi`` it does (UE 5.x)::

    if (HasAnyFlags(<RF mask>)) return;                                   // UObject::ObjectFlags
    if (!(Function->FunctionFlags & FUNC_Native) && Function->Script.Num() == 0) return;   // Script
    ... GetFunctionCallspace / CallRemoteFunction (virtual) ...
    if (Function->EventGraphFunction) { EventGraphParams = Function->EventGraphCallOffset; ... }
    uint8* Frame = alloca(Function->PropertiesSize) (aligned by Function->MinAlignment)
    memset(Frame + Function->ParmsSize, 0, PropertiesSize - ParmsSize); memcpy(Frame, Parms, Function->ParmsSize);
    FFrame NewStack(this, Function, Frame, nullptr, Function->ChildProperties);   // FFrame::Code = Function->Script
    for (P = Function->ChildProperties; P; P = P->Next) if (P->PropertyFlags & CPF_OutParm) ...   // FField::Next
    Function->Invoke(this, NewStack, ReturnValueOffset == MAX_uint16 ? nullptr : Frame + ReturnValueOffset);
    for (P = Function->FirstPropertyToInit; P; P = P->PostConstructLinkNext) P->InitializeValue(...)
    for (P = Function->DestructorLink; P; P = P->DestructorLinkNext) if (!P->IsInContainer(ParmsSize)) P->DestroyValue(...)

so a linear data-flow walk (``_flow.py``) of the function, with ``rdi`` / ``rsi`` tagged as ``this`` /
``Function``, yields every offset by the *role* of the access:

* ``Script``: the dword compared with zero right after the FUNC_Native test (``Script.Num``), minus 8
  (a ``TArray`` is data, Num, Max); the qword at that data offset is stored into the frame;
* ``ParmsSize``: the word that is the size argument of the ``memcpy`` of the parameters; ``PropertiesSize``:
  the dword that, together with it, forms the size argument of the ``memset`` of the rest; ``MinAlignment``: the
  word compared with 16 right after;
* ``ReturnValueOffset``: the word compared with 0xFFFF (``MAX_uint16`` = no return value);
* ``EventGraphFunction``: the qword that is tested for null and then used as a ``UFunction`` (its flags are
  read at the known ``FunctionFlags`` offset), ``EventGraphCallOffset`` the dword loaded next;
* the three property chains are told apart by what is done with them: the head that is also stored into the
  frame is ``ChildProperties``; of the other two, the one whose loop reads ``ParmsSize`` (``IsInContainer``) is
  ``DestructorLink``, the other ``FirstPropertyToInit``.  The link that each loop follows gives ``FField::Next``,
  ``DestructorLinkNext`` and ``PostConstructLinkNext``;
* ``Offset_Internal`` is the dword every loop reads; ``ArrayDim`` / ``ElementSize`` the two other dwords the
  loops multiply (UE declares ``ArrayDim`` first); ``PropertyFlags`` the qword the loops test;
* ``Func``: the callee that receives the ``UFunction`` as its first argument (``UFunction::Invoke``) makes one
  indirect call through that argument.

Each of these is one channel (``process_event.members``); the other detectors (object base, class registration,
default object) supply the second one wherever the same member appears in their code.
"""
from __future__ import annotations

from collections import Counter, defaultdict
from typing import Dict, List, Optional, Set, Tuple

from ..model import MEDIUM, Context, DetectorUnavailable
from ._book import book
from ._flow import Flow

THIS = ("arg", 0)
FUNC = ("arg", 1)
CHANNEL = "process_event.members"
METHOD = "process_event.member_flow"


def peel(tag) -> Optional[Tuple[int, int, List[int]]]:
    """For a property-chain element tag: (head displacement in the UFunction, depth, link displacements)."""
    links: List[int] = []
    t = tag
    while t and t[0] == "ld" and t[1] and t[1][0] == "ld":
        links.append(t[2])
        t = t[1]
    if t and t[0] == "ld" and t[1] == FUNC and t[3] == 8:
        return t[2], len(links), list(reversed(links))
    return None


def _canon(tag, eg_off: int):
    """``tag`` with every ``(*Function)->EventGraphFunction`` replaced by the function itself."""
    if tag is None or not isinstance(tag, tuple):
        return tag
    if tag[0] == "ld":
        inner = _canon(tag[1], eg_off)
        if inner == FUNC and tag[2] == eg_off and tag[3] == 8:
            return FUNC
        return ("ld", inner, tag[2], tag[3])
    if tag[0] == "ptr":
        return ("ptr", _canon(tag[1], eg_off), tag[2])
    if tag[0] == "op":
        return ("op", frozenset(_canon(t, eg_off) for t in tag[1]))
    if tag[0] in ("ret", "out"):
        return tag[:2] + (_canon(tag[2], eg_off),) + tag[3:]
    if tag[0] == "vcall":
        return ("vcall", _canon(tag[1], eg_off), tag[2])
    return tag


def _canon_event(e, eg_off: int):
    from ._flow import Event
    args = {k: _canon(v, eg_off) for k, v in e.args.items()}
    return Event(e.kind, e.addr, _canon(e.base, eg_off), e.disp, e.size, _canon(e.value, eg_off), _canon(e.target, eg_off), args, e.mnemonic)


def analyse(ctx: Context, fn: int) -> dict:
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(fn)
    ins = dec.decode_function(st, min(en, st + 0x1800))
    flow = Flow(dec, ins)
    ev = flow.events
    res: Dict[str, object] = {"instructions": len(ins), "found": {}}
    found: Dict[str, Tuple[int, str]] = res["found"]  # member -> (offset, why)

    def put(member: str, off: int, why: str) -> None:
        found.setdefault(member, (off, why))

    # -- UFunction flags (the FUNC_Native test); the offset itself is the process_event detector's
    ff = ctx.val("structs.UFunction.FunctionFlags")
    ff = int(ff) if ff is not None else None
    res["function_flags"] = ff
    # -- EventGraphFunction: the qword whose target is itself used as a UFunction (its flags are tested)
    if ff is not None:
        for e in ev:
            if e.kind == "test" and e.base and e.base[0] == "ld" and e.base[1] == FUNC and e.base[3] == 8 and ff <= e.disp < ff + 4:
                put("UFunction.EventGraphFunction", e.base[2],
                    "qword whose target is itself used as a UFunction (its flags are tested at 0x%x)" % e.addr)
                break
    eg = found.get("UFunction.EventGraphFunction")
    if eg is not None:
        # From here on the redirected function and the function itself are the same kind of object: after the redirection the
        # compiler may keep either pointer in the register that the rest of the function uses.  Treat them as one.
        ev = [_canon_event(e, eg[0]) for e in ev]
        flow_calls = [e for e in ev if e.kind == "call"]
    else:
        flow_calls = flow.calls()

    # -- UObject::ObjectFlags: the first value loaded through this and tested against a mask
    for e in ev:
        if e.kind == "cmpreg" and e.base and e.base[0] == "ld" and e.base[1] == THIS and e.base[3] == 4 and e.base[2] != 0 \
                and e.mnemonic == "test":
            put("UObject.ObjectFlags", e.base[2], "dword loaded through this and tested against 0x%x at 0x%x" % (e.disp, e.addr))
            break
    # -- Script: first dword compared with zero on the function, after the flags test
    script_num = None
    for e in ev:
        if e.kind == "cmp" and e.base == FUNC and e.size == 4 and e.value == ("imm", 0):
            script_num = e.disp
            break
    if script_num is not None and script_num >= 8:
        data = [e for e in ev if e.kind == "load" and e.base == FUNC and e.disp == script_num - 8 and e.size == 8]
        stored = any(s.kind == "store" and s.value == ("ld", FUNC, script_num - 8, 8) for s in ev)
        if data and stored:
            put("UStruct.Script", script_num - 8,
                "dword compared with 0 at +0x%x (Script.Num); the qword at +0x%x is stored into the frame (FFrame::Code)" % (
                    script_num, script_num - 8))
    # -- ParmsSize / PropertiesSize / MinAlignment from the memcpy / memset sizes
    parms = None
    for c in flow_calls:
        t = c.args.get("rdx")
        if t and t[0] == "ld" and t[1] == FUNC and t[3] == 2:
            parms = t[2]
            put("UFunction.ParmsSize", parms, "word that is the byte count of the call at 0x%x copying the parameters into the frame" % c.addr)
            break
    if parms is not None:
        for c in flow_calls:
            t = c.args.get("rdx")
            if t and t[0] == "op" and ("ld", FUNC, parms, 2) in t[1]:
                dw = [x for x in t[1] if x[0] == "ld" and x[1] == FUNC and x[3] == 4]
                if len(dw) == 1:
                    put("UStruct.PropertiesSize", dw[0][2],
                        "dword that, minus ParmsSize, is the byte count of the call at 0x%x clearing the rest of the frame" % c.addr)
                    ps = dw[0][2]
                    for e in ev:
                        if e.kind == "cmpreg" and e.base and e.base[0] == "ld" and e.base[1] == FUNC and e.base[2] == ps + 4 \
                                and e.base[3] == 2 and e.disp == 16:
                            put("UStruct.MinAlignment", ps + 4, "word compared with 16 at 0x%x (alignment above 16 needs a manual round-up)" % e.addr)
                    break
    # -- ReturnValueOffset
    for e in ev:
        if e.kind == "cmpreg" and e.base and e.base[0] == "ld" and e.base[1] == FUNC and e.base[3] == 2 and e.disp == 0xFFFF:
            put("UFunction.ReturnValueOffset", e.base[2], "word compared with 0xFFFF (no return value) at 0x%x" % e.addr)
            break
    # -- EventGraphCallOffset
    if eg is not None:
        for e in ev:
            if e.kind == "load" and e.base == FUNC and e.size == 4 and e.disp == eg[0] + 8 and \
                    any(s.kind == "store" and s.value == ("ld", FUNC, e.disp, 4) for s in ev):
                put("UFunction.EventGraphCallOffset", e.disp, "dword loaded right after the event-graph pointer and stored as the call offset")
                break
    # -- property chains
    heads: Dict[int, dict] = {}
    for e in ev:
        pk = peel(e.base) if e.base else None
        if pk is None:
            continue
        head, depth, links = pk
        h = heads.setdefault(head, {"links": Counter(), "events": [], "first": e.addr, "last": e.addr})
        h["events"].append(e)
        h["first"], h["last"] = min(h["first"], e.addr), max(h["last"], e.addr)
        if depth >= 1:
            h["links"][links[0]] += 1
    # the head element (depth 0) is `[fn+D]` itself: a qword load from FUNC creates the chain
    for e in ev:
        if e.kind == "load" and e.base == FUNC and e.size == 8 and e.disp in heads:
            heads[e.disp]["first"] = min(heads[e.disp]["first"], e.addr)
    stored_heads = {s.value[2] for s in ev if s.kind == "store" and s.value and s.value[0] == "ld" and s.value[1] == FUNC and s.value[3] == 8}
    res["heads"] = {hex(k): {"first": hex(v["first"]), "last": hex(v["last"]), "links": dict(v["links"])} for k, v in heads.items()}
    walkers = {d: h for d, h in heads.items() if h["links"]}
    script = found.get("UStruct.Script", (None,))[0]
    child = [d for d in walkers if d in stored_heads and d != script]
    chain_of: Dict[str, int] = {}
    if len(child) == 1:
        chain_of["ChildProperties"] = child[0]
        put("UStruct.ChildProperties", child[0], "qword stored into the frame (PropertyChainForCompiledIn) and walked through FField::Next")
    others = [d for d in walkers if d not in child and d != script]
    if parms is not None and len(others) == 2:
        in_parms = [d for d in others if any(e.kind == "load" and e.base == FUNC and e.disp == parms and walkers[d]["first"] < e.addr < walkers[d]["last"]
                                              for e in ev)]
        if len(in_parms) == 1:
            dtor = in_parms[0]
            init = [d for d in others if d != dtor][0]
            chain_of["DestructorLink"] = dtor
            chain_of["FirstPropertyToInit"] = init
            put("UStruct.DestructorLink", dtor, "chain head whose loop reads ParmsSize (IsInContainer) and destroys the value")
            put("UFunction.FirstPropertyToInit", init, "chain head whose loop initialises the locals (no ParmsSize read)")
    nexts = {}
    for name, d in chain_of.items():
        c = walkers[d]["links"]
        if c:
            nexts[name] = c.most_common(1)[0][0]
    if "ChildProperties" in nexts:
        put("FField.Next", nexts["ChildProperties"], "link followed by the loop over the ChildProperties chain")
    if "DestructorLink" in nexts:
        put("FProperty.DestructorLinkNext", nexts["DestructorLink"], "link followed by the DestructorLink loop")
    if "FirstPropertyToInit" in nexts:
        put("FProperty.PostConstructLinkNext", nexts["FirstPropertyToInit"], "link followed by the FirstPropertyToInit loop")
    # -- FProperty fields: loads through property-chain elements, by chain
    per_chain: Dict[int, Counter] = defaultdict(Counter)
    flag_bytes: Dict[int, Set[int]] = defaultdict(set)
    vcalls: Dict[int, Set[int]] = defaultdict(set)
    for d, h in heads.items():
        for e in h["events"]:
            if e.kind == "load":
                per_chain[d][(e.disp, e.size, e.mnemonic)] += 1
            if e.kind == "test" and e.size == 1:
                flag_bytes[d].add(e.disp)
    prop_chains = [d for d in chain_of.values()]
    if len(prop_chains) == 3:
        sets = [{k for k in per_chain[d] if k[1] == 4 and k[2] == "movsxd"} for d in prop_chains]
        common = set.intersection(*sets) if sets else set()
        if len(common) == 1:
            off = next(iter(common))[0]
            put("FProperty.Offset_Internal", off, "dword every property loop reads (the member offset added to the frame)")
        pair = set.intersection(*[sets[i] for i in range(3) if i != 0]) - common if len(sets) == 3 else set()
        dd = sorted(k[0] for k in pair)
        if len(dd) == 2:
            put("FProperty.ArrayDim", dd[0], "lower of the two dwords the loops multiply (ElementSize * ArrayDim); UE declares ArrayDim first")
            put("FProperty.ElementSize", dd[1], "higher of the two dwords the loops multiply (ElementSize * ArrayDim)")
        q = {name: {k[0] for k in per_chain[d] if k[1] == 8 and k[2] == "mov" and k[0] not in nexts.values() and k[0] != 0}
             for name, d in chain_of.items()}
        qc = q["ChildProperties"] & q["DestructorLink"]
        for cand in sorted(qc):
            if any((cand + 1) in flag_bytes[d] for d in prop_chains):
                put("FProperty.PropertyFlags", cand, "qword that two loops load and test for CPF_* bits; a byte test at +1 reads its second byte")
                break
    # property vtable slots: vcalls whose receiver is the loop element
    for c in flow_calls:
        t = c.target
        if not (isinstance(t, tuple) and t and t[0] == "vcall"):
            continue
        recv = c.args.get("rdi")
        pk = peel(recv) if recv else None
        if pk is None:
            continue
        for name, d in chain_of.items():
            if d == pk[0]:
                vcalls[d].add(t[2])
    if "DestructorLink" in chain_of and len(vcalls[chain_of["DestructorLink"]]) == 1:
        put("FProperty.DestroyValueInternal", next(iter(vcalls[chain_of["DestructorLink"]])), "virtual called on each element of the DestructorLink loop")
    if "FirstPropertyToInit" in chain_of and len(vcalls[chain_of["FirstPropertyToInit"]]) == 1:
        put("FProperty.InitializeValueInternal", next(iter(vcalls[chain_of["FirstPropertyToInit"]])),
            "virtual called on each element of the FirstPropertyToInit loop")
    # -- OuterPrivate: the object whose virtual is called with the function as argument (Function->GetOuter()->GetPersistentFrame(this, Function))
    for c in flow_calls:
        t = c.target
        recv = c.args.get("rdi")
        if isinstance(t, tuple) and t and t[0] == "vcall" and recv and recv[0] == "ld" and recv[1] == FUNC and recv[3] == 8 \
                and c.args.get("rdx") == FUNC and recv[2] not in (found.get("UStruct.ChildProperties", (None,))[0],):
            put("UObject.OuterPrivate", recv[2], "qword of the UFunction used as the receiver of the virtual call at 0x%x (Function->GetOuter()->...(this, Function))" % c.addr)
            break
    # -- Func: the callee that receives the UFunction first and calls through it
    for c in flow_calls:
        if isinstance(c.target, int) and c.args.get("rdi") == FUNC:
            f2 = c.target
            s2, e2 = idx.funcs.span(f2)
            fl2 = Flow(dec, dec.decode_function(s2, min(e2, s2 + 0x200)))
            ind = [x for x in fl2.calls() if isinstance(x.target, tuple) and x.target and x.target[0] == "vcall" and x.target[1] == ("arg", 0)]
            ind = [x for x in ind if x.args.get("rdi") is not None]
            if len(ind) == 1:
                put("UFunction.Func", ind[0].target[2], "UFunction::Invoke (0x%x) makes one indirect call through its UFunction argument" % f2)
                res["invoke"] = f2
                break
    return res


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    pe = ctx.facts.get("process_event")
    expected = ["structs.UObjectBase.ObjectFlags", "structs.UObjectBase.OuterPrivate", "structs.UStruct.Script", "structs.UStruct.PropertiesSize",
                "structs.UStruct.MinAlignment", "structs.UStruct.ChildProperties", "structs.UStruct.DestructorLink",
                "structs.UFunction.ParmsSize", "structs.UFunction.ReturnValueOffset", "structs.UFunction.EventGraphFunction",
                "structs.UFunction.EventGraphCallOffset", "structs.UFunction.FirstPropertyToInit", "structs.UFunction.Func",
                "structs.FField.Next", "structs.FProperty.ArrayDim", "structs.FProperty.ElementSize", "structs.FProperty.PropertyFlags",
                "structs.FProperty.Offset_Internal", "structs.FProperty.DestructorLinkNext", "structs.FProperty.PostConstructLinkNext",
                "vtables.FProperty.DestroyValueInternal", "vtables.FProperty.InitializeValueInternal"]
    b.expect(expected, "UObject::ProcessEvent was not located, so none of the members it reads could be measured")
    if pe is None:
        return
    slot, fn = pe
    res = analyse(ctx, fn)
    where = "ProcessEvent 0x%x" % fn
    mapping = {"UObject.ObjectFlags": "structs.UObjectBase.ObjectFlags", "UObject.OuterPrivate": "structs.UObjectBase.OuterPrivate"}
    for member, (off, why) in res["found"].items():
        cls, name = member.split(".", 1)
        if member in mapping:
            path = mapping[member]
        elif cls == "UObject":
            continue
        elif cls == "FProperty" and name.endswith("ValueInternal"):
            path = "vtables.FProperty." + name
        elif name in ("ParmsSize", "ReturnValueOffset", "EventGraphFunction", "EventGraphCallOffset", "FirstPropertyToInit", "Func"):
            path = "structs.UFunction." + name
        else:
            path = "structs.%s.%s" % (cls, name)
        b.observe(path, off, CHANNEL, METHOD, ["%s: %s" % (where, why)], MEDIUM)
    # the offset the process_event detector measured joins the book as its own channel
    ctx.evidence_files["process_event_members"] = {
        "function": hex(fn), "instructions": res["instructions"], "found": {k: [hex(v[0]), v[1]] for k, v in res["found"].items()},
        "heads": res.get("heads", {}), "invoke": hex(res["invoke"]) if res.get("invoke") else None}
