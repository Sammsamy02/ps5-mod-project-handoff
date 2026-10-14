"""Engine version (major.minor.patch), changelist, branch and the assembled revision tag.

UE keeps its compiled-in version in a ``FEngineVersionBase`` (``uint16 Major, Minor, Patch;
uint32 Changelist``) that is filled by a store sequence in ``FEngineVersion::InitializeVersion``.
A compiler turns the first two fields into one 32-bit immediate store and the third into a 16-bit
store, giving the shape (the same one patternsleuth's ``scan_version_stores`` looks for)

    mov dword [mem],   (minor << 16) | major     ; imm32, major 4/5
    mov word  [mem+4], patch                      ; 66-prefixed, immediately after
    ...
    mov dword [mem+8], changelist_word            ; within the next few instructions

Strategies, strongest first:

1. ``engine.store_site_scan``   the store shape above (needs capstone).
2. ``engine.version_string``    a NUL-delimited UTF-16 string ``M.m.p-...+...`` in the image.
3. ``engine.branch_string``     a UTF-16 string ``++UE5+Release-M.m`` / ``++UE4+Release-M.m``.

The marker fingerprint (``build_axes``) provides an independent version estimate used to
corroborate (never to overrule) the store scan.

Changelist and branch: the stored changelist word and the branch name come from tiny leaf
functions called just before the store (``xor eax,eax; ret`` returns 0, ``lea rax,[rip+str]; ret``
returns a string pointer).  A small abstract interpreter replays the instructions from the function
start to the store and reads off the value written to ``[mem+8]``.
"""
from __future__ import annotations

import re
from typing import Dict, List, Optional, Tuple

from ..model import HIGH, MEDIUM, Context, DetectorUnavailable, Hex

# imm32 = (minor << 16) | major with major 4 or 5 and a plausible minor, followed directly by the
# 0x66 operand-size prefix of the 16-bit patch store that must follow.
_IMM_PREFIX = re.compile(rb"[\x04\x05]\x00[\x01-\x1e]\x00\x66", re.S)

CALLER_SAVED = ("rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11")


def plausible(major: int, minor: int) -> bool:
    if major == 4:
        return 0 <= minor <= 27
    if major == 5:
        return 0 <= minor <= 20
    return False


# ---------------------------------------------------------------------------------------------
def _mem_key(op) -> Tuple[int, int, int, int]:
    m = op.mem
    return (m.base, m.index, m.scale, m.disp)


def scan_version_stores(ctx: Context) -> List[dict]:
    """Candidates from the imm32 / imm16 / [mem+8] store sequence."""
    dec = ctx.index.decoder
    if dec is None:
        raise DetectorUnavailable("capstone is not installed")
    from capstone import x86_const as xc
    from capstone import x86 as xo
    elf = ctx.elf
    out: List[dict] = []
    for seg in elf.exec_segments:
        lo, hi = seg.offset, seg.offset + seg.filesz
        delta = seg.vaddr - seg.offset
        for m in _IMM_PREFIX.finditer(elf.data, lo, hi):
            imm_off = m.start()
            store_va = None
            ins = None
            for k in range(2, 12):                       # instruction start .. imm32
                va = imm_off - k + delta
                cand = dec.decode(va, k + 4 + 12, 1)
                if not cand:
                    continue
                i = cand[0]
                if (i.mnemonic == "mov" and i.size == k + 4 and len(i.operands) == 2 and
                        i.operands[0].type == xo.X86_OP_MEM and i.operands[0].size == 4 and
                        i.operands[1].type == xo.X86_OP_IMM):
                    ins, store_va = i, va
                    break
            if ins is None:
                continue
            mem = ins.operands[0]
            if mem.mem.disp % 4 != 0:
                continue
            imm = ins.operands[1].imm & 0xFFFFFFFF
            major, minor = imm & 0xFFFF, imm >> 16
            if not plausible(major, minor):
                continue
            tail = dec.decode(store_va + ins.size, 64)
            if not tail:
                continue
            patch = tail[0]
            if not (patch.mnemonic == "mov" and len(patch.operands) == 2 and
                    patch.operands[0].type == xo.X86_OP_MEM and patch.operands[0].size == 2):
                continue
            pm, sm = patch.operands[0].mem, mem.mem
            if (pm.base, pm.index, pm.scale) != (sm.base, sm.index, sm.scale) or pm.disp != sm.disp + 4:
                continue
            patch_val = None
            if patch.operands[1].type == xo.X86_OP_IMM:
                patch_val = patch.operands[1].imm & 0xFFFF
                if patch_val > 30:
                    continue
            cl_site = None
            for j in tail[1:5]:                           # changelist follows shortly after
                if j.operands and j.operands[0].type == xo.X86_OP_MEM and j.operands[0].size == 4 and \
                        (j.operands[0].mem.base, j.operands[0].mem.index) == (sm.base, sm.index) and \
                        j.operands[0].mem.disp == sm.disp + 8 and j.mnemonic == "mov":
                    cl_site = j
                    break
            if cl_site is None:
                continue
            out.append({"major": major, "minor": minor, "patch": patch_val, "site": store_va,
                        "patch_site": patch.address, "changelist_store": cl_site, "bytes": bytes(ins.bytes)})
    return out


