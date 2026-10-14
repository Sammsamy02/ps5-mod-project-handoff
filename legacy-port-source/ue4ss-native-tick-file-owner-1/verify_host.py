#!/usr/bin/env python3
import json,os,subprocess,sys,hashlib
from pathlib import Path
W=Path(__file__).resolve().parent;S=W.parent;ROOT=S.parent.parent
out=W/sys.argv[1];out.mkdir(exist_ok=False);host=W/'host-harness';calls=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(name,args,env=None):
 args=list(map(str,args));p=subprocess.run(args,capture_output=True,text=True,env=env,cwd=W);(out/(name+'.stdout')).write_text(p.stdout);(out/(name+'.stderr')).write_text(p.stderr);calls.append(dict(name=name,argv=args,exit=p.returncode));(out/'commands.json').write_text(json.dumps(calls,indent=2)+'\n');return p
# Re-link the two retained owned FDE controls at this host's16KiB page scale.
# Original objects and frozen ELF controls remain untouched.
(W/'tests/controls').mkdir(exist_ok=True)
for label in ['unique','ambiguous']:
 obj=S/'ue4ss-native-game-loop-resolver-1/tick-owned'/(label+'.o')
 p=run(label+'-control-link',[ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/ld.lld','-shared','--eh-frame-hdr','-z','max-page-size=16384',obj,'-o',W/'tests/controls'/(label+'.elf')]);assert p.returncode==0,p.stderr
witness_root=S/'ue4ss-native-tick-witness-1'
rust_receipt=witness_root/'native-1/receipt.json';rust_native=json.loads(rust_receipt.read_text())
rust_hashes={str(witness_root/'crate'/p):h for p,h in rust_native['source_hashes'].items()}
for p,h in rust_hashes.items():assert sha(p)==h,('Rust source changed',p)
known=json.loads((W/'green-2/receipt.json').read_text())['input_hashes']
source_before={p:sha(p) for p in known if Path(p).is_file() and not p.endswith(('.o','.elf'))}
for p in [*W.glob('include/*'),*W.glob('src/*'),*W.glob('tests/*.cpp'),*W.glob('tests/*.py'),W/'verify_host.py',host/'Cargo.toml',host/'Cargo.lock',host/'build.rs',host/'src/lib.rs',witness_root/'include/patternsleuth_tick_file_witness.h',rust_receipt]:source_before[str(p)]=sha(p)
commands=json.loads((S/'ue4ss-native-mapped-candidate-selection-1/build-1/commands.json').read_text())
base=next(c['argv'] for c in commands if c['name']=='host-controls');base=base[:1]+['-I',str(W/'include'),'-I',str(S/'ue4ss-native-tick-witness-1/include'),'-I',str(S/'ue4ss-native-loader-factory-1/include')]+base[1:]
source={
 'controls':W/'tests/owner_controls.cpp',
 'context':W/'src/NativeLoaderContext.cpp',
 'source':W/'src/OwnedMappedScanSource.cpp',
 'selection':S/'ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedCandidateSelection.cpp',
 'collector':S/'ue4ss-native-mapped-collector-properties-1/src/OwnedMappedResolverCollector.cpp',
}
if (W/'src/OwnedSerializedElfSource.cpp').exists():source['serialized']=W/'src/OwnedSerializedElfSource.cpp'
objects=[];hashes=dict(source_before)
for name,src in source.items():
 argv=base.copy();argv[argv.index('-c')+1]=str(src);obj=out/(name+'.o');argv[argv.index('-o')+1]=str(obj);argv+=['-MMD','-MF',str(out/(name+'.d'))];p=run(name,argv);assert p.returncode==0,p.stderr;objects.append(obj)
 for s in (out/(name+'.d')).read_text().replace('\\\n',' ').split()[1:]:
  if Path(s).is_file():hashes[str(Path(s).resolve())]=sha(s)
# Reuse the real owned-memory implementation's host object, independently hash it.
memory=S/'ue4ss-native-mapped-candidate-selection-1/host-harness-1/memory.o';objects.append(memory);hashes[str(memory)]=sha(memory)
archive=host/'libtick_owner_controls.a';archive.unlink(missing_ok=True);p=run('archive',['/usr/bin/ar','rcs',archive,*objects]);assert p.returncode==0
rust=S/'unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin';e=dict(os.environ,CARGO_HOME=str(S/'ue4ss-native-mapped-candidates-1/tools/cargo'),RUSTC=str(rust/'bin/rustc'),RUSTDOC=str(rust/'bin/rustdoc'),RUSTC_BOOTSTRAP='1',MACOSX_DEPLOYMENT_TARGET='15.0');e['PATH']=str(rust/'bin')+':'+e['PATH']
p=run('host-test',[rust/'bin/cargo','test','--locked','--offline','--manifest-path',host/'Cargo.toml','--','--nocapture'],e)
print((p.stdout+p.stderr)[-5000:]);expected=sys.argv[1].startswith('red')
if expected:assert p.returncode!=0 and 'r.selected.results.gameengine_tick==' in p.stderr
else:assert p.returncode==0
for path,h in {**hashes,**source_before,**rust_hashes}.items():assert sha(path)==h
for path in [*(W/'tests/controls').glob('*.elf'),*(S/'ue4ss-native-game-loop-resolver-1/tick-owned').glob('*.o'),W/'verify_host.py',*W.glob('include/*'),*W.glob('src/*'),*W.glob('tests/*'),*host.glob('*.toml'),host/'build.rs',host/'src/lib.rs']:
 if path.is_file():hashes[str(path)]=sha(path)
(W/sys.argv[1]/'receipt.json').write_text(json.dumps(dict(status='EXPECTED_MISSING_CONNECTION' if expected else 'OWNED_CPP_FILE_FALLBACK_HOST_PASS',commands=calls,input_hashes=hashes,rust_source_hashes=rust_hashes,rust_receipt=str(rust_receipt),rust_receipt_sha256=sha(rust_receipt),artifacts={str(p):sha(p) for p in [*objects,archive]},target_executed=False,console_contact=False,scope='Owned GNU/SCE-shaped mapped fixtures; no captured code executed and no production live image supplier.'),indent=2)+'\n')
