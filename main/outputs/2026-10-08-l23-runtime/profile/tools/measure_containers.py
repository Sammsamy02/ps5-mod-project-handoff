#!/usr/bin/env python3
"""Read-only authenticated saved-image container facts. Never imports source layout values."""
from image_lib import *
import json
OUT=Path(__file__).resolve().parents[1]/'evidence'
E={};ranges={};bindings=[];relocs=[]
def capture(name,a,n):
 rows=[]
 for i in dis(a,n):
  row={'address':hex(i.address),'file_offset':hex(off(i.address,i.size)),'bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str}
  E[i.address]=row;rows.append(row)
 ranges[name]={'start':hex(a),'length':n,'sha256':hashlib.sha256(read(a,n)).hexdigest(),'instructions':rows}
def require(a,s):assert E[a]['instruction']==s,(hex(a),E[a],s)
def rip(a):
 i=dis(a,7)[0];return i.address+i.size+i.operands[1].mem.disp
def text_at(a,s):assert read(a,len((s+'\0').encode('utf-16le')))==(s+'\0').encode('utf-16le')
for name,desc,cb,leav,vptr,size,end in [('FMapProperty',0x40abf5,0x40aa80,0x40aad7,0x7ef0ff0,0xa0,0x40ac41),('FSetProperty',0x42499d,0x424830,0x424887,0x7ef31d0,0x90,0x4249e9),('FArrayProperty',0x3e51e6,0x3e5930,0x3e5987,0x7eea620,0x80,0x3e5a5c)]:
 capture(name+'_descriptor',desc,0x27 if name=='FArrayProperty' else 0x20)
 capture(name+'_constructor',cb,end-cb)
 na=rip(desc);text_at(na,name)
 callback_lea=desc+(14 if name=='FArrayProperty' else 7)
 assert rip(callback_lea)==cb and rip(leav)==vptr
 call=dis(desc+ (0x22 if name=='FArrayProperty' else 0x1b),5)[0];assert call.mnemonic=='call' and call.op_str=='0x363990'
 bindings.append({'class':name,'name_literal_va':hex(na),'name_literal_file':hex(off(na)),'name_literal_bytes':read(na,(len(name)+1)*2).hex(),'descriptor':hex(desc),'registered_callback':hex(cb),'constructor_vptr_lea':hex(leav),'vptr':hex(vptr),'allocation_size_context_only':size})
 for slot in (0x68,0xf8,0x140):
  t,o=R[vptr+slot];assert any(p[0]==1 and p[1]&1 and p[3]<=t<p[3]+p[5] for p in P)
  relocs.append({'class':name,'slot':hex(slot),'target':hex(t),'rela_file_offset':hex(o),'rela_bytes':B[o:o+24].hex()})
for name,a,n in [('common_descriptor',0x363990,0x68),('map_copy',0x411220,0x331),('map_rehash_callback',0x42cd10,0x14),('property_hash_forwarder',0x3e5700,0xf),('bucket_rebuild',0x32f0f0,0x210),('map_type_name',0x40fd50,0x11a),('set_type_name',0x426820,0xdc),('set_copy',0x427a70,0x27b),('set_destroy',0x427e50,0x190),('array_copy',0x3ea490,0x100),('array_destroy',0x3ea720,0xb0)]:capture(name,a,n)
checks={0x3639d2:'mov qword ptr [rdi + 0x30], r9',0x41124c:'vmovups xmm0, xmmword ptr [rdi + 0x70]',0x41126e:'vmovups xmmword ptr [rbp - 0x70], xmm0',0x411450:'mov rdi, qword ptr [rax + 0x70]',0x411494:'mov rdi, qword ptr [rax + 0x78]',0x41146b:'call qword ptr [rax + 0xf8]',0x41131c:'mov ecx, dword ptr [rdi + 0x2c]',0x411328:'call qword ptr [rax + 0xf8]',0x4114d4:'lea rax, [rbp - 0x70]',0x4114e8:'mov qword ptr [rbp - 0x88], rax',0x4114f7:'mov qword ptr [rbp - 0x78], rcx',0x4114fb:'call 0x32f0f0',0x42cd14:'mov rax, qword ptr [rdi]',0x42cd1a:'mov rdi, qword ptr [rax]',0x42cd1d:'call 0x3e5700',0x3e5707:'call qword ptr [rax + 0x100]',0x32f289:'call qword ptr [rax]',0x32f299:'and rax, rcx',0x32f2d0:'mov dword ptr [rcx + rax*4], ebx',0x40fd80:'mov rsi, qword ptr [rsi + 0x70]',0x40fd9e:'mov rsi, qword ptr [r13 + 0x78]',0x40fdc5:'mov rdx, r12',0x40fdc8:'mov rcx, r15',0x427aa4:'mov rax, qword ptr [rdi + 0x70]',0x427bd8:'mov rdi, qword ptr [rax + 0x70]',0x427b2a:'call qword ptr [rax + 0xf8]',0x427e7f:'mov rdx, qword ptr [rdi + 0x70]',0x427fc5:'call qword ptr [rax + 0x140]',0x42684c:'mov rsi, qword ptr [rsi + 0x70]',0x3ea4b8:'mov rdi, qword ptr [rdi + 0x78]'}
for a,s in checks.items():require(a,s)
assert rip(0x4114da)==0x42cd10
strings=[]
for a,s in [(0x40fdbe,'%s,%s'),(0x40fe32,'TMAP'),(0x42686d,'%s'),(0x4268cd,'TSET')]:
 t=rip(a);text_at(t,s);strings.append({'reference':hex(a),'address':hex(t),'text':s,'bytes':read(t,(len(s)+1)*2).hex()})
facts=[
 {'name':'FMapProperty::KeyProp','offset':0x70,'width':8,'classification':'The named Map constructor installs the relocated copy vtable. Copy reads the pair at +0x70 into a helper; rehash callback dereferences the first helper pointer and invokes the property operation whose result masks into hash buckets. Only this first nested property selects bucket placement, independently distinguishing key from value. TMAP type formatter also places +0x70 first in %s,%s.'},
 {'name':'FMapProperty::ValueProp','offset':0x78,'width':8,'classification':'Named Map copy loads +0x78 as its second nested FProperty, consumes its ArrayDim, ElementSize and Offset_Internal and calls the established CopyValuesInternal slot. It is the second TMAP type-format argument; the +0x70 nested property independently supplies map bucket hash. Thus +0x78 is value, not a guessed order.'},
 {'name':'FSetProperty::ElementProp','offset':0x70,'width':8,'classification':'Named Set constructor binds vptr; its copy and destroy relocated slots read the only nested property at +0x70, consume FProperty flags/offset/size, dispatch CopyValuesInternal/DestroyValueInternal. TSET formatter has the same single property type argument.'},
 {'name':'FArrayProperty::Inner','offset':0x78,'width':8,'classification':'Calibration: named Array constructor and relocated copy slot freshly verified; copy loads the nested property from +0x78, agreeing with retained authenticated subtype proof.'}]
result={'format':'l23-container-image-evidence-v1','status':'STATIC_IMAGE_MEASURED_NOT_SOURCE_PROMOTED','image':{'path':str(IMAGE),'sha256':SHA,'size':len(B)},'address_model':'Linked ELF VA; file offsets use PT_LOAD. No live memory reads.','bindings':bindings,'relocations':relocs,'strings':strings,'facts':facts,'asserted_instruction_count':len(checks),'code_ranges':'image-functions.json','official_header_evidence_used':False,'no_live_success_claimed':True}
OUT.mkdir(exist_ok=True)
(OUT/'image-functions.json').write_text(json.dumps(ranges,indent=2)+'\n')
(OUT/'image-measurements.json').write_text(json.dumps(result,indent=2)+'\n')
(OUT/'image-disassembly.txt').write_text('\n\n'.join('## '+n+'\n'+'\n'.join(r['address']+' file='+r['file_offset']+' '+r['bytes']+' '+r['instruction'] for r in v['instructions']) for n,v in ranges.items())+'\n')
print(json.dumps({'status':'PASS','facts':[{k:x[k] for k in ('name','offset','width')} for x in facts],'asserted_instructions':len(checks)}))