# ---------------------------------------------------------------------------------------------
def leaf_value(ctx: Context, target: int):
    """Evaluate a tiny leaf function: ('const', n) for ``xor eax,eax``/``mov eax,imm``,
    ('ptr', va) for ``lea rax,[rip+d]``, else None."""
    dec = ctx.index.decoder
    from capstone import x86 as xo
    ins = dec.decode(target, 24)
    kind = None
    for i in ins[:6]:
        if i.mnemonic in ("push", "pop") or (i.mnemonic == "mov" and i.op_str == "rbp, rsp"):
            continue
        if i.mnemonic == "xor" and len(set(i.op_str.replace(" ", "").split(","))) == 1 and i.op_str.startswith("eax"):
            kind = ("const", 0)
        elif i.mnemonic == "mov" and i.op_str.startswith("eax, ") and i.operands[1].type == xo.X86_OP_IMM:
            kind = ("const", i.operands[1].imm & 0xFFFFFFFF)
        elif i.mnemonic == "lea" and i.op_str.startswith("rax, [rip") and i.operands[1].mem.base != 0:
            kind = ("ptr", i.address + i.size + i.operands[1].mem.disp)
        elif i.mnemonic == "ret":
            return kind
        else:
            return None
    return None


def eval_changelist(ctx: Context, store_va: int, cl_store) -> dict:
    """Replay the function from its start to the ``[mem+8]`` store and return what it stores."""
    dec = ctx.index.decoder
    from capstone import x86 as xo
    funcs = ctx.index.funcs
    start = funcs.containing(store_va)
    if start is None:
        return {}
    ins_list = dec.decode(start, cl_store.address + cl_store.size - start + 4)
    regs: Dict[str, Optional[tuple]] = {}
    branch = None
    calls_seen: List[str] = []
    name = lambda rid: dec.reg_name(rid)
    wide = {"eax": "rax", "ecx": "rcx", "edx": "rdx", "ebx": "rbx", "esi": "rsi", "edi": "rdi"}
    for r in (8, 9, 10, 11, 12, 13, 14, 15):
        wide["r%dd" % r] = "r%d" % r
    canon = lambda n: wide.get(n, n)
    byte_of = {"al": "rax", "cl": "rcx", "dl": "rdx", "bl": "rbx"}
    for i in ins_list:
        if i.address >= cl_store.address:
            break
        mn = i.mnemonic
        ops = i.operands
        if mn == "call" and ops and ops[0].type == xo.X86_OP_IMM:
            v = leaf_value(ctx, ops[0].imm)
            for r in CALLER_SAVED:
                regs[r] = None
            regs["rax"] = v
            if v is not None and v[0] == "ptr":
                s = ctx.elf.utf16(v[1], 64) or ctx.elf.cstring(v[1], 64)
                if s:
                    branch = (v[1], s)
                calls_seen.append("leaf@0x%x -> ptr 0x%x" % (ops[0].imm, v[1]))
            elif v is not None:
                calls_seen.append("leaf@0x%x -> %d" % (ops[0].imm, v[1]))
            continue
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG:
            regs[canon(name(ops[0].reg))] = regs.get(canon(name(ops[1].reg)))
        elif mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_IMM:
            regs[canon(name(ops[0].reg))] = ("const", ops[1].imm & 0xFFFFFFFF)
        elif mn == "movzx" and len(ops) == 2 and ops[1].type == xo.X86_OP_REG and name(ops[1].reg) in byte_of:
            v = regs.get(byte_of[name(ops[1].reg)])
            regs[canon(name(ops[0].reg))] = ("const", v[1] & 0xFF) if v and v[0] == "const" else None
        elif mn == "shl" and len(ops) == 2 and ops[1].type == xo.X86_OP_IMM and ops[0].type == xo.X86_OP_REG:
            v = regs.get(canon(name(ops[0].reg)))
            regs[canon(name(ops[0].reg))] = ("const", (v[1] << ops[1].imm) & 0xFFFFFFFF) if v and v[0] == "const" else None
        elif mn == "or" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG:
            a, b = regs.get(canon(name(ops[0].reg))), regs.get(canon(name(ops[1].reg)))
            regs[canon(name(ops[0].reg))] = ("const", a[1] | b[1]) if a and b and a[0] == b[0] == "const" else None
        elif mn == "xor" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and \
                ops[0].reg == ops[1].reg:
            regs[canon(name(ops[0].reg))] = ("const", 0)
        else:
            try:
                _, written = i.regs_access()
            except Exception:
                written = []
            for r in written:
                regs[canon(name(r))] = None
    src = cl_store.operands[1]
    value = None
    if src.type == xo.X86_OP_REG:
        v = regs.get(canon(name(src.reg)))
        if v and v[0] == "const":
            value = v[1]
    elif src.type == xo.X86_OP_IMM:
        value = src.imm & 0xFFFFFFFF
    return {"changelist_word": value, "branch": branch, "calls": calls_seen, "function": start}


