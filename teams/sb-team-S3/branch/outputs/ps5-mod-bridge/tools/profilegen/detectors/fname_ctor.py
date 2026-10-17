"""``FName::FName(const wchar_t*, EFindName)`` -- the wide-string FName constructor.

Method (a generic port of the survey tool's "five-anchor" route, ``fname_call``):

UE registers its modules with calls of the shape ``FName(TEXT("Engine"), FNAME_Add)``.  Every such
call site, compiled by clang for SysV x86-64, looks like

    lea rsi, [rip + L"Engine"]        ; arg2 = the UTF-16 module name literal
    lea rdi, [rsp/rbp + X]            ; arg1 = address of the FName result on the stack
    mov edx, 1                        ; arg3 = EFindName::FNAME_Add
    call <FName ctor>

Module names (``Engine``, ``Renderer``, ``AnimGraphRuntime``, ...) are properties of Unreal Engine,
not of one game.  For each name that exists as a NUL-delimited UTF-16 literal, every rip-relative
``lea rsi`` reference to it is decoded; if the instructions up to the next ``call`` keep ``rsi``
intact, build the stack out-pointer in ``rdi`` and load ``edx = 1``, the call target is a ctor
candidate.  The constructor is the target that the *different* names agree on.

Why agreement across names is strong evidence: a stray ``lea rsi,[Engine]; ...; call X`` can occur
anywhere (the literal ``Engine`` has thousands of references), but the same X being the common
target for several unrelated module names, with the stack-out-pointer and ``edx = 1`` argument
setup, is only explained by the FName constructor.
"""
from __future__ import annotations

import re
from collections import Counter, defaultdict
from typing import Dict, List, Optional, Set, Tuple

from ..index import KIND_LEA
from ..model import HIGH, LOW, MEDIUM, Context, DetectorUnavailable, Hex

PRIMARY_ANCHORS = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]
# further module names used when the primary five do not suffice (still engine facts)
EXTRA_ANCHORS = ["Core", "CoreUObject", "SlateCore", "Slate", "InputCore", "UMG", "RHI", "NavigationSystem",
                 "AIModule", "PhysicsCore", "Chaos", "Niagara", "GameplayTags"]
RSI = 6          # register index of rsi in the xref index
MAX_STRINGS, MAX_XREFS = 64, 16384
_EDX_IMM = re.compile(rb"\xba[\x00-\x02]\x00\x00\x00")


def _decode_call_after(ctx: Context, site: int, string_vas: Set[int], exec_ok, require_enum: bool = True):
    """Port of the survey's ``call_after``: returns (call_va, target, edx_imm) or None."""
    from capstone import x86 as xo
    from capstone import x86_const as xc
    dec = ctx.index.decoder
    if require_enum:
        # cheap pre-filter: the window must contain `mov edx, EFindName` (0 = find, 1 = add, 2 = replace)
        raw = ctx.elf.read_upto(site, 64)
        if _EDX_IMM.search(raw) is None:
            return None
    ins = dec.decode(site, 64)
    if not ins:
        return None
    first = ins[0]
    if first.mnemonic != "lea" or len(first.operands) != 2:
        return None
    o0, o1 = first.operands
    if o0.type != xo.X86_OP_REG or o0.reg != xc.X86_REG_RSI:
        return None
    if o1.type != xo.X86_OP_MEM or o1.mem.base != xc.X86_REG_RIP:
        return None
    if first.address + first.size + o1.mem.disp not in string_vas:
        return None
    stack_arg = False
    enum_imm: Optional[int] = None
    rdx_names = {"rdx", "edx", "dx", "dl"}
    rdi_names = {"rdi", "edi", "di", "dil"}
    rsi_names = {"rsi", "esi", "si", "sil"}
    stop_groups = {xc.X86_GRP_JUMP, xc.X86_GRP_RET, xc.X86_GRP_INT, xc.X86_GRP_IRET}
    for i in ins[1:]:
        if i.mnemonic == "call":
            if len(i.operands) == 1 and i.operands[0].type == xo.X86_OP_IMM and stack_arg and \
                    (enum_imm is not None or not require_enum):
                target = i.operands[0].imm
                if exec_ok(target):
                    return i.address, target, enum_imm
            return None
        if set(i.groups) & stop_groups or i.mnemonic in ("ret", "retf", "hlt", "jmp", "loop", "loope", "loopne", "jrcxz"):
            return None
        try:
            _, written = i.regs_access()
        except Exception:
            written = []
        wnames = {dec.reg_name(r) for r in written}
        if wnames & rsi_names:
            return None
        if wnames & rdi_names:
            ok = False
            if i.mnemonic == "lea" and i.operands and i.operands[0].type == xo.X86_OP_REG and \
                    i.operands[0].reg == xc.X86_REG_RDI:
                mo = i.operands[1]
                ok = mo.type == xo.X86_OP_MEM and mo.mem.base in (xc.X86_REG_RSP, xc.X86_REG_RBP) and mo.mem.index == 0
            stack_arg = ok
        if wnames & rdx_names:
            enum_imm = None
            if i.mnemonic == "mov" and i.operands and i.operands[0].type == xo.X86_OP_REG and \
                    i.operands[0].reg == xc.X86_REG_EDX and i.operands[1].type == xo.X86_OP_IMM and \
                    (i.operands[1].imm & 0xFFFFFFFF) in (0, 1, 2):
                enum_imm = i.operands[1].imm & 0xFFFFFFFF
    return None


