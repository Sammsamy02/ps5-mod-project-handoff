#!/usr/bin/env python3
"""pak_vtable_pins.py - offline pin-pass extraction for the UE4SS PS5 pak seam.

Agent HJ (ZCode), 2026-10-10. Read-only static analysis of the preserved
Townfall decrypted eboot (battery-safe; no builds, no console).

What it does (all vaddrs are IMAGE vaddrs of the pinned Townfall image):
  phdrs          dump program headers / vaddr<->fileoff mapping
  rela           dump RELA window [start,end) as reconstructed data words
  xref           scan .text for rip-relative references to a data vaddr
                 (LEA/MOV imm loads; reports store-to-object ctor shape)
  disasm         annotated capstone disassembly of a vaddr window; resolves
                 rip-relative targets, call targets, and tracks simple
                 vtable register defs to label indirect calls [reg+disp]
  slots          reconstruct a vtable from RELA at VADDR for N slots and
                 classify each slot function (forwarder-to-inner detection:
                 a function whose body loads [this+K] then jumps
                 [innervt+SLOT] proves the slot's own offset identity)

The vtable data words are ZERO in the raw decrypted file; real values live
in R_X86_64_RELATIVE relocations (DT_RELA), same reconstruction law as the
GMalloc vtable pass (per-version-table.json fmalloc entry).

Usage:
  python3 pak_vtable_pins.py phdrs   [--image PATH]
  python3 pak_vtable_pins.py rela --start 0x7fee400 --end 0x7feeb00
  python3 pak_vtable_pins.py xref --target 0x7fee8d0 [--image PATH]
  python3 pak_vtable_pins.py disasm --start 0x176ec30 --len 0x300
  python3 pak_vtable_pins.py slots --start 0x7fee8d0 --count 96
"""
import argparse
import bisect
import hashlib
import struct
import sys

import capstone

DEFAULT_IMAGE = "/tmp/eboot-3bc-verify.bin"
PINNED_SHA256 = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"

PHDRS = None
BLOB = None
RELA = None  # sorted list of (r_offset, r_addend) for R_X86_64_RELATIVE
STRINGS = None  # sorted (vaddr, text) ascii strings from rodata scan


def load_image(path, check_hash=False):
    global BLOB, PHDRS
    if BLOB is not None:
        return
    with open(path, "rb") as f:
        BLOB = f.read()
    if check_hash:
        h = hashlib.sha256(BLOB).hexdigest()
        if h != PINNED_SHA256:
            sys.exit(f"REFUSAL: image sha {h} != pinned {PINNED_SHA256}")
    eh = struct.unpack_from("<16sHHIQQQIHHHHHH", BLOB, 0)
    (e_type, e_machine, _v, e_entry, e_phoff, _sh, _f, _e, e_phentsize,
     e_phnum, _s1, _s2, _s3) = eh[1:]
    PHDRS = []
    for i in range(e_phnum):
        (p_type, p_flags, p_off, p_vaddr, _p, p_filesz, p_memsz,
         p_align) = struct.unpack_from("<IIQQQQQQ", BLOB, e_phoff + i * e_phentsize)
        PHDRS.append(dict(i=i, type=p_type, flags=p_flags, off=p_off,
                          vaddr=p_vaddr, filesz=p_filesz, memsz=p_memsz,
                          align=p_align))


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
    global RELA
    if RELA is not None:
        return
    # DT_RELA=0x9653430 DT_RELASZ=0x2607ee8 (phdr8 mapping)
    off = v2o(0x9653430)
    n = 0x2607EE8 // 24
    RELA = []
    raw = BLOB[off:off + 0x2607EE8]
    for i in range(n):
        r_off, r_info, r_add = struct.unpack_from("<QQq", raw, i * 24)
        if (r_info & 0xFFFFFFFF) == 8:  # R_X86_64_RELATIVE
            RELA.append((r_off, r_add))
    RELA.sort()


def rela_at(vaddr):
    if RELA is None:
        load_rela()
    i = bisect.bisect_left(RELA, (vaddr,))
    if i < len(RELA) and RELA[i][0] == vaddr:
        return RELA[i][1]
    return None


def load_strings():
    global STRINGS
    if STRINGS is not None:
        return
    STRINGS = []
    # ascii strings >=5 chars in the R segments (rodata phdr1 + data phdr3)
    for p in PHDRS:
        if p["type"] == 1 and (p["flags"] & 0x2) == 0 and p["filesz"]:
            data = BLOB[p["off"]:p["off"] + p["filesz"]]
            start = None
            for j in range(len(data)):
                c = data[j]
                if 32 <= c < 127:
                    if start is None:
                        start = j
                else:
                    if start is not None and j - start >= 6:
                        STRINGS.append((p["vaddr"] + start,
                                        data[start:j].decode("ascii", "replace")))
                    start = None
    STRINGS.sort()


