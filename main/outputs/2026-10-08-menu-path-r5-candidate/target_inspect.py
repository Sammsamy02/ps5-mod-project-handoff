#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,hashlib,os,struct
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];OUT=HERE/'target-controls';OUT.mkdir(exist_ok=True);C=HERE/'candidate'
recipe_file=ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json';r=json.loads(recipe_file.read_text())['recipes']['unreal-ufunction'];cmd=list(r['pass1_argv']);cmd[1:1]=['-I',str(C/'include'),'-I',str(C)]
for k,v in [('-c',str(HERE/'target_control.cpp')),('-o',str(OUT/'control.o')),('-MF',str(OUT/'control.d'))]:cmd[cmd.index(k)+1]=v
p=subprocess.run(cmd,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']),capture_output=True,text=True,timeout=120);(OUT/'compile.stdout').write_text(p.stdout);(OUT/'compile.stderr').write_text(p.stderr)
if p.returncode:raise RuntimeError(p.stderr)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
obj=(OUT/'control.o').read_bytes();assert obj[:6]==b'\x7fELF\x02\x01'
_,_,_,_,_,_,shoff,_,_,_,_,shentsize,shnum,shstrndx=struct.unpack_from('<16sHHIQQQIHHHHHH',obj)
sections=[struct.unpack_from('<IIQQQQIIQQ',obj,shoff+i*shentsize) for i in range(shnum)]
facts=None
for sec in sections:
 if sec[1]!=2:continue
 strings=sections[sec[6]];strtab=obj[strings[4]:strings[4]+strings[5]]
 for off in range(sec[4],sec[4]+sec[5],sec[9]):
  name,info,other,ndx,value,size=struct.unpack_from('<IBBHQQ',obj,off);n=strtab[name:strtab.find(b'\0',name)].decode()
  if n=='ScriptFuncStorageFacts':
   owner=sections[ndx];facts=list(struct.unpack_from('<5Q',obj,owner[4]+value-owner[3]));assert size==40
assert facts and facts[:4]==[8,8,8,1],facts
nm=Path(cmd[0]).with_name('prospero-nm');objdump=Path(r['LLVM_CONFIG']).with_name('llvm-objdump');assert nm.exists() and objdump.exists()
objects=[HERE/'target'/s for s in ('UFunctionStructs.o','UFunction.o','ScriptHookThunks.o')]
rows=[];defined=set();undefined=set();nmoutputs={}
for o in objects:
 x=subprocess.run([str(nm),str(o)],capture_output=True,text=True,check=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']));dest=OUT/(o.stem+'.nm');dest.write_text(x.stdout);nmoutputs[o.stem]=x.stdout
 for line in x.stdout.splitlines():
  t=line.split()
  if len(t)>=2:
   if t[-2]=='U':undefined.add(t[-1])
   else:defined.add(t[-1])
 rows.append({'path':str(o),'sha256':sha(o),'nm_sha256':sha(dest)})
assert 'bridge_native_code_residence_required' in defined
assert sum('5ThunkILm' in line and line.split()[-2].lower()=='t' for line in nmoutputs['ScriptHookThunks'].splitlines())==256
baseline=set()
for f in ('unreal-flat-ufunctionstructs.o','unreal-ufunction.o'):
 o=ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1'/f;x=subprocess.run([str(nm),'-u',str(o)],capture_output=True,text=True,check=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']));(OUT/(f+'.baseline.nm')).write_text(x.stdout)
 for line in x.stdout.splitlines():
  t=line.split()
  if t:baseline.add(t[-1])
remaining=sorted(undefined-defined);new=sorted(set(remaining)-baseline);assert not any(x.startswith('__atomic') for x in remaining),remaining
x=subprocess.run([str(objdump),'--disassemble','--demangle',str(OUT/'control.o')],capture_output=True,text=True,check=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']));(OUT/'control.disassembly').write_text(x.stdout)
assert 'cmpxchgq' in x.stdout and 'lock' in x.stdout
receipt={'status':'TARGET_STORAGE_AND_PROVIDER_PASS','scope':'Actual selected public storage type, SDK target objects and compiler machine operations; not native engine concurrency/liveness proof','command':cmd,'LLVM_CONFIG':r['LLVM_CONFIG'],'source_sha256':sha(HERE/'target_control.cpp'),'object_sha256':sha(OUT/'control.o'),'facts':{'storage_size':facts[0],'storage_alignment':facts[1],'native_function_pointer_size':facts[2],'always_lock_free':facts[3],'script_data_size':facts[4]},'facts_from':'actual ELF symbol payload','objects':rows,'static_thunk_symbols':256,'residence_export':'bridge_native_code_residence_required','new_undefined_vs_two_original_TUs':new,'atomic_runtime_imports':[],'disassembly_sha256':sha(OUT/'control.disassembly')}
(OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'status':receipt['status'],'facts':receipt['facts'],'new_undefined':new}))
