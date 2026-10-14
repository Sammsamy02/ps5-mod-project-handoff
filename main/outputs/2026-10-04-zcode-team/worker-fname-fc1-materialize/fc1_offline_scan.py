#!/usr/bin/env python3
"""FC-1 calls_by_anchor offline materialization (worker-fname-fc1-materialize).

Faithful offline port of the UNMODIFIED survey resolver, used only because the
native binary (fc1_calls_by_anchor) wedges in-kernel under current host memory
pressure (swap 11.2/12.3GB used; multiple runs stuck uninterruptible inside
fname_call::resolve). READ-ONLY: mmap PROT_READ, no writes, no network.
Contract mirrored from:
  .superpowers/native-dependency-spike/ue4ss-native-tick-relocation-resolver-1/
    crate/src/fname_call.rs  (resolve_with_ranges, call_after)
    crate/src/xrefs.rs       (scan_xrefs, targets_with_indirection)
"""
import sys, json, hashlib, mmap, struct
import capstone as cs
from capstone.x86_const import (X86_REG_RIP, X86_REG_RSP, X86_REG_RBP,
    X86_GRP_JUMP, X86_GRP_RET, X86_GRP_INT, X86_GRP_IRET)

CAP = sys.argv[1]
NAMES = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]  # fname_call.rs order
TARGET_EXPECT = 0x23D590
LOAD0_RUNTIME_BASE = 0x400000
MAX_STRINGS, MAX_XREFS, MAX_CALLS = 64, 16384, 4096

f = open(CAP, 'rb')
mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
N = len(mm)

# --- PT_LOAD map (loader semantics mirror fc1 main.rs loads()) ---
phoff = struct.unpack_from('<Q', mm, 0x20)[0]
phentsize, phnum = struct.unpack_from('<HH', mm, 0x36)
LOADS = []
for i in range(phnum):
    p = phoff + i * phentsize
    ptype = struct.unpack_from('<I', mm, p)[0]
    flags = struct.unpack_from('<I', mm, p + 4)[0]
    off, va = struct.unpack_from('<QQ', mm, p + 8)
    filesz = struct.unpack_from('<Q', mm, p + 32)[0]
    if ptype == 1:
        LOADS.append(dict(off=off, va=va, filesz=filesz, flags=flags, exec=bool(flags & 1)))

def off2va(o):
    for l in LOADS:
        if l['off'] <= o < l['off'] + l['filesz']:
            return l['va'] + (o - l['off'])
    return None
def va2slice(va, n):
    for l in LOADS:
        if l['va'] <= va < l['va'] + l['filesz']:
            o = l['off'] + (va - l['va'])
            if o + n <= l['off'] + l['filesz']:
                return mm[o:o + n]
    return None
def in_exec(va):
    return any(l['exec'] and l['va'] <= va < l['va'] + l['filesz'] for l in LOADS)

def find_all(needle):
    out, i = [], mm.find(needle)
    while i != -1:
        out.append(i)
        i = mm.find(needle, i + 1)
    return out

# --- scan_xrefs (xrefs.rs) ---
LEA_COMBOS = [(rex, modrm) for rex in (0x48, 0x4C) for modrm in (0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D)]

def scan_lea_riprel(target):
    refs = []
    for rex, modrm in LEA_COMBOS:
        pre = bytes((rex, 0x8D, modrm))
        i = mm.find(pre)
        while i != -1:
            if i + 7 <= N:
                disp = struct.unpack_from('<i', mm, i + 3)[0]
                va = off2va(i)
                if va is not None and va + 7 + disp == target:
                    refs.append(va)
            i = mm.find(pre, i + 1)
    return refs

def scan_mov_imm32(target):
    refs = []
    if target <= 0xFFFFFFFF:
        tgt = struct.pack('<I', target)
        for op in range(0xB8, 0xC0):
            i = mm.find(bytes((op,)) + tgt)
            while i != -1:
                va = off2va(i)
                if va is not None:
                    refs.append(va)
                i = mm.find(bytes((op,)) + tgt, i + 1)
    return refs

# --- capstone setup ---
md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
md.detail = True
FAM_NAMES = {'RSI': {'rsi', 'esi', 'si', 'sil'},
             'RDI': {'rdi', 'edi', 'di', 'dil'},
             'RDX': {'rdx', 'edx', 'dx', 'dl'}}

def writes_reg(ins, fam):
    try:
        _, wr = ins.regs_access()
    except Exception:
        return False
    return any(md.reg_name(r) in FAM_NAMES[fam] for r in wr)

def decode_at(va, nbytes=64):
    b = va2slice(va, nbytes)
    if b is None:
        return None
    return list(md.disasm(b, va))

