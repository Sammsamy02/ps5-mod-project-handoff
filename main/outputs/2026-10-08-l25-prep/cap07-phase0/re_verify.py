#!/usr/bin/env python3
"""re_verify.py - independent re-derivation of the Phase-0 pak-launch facts.

Read-only static analysis of the pinned Townfall decrypted eboot
(UE 5.6.1, sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc).
No builds, no console, no file edits elsewhere. All vaddrs are IMAGE vaddrs.

Mapping facts given to us (verified below):
  file offset = vaddr + 0x4000      for text/rodata/RELRO (ph0/ph1/ph3)
  DT_RELA = 0x9653430, size 0x2607ee8 (R_X86_64_RELATIVE pointer words)
  ph8 fileoff = vaddr - 0x324000 (holds RELA + dynsym)
  vtable pointer words are zero on disk; real values are the RELA addends.

Commands:
  verify                 sha + phdr map sanity
  chain                  Q1: Get()/GetPlatformFile()/accessors + SetPlatformFile writers
  vtslots VADDR [N]      reconstruct a vtable's N slots from RELA with impl shapes
  pakvt                  Q2: FPakPlatformFile vtable slot-by-slot shape
  physvt                 Q2: physical platform-file vtable 0x7ebc6a0 slots
  wrappers               Q2: every vtable with GetLowerLevel 0xb3500 at +0x68
  iostore                Q3: .utoc/.ucas/IoStore strings, xrefs, open path
  bootorder              Q4: callers of Initialize 0x1769ee0 + container mount
  disasm VADDR LEN       annotated disasm window
  xref TARGET            rip-relative refs to a data vaddr
"""
import bisect
import hashlib
import struct
import sys

import capstone

IMAGE = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
PINNED_SHA256 = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
RELA_VADDR = 0x9653430
RELA_SIZE = 0x2607EE8

BLOB = None
PHDRS = None
RELA = None          # sorted list of (r_offset, r_addend) for R_X86_64_RELATIVE
RELA_OFFS = None     # sorted list of r_offset only (for bisect)
STRINGS = None


def load():
    global BLOB, PHDRS
    if BLOB is not None:
        return
    with open(IMAGE, "rb") as f:
        BLOB = f.read()
    h = hashlib.sha256(BLOB).hexdigest()
    if h != PINNED_SHA256:
        sys.exit(f"REFUSAL: image sha {h} != pinned {PINNED_SHA256}")
    e_phoff = struct.unpack_from("<Q", BLOB, 0x20)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", BLOB, 0x36)
    PHDRS = []
    for i in range(e_phnum):
        (p_type, p_flags, p_off, p_vaddr, _pp, p_filesz, p_memsz,
         p_align) = struct.unpack_from("<IIQQQQQQ", BLOB, e_phoff + i * e_phentsize)
        PHDRS.append(dict(i=i, type=p_type, flags=p_flags, off=p_off,
                          vaddr=p_vaddr, filesz=p_filesz, memsz=p_memsz))


def v2o(vaddr):
    for p in PHDRS:
        if p["type"] == 1 and p["vaddr"] <= vaddr < p["vaddr"] + p["filesz"]:
            return p["off"] + (vaddr - p["vaddr"])
    return None


def seg_of(vaddr):
    for p in PHDRS:
        if p["vaddr"] <= vaddr < p["vaddr"] + p["memsz"]:
            return p
    return None


