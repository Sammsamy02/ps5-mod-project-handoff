#!/usr/bin/env python3
"""Read-only reproduction of saved Townfall allocator evidence; never connects to a console.
Requires Python 3 and Capstone. All outputs stay below this script's directory.
Numerical slots are measured from ELF RELA. Semantic names are separately qualified.
No runtime build, no profile mutation, no automatic +8 ABI conversion.
"""
import argparse, hashlib, json, re, struct
from pathlib import Path
from collections import Counter
import capstone
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_REG
from capstone.x86 import X86_REG_RIP

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
P = '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/'
IMAGE = '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
EXPECTED = '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
VT, GM, INSTANCE, BASE = 0x7ec26e0, 0x930add8, 0x93089b8, 0x400000
SELECTION = 'outputs/2026-10-08-menu-complete-build/SOURCE-SELECTION.json'
FACTS = 'outputs/2026-10-07-l19-local-profile-r4/profiles/townfall-build-facts.json'
OLD = 'outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/GMALLOC-CANDIDATES.json'
LIVE = 'outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/UE4SS.log'
BARE = 'outputs/2026-10-08-menu-complete-package/pass-1/bare-sdk/modbridge_supplier_composition.prx'
SOURCE_PATHS = [P+'src/FMemory.cpp', P+'src/Core/HAL/UnrealMemory.cpp', P+'include/Unreal/FMemory.hpp', P+'generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp', 'outputs/2026-10-07-l19-local-profile-r4/units/SeedValidation.hpp', 'outputs/2026-10-07-l19-local-profile-r4/units/NativeLayoutBinding.cpp', 'outputs/2026-10-07-l19-r4-official-engine-source/SOURCE-RECEIPT.json', FACTS, OLD, LIVE, SELECTION, BARE, 'outputs/2026-10-08-menu-complete-package/FINAL-RECEIPT.json', 'outputs/2026-10-08-menu-complete-package/pass-1/modbridge_supplier_composition.prx']

