from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import struct, json, hashlib

w = Path(__file__).resolve().parents[2]
o = Path(__file__).resolve().parent
p = w / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b = p.read_bytes()
sha = hashlib.sha256(b).hexdigest()
assert sha == '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
po = struct.unpack_from('<Q', b, 32)[0]
pe, pn = struct.unpack_from('<HH', b, 54)
ss = [struct.unpack_from('<IIQQQQQQ', b, po + i*pe) for i in range(pn)]
def vf(a):
    for ty, fl, off, v, pa, fs, ms, al in ss:
        if ty == 1 and v <= a < v+fs:
            return off+a-v
    raise ValueError(hex(a))
groups = {
    'UClassNamedDescriptor.asm': [0x32650a,0x32651d,0x326531,0x326599,0x3265d9],
    'UClassCreateDefaultObject.asm': [0x3222bb,0x3222c6,0x3222d4,0x3222ea,0x3222fc,0x322302,0x322398,0x3223a6,0x3223aa,0x3223ad,0x3223d1,0x3223d6,0x3223f2,0x322625],
    'UClassGetDefaultObject.asm': [0x3235b4,0x3235b7],
    'UClassVtableCandidateDestruct.asm': [0x32280e,0x322828,0x32282f],
}
md = Cs(CS_ARCH_X86, CS_MODE_64)
ev = {}
for art, adds in groups.items():
    for a in adds:
        f = vf(a)
        i = next(md.disasm(b[f:f+15], a, count=1))
        ev[hex(a)] = {'elf_image_vaddr':hex(a),'file_byte_offset':hex(f),'instruction_bytes':bytes(i.bytes).hex(),'instruction':f'{i.mnemonic} {i.op_str}','disassembly_artifact':art}
dyn = {}
for ty, fl, off, v, pa, fs, ms, al in ss:
    if ty == 2:
        for f in range(off,off+fs,16):
            t,x = struct.unpack_from('<QQ',b,f)
            if t == 0: break
            dyn[t] = x
rv,rs,re = dyn[7],dyn[8],dyn[9]
assert re == 24
rf = vf(rv)
records = []
for f in range(rf,rf+rs,re):
    a,info,add = struct.unpack_from('<QQq',b,f)
    if a == 0x7ed7a28+0x400: records.append((f,a,info,add))
assert records == [(0x9373f80,0x7ed7e28,8,0x3222a0)]
f,a,info,add = records[0]
rel = {'file_byte_offset':hex(f),'bytes':b[f:f+24].hex(),'r_offset':hex(a),'r_info':hex(info),'r_addend':hex(add),'interpretation':'R_X86_64_RELATIVE: named UClass vptr+400 resolves to image function3222A0; runtime address adds module base.'}
ns = 0x76ae74e
nf = vf(ns)
name = b[nf:nf+12]
assert name.decode('utf-16le') == 'Class\0'
base = {'status':'validated-static','confidence':'high','live_validation':False}
facts = [
    dict(base,key='UClass.ClassCastFlags',value='0xd8',width_bytes=8,evidence_keys=['0x32650a','0x32651d','0x3265d9','0x32280e','0x322828','0x32282f'],inference='Named Class descriptor creation constructs FName Class and stores CASTCLASS_UClass20 as a qword atD8. Independent class-super inheritance reads super+D8 as qword and ORs into self+D8. Native width8 independently matches selected enum:uint64_t accessor type.'),
    dict(base,key='UClass.ClassDefaultObject',value='0x110',width_bytes=8,evidence_keys=['0x326531','0x326599','0x3222c6','0x3222ea','0x3222fc','0x322302','0x322398','0x3223a6','0x3223ad','0x3223d1','0x3223d6','0x3223f2','0x322625'],inference='Named UClass vtable400 relocates to3222A0. Function checks existing self110, obtains superclass CDO through identical slot400, calls known StaticAllocateObjectInternal490FB0 with self class, self Outer20 and RF_Public|RF_ClassDefaultObject|RF_ArchetypeObject31, stores returned UObject pointer into self110, and returns same field.'),
    dict(base,key='UClass.CreateDefaultObject.vtable_byte_offset',value='0x400',width_bytes=8,required=False,evidence_keys=['0x326531','0x326599','0x3235b4','0x3235b7','0x3222fc','0x3223ad','0x3223d1','0x3223f2'],data_evidence_keys=['UClass_CreateDefaultObject_rela'],inference='Named UClass descriptor installs native vptr7ED7A28; exact standard DT_RELA record targets7ED7E28=vptr+400 with addend3222A0. Separate GetDefaultObject thunk and superclass creation use virtual400; callee semantics allocate and publish the CDO. Supplemental fact only: selected port wrapper has no traced active caller. Slot is byteoffset400.'),
]
paths = [o/n for n in groups] + [o/'UClass-vtable-relocations.json',Path(__file__).resolve(),w/'outputs/2026-10-07-l19-static-layout-check/StaticAllocateObjectInternal.asm',w/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp',w/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UClass.cpp',w/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp']
d = {'format':1,'scope':'local-static-only; no source/profile mutation; exact sizes owned by separate class-size evidence','image':{'path':str(p),'bytes':len(b),'sha256':sha},'address_model':'ELF image vaddr, actual PT_LOAD mapping; DT_RELA decoded from actual PT_DYNAMIC tags7/8/9','facts':facts,'unresolved':[],'evidence':ev,'data_evidence':{'UClass_name':{'elf_image_vaddr':hex(ns),'file_byte_offset':hex(nf),'bytes':name.hex(),'decoded_utf16le':'Class\u0000'},'UClass_CreateDefaultObject_rela':rel},'relocation_table':{'elf_image_vaddr':hex(rv),'file_byte_offset':hex(rf),'size_bytes':rs,'entry_bytes':re,'matching_target_records':len(records)},'sources':[{'path':str(x),'bytes':x.stat().st_size,'sha256':hashlib.sha256(x.read_bytes()).hexdigest()} for x in paths],'accessor_width_implications':{'UClass.ClassCastFlags':'Native8B matches compiled EClassCastFlags:uint64_t and GetClassCastFlagsBase uint64&. Existing generic sizeof accessor provider yields8, no game-specific fallback needed.'},'validation':{'instruction_windows_checked':len(ev),'all_instruction_bytes_exact':True,'unique_relocation_record_exact':True,'actual_DT_RELA_bounds_checked':True,'source_mutations':0,'console_network_account_actions':0,'runtime_tests':0}}
q = o/'UCLASS-FACTS.json'
q.write_text(json.dumps(d,indent=2)+'\n')
print(q)
print(hashlib.sha256(q.read_bytes()).hexdigest())
print(len(ev),'exact windows; 2 required members, 1 supplemental vtable')
