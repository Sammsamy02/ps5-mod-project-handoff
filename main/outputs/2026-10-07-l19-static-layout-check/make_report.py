#!/usr/bin/env python3
"""Save bounded, byte-checked evidence from the owned static image. No game execution."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = ROOT / 'outputs/2026-10-07-l19-static-layout-check'
IMG = ROOT / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
EXPECTED = '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
data = IMG.read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == EXPECTED and len(data) == 194241424
phoff = struct.unpack_from('<Q', data, 32)[0]
phentsize, phnum = struct.unpack_from('<HH', data, 54)
segments = []
for n in range(phnum):
    p_type,p_flags,p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_align = struct.unpack_from('<IIQQQQQQ', data, phoff+n*phentsize)
    if p_type == 1:
        segments.append(dict(type=p_type, flags=p_flags, file_offset=p_offset, elf_image_vaddr=p_vaddr, file_size=p_filesz, memory_size=p_memsz))

def evidence(addr, bytehex, insn, file):
    addr = int(addr,16)
    b = bytes.fromhex(bytehex)
    seg = next(s for s in segments if s['elf_image_vaddr'] <= addr and addr+len(b) <= s['elf_image_vaddr']+s['file_size'])
    off = seg['file_offset'] + addr - seg['elf_image_vaddr']
    assert data[off:off+len(b)] == b, (hex(addr),data[off:off+len(b)].hex(),bytehex)
    return dict(elf_image_vaddr=hex(addr), file_byte_offset=hex(off), instruction_bytes=bytehex, instruction=insn, disassembly_artifact=file)

E = {}
def add(key, *args): E[key] = evidence(*args)
add('ctor_flags','48545b','895708','mov dword ptr [rdi+8], edx','UObjectBaseConstructor.asm')
add('ctor_index','48545e','c7470cffffffff','mov dword ptr [rdi+0xc], 0xffffffff','UObjectBaseConstructor.asm')
add('ctor_class','485465','48897710','mov qword ptr [rdi+0x10], rsi','UObjectBaseConstructor.asm')
add('ctor_name','4854af','4c897b18','mov qword ptr [rbx+0x18], r15','UObjectBaseConstructor.asm')
add('ctor_outer','485492','4c896320','mov qword ptr [rbx+0x20], r12','UObjectBaseConstructor.asm')
add('name_ctor_call','4855b4','e8d77fdbff','call image 0x23d590 (authenticated native FName constructor)','UObjectBaseRegister-confirmed.asm')
add('name_ctor_output','4855b9','488b45c8','mov rax, qword ptr [rbp-0x38]','UObjectBaseRegister-confirmed.asm')
add('name_ctor_store','4855bd','48894318','mov qword ptr [rbx+0x18], rax','UObjectBaseRegister-confirmed.asm')
add('item_index_low16','483b98','410fb7cf','movzx ecx, r15w','AllocateUObjectIndex.asm')
add('item_index_times3','483b9f','4c8d2449','lea r12, [rcx+rcx*2]','AllocateUObjectIndex.asm')
add('item_index_high16','483ba3','c1e810','shr eax, 0x10','AllocateUObjectIndex.asm')
add('item_index_times8','483ba6','41c1e403','shl r12d, 3','AllocateUObjectIndex.asm')
add('item_index_base','483baa','4c0324c2','add r12, qword ptr [rdx+rax*8]','AllocateUObjectIndex.asm')
add('chunk_allocation','483c79','bf08001800','mov edi, 0x180008','AllocateUObjectIndex.asm')
add('chunk_header_count','483c92','48c70000000100','mov qword ptr [rax], 0x10000','AllocateUObjectIndex.asm')
add('chunk_payload_bytes','483c8d','ba00001800','mov edx, 0x180000','AllocateUObjectIndex.asm')
add('chunk_skip_header','483c9b','4983c408','add r12, 8','AllocateUObjectIndex.asm')
add('item_flags_init','483d14','41c744240800000080','mov dword ptr [r12+8], 0x80000000','AllocateUObjectIndex.asm')
add('item_flags_atomic','483d84','f0410fb14c2408','lock cmpxchg dword ptr [r12+8], ecx','AllocateUObjectIndex.asm')
add('item_object_store','483d40','49891c24','mov qword ptr [r12], rbx','AllocateUObjectIndex.asm')
add('object_index_store','483d5e','44897b0c','mov dword ptr [rbx+0xc], r15d','AllocateUObjectIndex.asm')
add('free_index_read','4840bb','8b5e0c','mov ebx, dword ptr [rsi+0xc]','FreeUObjectIndex.asm')
add('free_times3','4840d2','4c8d2449','lea r12, [rcx+rcx*2]','FreeUObjectIndex.asm')
add('free_times8','4840d9','41c1e403','shl r12d, 3','FreeUObjectIndex.asm')
add('free_object_read','4840e1','4d8b0c24','mov r9, qword ptr [r12]','FreeUObjectIndex.asm')
add('free_object_compare','4840e5','4d39f9','cmp r9, r15 (original object)','FreeUObjectIndex.asm')
add('free_item_clear16','484136','c4c178110424','vmovups xmmword ptr [r12], xmm0','FreeUObjectIndex.asm')
add('free_item_clear8','48413c','49c744241000000000','mov qword ptr [r12+0x10], 0','FreeUObjectIndex.asm')
add('object_flags_guard','45985c','8b4708','mov eax, dword ptr [rdi+8]','ProcessEvent.asm')
add('object_flags_bit','45985f','a900000040','test eax, 0x40000000','ProcessEvent.asm')
add('function_native_bit','45986a','f686b100000004','test byte ptr [rsi+0xb1], 4','ProcessEvent.asm')
add('event_graph_load','4598cc','4c8bbec8000000','mov r15, qword ptr [rsi+0xc8]','ProcessEvent.asm')
add('event_graph_call_offset','4598d8','8b86d0000000','mov eax, dword ptr [rsi+0xd0]','ProcessEvent.asm')
add('event_graph_flags_read','4598e5','41f687b100000080','test byte ptr [r15+0xb1], 0x80','ProcessEvent.asm')
add('event_graph_outer_read','45990c','498b7f20','mov rdi, qword ptr [r15+0x20]','ProcessEvent.asm')
add('parms_size_read','459970','410fb787b6000000','movzx eax, word ptr [r15+0xb6]','ProcessEvent.asm')
add('return_value_offset_read','459b4d','410fb787b8000000','movzx eax, word ptr [r15+0xb8]','ProcessEvent.asm')
add('invoke_call','459b6f','e84ccfecff','call image 0x326ac0','ProcessEvent.asm')
add('invoke_self_save','326adb','4989ff','mov r15, rdi (UFunction self)','UFunctionInvoke.asm')
add('invoke_args_context','326b15','4c89e7','mov rdi, r12 (Context)','UFunctionInvoke.asm')
add('invoke_args_frame','326b18','4889de','mov rsi, rbx (FFrame)','UFunctionInvoke.asm')
add('invoke_args_result','326b1b','4c89f2','mov rdx, r14 (result storage)','UFunctionInvoke.asm')
add('native_func_call','326b25','41ff97d8000000','call qword ptr [r15+0xd8]','UFunctionInvoke.asm')

facts = []
def fact(key,value,width,ev,inference):
    facts.append(dict(key=key, value=value, width_bytes=width, status='validated-static', confidence='high', evidence_keys=ev, inference=inference, live_validation=False))
fact('FUObjectItem.UEP_TotalSize','0x18',None,['item_index_low16','item_index_times3','item_index_times8','item_index_base','chunk_allocation','chunk_payload_bytes','chunk_header_count','chunk_skip_header','free_times3','free_times8','free_item_clear16','free_item_clear8'], 'Both index allocation and free compute chunk[high16]+low16*3*8; payload allocation is 65536*24=0x180000, after a separate8B array header. Free clears exactly24B. Item array stride/size24 is unique for these routines.')
fact('FUObjectItem.Object','0x0',8,['item_object_store','free_object_read','free_object_compare'], 'Allocation stores the original UObject* at item+0; free reads it and compares with its original UObject* argument. Direct qword pointer, no encoded-pointer transform in this path.')
fact('FUObjectItem.Flags','0x8',4,['item_flags_init','item_flags_atomic'], 'Allocation initializes and atomically updates the item flags as a dword at+8, after the direct qword object pointer.')
fact('UObjectBase.ObjectFlags','0x8',4,['ctor_flags','object_flags_guard','object_flags_bit'], 'Object base constructor stores flags argument at+8; ProcessEvent independently tests the32B flag word at that displacement (4 bytes, not32 bytes).')
fact('UObjectBase.InternalIndex_Private','0xc',4,['ctor_index','object_index_store','free_index_read'], 'Constructor initializes-1; array allocation stores its chosen index; free reads that index and roundtrips to the original object.')
fact('UObjectBase.ClassPrivate','0x10',8,['ctor_class'], 'StaticConstructObject/Internal allocation passes its requested class asRSI to constructor485430; constructor stores it at+10. The allocation path also reads [object+10] as the class and tests UClass properties.')
fact('UObjectBase.NamePrivate','0x18',8,['ctor_name','name_ctor_call','name_ctor_output','name_ctor_store'], 'The static constructor stores its8B name argument at+18. Independently, object registration calls authenticated native FName constructor23d590, loads its8B output and stores that value atobject+18. No Windows layout assumption is needed.')
fact('UObjectBase.OuterPrivate','0x20',8,['ctor_outer','event_graph_outer_read'], 'Constructor stores its outer argument at+20. ProcessEvent and Invoke read the function owner at+20 as an object and call the owner vtable/type checks.')
fact('UFunction.Func','0xd8',8,['invoke_call','invoke_self_save','invoke_args_context','invoke_args_frame','invoke_args_result','native_func_call'], 'ProcessEvent calls UFunction Invoke326ac0 with function self inRDI. Invoke saves self inR15 and performs one qword indirect call atself+D8 with Context,Frame,result inSysV RDI,RSI,RDX. This uniquely establishes the native call slot and consumed8B representation.')
fact('UFunction.EventGraphFunction','0xc8',8,['event_graph_load','event_graph_flags_read','event_graph_outer_read'], 'Loaded pointer is subsequently used as a UFunction object: reads+ B1 flags and+20 owner; null selects originalRSI function. It is not an executable callback. Matching field name is semantic interpretation of this graph substitution path; prior Func=C8 claim is refuted.')
fact('UFunction.EventGraphCallOffset','0xd0',4,['event_graph_load','event_graph_call_offset'], 'For nonnull event-graph target, a dword from originalfunction+D0 becomes local call parameter. Field name follows that dataflow; numeric offset and4B access are direct evidence.')
fact('UFunction.ParmsSize','0xb6',2,['parms_size_read'], 'ProcessEvent reads this word to bound the function parameter copy/zero regions against struct storage;2B instruction width is direct evidence.')
fact('UFunction.ReturnValueOffset','0xb8',2,['return_value_offset_read','invoke_call'], 'ProcessEvent adds this word to the caller parameter base;0xffff selectsnull; otherwise passes resulting result pointer to Invoke.')

unresolved = [
 dict(key='UObjectBase.UEP_TotalSize',status='unresolved',lower_bound='0x28',candidate='0x28',reason='Highest established member isOuter+20 width8, proving28B envelope only. Bounded constructor/allocator paths did not independently establish exact sizeof/base-class tail. Template total28 not promoted.'),
 dict(key='UFunction.UEP_TotalSize',status='unresolved',lower_bound='0xe0',candidate='0xe0',reason='Native Func+D8 qword provesE0B envelope only. No independently identified sizeof/allocation or class-registration total was recovered; template totalE0 not promoted.'),
 dict(key='UFunction.NumParms',status='unresolved',candidate='0xb4',reason='No exact byte read/store atthis field identified in bounded pass; retained template is not target evidence.'),
 dict(key='UFunction.FunctionFlags',status='partial-static',candidate='0xb0',observed_byte='0xb1',reason='ProcessEvent directly tests byteB1. Flag-word baseB0/4B matches known semantics and selected getter but this bounded pass has no directfull dword target operand proving base/width; do not substitute byte test for complete field proof.'),
 dict(key='UE_ENABLE_FUOBJECT_ITEM_PACKING',status='unresolved-as-build-macro',reason='Observed item path uses directqword Object0/dword Flags8/stride18. This proves effective layout of these routines, not globalmacro spelling/value.'),
 dict(key='EObjectFlags.complete_valid_mask',status='unresolved',observed_test='0x40000000',reason='ProcessEvent tests this bit in ObjectFlags at+8. Retained RF_AllFlags0x0FFFFFFF predicate would reject it when set; no complete acceptedflagmask is recovered by this pass.')
]
sources = [IMG, ROOT/'outputs/2026-10-04-zcode-team/worker-re-fname-pool/tools/rdis.py', ROOT/'outputs/2026-10-04-zcode-team/worker-re-fname-pool/tools/vmap.py', ROOT/'outputs/2026-10-04-zcode-team/worker-gap-l2a/REPORT.md', Path('/tmp/wrfp_gap_l2a/vt2.txt'), ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UFunction.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UFunction.cpp',ROOT/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/coverage-delta.json',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp']
manifest = [dict(path=str(p), bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources]
result = dict(format=1,scope='local-static-only',image=dict(path=str(IMG),bytes=len(data),sha256=digest,segments=segments),address_model='elf_image_vaddr, translated throughPT_LOAD to file_byte_offset; disassembly leading address adds historicalbase400000 and is only candidate mapped address, no new runtime validation',facts=facts,unresolved=unresolved,evidence=E,sources=manifest,selected_accessor=dict(native_width_bytes=8,read_write_width_bytes=8,source_chain=['Class.hpp356:GetFunc=>GetFunc419', 'SrcWrapper_UFunction.hpp195:GetFunc419 returns std::function<...>*&','UFunction.cpp28-35:GetFuncPtr/SetFuncPtr useGetFunc;bit_cast pointer value'],conservative_width_authority=16,conservative_authority_reason='Maximum ofGetFuncBase alternative16 andGetFunc419 alternative8; distinct from actual selectedGetFunc path',publication_policy='Source review supports actualselected8B access; modify widthauthority only throughexplicitselected-accessor evidence, not overwriting historical16Bfixture result'),validation=dict(instruction_windows_checked=len(E),all_instruction_bytes_exact=True,source_mutations=0,console_network_account_actions=0,runtime_tests=0),diagnostic_limits=['rdis.py final calls footer accidentally addsDELTA twice tocallsite; useindividual instructionrecords only.','UObjectBaseRegister.asm was exploratory decode from485590 insideinstruction48558f; retain it as rejected/non-evidence. Correctfunction485530 dumpisUObjectBaseRegister-confirmed.asm.'])
OUT.joinpath('STATIC-FACTS.json').write_text(json.dumps(result,indent=2)+'\n')
OUT.joinpath('SOURCE-HASHES.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(dict(image_sha256=digest,instruction_windows_checked=len(E),facts=len(facts),unresolved=len(unresolved),static_facts_sha256=hashlib.sha256(OUT.joinpath('STATIC-FACTS.json').read_bytes()).hexdigest())))