def pin(path):
    path = Path(path)
    if not path.is_absolute(): path = ROOT / path
    data = path.read_bytes()
    return {'path':str(path),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out',default='measured'); args=ap.parse_args()
    out=(HERE/args.out).resolve()
    if not out.is_relative_to(HERE) or out == HERE: raise SystemExit('Output must be a subdirectory of allocator/')
    out.mkdir(parents=True,exist_ok=True)
    def save(name, obj): (out/name).write_text(json.dumps(obj,indent=2)+'\n')
    data=(ROOT/IMAGE).read_bytes()
    image_pin=pin(IMAGE); assert image_pin['sha256']==EXPECTED and len(data)==194241424
    assert data[:6] == b'\x7fELF\x02\x01'
    phoff=struct.unpack_from('<Q',data,32)[0]; entsize,n=struct.unpack_from('<HH',data,54)
    ph=[]
    for i in range(n):
        p=struct.unpack_from('<IIQQQQQQ',data,phoff+i*entsize)
        ph.append(dict(zip(['type','flags','offset','vaddr','paddr','filesz','memsz','align'],p),index=i))
    loads=[p for p in ph if p['type']==1]
    def segment(va): return next((p for p in loads if p['vaddr'] <= va < p['vaddr']+p['memsz']),None)
    def fileoff(va,n=1):
        p=next((p for p in loads if p['vaddr']<=va and va+n<=p['vaddr']+p['filesz']),None)
        if p is None: raise ValueError('Not file-backed: '+hex(va))
        return p['offset']+va-p['vaddr']
    def read(va,n):
        o=fileoff(va,n); return data[o:o+n]
    dyn=next(p for p in ph if p['type']==2)
    tags={}
    for o in range(dyn['offset'],dyn['offset']+dyn['filesz'],16):
        t,v=struct.unpack_from('<qQ',data,o)
        if t==0: break
        tags[t]=v
    assert tags[9]==24
    roff=fileoff(tags[7],tags[8]); reloc={}; types=Counter()
    for o in range(roff,roff+tags[8],24):
        va,info,add=struct.unpack_from('<QQq',data,o); types[info&0xffffffff]+=1
        if VT-16 <= va < VT+0x120: reloc[va]={'file_offset':hex(o),'r_offset':hex(va),'r_info':hex(info),'type':info&0xffffffff,'symbol_index':info>>32,'addend':add}
    slots=[]
    for delta in range(0,0x100,8):
        r=reloc.get(VT+delta); assert r and r['type']==8 and r['symbol_index']==0
        t=r['addend']; p=segment(t); assert p and p['flags']&1
        slots.append({'byte_offset':hex(delta),'index':delta//8,'linked_slot_address':hex(VT+delta),'raw_file_qword':hex(struct.unpack('<Q',read(VT+delta,8))[0]),'relocation':r,'linked_target':hex(t),'expected_target_at_saved_base':hex(t+BASE),'target_pt_load':p['index'],'target_pt_load_flags':p['flags'],'target_file_offset':hex(fileoff(t))})
    md=Cs(CS_ARCH_X86,CS_MODE_64); md.detail=True
    def dis(va,size): return list(md.disasm(read(va,size),va))
    def insrow(ins): return {'linked_address':hex(ins.address),'file_offset':hex(fileoff(ins.address)), 'bytes':ins.bytes.hex(),'mnemonic':ins.mnemonic,'op_str':ins.op_str}
    def dis_text(rows):return '\n'.join(f"{x['linked_address']:>10} file={x['file_offset']:>10} {x['bytes']:<28} {x['mnemonic']} {x['op_str']}" for x in rows)
    refs=[]
    pattern=re.compile(rb'[\x48-\x4f][\x8b\x89\x8d][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]')
    targets={GM,INSTANCE,VT}
    for p in loads:
        if not p['flags']&1: continue
        b=data[p['offset']:p['offset']+p['filesz']]
        for m in pattern.finditer(b):
            pos=m.start()
            if pos+7>len(b): continue
            va=p['vaddr']+pos; target=va+7+struct.unpack_from('<i',b,pos+3)[0]
            if target not in targets: continue
            inst=next(md.disasm(b[pos:pos+15],va),None)
            if not inst or not any(op.type==CS_OP_MEM and op.mem.base==X86_REG_RIP and inst.address+inst.size+op.mem.disp==target for op in inst.operands):continue
            row={'target':hex(target),'instruction':insrow(inst),'first_virtual_dispatch':None}
            if target==GM and inst.mnemonic=='mov' and inst.operands[0].type==CS_OP_REG:
                regs={inst.reg_name(inst.operands[0].reg):'instance'}; seq=[]
                for q in dis(va,128):
                    seq.append(insrow(q))
                    if q.mnemonic in ('call','jmp'):
                        op=q.operands[0]
                        if op.type==CS_OP_MEM and regs.get(q.reg_name(op.mem.base))=='vptr': row['first_virtual_dispatch']={'instruction':insrow(q),'byte_offset':hex(op.mem.disp),'index':op.mem.disp//8}
                        break
                    if q.mnemonic in ('ret','int3'):break
                    if q.address==va:continue
                    if q.mnemonic=='mov' and len(q.operands)==2 and q.operands[0].type==CS_OP_REG:
                        dst=q.reg_name(q.operands[0].reg); src=q.operands[1]; value=None
                        if src.type==CS_OP_REG:value=regs.get(q.reg_name(src.reg))
                        elif src.type==CS_OP_MEM and src.mem.disp==0 and regs.get(q.reg_name(src.mem.base))=='instance':value='vptr'
                        regs[dst]=value
                row['load_to_first_call_or_return']=seq
            refs.append(row)
    ranges={
        'factory_and_gmalloc_store':(0xb3f0,0x700), 'constructor_vptr_stores':(0xafd40,0xb0),
        'slot0_destruction_shape':(0xb1070,0x3d0), 'slot1_destroy_then_deallocate_shape':(0xb1440,0x20),
        'deallocation_forwarder':(0xaf390,0x10),'public_allocator_thunks':(0xddc0,0x1a0),
        'malloc_shape':(0xb48e0,0xd0), 'realloc_shape':(0xb49b0,0x240), 'free_shape':(0xb4bf0,0xe0),
        'quantize_shape':(0xb4cd0,0xe0),'allocation_size_shape':(0xb4db0,0x40),
        'default_malloc_realloc_zeroed_forwarders':(0xb8a20,0x50),
        'tls_shape_cluster':(0xb4df0,0x110),'trim_bool_shape':(0xb2820,0x10),
        'get_descriptive_string_shape':(0xb2810,0x10), 'return_false_shared':(0xe0,0x10),
        'return_true_shared':(0x120,0x10),'void_return_shared':(0xb290,0x10),
    }
    function_evidence={}; combined=[]
    for name,(va,size) in ranges.items():
        rows=[insrow(i) for i in dis(va,size)]
        function_evidence[name]={'linked_start':hex(va),'size':size,'sha256':hashlib.sha256(read(va,size)).hexdigest(),'instructions':rows}
        combined.append('\n## '+name+'\n'+dis_text(rows))
    string_ins=list(dis(0xb2810,0xd))[2]
    string_va=string_ins.address+string_ins.size+string_ins.operands[1].mem.disp
    sb=read(string_va,64); text=sb.decode('utf-16le').split('\0')[0]
    assert text=='Binned3'
    string_evidence={'linked_address':hex(string_va),'file_offset':hex(fileoff(string_va)),'encoding':'UTF-16LE','value':text,'bytes':read(string_va,(len(text)+1)*2).hex()}
    (out/'image-disassembly.txt').write_text('\n'.join(combined)+'\n')
    save('image-functions.json',function_evidence)
    save('image-measurements.json',{'image':image_pin,'capstone_version':capstone.__version__,'load_base_from_saved_profile':hex(BASE),'address_convention':'linked VA in ELF; historical runtime expected = linked VA + 0x400000; file offset is calculated through PT_LOAD, not generally equal to VA','program_headers':ph,'dynamic_rela':{'vaddr':hex(tags[7]),'file_offset':hex(roff),'size':tags[8],'entry_size':tags[9],'types':dict(types)},'globals':{'gmalloc_linked':hex(GM),'instance_linked':hex(INSTANCE),'vtable_linked_address_point':hex(VT),'gmalloc_file_backed':False,'instance_file_backed':False,'expected_gmalloc_at_saved_base':hex(GM+BASE),'expected_instance_at_saved_base':hex(INSTANCE+BASE),'expected_vtable_at_saved_base':hex(VT+BASE)},'slots':slots,'direct_rip_reference_census':refs,'reference_census_limit':'REX.W MOV/LEA RIP-relative encoding scan, exact Capstone decode at each candidate; this is not an exhaustive whole-program CFG or alias analysis. It does not prove method names. Stops first call/return.','descriptive_name_literal':string_evidence,'no_live_read':True})
    selection=json.loads((ROOT/SELECTION).read_text())
    selected={str((ROOT/x['path']).resolve()) if not Path(x['path']).is_absolute() else x['path']:x for x in selection['source_pins']}
    pins=[pin(x) for x in SOURCE_PATHS]+[image_pin,pin(__file__)]
    for p in pins:
        if p['path'] in selected:p['matches_build_selection']=p['sha256']==selected[p['path']]['sha256'];assert p['matches_build_selection']
    wrapper=(ROOT/(P+'src/FMemory.cpp')).read_text()
    methods=re.findall(r'IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(?:_NO_PARAMS)?\(FMalloc, (\w+)',wrapper)
    generated=(ROOT/(P+'generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp')).read_text()
    old_offsets={m:int(v,16) for m,v in re.findall(r'emplace\(STR\("([^"]+)"\), (0x[0-9a-fA-F]+)\)',generated)}
    source_evidence=[]; calls={m:[] for m in methods}
    for relative in [P+'src/FMemory.cpp',P+'src/Core/HAL/UnrealMemory.cpp',P+'include/Unreal/FMemory.hpp',P+'generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp']:
        for lineno,line in enumerate((ROOT/relative).read_text().splitlines(),1):
            if 'FMalloc' in line or any(m in line for m in methods):source_evidence.append({'path':str(ROOT/relative),'line':lineno,'text':line})
            m=re.search(r'\(\*GMalloc\)->(\w+)\(',line)
            if m and m[1] in calls:calls[m[1]].append({'path':str(ROOT/relative),'line':lineno,'text':line.strip()})
    save('source-excerpts.json',source_evidence)
    # These six associations use byte-level behavior, never arithmetic from PC-generated slots.
    assoc={
        'Malloc':(0x30,'malloc_shape','GMalloc call shape passes Count in RSI and Alignment in EDX; target removes a free-list entry and returns a pointer; slow-path call0xb1460.'),
        'Realloc':(0x40,'realloc_shape','GMalloc dispatch preserves pointer/Count/Alignment as RSI/RDX/ECX; target chooses allocation, copies bounded content, returns/retains pointer and releases old block.'),
        'Free':(0x50,'free_shape','GMalloc dispatch moves one pointer into RSI; target puts that block on a free list; second public wrapper is null-guarded.'),
        'QuantizeSize':(0x68,'quantize_shape','Target rounds Count with alignment and bin tables then returns RAX, with no allocation or output-pointer write in bounded complete body.'),
        'GetAllocationSize':(0x70,'allocation_size_shape','Target derives size from Original in RSI, stores qword [RDX], then sets AL=1; exceptional path calls0xb5340.'),
        'GetDescriptiveName':(0xd0,'get_descriptive_string_shape','Target returns a RIP-relative UTF-16 pointer containing Binned3.'),
    }
    limitations={
        'TryMalloc':'Slot0x38 forwards to0x30, but TryMalloc naming/order lacks authoritative declaration; a forwarding slot is not uniquely named.',
        'TryRealloc':'Slot0x48 forwards to0x40, but TryRealloc naming/order lacks authoritative declaration.',
        'Trim':'Slot0x78 tests ESI then calls helper if true; consistent with Trim(bool), insufficient unique naming without source.',
        'SetupTLSCachesOnCurrentThread':'TLS-shaped target at slot0x80 initializes/retrieves state; no exact source-to-symbol mapping.',
        'ClearAndDisableTLSCachesOnCurrentThread':'TLS-shaped target at slot0x98 calls0xb2ae0(false) then0xb5f20; no exact source-to-symbol mapping.',
        'InitializeStatsMetadata':'Several no-op slots share target0xb290; body cannot distinguish names or order.',
        'UpdateStats':'Several no-op slots share target0xb290; body cannot distinguish names or order.',
        'GetAllocatorStats':'Several no-op slots share target0xb290; body cannot distinguish names or order.',
        'DumpAllocatorStats':'Slot0xb8 has a larger output-shaped function0xb2c00, but no authoritative declaration or independent symbol binding.',
        'IsInternallyThreadSafe':'Slots0xc0 and0xc8 share return-true target0x120; cannot distinguish names.',
        'ValidateHeap':'Slots0xc0 and0xc8 share return-true target0x120; cannot distinguish names.',
    }
    entries=[]
    for method in methods:
        evidence=assoc.get(method); slot=next((s for s in slots if evidence and int(s['byte_offset'],16)==evidence[0]),None)
        entries.append({'method':method,'linked_wrapper':True,'source_gmalloc_calls':calls[method],'source_caller_status':'direct GMalloc call found' if calls[method] else 'wrapper emitted; no direct GMalloc call in inspected UnrealMemory source; reachability unproved','declared_ue561_shipping_offset':None,'image_semantic_candidate_offset':hex(evidence[0]) if evidence else None,'measured_slot':slot,'image_semantics':evidence[2] if evidence else limitations[method],'image_citations':['image-functions.json#'+evidence[1],'image-measurements.json#slots'] if evidence else ['image-measurements.json#slots','image-disassembly.txt'],'generated_pc_offset_not_native_evidence':hex(old_offsets[method]),'generated_pc_source':str(ROOT/(P+'generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp')),'status':'IMAGE_SEMANTICS_SUPPORTED__DECLARATION_UNRESOLVED' if evidence else 'UNRESOLVED_SEMANTIC_SLOT','approved_for_profile':False})
    assert len(entries)==17
    bare=(ROOT/BARE).read_bytes(); literal_set=set()
    for raw in re.findall(rb'Virtual FMalloc::[A-Za-z0-9_]+ is unavailable',bare):literal_set.add(raw.decode().split('::')[1].split()[0])
    assert set(methods)==literal_set, 'ELF FMalloc error literals differ from source wrapper set'
    save('candidate-table.json',{'scope':'Saved Townfall Shipping Prospero x86_64 image with pinned SHA and build provenance only; proposed UE5.6 Shipping ABI remains unresolved and is not unconditional other-game proof.','authoritative_engine_declaration_status':'NOT_AVAILABLE_IN_INSPECTED_LOCAL_CACHE','runtime_wrapper_count':len(entries),'bare_error_literal_methods':sorted(literal_set),'bare_literal_set_matches_source_wrappers':set(methods)==literal_set,'ready_for_build':False,'entries':entries})
    live=[]
    for n,line in enumerate((ROOT/LIVE).read_text(errors='replace').splitlines(),1):
        normalized=line.replace('\\u0000','').replace('\0','')
        if 'gmalloc bound' in normalized:live.append({'path':str(ROOT/LIVE),'line':n,'normalized':normalized})
    save('source-pins.json',{'pins':pins,'live_observation_excerpts':live,'live_evidence_limit':'Saved V1 instance and V2 vptr equality log only. Source SeedValidation.hpp says actual vtable entries were never dereferenced at this gate. RELA targets and PT_LOAD flags are file evidence; no present-run slot bytes or actual call success measured.','build_provenance':json.loads((ROOT/FACTS).read_text())['layout']})
    checks={'image_hash_pass':True,'source_pins_match_selection':all(p.get('matches_build_selection',True) for p in pins),'measured_executable_slots':len(slots),'direct_gmalloc_loads':sum(r['instruction']['mnemonic']=='mov' and r['target']==hex(GM) and 'load_to_first_call_or_return' in r for r in refs),'direct_gmalloc_dispatches':sum(r['first_virtual_dispatch'] is not None for r in refs),'dispatch_offset_counts':dict(Counter(r['first_virtual_dispatch']['byte_offset'] for r in refs if r['first_virtual_dispatch'])),'all_17_wrappers_accounted_for':len(entries)==17 and set(methods)==literal_set,'native_offsets_source_proved':0,'build_allowed_by_this_report':False}
    save('verification.json',checks)
    print(json.dumps(checks,indent=2))
if __name__=='__main__':main()