# ---------------------------------------------------------------------------------------------
_VERSION_STRING = re.compile(rb"\x00\x00([\x34\x35]\x00\.\x00(?:[0-9]\x00){1,2}\.\x00(?:[0-9]\x00){1,2}-\x00)", re.S)


def scan_version_strings(ctx: Context) -> List[dict]:
    """NUL-delimited UTF-16 ``M.m.p-...`` strings that look like FEngineVersion::ToString output."""
    elf = ctx.elf
    out = []
    for seg in elf.loads:
        for m in _VERSION_STRING.finditer(elf.data, seg.offset, seg.offset + seg.filesz):
            va = seg.vaddr + (m.start(1) - seg.offset)
            s = elf.utf16(va, 128)
            if not s:
                continue
            head, _, build = s.partition("-")
            parts = head.split(".")
            if len(parts) != 3 or not all(p.isdigit() for p in parts):
                continue
            if "+" not in build and "-CL-" not in build:
                continue
            major, minor = int(parts[0]), int(parts[1])
            if plausible(major, minor):
                out.append({"major": major, "minor": minor, "va": va, "text": s})
    return out


def scan_branch_names(ctx: Context) -> List[dict]:
    out = []
    for branch in ("++UE4+Release-", "++UE5+Release-"):
        for va in ctx.elf.find_all_vas(branch.encode("utf-16-le")):
            s = ctx.elf.utf16(va, 64)
            if not s or not s.startswith(branch):
                continue
            mm = re.match(r"(\d+)\.(\d+)", s[len(branch):])
            if mm and plausible(int(mm.group(1)), int(mm.group(2))):
                out.append({"major": int(mm.group(1)), "minor": int(mm.group(2)), "va": va, "text": s})
    return out


def _distinct(cands: List[dict]) -> List[Tuple[int, int]]:
    return sorted({(c["major"], c["minor"]) for c in cands})