def _anchor_sites(ctx: Context, name: str) -> Tuple[Set[int], List[int], Optional[str]]:
    strings = ctx.index.strings.find_utf16(name)
    if not strings:
        return set(), [], "no NUL-delimited UTF-16 literal"
    if len(strings) > MAX_STRINGS:
        return set(strings), [], "%d literals (> %d)" % (len(strings), MAX_STRINGS)
    sites: List[int] = []
    xr = ctx.index.xrefs
    for s in strings:
        for site in xr.refs_to(s, [KIND_LEA]):
            sites.append(site)
    if len(sites) > MAX_XREFS:
        return set(strings), [], "%d references (> %d)" % (len(sites), MAX_XREFS)
    return set(strings), sorted(set(sites)), None


def _ctor_body_facts(ctx: Context, target: int) -> dict:
    """What the constructor does with rdx/edx and rdi (calling-convention evidence)."""
    from capstone import x86 as xo
    dec = ctx.index.decoder
    ins = dec.decode(target, 200)
    reads_edx = False
    alias = {"rdi"}                         # registers that currently hold the caller's rdi
    stored_through_this = False
    callee = None
    after_call = False
    n = 0
    rdx_names = {"rdx", "edx", "dx", "dl"}
    for i in ins:
        n += 1
        try:
            read, _written = i.regs_access()
        except Exception:
            read = []
        rnames = {dec.reg_name(r) for r in read}
        if not after_call and rnames & rdx_names and i.mnemonic != "push":
            reads_edx = True
        if i.mnemonic == "mov" and len(i.operands) == 2 and i.operands[0].type == xo.X86_OP_REG and \
                i.operands[1].type == xo.X86_OP_REG and dec.reg_name(i.operands[1].reg) in alias:
            alias.add(dec.reg_name(i.operands[0].reg))
        if i.mnemonic == "mov" and len(i.operands) == 2 and i.operands[0].type == xo.X86_OP_MEM and \
                i.operands[0].size == 8 and dec.reg_name(i.operands[0].mem.base) in alias and \
                i.operands[1].type == xo.X86_OP_REG:
            stored_through_this = True
        if i.mnemonic == "call":
            if callee is None and i.operands and i.operands[0].type == xo.X86_OP_IMM:
                callee = i.operands[0].imm
            after_call = True
        if i.mnemonic == "ret":
            break
    return {"reads_edx": reads_edx, "stores_8_through_this": stored_through_this, "delegate": callee,
            "instructions_scanned": n}