def load_rela():
    global RELA, RELA_OFFS
    if RELA is not None:
        return
    off = v2o(RELA_VADDR)
    if off is None:
        # ph8 special mapping: fileoff = vaddr - 0x324000
        off = RELA_VADDR - 0x324000
    raw = BLOB[off:off + RELA_SIZE]
    RELA = []
    for i in range(RELA_SIZE // 24):
        r_off, r_info, r_add = struct.unpack_from("<QQq", raw, i * 24)
        if (r_info & 0xFFFFFFFF) == 8:  # R_X86_64_RELATIVE
            RELA.append((r_off, r_add))
    RELA.sort()
    RELA_OFFS = [x[0] for x in RELA]


def rela_at(vaddr):
    load_rela()
    i = bisect.bisect_left(RELA_OFFS, vaddr)
    if i < len(RELA) and RELA[i][0] == vaddr:
        return RELA[i][1]
    return None


def load_strings():
    global STRINGS
    if STRINGS is not None:
        return
    STRINGS = []
    for p in PHDRS:
        if p["type"] == 1 and (p["flags"] & 0x2) == 0 and p["filesz"]:
            data = BLOB[p["off"]:p["off"] + p["filesz"]]
            start = None
            for j, c in enumerate(data):
                if 32 <= c < 127:
                    if start is None:
                        start = j
                else:
                    if start is not None and j - start >= 5:
                        STRINGS.append((p["vaddr"] + start,
                                        data[start:j].decode("ascii", "replace")))
                    start = None
    STRINGS.sort()


def string_at(vaddr, maxlen=96):
    load_strings()
    i = bisect.bisect_right(STRINGS, (vaddr, "\xff")) - 1
    if i < 0:
        return None
    start, text = STRINGS[i]
    if start <= vaddr < start + len(text) + 1:
        return text[vaddr - start:vaddr - start + maxlen]
    return None


def utf16_at(vaddr, maxlen=64):
    o = v2o(vaddr)
    if o is None:
        return None
    out = []
    for j in range(maxlen):
        w = struct.unpack_from("<H", BLOB, o + j * 2)[0]
        if w == 0:
            break
        if 32 <= w < 127:
            out.append(chr(w))
        else:
            return None
    return "".join(out) if len(out) >= 2 else None


def disassembler():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    return md


def annotate(t):
    if t is None:
        return ""
    notes = []
    s = string_at(t)
    if s and len(s) >= 3:
        notes.append(f'a"{s[:40]}"')
    w = utf16_at(t)
    if w:
        notes.append(f'u"{w[:40]}"')
    seg = seg_of(t)
    if seg:
        notes.append(f"ph{seg['i']}")
    a = rela_at(t)
    if a is not None:
        notes.append(f"rela->{a:#x}")
    return ("  ; " + " ".join(notes)) if notes else ""


def bytes_at(vaddr, n):
    o = v2o(vaddr)
    return BLOB[o:o + n]


def hexb(vaddr, n):
    return " ".join(f"{b:02x}" for b in bytes_at(vaddr, n))


def disasm_lines(start, length, track=True):
    """Yield (address, mnemonic, op_str, note) with rip/vtable annotation."""
    md = disassembler()
    o = v2o(start)
    code = BLOB[o:o + length]
    regs = {}
    for ins in md.disasm(code, start):
        note = []
        ops = ins.operands
        if ins.mnemonic == "lea" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG:
            if ops[1].type == capstone.x86.X86_OP_MEM and ops[1].mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + ops[1].mem.disp
                regs[ins.reg_name(ops[0].reg)] = t
                note.append(f"[={t:#x}]{annotate(t)}")
        elif ins.mnemonic == "mov" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG:
            s = ops[1]
            rn = ins.reg_name(ops[0].reg)
            if s.type == capstone.x86.X86_OP_MEM and s.mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + s.mem.disp
                regs[rn] = t
                note.append(f"[={t:#x}]{annotate(t)}")
            else:
                regs.pop(rn, None)
        # rip-relative store detection + general rip annotation
        for op in ops:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                if not note:
                    note.append(f"[rip->{t:#x}]{annotate(t)}")
        if ins.mnemonic in ("call", "jmp") and ops:
            op = ops[0]
            if op.type == capstone.x86.X86_OP_IMM:
                note.append(f"-> {op.imm:#x}{annotate(op.imm)}")
            elif op.type == capstone.x86.X86_OP_MEM:
                m = op.mem
                base = ins.reg_name(m.base) if m.base else ""
                tag = ""
                if base in regs:
                    a = rela_at(regs[base] + m.disp)
                    tag = f" VT[{regs[base]:#x}+{m.disp:#x}]" + (f"->{a:#x}" if a else "")
                note.append(f"ind [{base}{m.disp:+#x}]{tag}")
            regs.clear()
        yield (ins.address, ins.mnemonic, ins.op_str, " ".join(note))


def cmd_disasm(start, length=0x100):
    load()
    load_rela()
    for a, m, o, n in disasm_lines(start, length):
        print(f"  {a:#x}: {m:<7} {o}" + (f"   ; {n}" if n else ""))


RIP_MODRM = {0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D}
REX = {0x48, 0x49, 0x4C, 0x4D, 0x44, 0x40, 0x41, 0x45}


def fast_rip_refs(target):
    """Byte-scan all executable segments for rip-relative lea/mov to target.
    Returns list of (vaddr, kind, reg_field) where kind in
    {'lea','mov_load','mov_store','movimm_store','cmp'}.
    Instruction forms handled: [REX] 8D/8B/89/C7/39/3B modrm(rip) [disp32][imm]."""
    load()
    OPC = {0x8D: "lea", 0x8B: "mov_load", 0x89: "mov_store",
           0xC7: "movimm_store", 0x39: "cmp", 0x3B: "cmp", 0x63: "movsxd"}
    hits = []
    for p in PHDRS:
        if not (p["type"] == 1 and (p["flags"] & 1)):
            continue
        data = BLOB[p["off"]:p["off"] + p["filesz"]]
        base = p["vaddr"]
        n = len(data)
        i = 0
        while i < n - 7:
            j = i
            rex = 0
            if data[j] in REX:
                rex = data[j]
                j += 1
            opc = data[j] if j < n else 0
            if opc in OPC and j + 1 < n and data[j + 1] in RIP_MODRM:
                modrm = data[j + 1]
                reg = ((modrm >> 3) & 7) | (0x8 if rex & 0x4 else 0)
                disp_off = j + 2
                if disp_off + 4 <= n:
                    disp = struct.unpack_from("<i", data, disp_off)[0]
                    ilen = disp_off + 4 - i
                    if opc == 0xC7:
                        ilen += 4  # imm32
                    tgt = base + i + ilen + disp
                    if tgt == target:
                        hits.append((base + i, OPC[opc], reg, ilen))
            i += 1
    return hits


def cmd_stores(target):
    """Report rip-relative stores/refs to target (fast)."""
    load()
    load_rela()
    refs = fast_rip_refs(target)
    kinds = {}
    for _, k, _, _ in refs:
        kinds[k] = kinds.get(k, 0) + 1
    print(f"fast rip-relative refs to {target:#x}: {len(refs)}  {kinds}")
    stores = [r for r in refs if "store" in r[1]]
    print(f"direct STORES (mov [rip->{target:#x}], ...): {len(stores)}")
    for a, k, reg, ilen in stores:
        fn = func_start_of(a)
        print(f"  store@{a:#x} kind={k} fn~{fn:#x}  bytes={hexb(a, ilen)}")
        for la, lm, lo, ln in disasm_lines(a, 0x10):
            print(f"      {la:#x}: {lm} {lo}" + (f"   ; {ln}" if ln else ""))
            break
    # also show non-store refs grouped (lea = address-taken, likely Set via Get())
    leas = [r for r in refs if r[1] == "lea"]
    print(f"\nLEA (address-taken) sites: {len(leas)} (first 40)")
    for a, k, reg, ilen in leas[:40]:
        print(f"  lea@{a:#x} fn~{func_start_of(a):#x}")
    return refs


def cmd_getcallers(getfn=0xB8ED0, span=0x40):
    """Find E8 callers of Get(); classify what each does with the returned
    pointer (rax): store into [rax+0]/[rax+8], or pass as this (rdi) to a call
    => SetPlatformFile-class chain builders."""
    load()
    load_rela()
    callers = find_call_sites(getfn)
    print(f"Get() {getfn:#x} direct call sites: {len(callers)}")
    md = disassembler()
    builders = []
    for a in callers:
        # disasm after the call for `span` bytes, track rax/rdi provenance
        o = v2o(a)
        code = BLOB[o + 5:o + 5 + span]
        rax_is_mgr = True  # right after call, rax holds mgr
        rdi_is_mgr = False
        verdict = None
        for ins in md.disasm(code, a + 5):
            ops = ins.operands
            m = ins.mnemonic
            # track mov rdi, rax
            if m == "mov" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG and ops[1].type == capstone.x86.X86_OP_REG:
                dn = md.reg_name(ops[0].reg); sn = md.reg_name(ops[1].reg)
                if dn == "rdi" and sn == "rax":
                    rdi_is_mgr = True
                elif dn == "rax":
                    rax_is_mgr = False
                elif dn == "rdi":
                    rdi_is_mgr = False
            # store into [rax] / [rax+8] or [rdi]/[rdi+8]
            if m == "mov" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_MEM and ops[0].mem.index == 0:
                bn = md.reg_name(ops[0].mem.base) if ops[0].mem.base else ""
                if (bn == "rax" and rax_is_mgr) or (bn == "rdi" and rdi_is_mgr):
                    if ops[0].mem.disp in (0, 8):
                        verdict = f"STORE [{bn}+{ops[0].mem.disp:#x}] <- {ins.op_str.split(',')[-1].strip()}"
                        break
            # call with rdi=mgr to a NON-accessor fn => possible SetPlatformFile
            if m == "call" and ops and ops[0].type == capstone.x86.X86_OP_IMM and rdi_is_mgr:
                if ops[0].imm not in (0xB8A70, 0xB8AA0, 0xB8ED0):
                    verdict = f"call(this=mgr) -> {ops[0].imm:#x}"
                break
            if m in ("ret", "jmp"):
                break
        if verdict:
            builders.append((a, verdict))
    print(f"chain-building call sites (store into mgr or SetPlatformFile call): {len(builders)}")
    for a, v in builders:
        print(f"  @{a:#x} fn~{func_start_of(a):#x}: {v}")
    return builders


def cmd_xref(target, mnems=("lea", "mov", "cmp", "push"), limit=300):
    """Scan all executable segments for rip-relative refs to target."""
    load()
    md = disassembler()
    hits = []
    for p in PHDRS:
        if not (p["type"] == 1 and (p["flags"] & 1)):
            continue
        code = BLOB[p["off"]:p["off"] + p["filesz"]]
        for ins in md.disasm(code, p["vaddr"]):
            if ins.mnemonic not in mnems:
                continue
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    if ins.address + ins.size + op.mem.disp == target:
                        hits.append((ins.address, ins.mnemonic, ins.op_str))
    print(f"xrefs to {target:#x}: {len(hits)}")
    for a, m, o in hits[:limit]:
        print(f"  {a:#x}: {m} {o}")
    return hits


def classify_slot(func_vaddr, window=0x50):
    """Return a short shape string for a vtable impl."""
    if func_vaddr is None:
        return "NULL"
    md = disassembler()
    o = v2o(func_vaddr)
    if o is None:
        return "(off-image)"
    code = BLOB[o:o + window]
    insns = list(md.disasm(code, func_vaddr))
    if not insns:
        return "(undisasm)"
    # tiny stub detection
    first = insns[0]
    raw = " ".join(f"{b:02x}" for b in bytes_at(func_vaddr, 8))
    inner_off = None
    for ins in insns:
        if ins.mnemonic == "mov" and ins.operands and len(ins.operands) == 2:
            d, s = ins.operands
            if (d.type == capstone.x86.X86_OP_REG and
                    s.type == capstone.x86.X86_OP_MEM and
                    s.mem.base == capstone.x86.X86_REG_RDI and s.mem.index == 0):
                inner_off = s.mem.disp
        if ins.mnemonic in ("jmp", "call") and ins.operands:
            op = ins.operands[0]
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base and op.mem.index == 0:
                inner = f"inner[+{inner_off:#x}]" if inner_off is not None else f"[{md.reg_name(op.mem.base)}]"
                return f"forwarder {inner}->[vt{op.mem.disp:+#x}] ({ins.mnemonic})"
            if op.type == capstone.x86.X86_OP_IMM:
                return f"tail-> {op.imm:#x}{annotate(op.imm)}"
        if ins.mnemonic == "ret":
            # summarize preceding
            pre = "; ".join(f"{i.mnemonic} {i.op_str}" for i in insns[:4])
            return f"leaf [{pre}] ret"
    pre = "; ".join(f"{i.mnemonic} {i.op_str}" for i in insns[:4])
    return f"[{pre} ...]"


def reconstruct_vtable(vtable_vaddr, count):
    """Return list of (slot_off, slot_vaddr, impl_addend)."""
    load_rela()
    out = []
    for i in range(count):
        sv = vtable_vaddr + i * 8
        out.append((i * 8, sv, rela_at(sv)))
    return out


def cmd_vtslots(vtable_vaddr, count=74):
    load()
    load_rela()
    print(f"vtable @ {vtable_vaddr:#x}: offset-to-top={rela_at(vtable_vaddr-0x10)}, "
          f"typeinfo={rela_at(vtable_vaddr-0x8)} (header words)")
    for off, sv, impl in reconstruct_vtable(vtable_vaddr, count):
        shape = classify_slot(impl)
        print(f"  +{off:#06x} @{sv:#x}: {impl:#x}" if impl else f"  +{off:#06x} @{sv:#x}: NULL"
              + (f"   {shape}" if impl else ""))
        if impl:
            print(f"            {shape}")


def count_vtable_slots(vtable_vaddr, maxn=200):
    """Count consecutive non-null RELA pointer slots into text."""
    load_rela()
    n = 0
    for i in range(maxn):
        a = rela_at(vtable_vaddr + i * 8)
        if a is None:
            break
        n += 1
    return n


# ----- Q1: platform-file chain -----
def cmd_chain():
    load()
    load_rela()
    print("=== Q1: FPlatformFileManager chain ===\n")
    print("--- Get() @0xb8ed0 ---")
    cmd_disasm(0xB8ED0, 0x60)
    print("\n--- GetPlatformFile() @0xb8a70 ---")
    cmd_disasm(0xB8A70, 0x40)
    print("\n--- second accessor @0xb8aa0 ---")
    cmd_disasm(0xB8AA0, 0x50)
    print("\n--- lazy default-platform-file builder @0x4ca0 ---")
    cmd_disasm(0x4CA0, 0x80)
    print("\n--- guard byte 0x932a438 / singleton 0x932a428 segment ---")
    for v in (0x932A428, 0x932A438):
        seg = seg_of(v)
        print(f"  {v:#x}: {'ph%d'%seg['i'] if seg else '??'} "
              f"disk={hexb(v,8) if v2o(v) else '(nofile/bss)'} rela->{rela_at(v)}")
    print("\n(run 'stores 0x932a428' separately for SetPlatformFile writers)")


def find_stores_to(obj_vaddr):
    """Find functions that LEA obj_vaddr then store a register into [reg] or [reg+8]."""
    load()
    md = disassembler()
    results = []
    for p in PHDRS:
        if not (p["type"] == 1 and (p["flags"] & 1)):
            continue
        code = BLOB[p["off"]:p["off"] + p["filesz"]]
        reg_has = {}  # regname -> address where loaded
        for ins in md.disasm(code, p["vaddr"]):
            ops = ins.operands
            if ins.mnemonic == "lea" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG:
                if ops[1].type == capstone.x86.X86_OP_MEM and ops[1].mem.base == capstone.x86.X86_REG_RIP:
                    t = ins.address + ins.size + ops[1].mem.disp
                    rn = md.reg_name(ops[0].reg)
                    if t == obj_vaddr:
                        reg_has[rn] = ins.address
                    else:
                        reg_has.pop(rn, None)
            elif ins.mnemonic == "mov" and len(ops) == 2:
                d, s = ops
                # store into [reg] or [reg+8] where reg holds obj
                if d.type == capstone.x86.X86_OP_MEM and d.mem.base and d.mem.index == 0:
                    bn = md.reg_name(d.mem.base)
                    if bn in reg_has and d.mem.disp in (0, 8):
                        src = s
                        srcname = md.reg_name(src.reg) if src.type == capstone.x86.X86_OP_REG else ins.op_str.split(",")[-1].strip()
                        results.append((reg_has[bn], ins.address, d.mem.disp, ins.mnemonic, ins.op_str))
                # also absolute rip store mov [rip+x], reg captured elsewhere
                if d.type == capstone.x86.X86_OP_REG:
                    rn = md.reg_name(d.reg)
                    # overwrite of the reg loses the lea tag unless same
                    if rn in reg_has and not (s.type == capstone.x86.X86_OP_MEM):
                        reg_has.pop(rn, None)
    print(f"  stores into [{obj_vaddr:#x}+0/+8] via LEA'd register: {len(results)}")
    for lea_a, st_a, disp, m, o in results:
        fn = func_start_of(st_a)
        print(f"    store@{st_a:#x} (+{disp:#x}) lea@{lea_a:#x} fn~{fn:#x}: {m} {o}")
    return results


def func_start_of(addr, back=0x600):
    """Crude: scan backward for a likely prologue (push rbp / endbr64 / ret+1)."""
    load()
    o = v2o(addr)
    if o is None:
        return addr
    # look back for 0xc3 (ret) boundary then next insn start, or endbr64 f3 0f 1e fa
    base = max(0, o - back)
    window = BLOB[base:o + 1]
    # find last endbr64
    idx = window.rfind(b"\xf3\x0f\x1e\xfa")
    if idx >= 0:
        return addr - (o - (base + idx))
    idx = window.rfind(b"\xc3")
    if idx >= 0 and idx + 1 < len(window):
        return addr - (o - (base + idx + 1))
    return addr


# ----- Q3: IoStore -----
def find_utf16_strings(needles):
    """Scan rodata for UTF-16LE occurrences of needles; return {needle:[vaddrs]}."""
    load()
    res = {n: [] for n in needles}
    for p in PHDRS:
        if p["type"] != 1 or not p["filesz"]:
            continue
        data = BLOB[p["off"]:p["off"] + p["filesz"]]
        for n in needles:
            pat = n.encode("utf-16-le")
            i = 0
            while True:
                j = data.find(pat, i)
                if j < 0:
                    break
                res[n].append(p["vaddr"] + j)
                i = j + 2
    return res


def find_ascii_strings(needles):
    load()
    res = {n: [] for n in needles}
    for p in PHDRS:
        if p["type"] != 1 or not p["filesz"]:
            continue
        data = BLOB[p["off"]:p["off"] + p["filesz"]]
        for n in needles:
            pat = n.encode()
            i = 0
            while True:
                j = data.find(pat, i)
                if j < 0:
                    break
                res[n].append(p["vaddr"] + j)
                i = j + 1
    return res


def cmd_iostore():
    load()
    load_rela()
    print("=== Q3: IoStore / .utoc / .ucas strings ===\n")
    u = find_utf16_strings([".utoc", ".ucas", "global", "IoStore", "FileIoStore", "_s"])
    for k, vs in u.items():
        print(f"  u'{k}': {len(vs)} hits: " + ", ".join(f"{v:#x}" for v in vs[:12]))
    a = find_ascii_strings(["FileIoStore", "IoStore", ".ucas", ".utoc"])
    for k, vs in a.items():
        print(f"  a'{k}': {len(vs)} hits: " + ", ".join(f"{v:#x}" for v in vs[:12]))


def cmd_bootorder():
    load()
    print("=== Q4: Initialize 0x1769ee0 callers ===\n")
    callers = find_call_sites(0x1769EE0)
    print(f"  direct E8 call sites to 0x1769ee0: {len(callers)}")
    for a in callers[:40]:
        print(f"    {a:#x} (fn~{func_start_of(a):#x})")


def find_call_sites(target):
    """Find direct relative E8 call sites to target across text."""
    load()
    md = disassembler()
    hits = []
    for p in PHDRS:
        if not (p["type"] == 1 and (p["flags"] & 1)):
            continue
        code = BLOB[p["off"]:p["off"] + p["filesz"]]
        # fast scan: only disasm around E8 bytes
        start = 0
        while True:
            j = code.find(b"\xe8", start)
            if j < 0:
                break
            start = j + 1
            if j + 5 > len(code):
                break
            rel = struct.unpack_from("<i", code, j + 1)[0]
            tgt = p["vaddr"] + j + 5 + rel
            if tgt == target:
                hits.append(p["vaddr"] + j)
    return hits


MAIN = {
    "verify": lambda a: (load(), load_rela(), print("sha OK;",
             f"RELA entries={len(RELA)}; phdrs={len(PHDRS)}")),
    "chain": lambda a: cmd_chain(),
    "pakvt": lambda a: cmd_vtslots(0x7FEE8C0, 74),
    "physvt": lambda a: cmd_vtslots(0x7EBC6A0, count_vtable_slots(0x7EBC6A0)),
    "iostore": lambda a: cmd_iostore(),
    "bootorder": lambda a: cmd_bootorder(),
}


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "verify"
    if cmd == "vtslots":
        load(); load_rela()
        cmd_vtslots(int(sys.argv[2], 0), int(sys.argv[3], 0) if len(sys.argv) > 3 else 74)
    elif cmd == "disasm":
        load(); load_rela()
        cmd_disasm(int(sys.argv[2], 0), int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x100)
    elif cmd == "xref":
        cmd_xref(int(sys.argv[2], 0))
    elif cmd == "stores":
        cmd_stores(int(sys.argv[2], 0))
    elif cmd == "getcallers":
        cmd_getcallers(int(sys.argv[2], 0) if len(sys.argv) > 2 else 0xB8ED0)
    elif cmd == "wrappers":
        cmd_wrappers()
    elif cmd == "callers":
        load()
        for a in find_call_sites(int(sys.argv[2], 0)):
            print(f"  {a:#x} fn~{func_start_of(a):#x}")
    elif cmd in MAIN:
        MAIN[cmd](sys.argv[2:])
    else:
        print(__doc__)


def cmd_wrappers():
    """Q2: find every vtable whose +0x68 slot impl == 0xb3500 (GetLowerLevel)."""
    load()
    load_rela()
    GLL = 0xB3500
    # all RELA offsets landing in RELRO/data that equal 0xb3500 are slot +0x68 candidates
    SLL = 0xB3510
    loose, strict = [], []
    for off, add in RELA:
        if add == GLL:
            vt = off - 0x68
            s0 = rela_at(vt)
            if s0 and 0x1000 <= s0 < 0x7479B1C:
                loose.append(vt)
                if rela_at(vt + 0x70) == SLL:   # paired SetLowerLevel
                    strict.append(vt)
    print(f"vtables with GetLowerLevel 0xb3500 at +0x68 (loose): {len(set(loose))}")
    print(f"  of those with SetLowerLevel 0xb3510 at +0x70 (IPlatformFile wrappers): {len(set(strict))}\n")
    for vt in sorted(set(strict)):
        s0 = rela_at(vt)
        gtn = rela_at(vt + 0x78)   # GetTypeName
        name = read_literal_return(gtn) if gtn else None
        gtn_s = f"{gtn:#x}" if gtn else "NULL"
        print(f"  vt {vt:#x}: dtor={s0:#x} +0x78(GetTypeName)={gtn_s} typename={name!r}")


def slot0_str(vt):
    a = rela_at(vt)
    return f"{a:#x}" if a else "NULL"


def read_literal_return(func_vaddr, window=0x30):
    """If impl is 'lea rax,[rip+X]; ret' return the utf16/ascii string at X."""
    md = disassembler()
    o = v2o(func_vaddr)
    if o is None:
        return None
    code = BLOB[o:o + window]
    for ins in md.disasm(code, func_vaddr):
        if ins.mnemonic == "lea" and ins.operands:
            op = ins.operands[1]
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                return utf16_at(t) or string_at(t)
        if ins.mnemonic == "ret":
            break
    return None


if __name__ == "__main__":
    main()
