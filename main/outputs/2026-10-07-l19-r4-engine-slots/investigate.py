"""Read-only native image investigation; captures live only beside this script."""
import capstone
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
IMAGE = ROOT / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
B = IMAGE.read_bytes()
IMAGE_SHA = hashlib.sha256(B).hexdigest()
assert IMAGE_SHA == '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
H = struct.unpack_from('<16sHHIQQQIHHHHHH', B)
PH = [struct.unpack_from('<IIQQQQQQ', B, H[5] + i * H[9]) for i in range(H[10])]
LOADS = [r for r in PH if r[0] == 1]
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
MD.detail = True

def fmap(v, n=1):
    rows = [r for r in LOADS if r[3] <= v and v+n <= r[3]+r[5]]
    assert len(rows) == 1, (hex(v), n)
    return rows[0][2] + v - rows[0][3]

def save(name, obj):
    p = OUT / name
    assert not p.exists(), p
    p.write_text(json.dumps(obj, indent=2) + '\n')
    return p

def wide(v, limit=500):
    o = fmap(v)
    end = o
    while end + 2 <= len(B) and end - o < limit * 2 and B[end:end+2] != b'\0\0':
        end += 2
    text = B[o:end].decode('utf-16le', errors='replace')
    return text if all(c.isprintable() or c in '\n\r\t' for c in text) else ''

def dynamic():
    r = next(r for r in PH if r[0] == 2)
    rows = {}
    for o in range(r[2], r[2] + r[5], 16):
        k, v = struct.unpack_from('<QQ', B, o)
        if k == 0:
            break
        rows[k] = v
    return rows

DYN = dynamic()
assert DYN[9] == 24
RELA_OFFSET = fmap(DYN[7], DYN[8])
RELOCATIONS = {}
for o in range(RELA_OFFSET, RELA_OFFSET + DYN[8], 24):
    ro, ri, ra = struct.unpack_from('<QQq', B, o)
    RELOCATIONS.setdefault(ro, []).append((o, ri, ra))

def rela_row(target):
    matches = RELOCATIONS.get(target, [])
    return [{'relocation_file_offset':hex(o), 'r_offset':hex(target), 'r_info':hex(ri),
             'r_addend':hex(ra), 'entry_bytes':B[o:o+24].hex()} for o, ri, ra in matches]

def capture(v, n):
    o = fmap(v, n)
    rows = []
    for i in MD.disasm(B[o:o+n], v):
        row = {'elf_image_vaddr':hex(i.address), 'file_byte_offset':hex(fmap(i.address,i.size)),
               'instruction_bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str}
        rip = [i.address+i.size+x.mem.disp for x in i.operands if x.type==capstone.x86.X86_OP_MEM and x.mem.base==capstone.x86.X86_REG_RIP]
        if rip:
            row['rip_targets'] = [hex(x) for x in rip]
            row['utf16_candidates'] = [wide(x,150) for x in rip if any(r[3] <= x < r[3]+r[5] for r in LOADS)]
        rows.append(row)
    path = save(f'{v:#x}-{n:#x}.json', rows)
    text = '\n'.join(f"{r['elf_image_vaddr']}: {r['instruction_bytes']:24} {r['instruction']}" +
                     ((' ; '+repr(r['utf16_candidates'])) if any(r.get('utf16_candidates', [])) else '') for r in rows)
    p = OUT / f'{v:#x}-{n:#x}.asm'
    assert not p.exists()
    p.write_text(text+'\n')
    print(text)

def table(vptr, size=0x800):
    rows = []
    for slot in range(0,size,8):
        matches = RELOCATIONS.get(vptr+slot, [])
        if len(matches)!=1 or matches[0][1]!=8 or not 0 <= matches[0][2] < LOADS[0][5]:
            break
        o, ri, fn = matches[0]
        rows.append({'slot':hex(slot), 'callee':hex(fn), 'relocation':rela_row(vptr+slot)[0]})
    save(f'vtable-{vptr:#x}.json',rows)
    print('vtable',hex(vptr),'entries',len(rows))
    print(' '.join(f"{r['slot']}={r['callee']}" for r in rows))

def xrefs(targets):
    found = {hex(v):[] for v in targets}
    # Exact seven-byte REX.W LEA/MOV RIP-relative forms, then single-instruction validation.
    pat = re.compile(b'[\x48-\x4f][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....',re.DOTALL)
    wanted = set(targets)
    code = LOADS[0]
    for m in pat.finditer(B,code[2],code[2]+code[5]):
        addr=code[3]+m.start()-code[2]
        target=addr+7+struct.unpack_from('<i',m.group(),3)[0]
        if target not in wanted:
            continue
        ins=list(MD.disasm(m.group(),addr))
        if len(ins)!=1 or ins[0].size!=7:
            continue
        found[hex(target)].append({'elf_image_vaddr':hex(addr),'file_byte_offset':hex(m.start()),
                                  'instruction_bytes':m.group().hex(),'instruction':ins[0].mnemonic+' '+ins[0].op_str})
    save('xrefs-'+hashlib.sha256(','.join(hex(x) for x in targets).encode()).hexdigest()[:12]+'.json',found)
    print(json.dumps(found))

if __name__=='__main__':
    action=sys.argv[1]
    if action=='capture':
        capture(int(sys.argv[2],0),int(sys.argv[3],0))
    elif action=='tables':
        for v in sys.argv[2:]: table(int(v,0))
    elif action=='xrefs': xrefs([int(v,0) for v in sys.argv[2:]])
    elif action=='initial':
        facts=[]
        for term in ['LoadMap','InitGameState','GameModeBase','AGameModeBase']:
            for encoding in ['utf-16le','utf-8']:
                needle=term.encode(encoding);o=0;rows=[]
                while True:
                    o=B.find(needle,o)
                    if o<0:break
                    loads=[r for r in LOADS if r[2]<=o<r[2]+r[5]]
                    v=loads[0][3]+o-loads[0][2] if len(loads)==1 else None
                    rows.append({'file_offset':hex(o),'elf_image_vaddr':hex(v) if v is not None else None})
                    o+=len(needle)
                facts.append({'term':term,'encoding':encoding,'occurrences':rows})
        save('INITIAL-SURVEY.json',{'image':{'path':str(IMAGE),'sha256':IMAGE_SHA,'bytes':len(B)},'program_headers':PH,'dynamic':{hex(k):hex(v) for k,v in DYN.items()},'strings':facts,'scope':'Diagnostic anchors only; no named slot proof.'})
        print('Authenticated image; initial anchors captured.')
