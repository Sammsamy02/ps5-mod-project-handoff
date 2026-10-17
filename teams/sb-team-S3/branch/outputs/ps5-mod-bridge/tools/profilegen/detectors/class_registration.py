"""Native class registration (``GetPrivateStaticClassBody``): class sizes and the ``UClass`` / ``UStruct`` layout.

Every native ``UCLASS`` has a ``StaticClass()`` that builds its ``UClass`` once::

    GetPrivateStaticClassBody(PackageName,      // rdi  L"/Script/Engine"
                              Name,             // rsi  L"GameEngine"
                              ReturnClass,      // rdx
                              RegisterNativeFunc,// rcx
                              InSize,           // r8d  sizeof(Class)
                              InAlignment,      // r9d  alignof(Class)
                              InClassFlags,     // stack 0
                              InClassCastFlags, // stack 1
                              InConfigName,     // stack 2
                              InClassConstructor, InClassVTableHelperCtorCaller,            // stack 3, 4
                              InCppClassStaticFunctions,                                    // stack 5
                              InSuperClassFn, InWithinClassFn);                             // stack 6, 7

Method:

1. **The helper.**  The UTF-16 package literal (``/Script/<Module>``) is loaded into ``rdi`` right before
   the call; the call target the *different modules agree on* is the helper (hundreds to thousands of
   sites).  Nothing but its call sites is needed to read ``sizeof`` and ``alignof`` of every native
   class: ``rsi`` = the class-name literal, ``r8d`` / ``r9d`` = the immediates.
2. **Its body.**  The helper allocates the ``UClass`` and stores its arguments into it.  A linear
   data-flow walk (``_flow.py``) says where each argument lands: ``InSize`` is stored as a dword at
   ``UStruct::PropertiesSize``, ``InAlignment`` as a word at ``MinAlignment``, ``InClassFlags`` (or-ed
   with ``CLASS_Native``) at ``ClassFlags``, ``InClassCastFlags`` at ``ClassCastFlags``, the result of
   ``InWithinClassFn()`` at ``ClassWithin``, ``InConfigName`` goes to the ``FName`` constructor with
   ``rdi`` = ``&ClassConfigName``, the stack pair (``InClassConstructor``, ``InClassVTableHelperCtorCaller``)
   is stored as one 16-byte block.  The raw allocation size passed to the allocator is ``sizeof(UClass)``.
3. **Inline copies.**  Classes whose ``StaticClass`` the compiler open-coded (the reflection classes
   themselves) have no call: the same stores appear with immediates (``mov dword [x+PropertiesSize], size;
   mov word [x+MinAlignment], align``) next to the class-name literal.  They are found from the name
   literal and give the same offsets and the sizes of ``UClass`` / ``UFunction``.
"""
from __future__ import annotations

from collections import Counter
from typing import Dict, List, Optional, Tuple

from ..index import KIND_LEA
from ..model import HIGH, MEDIUM, Context, DetectorUnavailable, Hex
from ._book import book
from ._flow import Flow, mentions

PACKAGE_FRAGMENT = "/Script/"
WINDOW_BYTES = 200
MAX_WINDOW_INS = 64

# schema class -> registration name of the native class (UE's naming convention: UFoo / AFoo -> "Foo")
SIZE_CLASSES = [("UField", "Field"), ("UStruct", "Struct"), ("UClass", "Class"), ("UFunction", "Function"),
                ("UScriptStruct", "ScriptStruct"), ("UEnum", "Enum"), ("UWorld", "World"), ("AActor", "Actor"),
                ("AGameModeBase", "GameModeBase"), ("AGameMode", "GameMode"), ("UPlayer", "Player"),
                ("ULocalPlayer", "LocalPlayer"), ("UGameViewportClient", "GameViewportClient"),
                ("UDataTable", "DataTable")]
INLINE_NAMES = ["Function", "Class", "ScriptStruct", "Field", "Struct", "Object"]


