#!/usr/bin/env python3
"""Read real ELF/ar relocation tables, imports and retained owner callsites."""
from pathlib import Path
import collections,hashlib,json,os,re,struct,subprocess
HERE=Path(__file__).resolve().parents[1];local=json.loads((HERE/'LOCAL-INPUTS.json').read_text());llvm=Path(local['llvm_bin']);sdk=Path(local['sdk']);work=HERE/'target-work'
env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def command(name,cmd):
 r=subprocess.run(cmd,env=env,text=True,capture_output=True)
 (HERE/'evidence'/(name+'.txt')).write_text(r.stdout+r.stderr)
 assert r.returncode==0,(name,r.stderr)
 return dict(command=cmd,exit_code=r.returncode,output_sha256=sha(HERE/'evidence'/(name+'.txt'))),r.stdout+r.stderr
mode,mode_text=command('COMPILER-MODE',[str(sdk/'bin/prospero-clang'),'-###','-DHAVE_CONFIG_H','-DHOST_BSD','-DGC_FREEBSD_THREADS','-D_WITH_GETLINE','-O1','-fvisibility=hidden','-std=gnu99','-fno-strict-aliasing','-fwrapv','-I'+str(work),'-I'+str(work/'mono/eglib'),'-c',str(work/'mono/utils/mono-tls.c'),'-o',str(HERE/'probe/compile-mode-unwritten.o')])
assert '"-mrelocation-model" "pic"' in mode_text and '"-pic-level" "2"' in mode_text
corlib=HERE.parent/'cap10-cross-aot/artifacts/mscorlib.full-interp.ps5.o';archive=work/'mono/mini/.libs/libmonosgen-2.0.a';exe=HERE/'owner-link/mono-owner-link-only.elf'
aotmode,_=command('CORLIB-ASSEMBLER-MODE',[str(sdk/'bin/prospero-clang'),'-###','-c',str(corlib.with_suffix('.s')),'-o',str(HERE/'probe/corlib-mode-unwritten.o')])
callsite,disasm=command('OWNER-ROOTING-RELOCATIONS',[str(llvm/'llvm-objdump'),'-dr',str(HERE/'owner-link/MonoOwner.o'),str(HERE/'owner-link/InvocationBoundary.o')])
required=['mono_aot_module_mscorlib_info','mono_aot_register_module','mono_threads_enter_gc_unsafe_region','mono_threads_exit_gc_unsafe_region','mono_gchandle_new','mono_gchandle_free']
for symbol in required:assert re.search(r'R_X86_64_\w+\s+'+symbol+r'(?:-|\s)',disasm),symbol
names={1:'R_X86_64_64',2:'R_X86_64_PC32',4:'R_X86_64_PLT32',6:'R_X86_64_GLOB_DAT',7:'R_X86_64_JUMP_SLOT',8:'R_X86_64_RELATIVE',9:'R_X86_64_GOTPCREL',10:'R_X86_64_32',11:'R_X86_64_32S',24:'R_X86_64_PC64',41:'R_X86_64_GOTPCRELX',42:'R_X86_64_REX_GOTPCRELX'}
def objects(path):
 data=path.read_bytes()
 if data.startswith(b'\x7fELF'):return [(path.name,data)]
 assert data.startswith(b'!<arch>\n');pos=8;out=[];longnames=b''
 while pos<len(data):
  h=data[pos:pos+60];assert h[58:60]==b'`\n';size=int(h[48:58]);blob=data[pos+60:pos+60+size];raw=h[:16].decode().strip();pos+=60+size+(size%2)
  if raw=='//':longnames=blob;continue
  if raw.startswith('#1/'):
   n=int(raw[3:]);name=blob[:n].decode().rstrip('\0');blob=blob[n:]
  elif raw.startswith('/') and raw[1:].isdigit():name=longnames[int(raw[1:]):].split(b'/\n',1)[0].decode()
  else:name=raw.rstrip('/')
  if blob.startswith(b'\x7fELF'):out.append((name,blob))
 return out