def string_at(vaddr, maxlen=120):
    load_strings()
    i = bisect.bisect_right(STRINGS, (vaddr, "\xff")) - 1
    if i < 0:
        return None
    start, text = STRINGS[i]
    if start <= vaddr < start + len(text) + 1:
        off = vaddr - start
        s = text[off:off + maxlen]
        return s
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
    if len(out) >= 3:
        return "".join(out)
    return None


def annotate_target(t):
    """Human note for a constant/rip target vaddr."""
    if t is None:
        return ""
    notes = []
    s = string_at(t)
    if s and len(s) >= 4:
        notes.append(f'ascii"{s[:48]}"')
    w = utf16_at(t)
    if w:
        notes.append(f'utf16"{w[:48]}"')
    seg = seg_of(t)
    if seg:
        notes.append(f"ph{seg['i']}")
    a = rela_at(t)
    if a is not None:
        notes.append(f"rela->{a:#x}")
    return ("  ; " + " ".join(notes)) if notes else ""


def cmd_phdrs(args):
    load_image(args.image, args.verify)
    for p in PHDRS:
        print(f"[{p['i']:2}] type={p['type']:#x} off={p['off']:#10x} "
              f"vaddr={p['vaddr']:#10x} filesz={p['filesz']:#10x} "
              f"memsz={p['memsz']:#10x} fl={p['flags']:#x}")


def cmd_rela(args):
    load_image(args.image, args.verify)
    load_rela()
    lo, hi = args.start, args.end
    for off, add in RELA:
        if lo <= off < hi:
            print(f"  {off:#010x} ({off - lo:+#8x})  -> {add:#x}"
                  f"{annotate_target(add)}")


def find_func_end(start, limit=0x400):
    """Linear disasm until ret/jmp-to-reg padding boundary (rough)."""
    md = disassembler()
    o = v2o(start)
    code = BLOB[o:o + limit]
    last = start
    for ins in md.disasm(code, start):
        last = ins.address + ins.size
        if ins.mnemonic in ("ret",) or (ins.mnemonic == "jmp" and "[" in ins.op_str):
            break
    return last


def disassembler():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    return md


def disasm_window(start, length, out=print):
    md = disassembler()
    o = v2o(start)
    code = BLOB[o:o + length]
    regs = {}  # simple def-use: reg -> ('vt', base) or ('imm', val)
    for ins in md.disasm(code, start):
        line = f"  {ins.address:#x}: {ins.mnemonic:<8} {ins.op_str}"
        note = []
        # track mov reg,[reg] (vtable load) and lea/mov imm
        if ins.mnemonic == "mov" and ", " in ins.op_str:
            dst, src = ins.op_str.split(", ", 1)
            ops = ins.operands
            if len(ops) == 2:
                d, s = ops
                if d.type == capstone.x86.X86_OP_REG:
                    rn = ins.reg_name(d.reg)
                    if s.type == capstone.x86.X86_OP_MEM and s.mem.base == capstone.x86.X86_REG_RIP:
                        t = ins.address + ins.size + s.mem.disp
                        regs[rn] = ("imm", t)
                        note.append(f"[{rn}={t:#x}]{annotate_target(t)}")
                    elif s.type == capstone.x86.X86_OP_MEM and s.mem.base != capstone.x86.X86_REG_RIP and s.mem.index == 0 and s.mem.disp == 0:
                        base = ins.reg_name(s.mem.base)
                        if regs.get(base, ("",))[0] == "vt":
                            regs[rn] = ("inner2vt", base)
                        else:
                            regs[rn] = ("deref", base)
                    elif s.type == capstone.x86.X86_OP_IMM:
                        regs[rn] = ("imm", s.imm)
        elif ins.mnemonic == "lea":
            ops = ins.operands
            if len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG:
                rn = ins.reg_name(ops[0].reg)
                if ops[1].type == capstone.x86.X86_OP_MEM and ops[1].mem.base == capstone.x86.X86_REG_RIP:
                    t = ins.address + ins.size + ops[1].mem.disp
                    regs[rn] = ("imm", t)
                    note.append(f"[{rn}={t:#x}]{annotate_target(t)}")
        # rip-relative mem operand annotation + vtable/inner tracking
        for op in ins.operands:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                if not note:
                    note.append(f"[rip->{t:#x}]{annotate_target(t)}")
                # store of imm reg into data target => vtable publish
                for rn, val in regs.items():
                    if val[0] == "imm" and val[1] == t and ins.mnemonic == "mov" and ins.op_str.startswith(f"[{rn}]") is False and f"], {rn}" in ins.op_str:
                        note.append(f"<STORE {rn}={t:#x}>")
        if ins.mnemonic in ("call", "jmp"):
            op = ins.operands[0] if ins.operands else None
            if op is not None and op.type == capstone.x86.X86_OP_IMM:
                t = op.imm
                note.append(f"-> {t:#x}{annotate_target(t)}")
                regs.clear()
            elif op is not None and op.type == capstone.x86.X86_OP_REG:
                note.append(f"-> {ins.reg_name(op.reg)}")
                regs.clear()
            elif op is not None and op.type == capstone.x86.X86_OP_MEM:
                m = op.mem
                base = ins.reg_name(m.base) if m.base else ""
                disp = m.disp
                # vtable call through tracked register?
                tag = ""
                if base in regs and regs[base][0] == "imm":
                    vt = regs[base][1]
                    a = rela_at(vt + disp)
                    if a is not None:
                        tag = f" VT[{vt:#x}+{disp:+#x}] -> {a:#x}"
                    else:
                        tag = f" VT[{vt:#x}+{disp:+#x}]"
                if base in regs and regs[base][0] == "inner2vt":
                    tag = f" inner-vt +{disp:+#x}"
                note.append(f"call [{base}{f'{disp:+#x}' if disp else ''}]{tag}")
                regs.clear()
        out(line + ("   ;" + " ".join(note) if note else ""))