def call_after(ref, strings, code_ranges):
    """Mirror fname_call.rs call_after. Returns (call_ip, target, instrs) or None."""
    if not any(lo <= ref < hi for lo, hi in code_ranges):
        return None
    ins_list = decode_at(ref, 64)  # resolver decodes at most 64 bytes from the reference
    if not ins_list:
        return None
    first = ins_list[0]
    if first.mnemonic != 'lea' or len(first.operands) != 2:
        return None
    o0, o1 = first.operands
    if o0.type != cs.x86.X86_OP_REG or o0.reg != cs.x86.X86_REG_RSI:
        return None
    if o1.type != cs.x86.X86_OP_MEM or o1.mem.base != X86_REG_RIP:
        return None
    string_va = first.address + first.size + o1.mem.disp
    if string_va not in strings:
        return None
    stack_arg = enum_arg = False
    instrs = []
    for ins in ins_list[1:]:
        m = ins.mnemonic
        if m == 'call':
            ok_branch = (len(ins.operands) == 1 and ins.operands[0].type == cs.x86.X86_OP_IMM)
            if ok_branch and stack_arg and enum_arg:
                target = ins.operands[0].imm
                if any(lo <= target < hi for lo, hi in code_ranges):
                    return (ins.address, target, instrs)
            return None
        groups = set(ins.groups)
        if groups & {X86_GRP_JUMP, X86_GRP_RET, X86_GRP_INT, X86_GRP_IRET} or m in ('ret', 'retf', 'hlt', 'jmp', 'loop', 'loope', 'loopne', 'jrcxz'):
            return None
        if writes_reg(ins, 'RSI'):
            return None
        if writes_reg(ins, 'RDI'):
            ok = False
            if m == 'lea' and ins.operands and ins.operands[0].type == cs.x86.X86_OP_REG and ins.operands[0].reg == cs.x86.X86_REG_RDI:
                mo = ins.operands[1]
                if mo.type == cs.x86.X86_OP_MEM and mo.mem.base in (X86_REG_RSP, X86_REG_RBP) and mo.mem.index == 0:
                    ok = True
            stack_arg = ok
        if writes_reg(ins, 'RDX'):
            ok = False
            if m == 'mov' and ins.operands and ins.operands[0].type == cs.x86.X86_OP_REG and ins.operands[0].reg == cs.x86.X86_REG_EDX:
                mo = ins.operands[1]
                if mo.type == cs.x86.X86_OP_IMM and mo.imm == 1:
                    ok = True
            enum_arg = ok
        instrs.append(f"0x{ins.address:x}: {ins.mnemonic} {ins.op_str}")
    return None

# --- resolve_with_ranges mirror ---
groups = []
diag = {}
for name in NAMES:
    pat = b'\x00\x00' + name.encode('utf-16-le') + b'\x00\x00'
    strs = set()
    for o in find_all(pat):
        va = off2va(o)
        if va is not None:
            strs.add(va + 2)  # +2 = skip leading NUL pair (resolver a.checked_add(2))
    d = dict(name=name, string_candidates=len(strs))
    if not strs:
        diag[name] = 'NoCandidate: no anchor string'; groups.append({}); continue
    if len(strs) > MAX_STRINGS:
        diag[name] = f'Incomplete: {len(strs)} strings > {MAX_STRINGS}'; groups.append({}); continue
    # targets_with_indirection: strings + 8-byte pointer slots of each string
    targets = set(strs)
    for s in strs:
        for o in find_all(struct.pack('<Q', s)):
            va = off2va(o)
            if va is not None:
                targets.add(va)
    refs = []
    for t in sorted(targets):
        refs.extend(scan_lea_riprel(t))
        refs.extend(scan_mov_imm32(t))
    d['xref_targets'] = len(targets); d['xrefs'] = len(refs)
    if len(refs) > MAX_XREFS:
        diag[name] = f'Incomplete: {len(refs)} xrefs > {MAX_XREFS}'; groups.append({}); continue
    code_ranges = [(l['va'], l['va'] + l['filesz']) for l in LOADS if l['exec']]
    tmap = {}
    for ref in sorted(set(refs)):
        r = call_after(ref, strs, code_ranges)
        if r:
            call, tgt, instrs = r
            tmap.setdefault(tgt, [])
            if call not in tmap[tgt]:
                tmap[tgt].append(call)
    d['distinct_call_targets'] = len(tmap)
    diag[name] = d
    if not tmap:
        diag[name] = dict(d, error='NoCandidate: no call_after site'); groups.append({}); continue
    groups.append(tmap)

common = None
for g in groups:
    keys = set(g.keys())
    common = keys if common is None else (common & keys)

