from pathlib import Path
import json,hashlib,subprocess,os,shutil
r=Path(__file__).resolve().parent;sp=r.parent;root=r.parents[2];out=r/'build-1';out.mkdir(exist_ok=False);native=out/'native';native.mkdir();host=r/'host-harness-1';llvm=root/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'));calls=[]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(name,args):
 p=subprocess.run(list(map(str,args)),env=env,cwd=r,capture_output=True,text=True,timeout=90);calls.append({'name':name,'argv':list(map(str,args)),'exit':p.returncode,'LLVM_CONFIG':env['LLVM_CONFIG']});(out/'commands.json').write_text(json.dumps(calls,indent=2)+'\n');(out/(name+'.stdout')).write_text(p.stdout);(out/(name+'.stderr')).write_text(p.stderr);assert p.returncode==0,(name,p.stderr);return p.stdout
leaf=sp/'ue4ss-native-mapped-candidates-1';manifest=json.loads((leaf/'integration-manifest.json').read_text());assert sha(leaf/'integration-manifest.json')=='8affa598b3f58bf509203d6283af5137056baa4eaa536f968b4cb96dc4e1e3a9';assert len(manifest['native']['exports'])==17 and sha(manifest['native']['object'])==manifest['native']['sha256']
# Original compiler-owned candidate source and exact negative preprocessor flags.
s=(leaf/'candidate-fixtures/candidates.S').read_text();extra='''\n.text
.hidden original_construct,original_gu_slot,original_gu_other
.globl original_construct,original_gu_slot,original_gu_other
.type original_construct,@function
original_construct:
.cfi_startproc
ret
.cfi_endproc
.size original_construct,.-original_construct
.byte 0xe8
.long original_construct-.-4
.byte 0x48,0x8b,0xd8,0x48,0x39,0x75,0x30,0x74,0x15,0xc3
#ifdef ORIGINAL_GU
.byte 0x74,0,0x48,0x8d,0x0d
.long original_gu_slot-.-4
.byte 0xc6,0x05
.long 0
.byte 1,0xe8
.long 0
.byte 0xc6,0x05
.long 0
.byte 1,0xc3
#endif
#ifdef ORIGINAL_AMBIGUOUS
.byte 0x74,0,0x48,0x8d,0x0d
.long original_gu_other-.-4
.byte 0xc6,0x05
.long 0
.byte 1,0xe8
.long 0
.byte 0xc6,0x05
.long 0
.byte 1,0xc3
#endif
.section .rodata
.short 43,43,85,69,53,43,82,101,108,101,97,115,101,45,53,46,55,0
.bss
.balign 8
original_gu_slot:.zero 256
original_gu_other:.zero 256
'''
asm=r/'tests/fixtures/hybrid.S';asm.write_text(s+extra);variants={'hybrid':[],'original-gu':['ORIGINAL_GU'],'original-gu-ambiguous':['ORIGINAL_GU','ORIGINAL_AMBIGUOUS'],'ambiguous-gu':['AMBIGUOUS_GU'],'incomplete-gu':['INCOMPLETE_GU'],'ambiguous-fname':['AMBIGUOUS_FNAME'],'ambiguous-malloc':['AMBIGUOUS_MALLOC'],'incomplete-malloc-competitor':['INCOMPLETE_MALLOC_COMPETITOR']}
for name,flags in variants.items():
 obj=asm.parent/(name+'.o');exe=asm.parent/(name+'.elf');run(name+'-asm',[llvm/'clang','-target','x86_64-unknown-linux-gnu','-fPIC',*[('-D'+x)for x in flags],'-c',asm,'-o',obj]);run(name+'-elf',[llvm/'ld.lld','-shared','--eh-frame-hdr','--no-undefined','-z','max-page-size=16384',obj,'-o',exe]);assert 'There are no relocations' in run(name+'-relocations',[llvm/'llvm-readelf','-r',exe])