def census(path):
 counts=collections.Counter();alloc=collections.Counter();nonalloc=collections.Counter();absolute=collections.Counter();members=objects(path)
 for name,data in members:
  assert data[4:6]==b'\x02\x01','ELF64 LE expected'
  shoff=struct.unpack_from('<Q',data,40)[0];entsize,num,stridx=struct.unpack_from('<HHH',data,58)
  sections=[struct.unpack_from('<IIQQQQIIQQ',data,shoff+i*entsize) for i in range(num)]
  st=sections[stridx];strings=data[st[4]:st[4]+st[5]]
  def sname(sh):return strings[sh[0]:].split(b'\0',1)[0].decode()
  for sh in sections:
   if sh[1] not in [4,9]:continue
   target=sections[sh[7]];allocated=bool(target[2]&2)
   for off in range(sh[4],sh[4]+sh[5],sh[9]):
    _,info=struct.unpack_from('<QQ',data,off);kind=info&0xffffffff;label=names.get(kind,'ELF_RELOC_'+str(kind));counts[label]+=1
    (alloc if allocated else nonalloc)[label]+=1
    if allocated and kind in [1,10,11]:absolute[sname(target)+' '+label]+=1
 return dict(path=str(path),sha256=sha(path),elf_members=len(members),all_relocations=dict(counts),allocated_section_relocations=dict(alloc),nonallocated_or_unspecified_target_relocations=dict(nonalloc),absolute_relocations_in_allocated_sections=dict(absolute))
elf_text=(HERE/'owner-link/ELF-IMPORTS.txt').read_text();symbols=(HERE/'owner-link/SYMBOLS.txt').read_text()
imports=sorted(set(re.findall(r'^\s*\d+:.*\bUND\s+(\S+)\s*$',elf_text,re.M)))
needed=re.findall(r'\(NEEDED\).*\[([^]]+)\]',elf_text)
assert len(imports)==289 and len(needed)==3
mono_undefined=[s for s in imports if s.startswith(('mono_','sgen_','ves_icall_','SystemNative_','Mono_Posix_'))]
assert not mono_undefined
for symbol in required:assert re.search(r'^[0-9a-f]+ [TtDdBbRr] '+symbol+'$',symbols,re.M),symbol
archive_counts=census(archive);aot_counts=census(corlib);elf_counts=census(exe)
# Absolute debug relocations are not a shared-image problem. Allocated 64-bit
# pointer tables need loader relocations; allocated 32-bit absolutes constrain placement.
assert not any(archive_counts['allocated_section_relocations'].get(x,0) or aot_counts['allocated_section_relocations'].get(x,0) for x in ['R_X86_64_32','R_X86_64_32S'])
receipt=dict(verdict='TARGET_LINK_AND_RELOCATION_AUDIT_PASS',scope='Static link/format proof only; no PRX/shared link, execution, import-provider behavioral validation or production admission',compiler_dry_run=mode,corlib_assembler_dry_run=aotmode,compiler_relocation_model='pic',pic_level=2,corlib_is_generated_assembly=True,corlib_pic_judged_by_actual_relocations=True,callsite_inspection=callsite,retained_registration_and_rooting_calls=required,local_mono_symbols_resolved=True,undefined_mono_symbols=mono_undefined,dynamic_import_count=len(imports),dynamic_imports=imports,public_sdk_import_libraries=needed,runtime_archive=archive_counts,corlib_object=aot_counts,final_elf=elf_counts,elf_type='ET_DYN with PIE flag from public SDK default linker wrapper',entry_point=0,absolute_32bit_allocated_relocations=0,textrel_dynamic_tag_present='TEXTREL' in elf_text,shared_image_compatibility_proven=False,notes=['C compiler driver defaults to PIC level2; the runtime source build did not add fake OS macros.','Corlib is assembly; a PIC flag alone is not evidence, so its actual allocated relocation tables are counted.','Both objects contain absolute64 data pointers requiring normal ELF relocations; no allocated absolute32/32S relocations observed.','Final ELF dynamic imports are satisfied at link by public SDK import libraries, not local implementations or exercised functions.','Public wrapper provides -pie and its linker script; entry0 means this is a link-only image, not a deployment executable.','Driver includes /usr/local/include implicitly; that directory is absent on this build host.'])
(HERE/'evidence/TARGET-LINK-AUDIT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({k:receipt[k] for k in ['verdict','compiler_relocation_model','pic_level','dynamic_import_count','public_sdk_import_libraries','absolute_32bit_allocated_relocations','textrel_dynamic_tag_present']},indent=2))
