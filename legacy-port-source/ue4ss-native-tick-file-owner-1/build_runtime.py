#!/usr/bin/env python3
"""Freshly compile the selected Tick owner connection and compose pinned units.

Requires a final reviewed witness Rust receipt and a successful current host
receipt. Does not link a prior composite, package PRX, execute target code or
contact a console. Existing callbacks/activation/affinity/Notify fixes stay pinned.
"""
import argparse,hashlib,importlib.util,json,os,struct,subprocess
from pathlib import Path
W=Path(__file__).resolve().parent;S=W.parent;ROOT=S.parent.parent
BASE=S/'ue4ss-native-notify-cancel-1/build-1/receipt.json'
BASE_SHA='bcd23a2a4abef518d8f220786e80cb131036e351b2218a322d26aee6adca8384'
OLD_RUST_SHA='3c18b2caf57616267685d81c8439478dbafe9619f049df9f9878605563367f75'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def check(p,h):
 if not Path(p).is_file() or sha(p)!=h:raise ValueError('Changed input: '+str(p))
def deps(p):return {str(Path(x).resolve()) for x in Path(p).read_text().replace('\\\n',' ').split()[1:] if Path(x).is_file()}
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',required=True,type=Path);ap.add_argument('--rust-receipt',required=True,type=Path);ap.add_argument('--host-receipt',required=True,type=Path);ap.add_argument('--extra-unit',action='append',default=[],help='name=source_path; compiled with the loader-context argv template and appended to the link inputs');ap.add_argument('--extra-receipt',type=Path,help='hash-pinned attestation receipt covering the extra units');ap.add_argument('--extra-define',action='append',default=[],help='preprocessor define appended to extra-unit compiles')
 ap.add_argument('--extend-undefined-baseline',action='store_true',help='Allow NEW undefined native symbols introduced by extra units; every added symbol must be declared by a pinned SDK target library; the extended baseline is recorded');a=ap.parse_args()
 check(BASE,BASE_SHA);base=read(BASE);rust=read(a.rust_receipt);host=read(a.host_receipt)
 if host['status']!='OWNED_CPP_FILE_FALLBACK_HOST_PASS':raise ValueError('Host connection has not passed')
 for p,h in host['input_hashes'].items():check(p,h)
 for p,h in host.get('rust_source_hashes',{}).items():check(p,h)
 obj=rust['object'];check(obj['path'],obj['sha256'])
 if 'ps_ugameengine_tick_file_witness_v1' not in rust['public_exports']:raise ValueError('Rust witness ABI absent')
 for p,h in rust['source_hashes'].items():check(a.rust_receipt.parent/'source'/p,h)
 for i in rust['inputs']:check(i['path'],i['sha256'])
 spec=importlib.util.spec_from_file_location('sealed_build',S/'native-coherent-runtime-1/build_runtime.py');build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)
 pins={str(Path(p).resolve()):h for p,h in build.validate_inputs().items()}
 originals=read(BASE.parent/'input-hashes.json')
 for p,h in originals.items():check(p,h)
 if base['runtime_object'] in originals:raise ValueError('Prior combined object input refused')
 old_rust=[p for p,h in originals.items() if h==OLD_RUST_SHA]
 if len(old_rust)!=1:raise ValueError('Expected exactly one pinned old Rust object')
 units=base['selected_runtime_units'];affected=[];headers=['OwnedMappedScanSource.hpp','NativeLoaderContext.hpp','OwnedMappedCandidateSelection.hpp','NativeProgramFactory.hpp']
 for name,u in units.items():
  check(u['source'],u['source_sha256']);check(u['object'],u['object_sha256']);pins[str(Path(u['source']).resolve())]=u['source_sha256']
  if any(h in Path(u['dependency_file']).read_text() for h in headers):affected.append(name)
 if set(affected)!={'unreal-unrealinitializer','native-ownedmappedscansource','native-psscanbinding','native-ownedmappedcandidateselection','native-loadercontext','native-programfactory'}:raise ValueError('Affected header inventory changed')
 for u in base['bootstrap_units'].values():check(u['source'],u['source_sha256']);check(u['object'],u['object_sha256'])
 for p in [*W.glob('include/*'),*W.glob('src/*'),W/'build_runtime.py',S/'ue4ss-native-tick-witness-1/include/patternsleuth_tick_file_witness.h']:pins[str(p)]=sha(p)
 for name in affected:
  for p in deps(units[name]['dependency_file']):
   if p not in pins:raise ValueError('Unpinned original unit dependency: '+p)
   check(p,pins[p])
 receipt_pins={str(BASE):sha(BASE),str(a.rust_receipt.resolve()):sha(a.rust_receipt),str(a.host_receipt.resolve()):sha(a.host_receipt)}
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);calls=[]
 def run(name,args,env=None):
  argv=list(map(str,args));p=subprocess.run(argv,capture_output=True,text=True,env=env);(out/(name+'.stdout')).write_text(p.stdout);(out/(name+'.stderr')).write_text(p.stderr);calls.append(dict(name=name,argv=argv,exit=p.returncode));save(out/'commands.json',calls)
  if p.returncode:raise RuntimeError(name+': '+p.stderr[-5000:])
  return p
 selected=dict(units);replacements={};fresh=[];template=units['native-loadercontext']['argv'].copy();template_source=next(x for x in template if x.endswith('NativeLoaderContext.cpp'))
 new_sources={'native-ownedmappedscansource':W/'src/OwnedMappedScanSource.cpp','native-loadercontext':W/'src/NativeLoaderContext.cpp','native-programfactory':W/'src/NativeProgramFactory.cpp','native-ownedserializedelfsource':W/'src/OwnedSerializedElfSource.cpp'}
 for name in affected+['native-ownedserializedelfsource']:
  old=units['native-ownedmappedscansource' if name=='native-ownedserializedelfsource' else name];source=new_sources.get(name,Path(old['source']));target=out/(name+'.o');dep=out/(name+'.d');argv=old['argv'].copy();argv[argv.index(old['source'])]=str(source);argv[argv.index('-o')+1]=str(target);argv[argv.index('-MF')+1]=str(dep);argv[1:1]=['-I',str(W/'include'),'-I',str(S/'ue4ss-native-tick-witness-1/include')]
  run(name,argv,dict(os.environ,LLVM_CONFIG=old['LLVM_CONFIG']))
  for p in deps(dep):
   if p not in pins:raise ValueError('Unpinned fresh dependency: '+p)
   check(p,pins[p])
  selected[name]={**old,'name':name,'source':str(source),'source_sha256':sha(source),'object':str(target),'object_sha256':sha(target),'dependency_file':str(dep),'argv':argv,'exit':0};fresh.append(name)
  if name!='native-ownedserializedelfsource':replacements[old['object']]=str(target)
 extras=[];extra_pinned=[]
 if a.extra_unit and not a.extend_undefined_baseline:raise ValueError('Extra units require --extend-undefined-baseline')
 if a.extra_unit and not a.extra_receipt:raise ValueError('Extra units require --extra-receipt attestation')
 for spec in a.extra_unit:
  name,sep,src=spec.partition('=')
  if not sep or not name or not Path(src).is_file():raise ValueError('Bad extra unit spec: '+spec)
  src=Path(src);target=out/(name+'.o');dep=out/(name+'.d');argv=template.copy();argv[1:1]=['-I',str(W/'include'),'-I',str(S/'ue4ss-native-tick-witness-1/include')]
  for d in a.extra_define:argv.insert(1,'-D'+d)
  argv[argv.index(template_source)]=str(src);argv[argv.index('-o')+1]=str(target);argv[argv.index('-MF')+1]=str(dep)
  run('extra-'+name,argv,dict(os.environ,LLVM_CONFIG=units['native-loadercontext']['LLVM_CONFIG']))
  for pd in deps(dep):
   rp=str(Path(pd).resolve())
   if rp.startswith(str(ROOT)):
    if rp not in pins:
     if rp.startswith(str(S)) or rp.startswith(str(ROOT/'.superpowers')):raise ValueError('Unpinned extra project dependency: '+pd)
     pins[rp]=sha(pd);extra_pinned.append(rp)
    check(pd,pins[rp])
  selected[name]={'name':name,'source':str(src),'source_sha256':sha(src),'object':str(target),'object_sha256':sha(target),'dependency_file':str(dep),'argv':argv,'exit':0,'extra_unit':True}
  fresh.append(name);extras.append(str(target))
 replacements[old_rust[0]]=obj['path'];inputs=[]
 for p in originals:
  inputs.append(replacements.get(p,p))
  if p==units['native-ownedmappedscansource']['object']:inputs.append(selected['native-ownedserializedelfsource']['object'])
 inputs.extend(extras)
 hashes={p:sha(p) for p in inputs};save(out/'input-hashes.json',hashes)
 target=out/'coherent-tick-file-witness-runtime.o';run('link',[build.LLVM/'ld.lld','-m','elf_x86_64','-r',*inputs,'-Map='+str(out/'runtime.map'),'-o',target])
 raw=target.read_bytes();assert raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<HH',raw,16)==(1,62)
 current={line.split()[0]:line.split()[1] for line in run('undefined',[build.LLVM/'llvm-nm','--extern-only','--undefined-only','--format=posix',target]).stdout.splitlines() if line.strip()};previous=base['undefined_delta']['current']
 delta=dict(added=sorted(current.keys()-previous.keys()),removed=sorted(previous.keys()-current.keys()),changed_bindings={p:[previous[p],current[p]] for p in current.keys()&previous.keys() if current[p]!=previous[p]},current_count=len(current),current=current)
 if delta['added'] or delta['removed'] or delta['changed_bindings']:
  if a.extra_unit and a.extend_undefined_baseline and not delta['removed'] and not delta['changed_bindings']:
   sdk_lib_dir=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib'
   libsyms={lib.name:set(line.split()[0] for line in run('sdknm-'+lib.name,[build.LLVM/'llvm-nm','-D','--defined-only','--format=posix',lib]).stdout.splitlines() if line.strip()) for lib in sorted(sdk_lib_dir.glob('*.so'))}
   linker_provided={'_DYNAMIC'}  # resolved against the final PRX's own dynamic section at link time
   providers={sym:(['<linker-provided>'] if sym in linker_provided else [n for n,s in libsyms.items() if sym in s]) for sym in delta['added']}
   missing=[s for s,v in providers.items() if not v]
   if missing:raise ValueError('Extra-unit imports not declared by any pinned SDK library: '+','.join(missing))
   delta['baseline_extended']=True;delta['providers']=providers
   delta['provider_library_hashes']={str(lib):sha(lib) for lib in sorted(sdk_lib_dir.glob('*.so'))}
  else:raise ValueError('Unexpected native import change: '+json.dumps(delta))
 symbols={line.split()[0]:line.split()[1] for line in run('exports',[build.LLVM/'llvm-nm','--extern-only','--defined-only','--format=posix',target]).stdout.splitlines() if line.strip()}
 for name,kind in rust['public_exports'].items():
  if symbols.get(name)!=kind:raise ValueError('Lost Rust ABI export: '+name)
 for p,h in {**hashes,**pins,**receipt_pins}.items():check(p,h)
 for p,h in host.get('rust_source_hashes',{}).items():check(p,h)
 fixed=[{**i,'path':obj['path'],'resolved_path':obj['path'],'sha256':obj['sha256'],'reason':'reviewed Rust mapped/file resolvers with additive bounded Tick witness ABI'} if i.get('sha256')==OLD_RUST_SHA else i for i in base['retained_fixed_dependencies']]
 receipt={**base,'status':'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED','base_receipt':str(BASE),'base_receipt_sha256':BASE_SHA,'host_receipt':str(a.host_receipt.resolve()),'host_receipt_sha256':sha(a.host_receipt),'rust_receipt':str(a.rust_receipt.resolve()),'rust_receipt_sha256':sha(a.rust_receipt),'runtime_object':str(target),'runtime_sha256':sha(target),'runtime_bytes':target.stat().st_size,'runtime_compilation_units':len(selected),'freshly_compiled_units':fresh,'retained_runtime_units':len(units)-len(affected),'selected_runtime_units':selected,'retained_fixed_dependencies':fixed,'undefined_delta':delta,'source_hashes':pins,'commands':calls,'runtime_object_order':[{u['object']:n for n,u in selected.items()}[p] for p in inputs if p in {u['object'] for u in selected.values()}],'reused_composed_runtime_object':False,'linked_prx':False,'target_executed':False,'console_contact':False,'new_live_facts_supplier':bool(a.extra_unit),'optional_file_witness_connected':True,'scope':'Owned host mapping proof and native relocatable composition. No engine identity, callable Tick ABI, live-wide uniqueness, native game-thread or target execution claim.'}
 receipt['extra_pinned_dependencies']={p:pins[p] for p in extra_pinned}
 if a.extra_receipt:receipt['extra_receipt']={'path':str(a.extra_receipt.resolve()),'sha256':sha(a.extra_receipt)}
 receipt.pop('plan_sha256',None);save(out/'receipt.json',receipt);save(out/'undefined-delta.json',delta)
 print(json.dumps({k:receipt[k] for k in ['status','runtime_bytes','runtime_sha256','runtime_compilation_units','freshly_compiled_units','retained_runtime_units']},indent=2))
if __name__=='__main__':main()