def cmd_disasm(args):
    load_image(args.image, args.verify)
    disasm_window(args.start, args.len)


def cmd_xref(args):
    """Scan executable segment for rip-relative LEA/MOV of --target."""
    load_image(args.image, args.verify)
    target = args.target
    md = disassembler()
    text = None
    for p in PHDRS:
        if p["type"] == 1 and (p["flags"] & 1):
            text = p
    code = BLOB[text["off"]:text["off"] + text["filesz"]]
    hits = []
    for ins in md.disasm(code, text["vaddr"]):
        if ins.mnemonic not in ("lea", "mov", "cmp", "push"):
            continue
        for op in ins.operands:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                if t == target:
                    hits.append((ins.address, ins.mnemonic, ins.op_str))
    print(f"xrefs to {target:#x}: {len(hits)}")
    for a, m, o in hits[:200]:
        print(f"  {a:#x}: {m} {o}")


def classify_forwarder(func_vaddr, window=0x60):
    """Detect 'forwards to Inner at same slot' bodies:
    loads [this+K] (K usually 8), then its vtable, then jmp [vt+disp].
    Returns (inner_member_off, slot_off, tail_mnemonic) or Nones."""
    md = disassembler()
    o = v2o(func_vaddr)
    if o is None:
        return (None, None, None)
    code = BLOB[o:o + window]
    inner_off = None
    slot_off = None
    tail = None
    for ins in md.disasm(code, func_vaddr):
        ops = ins.operands
        if ins.mnemonic == "mov" and len(ops) == 2 and ops[0].type == capstone.x86.X86_OP_REG:
            if ops[1].type == capstone.x86.X86_OP_MEM and ops[1].mem.base == capstone.x86.X86_REG_RDI and ops[1].mem.index == 0:
                inner_off = ops[1].mem.disp
        if ins.mnemonic in ("jmp", "call") and ops and ops[0].type == capstone.x86.X86_OP_MEM:
            m = ops[0].mem
            if m.index == 0 and m.base != 0:
                slot_off = m.disp
                tail = ins.mnemonic
                break
        if ins.mnemonic == "ret":
            break
    return (inner_off, slot_off, tail)


def cmd_slots(args):
    load_image(args.image, args.verify)
    load_rela()
    for i in range(args.count):
        slot_v = args.start + i * 8
        add = rela_at(slot_v)
        if add is None:
            print(f"  +{i*8:#06x} @{slot_v:#x}: NULL")
            continue
        cls = classify_forwarder(add)
        fwd = ""
        if cls and cls[0] is not None and cls[1] is not None and cls[2] == "jmp":
            same = "SAME-OFFSET" if cls[1] == i * 8 else f"off{cls[1]:+#x}"
            fwd = f"  forwarder(inner@{cls[0]:+#x}, slot {same}, tail jmp)"
        elif cls and cls[0] is not None and cls[1] is not None:
            fwd = f"  calls[inner+{cls[1]:+#x}] via inner@{cls[0]:+#x} ({cls[2]})"
        print(f"  +{i*8:#06x} @{slot_v:#x}: {add:#x}{fwd}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=DEFAULT_IMAGE)
    ap.add_argument("--verify", action="store_true",
                    help="fail-closed pinned-sha check before analysis")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("phdrs")
    p = sub.add_parser("rela")
    p.add_argument("--start", type=lambda x: int(x, 0), required=True)
    p.add_argument("--end", type=lambda x: int(x, 0), required=True)
    p = sub.add_parser("xref")
    p.add_argument("--target", type=lambda x: int(x, 0), required=True)
    p = sub.add_parser("disasm")
    p.add_argument("--start", type=lambda x: int(x, 0), required=True)
    p.add_argument("--len", type=lambda x: int(x, 0), default=0x100)
    p = sub.add_parser("slots")
    p.add_argument("--start", type=lambda x: int(x, 0), required=True)
    p.add_argument("--count", type=int, default=96)
    args = ap.parse_args()
    {"phdrs": cmd_phdrs, "rela": cmd_rela, "disasm": cmd_disasm,
     "xref": cmd_xref, "slots": cmd_slots}[args.cmd](args)


if __name__ == "__main__":
    main()
