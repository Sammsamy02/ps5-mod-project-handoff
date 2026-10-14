#!/usr/bin/env python3
"""Freeze named-property evidence from the existing authenticated image only."""
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import hashlib, json, struct

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
IMAGE = ROOT / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
B = IMAGE.read_bytes()
H = hashlib.sha256(B).hexdigest()
assert H == '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
po = struct.unpack_from('<Q', B, 32)[0]
pe, pn = struct.unpack_from('<HH', B, 54)
P = [struct.unpack_from('<IIQQQQQQ', B, po + i*pe) for i in range(pn)]
def off(a, size=1):
    hits = [p[2]+a-p[3] for p in P if p[0] == 1 and p[3] <= a and a+size <= p[3]+p[5]]
    assert len(hits) == 1, (hex(a),size,hits)
    return hits[0]
M = Cs(CS_ARCH_X86, CS_MODE_64)
E, D, C = {}, {}, {}
def capture(name, start, end):
    raw = B[off(start,end-start):off(start,end-start)+end-start]
    keys, lines = [], []
    for i in M.disasm(raw,start):
        k = hex(i.address); f = off(i.address,i.size); bs = bytes(i.bytes)
        assert B[f:f+i.size] == bs
        r = {'elf_image_vaddr':k,'file_byte_offset':hex(f),'instruction_bytes':bs.hex(),
             'instruction':f'{i.mnemonic} {i.op_str}','disassembly_artifact':name+'.asm'}
        if k in E: assert E[k]['instruction_bytes'] == bs.hex()
        E[k] = r; keys.append(k)
        lines.append(f'{k} (file {hex(f)}) {bs.hex()} {r["instruction"]}')
        if i.mnemonic == 'call' and bs[0] == 0xe8:
            target = i.address+5+struct.unpack_from('<i',bs,1)[0]
            C[k] = hex(target)
    (OUT/(name+'.asm')).write_text('\n'.join(lines)+'\n')
    return keys
def require(a, text):
    assert E[hex(a)]['instruction'] == text, (hex(a),E[hex(a)],text)
def string(name,a,text):
    raw = (text+'\0').encode('utf-16le'); f = off(a,len(raw))
    assert B[f:f+len(raw)] == raw
    D[name] = {'elf_image_vaddr':hex(a),'file_byte_offset':hex(f),
               'bytes':raw.hex(),'decoded_utf16le':text+'\0'}

dyn = {}
for p in P:
    if p[0] == 2:
        for f in range(p[2],p[2]+p[5],16):
            t,x = struct.unpack_from('<QQ',B,f)
            if t == 0: break
            if t in (7,8,9): assert t not in dyn or dyn[t] == x
            dyn[t] = x
rv,rs,re = dyn[7],dyn[8],dyn[9]
assert re == 24 and rs % re == 0
rf = off(rv,rs)
RELA = {}
for f in range(rf,rf+rs,re):
    a,info,add = struct.unpack_from('<QQq',B,f)
    RELA.setdefault(a,[]).append((f,info,add))
def relocation(name,vptr,slot,target):
    rec = RELA[vptr+slot]
    assert len(rec) == 1 and rec[0][1:] == (8,target), (name,rec)
    f,info,add = rec[0]
    D[name] = {'vptr_elf_image_vaddr':hex(vptr),'slot_byte_offset':hex(slot),
        'r_offset':hex(vptr+slot),'r_info':hex(info),'r_addend':hex(add),
        'file_byte_offset':hex(f),'bytes':B[f:f+24].hex()}
    return name

descriptor = {}
construct = {}
types = [
 ('FEnumProperty',0x7630d66,0x360150,0x360196,0x360050,0x36014c,0x80,0x7edc468,0x36006e,0x36007f,0x36009d,0x3600be,0x36013a),
 ('FArrayProperty',0x78cd916,0x3e51c0,0x3e520d,0x3e5930,0x3e5a5c,0x80,0x7eea620,0x3e594e,0x3e595f,0x3e5987,0x3e5996,0x3e5a4a),
 ('FBoolProperty',0x7a16a78,0x3fe550,0x3fe58c,0x3fe420,0x3fe549,0x78,0x7eef940,0x3fe43e,0x3fe44f,0x3fe477,0x3fe486,0x3fe537),
 ('FStructProperty',0x78f9988,0x4367e0,0x43681b,0x4366e0,0x4367d8,0x78,0x7efcda8,0x4366fe,0x43670f,0x436737,0x436747,0x4367c6),
]
common = capture('CommonFieldClassConstructor',0x363990,0x3639f8)
require(0x3639d2,'mov qword ptr [rdi + 0x30], r9')
allocator = capture('AllocatorEntry',0xb7a20,0xb7a59)
F = []
def fact(key,value,width,keys,dk,inference,kind):
    F.append({'key':key,'value':hex(value),'width_bytes':width,'status':'validated-static',
              'confidence':'high','live_validation':False,'derivation_kind':kind,
              'evidence_keys':keys,'data_evidence_keys':dk,'inference':inference})