def package_sites(ctx: Context) -> List[Tuple[int, str]]:
    """(reference site, package text) for every ``lea reg,[L"/Script/<Module>"]``."""
    idx = ctx.index
    out: List[Tuple[int, str]] = []
    for va, text in idx.strings.find_utf16_containing(PACKAGE_FRAGMENT, limit=20000):
        body = text[len(PACKAGE_FRAGMENT):]
        if not text.startswith(PACKAGE_FRAGMENT) or not body or not body.replace("_", "").isalnum():
            continue
        for site in idx.xrefs.refs_to(va, [KIND_LEA]):
            out.append((site, text))
    return sorted(out)


def first_direct_call(ctx: Context, site: int) -> Optional[int]:
    from capstone import x86 as xo
    for i in ctx.index.decoder.decode(site, WINDOW_BYTES, MAX_WINDOW_INS):
        if i.mnemonic == "call":
            return int(i.operands[0].imm) if i.operands and i.operands[0].type == xo.X86_OP_IMM else None
        if i.mnemonic in ("ret", "jmp"):
            return None
    return None


def find_helper(ctx: Context, sites: List[Tuple[int, str]]) -> Optional[dict]:
    if len(sites) < 8:
        return None
    step = max(1, len(sites) // 600)
    votes: Counter = Counter()
    modules: Dict[int, set] = {}
    for site, pkg in sites[::step]:
        t = first_direct_call(ctx, site)
        if t is not None:
            votes[t] += 1
            modules.setdefault(t, set()).add(pkg)
    if not votes:
        return None
    ranked = votes.most_common(2)
    best, n = ranked[0]
    runner = ranked[1][1] if len(ranked) > 1 else 0
    return {"target": best, "votes": n, "runner_up": runner, "sampled": len(sites[::step]), "modules": len(modules[best]),
            "sites": len(sites)}


def registration_rows(ctx: Context, sites: List[Tuple[int, str]], helper: int) -> Dict[Tuple[str, str], List[Tuple[int, int, int]]]:
    """(package, class name) -> [(sizeof, alignof, call site)] for every call of the helper with immediate size/alignment."""
    from bisect import bisect_left
    from capstone import x86 as xo
    from capstone import x86_const as xc
    from ..x86util import canon64
    dec = ctx.index.decoder
    elf = ctx.elf
    call_sites = ctx.index.calls.callers(helper)
    rows: Dict[Tuple[str, str], List[Tuple[int, int, int]]] = {}
    for site, pkg in sites:
        k = bisect_left(call_sites, site)
        if k >= len(call_sites) or call_sites[k] - site > WINDOW_BYTES:
            continue
        call_at = call_sites[k]
        state: Dict[str, Tuple[str, int]] = {}
        for i in dec.decode(site, call_at - site + 5):
            if i.address >= call_at:
                break
            ops = i.operands
            if not ops or ops[0].type != xo.X86_OP_REG:
                continue
            reg = canon64(dec.reg_name(ops[0].reg))
            if reg not in ("rsi", "r8", "r9"):
                continue
            if i.mnemonic == "lea" and len(ops) == 2 and ops[1].type == xo.X86_OP_MEM and ops[1].mem.base == xc.X86_REG_RIP:
                state[reg] = ("addr", i.address + i.size + ops[1].mem.disp)
            elif i.mnemonic == "mov" and len(ops) == 2 and ops[1].type == xo.X86_OP_IMM:
                state[reg] = ("imm", int(ops[1].imm))
            else:
                state.pop(reg, None)
        name, size, align = state.get("rsi"), state.get("r8"), state.get("r9")
        if not (name and name[0] == "addr" and size and size[0] == "imm" and align and align[0] == "imm"):
            continue
        text = elf.utf16(name[1])
        if text:
            rows.setdefault((pkg, text), []).append((size[1], align[1], call_at))
    return rows


# ---------------------------------------------------------------------------------------------------------
def analyse_helper(ctx: Context, helper: int) -> dict:
    """Where the helper stores its arguments in the UClass it allocates."""
    idx = ctx.index
    dec = idx.decoder
    st, en = idx.funcs.span(helper)
    ins = dec.decode_function(st, min(en, st + 0x600))
    flow = Flow(dec, ins)
    stores = flow.stores()
    by_base: Counter = Counter(e.base for e in stores if e.base and e.base[0] == "ret")
    if not by_base:
        return {}
    obj = by_base.most_common(1)[0][0]
    out: Dict[str, object] = {"obj": obj, "instructions": len(ins), "alloc_size": None}
    alloc = next((c for c in flow.calls() if ("ret", c.addr, c.target) == obj), None)
    if alloc is not None:
        sz = alloc.args.get("rsi")
        if sz and sz[0] == "imm":
            out["alloc_size"] = (int(sz[1]), alloc.addr)
    found: Dict[str, Tuple[int, int, str]] = {}               # member -> (offset, store address, why)
    for e in stores:
        if e.base != obj or e.value is None:
            continue
        v = e.value
        if e.size == 4 and v == ("arg", 4) and "PropertiesSize" not in found:
            found["PropertiesSize"] = (e.disp, e.addr, "dword store of the InSize argument (r8d)")
        elif e.size == 2 and v == ("arg", 5) and "MinAlignment" not in found:
            found["MinAlignment"] = (e.disp, e.addr, "word store of the InAlignment argument (r9w)")
        elif e.size == 4 and mentions(v, "stk", 0) and "ClassFlags" not in found:
            found["ClassFlags"] = (e.disp, e.addr, "dword store of InClassFlags or-ed with a constant (CLASS_Native)")
        elif e.size == 8 and v == ("stk", 1) and "ClassCastFlags" not in found:
            found["ClassCastFlags"] = (e.disp, e.addr, "qword store of the InClassCastFlags argument")
        elif e.size == 16 and v == ("stkv", 3, 2) and "ClassConstructor" not in found:
            found["ClassConstructor"] = (e.disp, e.addr, "16-byte store of the (InClassConstructor, InClassVTableHelperCtorCaller) pair")
            found["ClassVTableHelperCtorCaller"] = (e.disp + 8, e.addr, "second half of the same 16-byte store")
        elif e.size == 8 and v[0] == "ret" and v[2] == ("stk", 7):
            found["ClassWithin"] = (e.disp, e.addr, "qword store of the result of the InWithinClassFn call")
    fname_ctor = ctx.facts.get("fname_ctor")
    if fname_ctor:
        for c in flow.calls():
            if c.target != fname_ctor:
                continue
            rdi, rsi = c.args.get("rdi"), c.args.get("rsi")
            if rdi and rdi[0] == "ptr" and rdi[1] == obj and rsi == ("stk", 2):
                found["ClassConfigName"] = (rdi[2], c.addr, "FName constructor called with rdi = &member and rsi = the InConfigName argument")
    for e in stores:
        if e.base == obj and e.disp == 0 and e.size == 8 and e.value and e.value[0] == "addr":
            out["vtable"] = (e.value[1], e.addr)
    out["found"] = found
    return out


def inline_copies(ctx: Context, names: List[str], size_off: int, align_off: int, skip: set) -> Dict[str, List[dict]]:
    """Open-coded registrations: ``mov dword [x+size_off], imm; mov word [x+align_off], imm`` in a function that loads the name literal."""
    idx = ctx.index
    dec = idx.decoder
    elf = ctx.elf
    found: Dict[str, List[dict]] = {}
    for name in names:
        addrs = idx.strings.find_utf16(name, anywhere=True)         # the compiler tail-merges "Function" into "UFunction"
        funcs = set()
        for va in addrs[:200]:
            for site in idx.xrefs.refs_to(va, [KIND_LEA])[:50]:
                f = idx.funcs.containing(site)
                if f is not None:
                    funcs.add(f)
        for f in sorted(funcs)[:40]:
            if f in skip:
                continue
            st, en = idx.funcs.span(f)
            if en - st > 0x1200:
                continue
            flow = Flow(dec, dec.decode_function(st, en), frame_args=False)
            for e in flow.stores(size=4):
                if e.disp != size_off or not e.value or e.value[0] != "imm":
                    continue
                mate = [m for m in flow.stores(size=2) if m.base == e.base and m.disp == align_off and m.value and m.value[0] == "imm"]
                if mate:
                    found.setdefault(name, []).append({"function": f, "size": int(e.value[1]), "align": int(mate[0].value[1]),
                                                       "site": e.addr})
                    break
    return found


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    b = book(ctx)
    b.expect(["structs.UStruct.PropertiesSize", "structs.UStruct.MinAlignment", "structs.UClass.ClassFlags",
              "structs.UClass.ClassCastFlags", "structs.UClass.ClassWithin", "structs.UClass.ClassConfigName",
              "structs.UClass.ClassConstructor", "structs.UClass.ClassVTableHelperCtorCaller"] +
             ["structs.%s.UEP_TotalSize" % c for c, _n in SIZE_CLASSES],
             "native class registration helper was not located (no /Script/<module> literal is followed by a call target "
             "shared by many modules)")
    sites = package_sites(ctx)
    helper = find_helper(ctx, sites)
    if helper is None or helper["votes"] < 8 or helper["votes"] < 2 * max(1, helper["runner_up"]):
        ctx.fail("symbols.class_registration_helper", "no call target is shared by the references to the /Script/<module> package literals "
                 "(%d references found)" % len(sites))
        return
    H = helper["target"]
    conf_h = HIGH if helper["votes"] >= 100 and helper["modules"] >= 8 and helper["votes"] >= 5 * max(1, helper["runner_up"]) else MEDIUM
    callers = len(ctx.index.calls.callers(H))
    ev = ["%d of %d sampled references to /Script/<module> literals (%d modules) are followed by a call to 0x%x; runner-up target: %d" % (
        helper["votes"], helper["sampled"], helper["modules"], H, helper["runner_up"]),
        "the target has %d direct callers" % callers]
    ctx.put("symbols.class_registration_helper.label", Hex(H), conf_h, "class_registration.package_literal_calls", ev)
    ctx.put("symbols.class_registration_helper.abi", "sysv-amd64; rdi=package, rsi=class name, rdx=&ReturnClass, rcx=RegisterNativeFunc, "
            "r8d=InSize, r9d=InAlignment, stack: ClassFlags, ClassCastFlags, ConfigName, ClassConstructor, "
            "ClassVTableHelperCtorCaller, CppClassStaticFunctions, SuperClassFn, WithinClassFn", MEDIUM,
            "class_registration.argument_flow", ["PropertiesSize/MinAlignment/ClassFlags/ClassCastFlags/ClassWithin stores follow these arguments"])
    ctx.facts["class_registration_helper"] = H

    # -- 2. the helper's body (its argument order is the UE 5 one; before UE 4.25 there is no alignment argument and the flags move)
    major = ctx.val("build.engine.major")
    if major is not None and int(major) < 5:
        for p in b.expected:
            if p.startswith(("structs.UStruct.", "structs.UClass.")) and not p.endswith("UEP_TotalSize"):
                b.why_not(p, "the engine is older than UE 5: the registration helper's argument order differs from the one this detector reads")
        body = {}
    else:
        body = analyse_helper(ctx, H)
    found = body.get("found", {}) if body else {}
    size_off = found.get("PropertiesSize", (None,))[0]
    align_off = found.get("MinAlignment", (None,))[0]
    if body.get("vtable"):
        ctx.facts["uclass_vtable"] = body["vtable"][0]
        ctx.put("symbols.uclass_vtable.label", Hex(body["vtable"][0]), MEDIUM, "class_registration.vtable_store",
                ["the helper stores this address at offset 0 of the UClass it allocates (0x%x)" % body["vtable"][1]])
    for member, (off, addr, why) in found.items():
        cls = "UStruct" if member in ("PropertiesSize", "MinAlignment") else "UClass"
        b.observe("structs.%s.%s" % (cls, member), off, "class_registration.helper_stores", "class_registration.argument_flow",
                  ["%s at 0x%x (helper 0x%x)" % (why, addr, H)], MEDIUM)

    # -- 3. class sizes from the call sites
    rows = registration_rows(ctx, sites, H)
    ctx.evidence_files["class_registration"] = {
        "helper": hex(H), "votes": helper["votes"], "runner_up": helper["runner_up"], "sites": len(sites), "classes": len(rows),
        "body": {k: (hex(v[0]), hex(v[1])) for k, v in found.items()}, "alloc_size": body.get("alloc_size"),
        "sizes": {"%s/%s" % k: [[hex(x[0]), x[1], hex(x[2])] for x in v] for k, v in sorted(rows.items())[:2000]}}
    seen_names = {}
    for (pkg, name), registrations in rows.items():
        for size, align, site in registrations:
            seen_names.setdefault(name, []).append((pkg, size, align, site))
    sized: Dict[str, Tuple[int, str]] = {}
    for cls, name in SIZE_CLASSES:
        hits = [h for h in seen_names.get(name, []) if h[0] in ("/Script/CoreUObject", "/Script/Engine")]
        if len({h[1] for h in hits}) == 1 and hits:
            pkg, size, align, site = hits[0]
            sized[cls] = (size, "call 0x%x: %s / %s, sizeof 0x%x, alignof %d" % (site, pkg, name, size, align))
    # -- 4. inline copies (also the second channel for the offsets)
    inline: Dict[str, List[dict]] = {}
    if size_off is not None and align_off is not None:
        inline = inline_copies(ctx, INLINE_NAMES, size_off, align_off, {H})
    inline_sizes: Dict[str, int] = {}
    for name, lst in inline.items():
        vals = {d["size"] for d in lst}
        if len(vals) == 1:
            inline_sizes[name] = lst[0]["size"]
    # inline copies confirm where PropertiesSize / MinAlignment are
    if inline:
        n_inline = sum(len(v) for v in inline.values())
        ev_inline = ["%d open-coded registration(s) (%s) store the size as a dword at +0x%x and the alignment as a word at +0x%x" % (
            n_inline, ", ".join(sorted(inline)), size_off, align_off)]
        b.observe("structs.UStruct.PropertiesSize", size_off, "class_registration.inline_copies", "class_registration.inline_immediates",
                  ev_inline, MEDIUM)
        b.observe("structs.UStruct.MinAlignment", align_off, "class_registration.inline_copies", "class_registration.inline_immediates",
                  ev_inline, MEDIUM)
    for cls, name in SIZE_CLASSES:
        if cls in sized:
            size, why = sized[cls]
            b.observe("structs.%s.UEP_TotalSize" % cls, size, "class_registration.sizeof", "class_registration.sizeof",
                      [why, "the size argument of the registration of the class named %r is sizeof(%s)" % (name, cls)], HIGH, True)
        elif name in inline_sizes:
            d = inline[name][0]
            b.observe("structs.%s.UEP_TotalSize" % cls, inline_sizes[name], "class_registration.sizeof", "class_registration.inline_immediates",
                      ["open-coded registration in 0x%x stores PropertiesSize = 0x%x at 0x%x" % (d["function"], d["size"], d["site"])],
                      HIGH, True)
    # the object class (UObject) size bounds UObjectBase
    obj_size = None
    for name in ("Object",):
        hits = [h for h in seen_names.get(name, []) if h[0] == "/Script/CoreUObject"]
        if hits:
            obj_size = (hits[0][1], hits[0][3]) if len({h[1] for h in hits}) == 1 else None
        elif name in inline_sizes:
            obj_size = (inline_sizes[name], inline[name][0]["site"])
    if obj_size:
        ctx.facts["uobject_size"] = obj_size[0]
        ctx.put("structs.UObject.sizeof", Hex(obj_size[0]), HIGH, "class_registration.sizeof",
                ["registration of the class named Object (0x%x) passes sizeof(UObject)" % obj_size[1]])
    # sizeof(UClass) is also the byte count the helper hands to the allocator for the UClass it creates
    if body.get("alloc_size"):
        sz, addr = body["alloc_size"]
        b.observe("structs.UClass.UEP_TotalSize", sz, "class_registration.alloc_size", "class_registration.alloc_size",
                  ["the helper allocates the UClass with a constant byte count 0x%x (call at 0x%x)" % (sz, addr)], MEDIUM)
    if not found:
        for p in ("PropertiesSize", "MinAlignment"):
            b.why_not("structs.UStruct." + p, "the registration helper at 0x%x stores none of its arguments through the object it allocates" % H)