# Same original report collector. New adapter/source compile against current genuine leaf.
commands=json.loads((sp/'ue4ss-native-mapped-collector-properties-1/commands.json').read_text());base=next(x['argv'] for x in commands if x['name']=='host-collector');base=base[:];base=base[:1]+['-I',str(r/'include'),'-I',str(leaf/'include'),'-I',str(r/'tests')]+base[1:]
objects=[]
for name,source in [('collector',r/'src/OwnedMappedResolverCollector.cpp'),('selection',r/'src/OwnedMappedCandidateSelection.cpp'),('source',r/'src/OwnedMappedScanSource.cpp'),('memory',sp/'ue4ss-native-read-lease-1/src/OwnedMemory.cpp'),('controls',r/'tests/selection_controls.cpp')]:
 a=base[:];a[a.index('-c')+1]=str(source);obj=host/(name+'.o');a[a.index('-o')+1]=str(obj);run('host-'+name,a);objects.append(obj)
run('host-static',['/usr/bin/ar','rcs',host/'libselection_controls.a',*objects])
# The actual SDK dependency closure found exactly three existing consumers.
inv=json.loads((r/'affected-unit-inventory.json').read_text());assert inv['all_dependency_probes_passed'] and inv['affected_units']==['native-ownedmappedscansource','native-psscanbinding','unreal-unrealinitializer'];rows=[]
for u in inv['units']:
 if not u['affected']:continue
 a=u['original_argv'][:];name=u['name'];source=Path(u['source'])
 if name=='native-ownedmappedscansource':source=r/'src/OwnedMappedScanSource.cpp'
 if name=='native-psscanbinding':source=r/'src/PsScanBinding.cpp'
 a[a.index('-c')+1]=str(source);obj=native/(name+'.o');a[a.index('-o')+1]=str(obj)
 if '-MF' in a:a[a.index('-MF')+1]=str(native/(name+'.d'))
 else:a+=['-MD','-MF',str(native/(name+'.d'))]
 run('native-'+name,a);rows.append({'name':name,'source':str(source),'source_sha256':sha(source),'object':str(obj),'object_bytes':obj.stat().st_size,'object_sha256':sha(obj),'exit':0,'argv':a,'dependency_file':str(native/(name+'.d'))})
prototype=next(u['original_argv']for u in inv['units']if u['name']=='native-ownedmappedscansource');a=prototype[:];a[a.index('-c')+1]=str(r/'src/OwnedMappedCandidateSelection.cpp');a[a.index('-o')+1]=str(native/'OwnedMappedCandidateSelection.o')
if '-MF' in a:a[a.index('-MF')+1]=str(native/'OwnedMappedCandidateSelection.d')
run('native-new-selection',a);rows.append({'name':'native-ownedmappedcandidateselection','source':str(r/'src/OwnedMappedCandidateSelection.cpp'),'source_sha256':sha(r/'src/OwnedMappedCandidateSelection.cpp'),'object':str(native/'OwnedMappedCandidateSelection.o'),'object_bytes':(native/'OwnedMappedCandidateSelection.o').stat().st_size,'object_sha256':sha(native/'OwnedMappedCandidateSelection.o'),'exit':0,'argv':a,'dependency_file':str(native/'OwnedMappedCandidateSelection.d')})
(out/'native-consumers.json').write_text(json.dumps({'all_compiled':True,'old_consumers_rebuilt':3,'new_adapter_objects':1,'units':rows,'single_rust_object':manifest['native']},indent=2)+'\n')
run('native-partial',[llvm/'ld.lld','-r',native/'native-ownedmappedscansource.o',native/'native-psscanbinding.o',native/'OwnedMappedCandidateSelection.o',sp/'ue4ss-native-mapped-collector-properties-1/native-1/OwnedMappedResolverCollector.o',manifest['native']['object'],'-o',native/'candidate-source-single-runtime.partial.o'])
print('hostC++archive/native3affectedconsumers+newadapter+single17runtime partiallink PASS')
