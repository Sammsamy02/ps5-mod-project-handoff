#!/usr/bin/env python3
"""Local target compile and relocatable hook/import proof, never a package."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,os,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
w=root.parents[1]
baseline=w/'outputs/2026-10-07-l17-local-lifecycle'
full=w/'outputs/2026-10-07-l16-full-build'
run=root/'native/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
sdk=w/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
llvm=w/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
commands=json.loads((full/'runs/run-l16-2/commands.json').read_text())
build=json.loads((full/'prx-candidate-l16-2/build-receipt.json').read_text())
old_owner=baseline/'units/runtime_owner.cpp'
templates={
 'initializer':next(c['argv'] for c in commands if c['name']=='unreal-unrealinitializer'),
 'owner':next(c['argv'] for c in commands if c['name']=='bootstrap-runtime_owner'),
 'bridge':next(c['argv'] for c in build['commands'] if '-c' in c.get('argv',[]) and any(s.endswith('/runtime_start_bridge.o') for s in c['argv'])),
}
sources={
 'initializer':(baseline/'units/UnrealInitializer.cpp',root/'units/UnrealInitializer.cpp'),
 'owner':(old_owner,root/'units/runtime_owner.cpp'),
 'bridge':(baseline/'units/runtime_start_bridge_r14_diag_spanpub.cpp',root/'units/runtime_start_bridge_r14_diag_spanpub.cpp'),
}
rows=[];objects={};reused=[]
previous_path=root/'receipts/NATIVE-FOCUS.json'
previous=json.loads(previous_path.read_text()) if previous_path.exists() else {}
previous_hash=hashlib.sha256(previous_path.read_bytes()).hexdigest() if previous_path.exists() else None
previous_saved=next((str(p) for p in (root/'native/runs').glob('*/RECEIPT.json') if hashlib.sha256(p.read_bytes()).hexdigest()==previous_hash),None)
headers_unchanged=all(p.exists() and hashlib.sha256(p.read_bytes()).hexdigest()==h for raw,h in previous.get('header_hashes',{}).items() for p in [Path(raw)])
def execute(cmd):
 env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
 r=subprocess.run([str(s) for s in cmd],cwd=w,env=env,capture_output=True,text=True,timeout=120)
 row={'argv':[str(s) for s in cmd],'environment_override':{'LLVM_CONFIG':env['LLVM_CONFIG']},'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr};rows.append(row);return r
for unit in ('bridge','owner','initializer'):
 for edition,p in zip(('l17','l18'),sources[unit]):
  prior=next((Path(raw) for raw,h in previous.get('objects',{}).items() if Path(raw).name==edition+'-'+unit+'.o' and Path(raw).exists() and hashlib.sha256(Path(raw).read_bytes()).hexdigest()==h),None)
  if prior and headers_unchanged and previous.get('source_hashes',{}).get(str(p))==hashlib.sha256(p.read_bytes()).hexdigest():
   objects[(edition,unit)]=prior;reused.append({'edition':edition,'unit':unit,'object':str(prior),'sha256':hashlib.sha256(prior.read_bytes()).hexdigest()});print(edition,unit,'verified_reuse');continue
  out=run/(edition+'-'+unit+'.o');cmd=list(templates[unit]);
  for i,s in enumerate(cmd):
   if s.endswith('.cpp'):cmd[i]=str(p)
  cmd[cmd.index('-o')+1]=str(out)
  if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(run/(edition+'-'+unit+'.d'))
  cmd[1:1]=['-I',str(p.parent)]
  r=execute(cmd);print(edition,unit,'compile_exit',r.returncode)
  if r.returncode:print(r.stderr)
  else:objects[(edition,unit)]=out
if len(objects)!=6:
 (root/'receipts/NATIVE-FOCUS.json').write_text(json.dumps({'runs':rows,'pass':False},indent=2)+'\n');sys.exit(1)
symbols={}
for key,p in objects.items():
 r=execute([llvm/'llvm-nm','-P',p]);assert r.returncode==0
 symbols['-'.join(key)]={s[0]:s[1] for line in r.stdout.splitlines() if len(s:=line.split())>=2}
joined=run/'l18-xom-focus.o';r=execute([llvm/'ld.lld','-r','-m','elf_x86_64','-o',joined,*[objects[('l18',u)] for u in ('bridge','owner','initializer')]])
assert r.returncode==0,r.stderr
r=execute([llvm/'llvm-nm','-P',joined]);joined_symbols={s[0]:s[1] for line in r.stdout.splitlines() if len(s:=line.split())>=2}
relocations={}
for unit in ('bridge','owner','initializer'):
 r=execute([llvm/'llvm-readelf','-r',objects[('l18',unit)]]);assert r.returncode==0
 relocations[unit]=[line for line in r.stdout.splitlines() if any(name in line for name in ('bridge_copyin_initializer','bridge_code_cache','sceKernelOpen','sceKernelPread','sceKernelClose'))]
delta={}
for unit in ('bridge','owner','initializer'):
 old={s for s,t in symbols['l17-'+unit].items() if t=='U'};new={s for s,t in symbols['l18-'+unit].items() if t=='U'}
 delta[unit]={'added_undefined':sorted(new-old),'removed_undefined':sorted(old-new)}
hooks=('bridge_copyin_initializer_enter','bridge_copyin_initializer_complete','bridge_copyin_initializer_leave','bridge_code_cache_context','bridge_code_cache_signature')
hook_proof={name:{'bridge_definition':symbols['l18-bridge'].get(name),'owner_reference':symbols['l18-owner'].get(name),'initializer_reference':symbols['l18-initializer'].get(name),'relocatable_join':joined_symbols.get(name)} for name in hooks}
unlink_exports=[]
for lib in sorted((sdk/'target/lib').glob('libkernel*.so'))+sorted((sdk/'target/lib').glob('libkernel*.a')):
 r=execute([llvm/'llvm-nm','-D','--defined-only','-P',lib]);hits=[line for line in r.stdout.splitlines() if line.startswith('sceKernelUnlink ')]
 if hits:unlink_exports.append({'path':str(lib),'sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),'exports':hits})
nid=execute([sdk/'bin/prospero-nid','sceKernelUnlink'])
passed=all(joined_symbols.get(h)=='T' for h in hooks) and bool(unlink_exports) and all(symbols['l18-bridge'].get(name)=='U' for name in ('sceKernelUnlink','sceKernelOpen','sceKernelPread','sceKernelClose')) and nid.returncode==0
receipt={'scope':'local full-TU cross-compile only and three-object relocatable join; no final runtime link, PRX/SELF, package, console or upload','pass':passed,'source_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for pair in sources.values() for p in pair},'header_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'units').glob('*.hpp')},'objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*objects.values(),joined]},'undefined_delta':delta,'hook_resolution':hook_proof,'relevant_relocations':relocations,'sceKernelUnlink_sdk_exports':unlink_exports,'sceKernelUnlink_nid':nid.stdout.strip(),'commands':rows}
receipt['reused_verified_objects']=reused;receipt['reused_receipt']={'path':previous_saved,'sha256':previous_hash}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'receipts/NATIVE-FOCUS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'pass':passed,'hook_resolution':hook_proof,'undefined_delta':delta,'sceKernelUnlink_sdk_exports':unlink_exports,'nid':nid.stdout.strip()},indent=2));sys.exit(0 if passed else 1)