for name,na,ds,de,cs,ce,size,vptr,sa,ca,va,store,ret in types:
    string(name+'_name',na,name)
    # Begin at the exact name/callback LEA boundary; a neighboring function tail
    # must not be mistaken for a disassembly entry point.
    starts=[]
    for a in range(de-133,de-5):
        bs=B[off(a,7):off(a,7)+7]
        if bs[:3] in (b'\x48\x8d\x35',b'\x4c\x8d\x0d'):
            dest=a+7+struct.unpack_from('<i',bs,3)[0]
            if (bs[:3]==b'\x48\x8d\x35' and dest==na) or (bs[:3]==b'\x4c\x8d\x0d' and dest==cs): starts.append(a)
    assert len(starts)==2,(name,starts)
    ds=min(starts)
    descriptor[name] = capture(name+'NamedDescriptor',ds,de)
    construct[name] = capture(name+'ConstructFn',cs,ce)
    # Require exact registered callback and name RIP-relative LEAs, not nearby labels.
    insts = [E[k] for k in descriptor[name]]
    found = {}
    for i in insts:
        bs = bytes.fromhex(i['instruction_bytes']); a = int(i['elf_image_vaddr'],16)
        if bs[:3] in (b'\x48\x8d\x35',b'\x4c\x8d\x0d') and len(bs)==7:
            found['name' if bs[:3]==b'\x48\x8d\x35' else 'callback'] = a+7+struct.unpack_from('<i',bs,3)[0]
    assert found == {'name':na,'callback':cs}, (name,found)
    assert C[hex(de-5)] == '0x363990'
    require(sa,f'mov edi, {hex(size)}'); assert C[hex(ca)] == '0xb7a20'
    bs = bytes.fromhex(E[hex(va)]['instruction_bytes'])
    assert va+len(bs)+struct.unpack_from('<i',bs,3)[0] == vptr
    require(store,'mov qword ptr [rbx], r13'); require(ret,'mov rax, rbx')
    fact(name+'.UEP_TotalSize',size,None,descriptor[name]+construct[name]+common+allocator,
         [name+'_name'],f'The UTF16 named {name} descriptor registers ConstructFn {hex(cs)} through R9 -> class+30. That exact callback requests {hex(size)} bytes from the same native allocator, keeps the returned pointer unchanged, installs its named vptr {hex(vptr)}, and returns that pointer. This is a named construction allocation request, not an envelope inferred from touched fields or a PC layout.', 'named-registered-constructor-allocation')

acopy = capture('FArrayCopy',0x3ea490,0x3ea590)
adestroy = capture('FArrayDestroy',0x3ea720,0x3ea7d0)
ark = [relocation('FArray_copy_rela',0x7eea620,0xf8,0x3ea490),
       relocation('FArray_destroy_rela',0x7eea620,0x140,0x3ea720)]
require(0x3ea4b8,'mov rdi, qword ptr [rdi + 0x78]')
require(0x3ea4c5,'mov r15, qword ptr [rbx + 0x78]')
fact('FArrayProperty.Inner',0x78,8,acopy+adestroy+construct['FArrayProperty'],ark+['FArrayProperty_name'],
     'Named Array copy and destroy virtuals dereference property+78 as a nested FProperty, read its independently validated ElementSize30 and PropertyFlags38, and call its GetMinAlignment170 to build the ScriptArrayHelper. The constructor zeroes the same qword; +70 is a separate byte of array flags, not Inner.','native-nested-property-array-flow')

salign = capture('FStructAlignment',0x436a60,0x436a72)
scopy = capture('FStructCopy',0x437400,0x43743c)
sdestroy = capture('FStructDestroy',0x437450,0x437490)
srk = [relocation('FStruct_alignment_rela',0x7efcda8,0x170,0x436a60),
       relocation('FStruct_copy_rela',0x7efcda8,0xf8,0x437400),
       relocation('FStruct_destroy_rela',0x7efcda8,0x140,0x437450)]
require(0x436a64,'mov rax, qword ptr [rdi + 0x70]')
require(0x436a68,'movsx eax, word ptr [rax + 0x5c]')
fact('FStructProperty.Struct',0x70,8,salign+scopy+sdestroy+construct['FStructProperty'],srk+['FStructProperty_name'],
     'Named StructProperty GetMinAlignment dereferences self+70 then reads the independently validated UStruct.MinAlignment5C as int16. Its copy and destroy virtuals dispatch through the same UStruct pointer; constructor initializes this qword.','native-struct-value-flow')

