"""Read the actual ELF and target-compiled AOT layout; never execute target code."""
from pathlib import Path
import hashlib,json,os,re,struct,subprocess
H=Path(__file__).resolve().parents[1]; W=H/'cross-work'; O=H/'artifacts/mscorlib.full-interp.ps5.o'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
inv=json.loads((H.parent/'cap10-port-research/LOCAL-INVENTORY.json').read_text())
llvm=Path(inv['llvm_bin']); cc=Path(inv['files'][0]['path'])
env=os.environ.copy();env['LLVM_CONFIG']=str(llvm/'llvm-config')
def run(label,cmd):
 r=subprocess.run(cmd,env=env,cwd=H,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 with (H/'logs'/f'{label}.log').open('x') as f:f.write(r.stdout)
 if r.returncode: raise ValueError((label,r.returncode,r.stdout[-2000:]))
 return r.stdout
fields=['version','flags','nmethods','nextra_methods','double_align','long_align','card_table_shift_bits','card_table_mask','aotid']
probe=H/'artifacts/aot-file-layout.c';ir=H/'artifacts/aot-file-layout.ll'
probe.write_text('#define MONO_GENERATING_OFFSETS 1\n#include "mono/mini/mini-cross-helpers.c"\n#include "mono/mini/aot-runtime.h"\n'+''.join(f'const unsigned long off_{f}=__builtin_offsetof(MonoAotFileInfo,{f});\n' for f in fields))
opts=json.loads((H/'evidence/TARGET-COMPILE-OPTIONS.json').read_text())['cppflags']
cmd=[str(cc),'-std=gnu99',*opts,*['-I'+str(p) for p in [H/'target-config',H/'target-config/mono/eglib',W,W/'mono',W/'mono/eglib']],'-S','-emit-llvm','-O0',str(probe),'-o',str(ir)]
run('AOT-LAYOUT',cmd);irt=ir.read_text();assert 'target triple = "x86_64-sie-ps5"' in irt
layout={k:int(v) for k,v in re.findall(r'@off_(\w+) = .*?constant i64 (\d+)',irt)};assert set(layout)==set(fields)
# ELF64 little-endian section and symbol tables; source layout comes from the target compiler.
b=O.read_bytes();assert b[:7]==b'\x7fELF\x02\x01\x01';etype,machine=struct.unpack_from('<HH',b,16);assert(etype,machine)==(1,62)
shoff=struct.unpack_from('<Q',b,40)[0];shsize,shnum,shstr=struct.unpack_from('<HHH',b,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shsize) for i in range(shnum)]
def section(i):s=sections[i];return b[s[4]:s[4]+s[5]]
def string(buf,start):return buf[start:buf.index(b'\0',start)].decode()
names=section(shstr); symbols={};symbol_indices={}
for sec in sections:
 if sec[1]!=2:continue
 strings=section(sec[6])
 for i,off in enumerate(range(sec[4],sec[4]+sec[5],sec[9])):
  name,info,other,idx,value,size=struct.unpack_from('<IBBHQQ',b,off)
  sym={'name':string(strings,name),'bind':info>>4,'type':info&15,'section':idx,'value':value,'size':size}
  symbol_indices[i]=sym
  if sym['name']:symbols[sym['name']]=sym
s=symbols['mono_aot_file_info'];start=sections[s['section']][4]+s['value']
values={f:struct.unpack_from('<I',b,start+offset)[0] for f,offset in layout.items() if f!='aotid'}
assert values['version']==172 and values['flags']&290==290
assert values['double_align']==8 and values['long_align']==8
export=symbols['mono_aot_module_mscorlib_info'];assert export['bind']==1 and export['type']==1
undefined=[s['name'] for s in symbols.values() if s['section']==0];assert not undefined
# The exported value is a relocation to the real file-info record, not its address.
registration_reloc=[]
for sec in sections:
 if sec[1]!=4 or sec[7]!=export['section']:continue
 for off in range(sec[4],sec[4]+sec[5],sec[9]):
  where,info,addend=struct.unpack_from('<QQq',b,off)
  if where==export['value']:registration_reloc.append({'symbol':symbol_indices[info>>32],'addend':addend,'type':info&0xffffffff})