def detect(ctx: Context) -> None:
    axes_ver = ctx.facts.get("axes.version")
    stores: List[dict] = []
    store_err = None
    try:
        stores = scan_version_stores(ctx)
    except DetectorUnavailable as exc:
        store_err = str(exc)
    strings = scan_version_strings(ctx)
    branches = scan_branch_names(ctx)
    # a branch name that is only the tail of a version string is the same literal, not a second channel
    branches = [b for b in branches if not any(s["va"] <= b["va"] < s["va"] + 2 * (len(s["text"]) + 1) for s in strings)]

    chosen = None
    method = None
    evidence: List[str] = []
    channels: List[str] = []
    for name, cands in (("engine.store_site_scan", stores), ("engine.version_string", strings),
                        ("engine.branch_string", branches)):
        d = _distinct(cands)
        if len(d) == 1:
            channels.append(name)
            if chosen is None:
                chosen, method = d[0], name
        elif len(d) > 1:
            evidence.append("%s: ambiguous, versions %s" % (name, ", ".join("%d.%d" % v for v in d)))
    axes_pair = None
    if axes_ver:
        try:
            axes_pair = tuple(int(x) for x in axes_ver.split("."))
        except ValueError:
            axes_pair = None
    if chosen is None and axes_pair is not None:
        # last resort: the marker fingerprint alone (one channel: never better than medium)
        chosen, method = axes_pair, "engine.marker_fingerprint"
        evidence.append("no store sequence or version string; the marker fingerprint selects %s (margin %s)" % (
            axes_ver, ctx.facts.get("axes.version_margin")))
        channels.append(method)
    if chosen is None:
        ctx.fail("build.engine.major", "no unambiguous engine version" + (" (%s)" % store_err if store_err else ""))
        ctx.fail("build.engine.minor", "no unambiguous engine version")
        return
    if method == "engine.store_site_scan":
        for c in stores:
            if (c["major"], c["minor"]) == chosen:
                evidence.append("store sequence at vaddr 0x%x: mov dword [mem], 0x%08x ; mov word [mem+4], %s ; "
                                "write to [mem+8] at 0x%x" % (
                                    c["site"], (c["minor"] << 16) | c["major"],
                                    "imm %d" % c["patch"] if c["patch"] is not None else "a register",
                                    c["changelist_store"].address))
    elif method == "engine.version_string":
        evidence.append("UTF-16 string %r at vaddr 0x%x" % (strings[0]["text"], strings[0]["va"]))
    elif method == "engine.branch_string":
        evidence.append("UTF-16 branch string %r at vaddr 0x%x" % (branches[0]["text"], branches[0]["va"]))
    independent = list(channels)
    disagrees = False
    if method != "engine.marker_fingerprint" and axes_pair is not None:
        if axes_pair == tuple(chosen):
            independent.append("axes.marker_fingerprint")
            evidence.append("marker fingerprint independently selects %s (margin %s)" % (
                axes_ver, ctx.facts.get("axes.version_margin")))
        else:
            disagrees = True
            evidence.append("marker fingerprint selects %s, which disagrees" % axes_ver)
    confidence = HIGH if len(independent) >= 2 and not disagrees else MEDIUM
    major, minor = chosen
    ctx.put("build.engine.major", Hex(major), confidence, method, evidence)
    ctx.put("build.engine.minor", Hex(minor), confidence, method, evidence)
    ctx.facts["engine.version"] = chosen

    # ---- patch / changelist / branch from the winning store site ------------------------
    # prefer a store whose patch is an immediate (the compiled-in constant), then the lowest address
    same = sorted((c for c in stores if (c["major"], c["minor"]) == chosen),
                  key=lambda c: (c["patch"] is None, c["site"]))
    site = same[0] if same else None
    if site is None:
        ctx.fail("build.engine.patch", "version came from a string, not a store sequence")
        return
    ctx.put("build.engine.store_site.label", Hex(site["site"]), MEDIUM, "engine.store_site_scan",
            ["bytes %s" % site["bytes"].hex()])
    if site["patch"] is not None:
        ctx.put("build.engine.patch", Hex(site["patch"]), MEDIUM, "engine.store_site_scan",
                ["immediate of the 16-bit store at vaddr 0x%x, [mem+4] right after the major/minor store" % site["patch_site"]])
    rep = {}
    try:
        rep = eval_changelist(ctx, site["site"], site["changelist_store"])
    except Exception as exc:                      # keep the profile; record why
        ctx.fail("build.engine.changelist", "changelist evaluation failed: %s" % exc)
    cl_word = rep.get("changelist_word")
    if cl_word is not None:
        changelist = cl_word & 0x7FFFFFFF
        licensee = cl_word >> 31
        ctx.put("build.engine.changelist", Hex(changelist), MEDIUM, "engine.changelist_replay",
                ["value stored to [mem+8] at 0x%x replayed from function 0x%x: %s ; licensee bit %d" % (
                    site["changelist_store"].address, rep["function"], "; ".join(rep["calls"]) or "no leaf calls", licensee)])
    elif "build.engine.changelist" not in [u.id for u in ctx.unresolved]:
        ctx.fail("build.engine.changelist", "the value stored to FEngineVersionBase+8 could not be replayed")
    branch = rep.get("branch")
    if branch:
        ctx.put("build.engine.branch_string.label", Hex(branch[0]), MEDIUM, "engine.branch_accessor",
                ["a leaf function called before the store returns the address of this string"])
        ctx.put("build.engine.branch", branch[1], MEDIUM, "engine.branch_accessor",
                ["leaf accessor returns pointer to UTF-16 string %r at vaddr 0x%x" % (branch[1], branch[0])])
    else:
        ctx.fail("build.engine.branch", "no leaf accessor returning a string was called before the version store")
    ctx.facts["engine.parts"] = {"major": major, "minor": minor, "patch": site["patch"],
                                 "changelist": (cl_word & 0x7FFFFFFF) if cl_word is not None else None,
                                 "branch": branch[1] if branch else None}
    parts = ctx.facts["engine.parts"]
    if None not in parts.values():
        tag = "%d.%d.%d-%d+++%s" % (parts["major"], parts["minor"], parts["patch"], parts["changelist"], parts["branch"])
        ctx.put("build.engine.revision_string", tag, MEDIUM, "engine.revision_join",
                ["joined as <major>.<minor>.<patch>-<changelist>+++<branch>, the opaque staging tag shape the layout "
                 "files carry; UE builds its own string at run time, so this exact text is not in the image",
                 "components: " + ", ".join("%s=%s" % kv for kv in parts.items())])
    else:
        ctx.fail("build.engine.revision_string", "missing components: " +
                 ", ".join(k for k, v in parts.items() if v is None))