# anchor string content vaddrs per name (for window attribution)
anchor_str_map = {}
for name in NAMES:
    pat = b'\x00\x00' + name.encode('utf-16-le') + b'\x00\x00'
    ss = set()
    for o in find_all(pat):
        va = off2va(o)
        if va is not None:
            ss.add(va + 2)
    anchor_str_map[name] = ss

# chunked sha256 (no 194MB copy under memory pressure)
h = hashlib.sha256()
for i in range(0, N, 8 << 20):
    h.update(mm[i:min(i + (8 << 20), N)])
result = dict(kind='fc1_calls_by_anchor_offline_port', capture=CAP, capture_sha256=h.hexdigest(),
              capture_size=N, anchors=NAMES, diagnostics=diag,
              address=None, address_matches_frozen_survey=None, runtime_address=None,
              calls_by_anchor=[[] for _ in NAMES], call_sites=[], revalidation={})
if not common:
    result['error'] = 'NoCandidate: empty common target set'
elif len(common) != 1:
    result['error'] = f'Ambiguous: {len(common)} common targets: ' + ','.join(hex(a) for a in sorted(common))
else:
    addr = next(iter(common))
    total = sum(len(g[addr]) for g in groups)
    if total > MAX_CALLS:
        result['error'] = 'call-site count over MAX_CALLS cap'
    else:
        result['address'] = f'0x{addr:x}'
        result['address_matches_frozen_survey'] = (addr == TARGET_EXPECT)
        result['runtime_address'] = f'0x{addr + LOAD0_RUNTIME_BASE:x}'
        tb = va2slice(addr, 64)
        result['target'] = dict(bytes64_hex=tb.hex() if tb else None)
        dec = []
        if tb:
            for ins in list(md.disasm(tb, addr, count=16)):
                dec.append(f"0x{ins.address:x}: {ins.mnemonic} {ins.op_str}")
        result['target']['decode'] = dec
        sites = []
        for ai, g in enumerate(groups):
            for call in g.get(addr, []):
                cb = va2slice(call, 5)
                rel = struct.unpack('<i', cb[1:5])[0] if cb and cb[0] == 0xE8 else None
                tgt = call + 5 + rel if rel is not None else None
                entry = dict(anchor=NAMES[ai], call_vaddr=f'0x{call:x}', call_runtime=f'0x{call + LOAD0_RUNTIME_BASE:x}',
                             call_bytes=cb.hex() if cb else None,
                             rel32_target=f'0x{tgt:x}' if tgt is not None else 'UNDECODABLE',
                             rel32_matches_candidate=(tgt == addr), windows=[])
                # windows_at mirror: candidate starts in [call-63, call]
                for w in range(call - 63, call + 1):
                    if not in_exec(w):
                        continue
                    ins_list = decode_at(w, 64)
                    if not ins_list:
                        continue
                    f0 = ins_list[0]
                    if f0.mnemonic != 'lea' or not f0.operands or f0.operands[0].type != cs.x86.X86_OP_REG or f0.operands[0].reg != cs.x86.X86_REG_RSI:
                        continue
                    if f0.operands[1].type != cs.x86.X86_OP_MEM or f0.operands[1].mem.base != X86_REG_RIP:
                        continue
                    sva = f0.address + f0.size + f0.operands[1].mem.disp
                    anchor = next((nm for nm, ss in anchor_str_map.items() if sva in ss), None)
                    if anchor is None:
                        continue
                    r = call_after(w, {sva}, code_ranges)
                    if r and r[0] == call and r[1] == addr:
                        entry['windows'].append(dict(start_vaddr=f'0x{w:x}', start_runtime=f'0x{w + LOAD0_RUNTIME_BASE:x}',
                                                     anchor=anchor, anchor_string_vaddr=f'0x{sva:x}',
                                                     anchor_string_runtime=f'0x{sva + LOAD0_RUNTIME_BASE:x}',
                                                     instrs=r[2]))
                sites.append(entry)
        result['calls_by_anchor'] = [[f'0x{c:x}' for c in groups[ai].get(addr, [])] for ai in range(len(NAMES))]
        result['call_sites'] = sites
        all_e8 = all(s['call_bytes'] and s['call_bytes'][0:2] == 'e8' for s in sites)
        all_rel = all(s['rel32_matches_candidate'] for s in sites)
        result['revalidation'] = dict(all_call_bytes_E8=all_e8, all_rel32_equal_candidate=all_rel)

with open(sys.argv[2], 'w') as out:
    json.dump(result, out, indent=1)
print(json.dumps({k: result[k] for k in ('address', 'address_matches_frozen_survey', 'runtime_address', 'calls_by_anchor', 'revalidation') if k in result}, indent=1))
print('diagnostics:', json.dumps(diag))