assert len(registration_reloc)==1
rr=registration_reloc[0];assert rr['type']==1 and rr['symbol']['section']==s['section'] and rr['symbol']['value']+rr['addend']==s['value']
def symbol_string(name):
 s=symbols[name];return string(b,sections[s['section']][4]+s['value'])
header=run('ELF-HEADER',[str(llvm/'llvm-readelf'),'-h',str(O)])
extern=run('ELF-EXTERNAL-SYMBOLS',[str(llvm/'llvm-nm'),'--extern-only',str(O)])
chosen=['generic_trampoline_jit','generic_trampoline_aot','System_Threading_Thread_GetCurrentThread','wrapper_managed_to_native_object___icall_wrapper_mono_tls_get_jit_tls_extern','wrapper_managed_to_native_object___icall_wrapper_mono_tls_get_lmf_addr_extern']
for sym in chosen:assert sym in symbols
excerpt=run('TLS-DISASSEMBLY',[str(llvm/'llvm-objdump'),'-d','--no-show-raw-insn','--disassemble-symbols='+','.join(chosen),str(O)])
assert not re.search(r'%[fg]s:',excerpt)
# Full text includes embedded data; report raw segment decodes without pretending all .text is code.
raw=[];current=None
with (H/'artifacts/mscorlib.full-interp.ps5.disassembly.txt').open() as f:
 for line in f:
  m=re.match(r'^([0-9a-f]+) <(.*)>:',line)
  if m:current=m.group(2)
  if re.search(r'%[fg]s:',line):raw.append({'enclosing_label':current,'instruction_text':line.strip(),'address':int(line.split(':',1)[0].strip(),16)})
early=[r for r in raw if r['address']<symbols['jit_code_end']['value']]
assert len(early)==3 and all(r['enclosing_label']=='System_Globalization_CalendricalCalculationsHelper_SumLongSequenceOfPeriodicTerms_double' for r in early)
config=(W/'config.h').read_text();assert '#define MONO_CROSS_COMPILE 1' in config and '#define TARGET_AMD64 1' in config
assert '#define TARGET_MACH 1' not in config and '#define MONO_KEYWORD_THREAD ' not in config
result={'object_sha256':sha(O),'object_bytes':len(b),'elf':{'class':'ELF64','endianness':'little','type':'REL','machine':'x86-64','osabi':b[7]},'source_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','compiler_sha256':sha(W/'mono/mini/mono-sgen'),'target_layout_command':cmd,'target_layout_ir_sha256':sha(ir),'file_info_offsets':layout,'file_info':values,'aotid_bytes_hex':b[start+layout['aotid']:start+layout['aotid']+16].hex(),'assembly_guid':symbol_string('assembly_guid'),'assembly_name':symbol_string('assembly_name'),'runtime_version_string':symbol_string('runtime_version'),'export':export,'registration_pointer_relocation':registration_reloc,'undefined_symbols':undefined,'tls':{'selected_symbols':chosen,'segment_operands_in_selected_functions':0,'raw_full_text_segment_decodes':len(raw),'decodes_before_jit_code_end':early,'interpretation':'The three early raw segment decodes are inside the calendrical method embedded constant pool after terminal return; remaining raw decodes are tables following jit_code_end. The selected real getter/trampoline instructions use calls and no FS/GS operands. This scan is not a formal control-flow proof.'},'inspection_tools':{str(p):sha(p) for p in [llvm/'llvm-readelf',llvm/'llvm-nm',llvm/'llvm-objdump',cc]},'target_execution':False}
with (H/'evidence/ARTIFACT-VERIFICATION.json').open('x') as f:json.dump(result,f,indent=2);f.write('\n')
print(json.dumps({'file_info':values,'assembly_guid':result['assembly_guid'],'undefined_symbols':undefined,'selected_tls_functions':len(chosen),'raw_segment_decodes':len(raw),'early_segment_decodes':len(early)},indent=2))