def _delegate_reads_edx(ctx: Context, delegate: int, limit: int = 48) -> bool:
    dec = ctx.index.decoder
    rdx_names = {"rdx", "edx", "dx", "dl"}
    for i in dec.decode(delegate, 400)[:limit]:
        try:
            read, written = i.regs_access()
        except Exception:
            return True
        if {dec.reg_name(r) for r in read} & rdx_names and i.mnemonic != "push":
            return True
        if {dec.reg_name(r) for r in written} & rdx_names:
            return False                      # rdx overwritten before any read: argument is dead
    return False


def detect(ctx: Context) -> None:
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    funcs = ctx.index.funcs
    elf = ctx.elf
    exec_ok = elf.is_exec
    imm_at: Dict[int, Optional[int]] = {}
    results: Dict[str, Dict[int, List[int]]] = {}
    anchor_info: Dict[str, dict] = {}
    site_cache: Dict[str, Tuple[Set[int], List[int], Optional[str]]] = {}
    done: Set[Tuple[str, bool]] = set()

    def run(names: List[str], require_enum: bool) -> None:
        for name in names:
            if (name, require_enum) in done:
                continue
            done.add((name, require_enum))
            if name not in site_cache:
                site_cache[name] = _anchor_sites(ctx, name)
            strs, sites, problem = site_cache[name]
            info = {"literals": sorted(strs), "xrefs": len(sites), "problem": problem, "call_sites": 0}
            tmap: Dict[int, List[int]] = defaultdict(list)
            if not problem:
                for site in sites:
                    r = _decode_call_after(ctx, site, strs, exec_ok, require_enum)
                    if r:
                        call, tgt, imm = r
                        if call not in tmap[tgt]:
                            tmap[tgt].append(call)
                            imm_at[call] = imm
                info["call_sites"] = sum(len(v) for v in tmap.values())
            results[name] = dict(tmap)
            anchor_info[name] = info

    # strict (stack out-pointer AND edx = immediate) over the primary names, then widen the name set,
    # then relax the edx requirement
    anchor_names = list(PRIMARY_ANCHORS)
    mode = "strict"
    run(anchor_names, True)
    usable = [n for n in anchor_names if results.get(n)]
    if len(usable) < 3:
        anchor_names = anchor_names + EXTRA_ANCHORS
        run(anchor_names, True)
        usable = [n for n in anchor_names if results.get(n)]
    if len(usable) < 3:
        mode = "relaxed (edx not required)"
        run(anchor_names, False)
        usable = [n for n in anchor_names if results.get(n)]
    votes: Counter = Counter()
    sites_by_target: Dict[int, int] = Counter()
    for n in usable:
        for tgt, calls in results[n].items():
            votes[tgt] += 1
            sites_by_target[tgt] += len(calls)
    if not votes:
        ctx.fail("symbols.fname_ctor_wchar", "no module-name anchor produced a lea rsi / lea rdi / mov edx,1 / call site")
        return
    ranked = sorted(votes, key=lambda t: (-votes[t], -sites_by_target[t], t))
    target = ranked[0]
    n_anchor = votes[target]
    runner = votes[ranked[1]] if len(ranked) > 1 else 0
    evidence = ["%d of %d usable module-name anchors agree on call target 0x%x (runner-up has %d); argument setup: %s" % (
        n_anchor, len(usable), target, runner, mode)]
    for n in usable:
        info = anchor_info[n]
        evidence.append("anchor %r: literal(s) %s, %d lea references, %d decoded ctor-shaped call sites, %d to this target" % (
            n, ",".join("0x%x" % a for a in info["literals"]), info["xrefs"], info["call_sites"],
            len(results[n].get(target, []))))
    all_calls = sorted({c for n in usable for c in results[n].get(target, [])})
    verified = 0
    for c in all_calls:
        b = elf.read(c, 5)
        if b and b[0] == 0xE8 and c + 5 + int.from_bytes(b[1:5], "little", signed=True) == target:
            verified += 1
    evidence.append("%d call sites, %d re-verified as E8 rel32 resolving to the target" % (len(all_calls), verified))
    is_start = funcs.is_start(target)
    prologue = elf.read(target, 4)
    evidence.append("target is a function start: %s; first bytes %s" % (is_start, prologue.hex() if prologue else "?"))
    callers = len(ctx.index.calls.callers(target))
    evidence.append("%d direct call sites image-wide (byte-level upper bound)" % callers)
    unique = runner < n_anchor
    if n_anchor >= 4 and unique and is_start and verified == len(all_calls):
        conf = HIGH
    elif n_anchor >= 2 and unique and is_start:
        conf = MEDIUM
    else:
        conf = LOW
    ctx.put("symbols.fname_ctor_wchar.label", Hex(target), conf, "fname_ctor.module_name_anchors", evidence)
    ctx.put("symbols.fname_ctor_wchar.callsites", ",".join("0x%x" % c for c in all_calls), conf,
            "fname_ctor.module_name_anchors", ["%d call sites that load edx = 1" % len(all_calls)])
    ctx.put("symbols.fname_ctor_wchar.anchor_modules", ",".join(n for n in usable if target in results[n]), conf,
            "fname_ctor.module_name_anchors", ["module-name literals whose call sites reach the target"])
    ctx.put("symbols.fname_ctor_wchar.anchor_string_labels",
            ",".join("%s=0x%x" % (n, anchor_info[n]["literals"][0]) for n in usable if target in results[n]), conf,
            "fname_ctor.module_name_anchors", ["vaddrs of the UTF-16 literals (not file offsets)"])
    pro = elf.read(target, 32)
    if pro:
        ctx.put("symbols.fname_ctor_wchar.prologue_32", pro.hex(), HIGH, "bytes", ["first 32 bytes at the function start"])
    imms = sorted({imm_at.get(c) for c in all_calls})
    if len(imms) == 1 and imms[0] is not None:
        ctx.put("symbols.fname_ctor_wchar.find_arg_at_callsites", Hex(imms[0]), conf, "fname_ctor.module_name_anchors",
                ["every decoded call site sets edx to the immediate %d before the call (1 = EFindName::FNAME_Add)" % imms[0]])
    # ---- calling convention --------------------------------------------------------------
    facts = _ctor_body_facts(ctx, target)
    deleg_reads = _delegate_reads_edx(ctx, facts["delegate"]) if facts["delegate"] else None
    abi_ev = ["call sites: rdi = lea of a stack slot (out FName*), rsi = the UTF-16 literal, edx = immediate 1",
              "constructor body (%d instructions to its return): reads edx before its first call = %s, stores 8 bytes through a copy of rdi = %s" % (
                  facts["instructions_scanned"], facts["reads_edx"], facts["stores_8_through_this"])]
    if facts["delegate"]:
        abi_ev.append("it delegates to 0x%x with rdx untouched; that function reads rdx in its first 48 instructions: %s" % (
            facts["delegate"], deleg_reads))
    if facts["reads_edx"] or deleg_reads:
        abi = "sysv-amd64; rdi=out FName*; rsi=const char16_t*; edx=EFindName (read)"
    else:
        abi = "sysv-amd64; rdi=out FName*; rsi=const char16_t*; edx=EFindName"
        abi_ev.append("edx is passed by every caller but not consumed along the decoded path (the overload always adds)")
    ctx.put("symbols.fname_ctor_wchar.abi", abi, MEDIUM if conf != LOW else LOW, "fname_ctor.abi_decode", abi_ev)
    if facts["stores_8_through_this"]:
        ctx.put("structs.FName.size", Hex(8), MEDIUM if conf != LOW else LOW, "fname_ctor.abi_decode",
                ["the constructor stores one 8-byte value through a copy of its first argument (the FName handle)"])
    if facts["delegate"]:
        ctx.put("symbols.fname_intern.label", Hex(facts["delegate"]), MEDIUM if conf != LOW else LOW, "fname_ctor.abi_decode",
                ["the single call the constructor makes: computes the wide flag and length, then interns the name"])
    ctx.facts["fname_ctor"] = target
    ctx.evidence_files["fname_ctor"] = {
        "target": target, "anchors": {n: {k: (sorted(v) if isinstance(v, set) else v) for k, v in anchor_info[n].items()}
                                       for n in anchor_info},
        "targets": {n: {hex(t): [hex(c) for c in cs] for t, cs in results[n].items()} for n in usable},
        "body": facts,
    }