bcopy = capture('FBoolCopy',0x3ffe80,0x3ffed5)
bset = capture('FBoolSetMasks',0x3fe720,0x3fe7e6)
bvalue = capture('FBoolSetValue',0x3ffdf0,0x3ffe72)
brk = [relocation('FBool_copy_rela',0x7eef940,0xf8,0x3ffe80)]
require(0x3ffe96,'movzx r11d, byte ptr [rdi + 0x73]')
require(0x3ffe9b,'movzx r10d, byte ptr [rdi + 0x71]')
require(0x3fe7d6,'mov byte ptr [rbx + 0x72], cl')
require(0x3fe7d9,'mov byte ptr [rbx + 0x71], al')
require(0x3fe7dc,'mov byte ptr [rbx + 0x73], cl')
for key,value,desc in [
 ('ByteOffset',0x71,'Copy loop adds byte+71 to each source/destination element address, distinct from mask bytes.'),
 ('ByteMask',0x72,'Setter stores the selected bit at+72 and the value-write helper reads+72 for the true-value OR. Native-bool case uses ByteMask1 while FieldMaskFF, proving these are distinct roles.'),
 ('FieldMask',0x73,'Copy/value write logic clears destination bits using inverted byte+73 and masks source values by the same byte. Setter stores the bit mask here, or FF for native-bool.')]:
    fact('FBoolProperty.'+key,value,1,bcopy+bset+bvalue+construct['FBoolProperty'],brk+['FBoolProperty_name'],desc,'native-bit-offset-and-mask-flow')

ealign = capture('FEnumAlignment',0x3617e0,0x3617f4)
eserial = capture('FEnumSerialize',0x3605d0,0x3606de)
egen = capture('FEnumGenParamsConstructor',0x3602f0,0x3603ac)
erefs = capture('FEnumReferenceSerialization',0x361170,0x3611dc)
enm = capture('FEnumNameArray',0x360e70,0x360eae)
erk = [relocation('FEnum_alignment_rela',0x7edc468,0x170,0x3617e0)]
require(0x3617e4,'mov rdi, qword ptr [rdi + 0x70]')
require(0x360386,'mov qword ptr [rbx + 0x78], r14')
require(0x360e74,'mov rax, qword ptr [rdi + 0x78]')
fact('FEnumProperty.UnderlyingProp',0x70,8,ealign+eserial+construct['FEnumProperty'],erk+['FEnumProperty_name'],
     'Named EnumProperty alignment dispatches through self+70 as a nested FProperty. Its serializer uses the same pointer for numeric get/set virtuals, while self+78 supplies UEnum name/value lookup. Constructor initializes both qwords independently.','native-numeric-underlying-property-flow')
fact('FEnumProperty.Enum',0x78,8,eserial+egen+erefs+enm+construct['FEnumProperty'],erk+['FEnumProperty_name'],
     'GenParams constructor calls the enum provider at params+38 and stores its result into self+78. Native serializer handles that field as an archive UObject reference and independently uses its Names array40/count48 and enum flags51; underlying field70 instead dispatches FProperty numeric virtuals. Combined named constructor, object-reference serialization and enum-value lookup identify UEnum pointer78.','native-enum-provider-and-object-reference-flow')
assert len(F)==11
J = {'format':'townfall-property-subtypes-static-v1','scope':'LOCAL ONLY; saved image; no game/source/profile mutation, console or network contact',
     'image':{'path':str(IMAGE),'sha256':H,'size_bytes':len(B)},'address_model':'ELF image vaddr; actual PT_LOAD mapping and PT_DYNAMIC DT_RELA bounds; no live observation',
     'facts':F,'evidence':E,'data_evidence':D,'direct_call_targets':C,
     'relocation_table':{'vaddr':hex(rv),'file_offset':hex(rf),'size':rs,'entry_size':re},
     'validation':{'exact_instruction_windows':len(E),'named_callbacks_verified':4,'unique_relative_vtable_records':7,'positive_facts':11,
                   'all_bytes_match':True,'no_PC_layout_guess':True,'console_contacts':0},
     'limits':['Native construction requests and instruction flow establish static facts only; no live object or menu/spawn proof.',
               'No unmeasured FField/FFieldClass size, UClass.Interfaces offset or engine/game-mode slot is supplied.']}
(OUT/'SUBTYPE-FACTS.json').write_text(json.dumps(J,indent=2)+'\n')
print(json.dumps(J['validation']))
print('sha256',hashlib.sha256((OUT/'SUBTYPE-FACTS.json').read_bytes()).hexdigest())
