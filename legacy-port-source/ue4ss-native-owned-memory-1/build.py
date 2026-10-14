from pathlib import Path
import json,hashlib,subprocess,os,sys,importlib.util
W=Path(__file__).resolve().parent;R=W.parents[2];S=W.parent;O=W/sys.argv[1];O.mkdir(exist_ok=False)
p=json.loads((S/'ue4ss-native-polyhook-sources-1/native-closure-1/polyhook-Misc/command.json').read_text())
sdk=R/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';llvm=Path(p['LLVM_CONFIG']).parent
env=dict(os.environ,LLVM_CONFIG=p['LLVM_CONFIG']);commands=[]
def run(label,argv):
 argv=list(map(str,argv));r=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=120)
 (O/(label+'.stdout')).write_text(r.stdout);(O/(label+'.stderr')).write_text(r.stderr)
 commands.append({'label':label,'argv':argv,'exit':r.returncode});(O/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 assert r.returncode==0,(label,r.stderr);return r.stdout
for name,src in [('OwnedMemory',W/'src/OwnedMemory.cpp'),('native-owned',W/'tests/native_owned.cpp')]:
 argv=p['argv'][:];argv[argv.index('-c')+1]=str(src);argv[argv.index('-o')+1]=str(O/(name+'.o'));argv[argv.index('-MF')+1]=str(O/(name+'.d'))
 argv[1:1]=['-I',str(W/'include')];run('native-'+name,argv)
run('host-build',['clang++','-std=c++23','-DPLH_OWNED_MEMORY_HOST_TEST','-O0','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',W/'include','-I',S/'ue4ss-unreal-native-build-1/source-dependencies/polyhook',W/'tests/owned_controls.cpp',W/'src/OwnedMemory.cpp','-o',O/'owned-controls'])
host=run('host-controls',[O/'owned-controls']);print(host)
# Dedicated host provider object for genuine downstream consumers.
run('host-provider',['clang++','-std=c++23','-DPLH_OWNED_MEMORY_HOST_TEST','-O0','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',W/'include','-I',S/'ue4ss-unreal-native-build-1/source-dependencies/polyhook','-c',W/'src/OwnedMemory.cpp','-o',O/'OwnedMemory-host.o'])
spec=importlib.util.spec_from_file_location('layout',R/'outputs/ps5-mod-bridge/native-host/archive/link_layout.py');layout=importlib.util.module_from_spec(spec);spec.loader.exec_module(layout)
script=layout.write_linker_script(sdk,O);elf=O/'native-owned.elf'
run('native-link',[sdk/'bin/prospero-clang++','-nostartfiles','-Wl,-T,'+script['caller_script'],'-Wl,-e,_start','-Wl,--no-undefined','-fno-lto','-fPIC',O/'native-owned.o',O/'OwnedMemory.o',sdk/'target/lib/crt1.o','-o',elf])
geometry=layout.validate_dynamic_layout(elf)
undefined=run('native-undefined',[llvm/'llvm-nm','-u',elf]);run('native-defined',[llvm/'llvm-nm','-C','--defined-only',O/'OwnedMemory.o'])
run('native-headers',[llvm/'llvm-readelf','-h','-l','-d',elf])
providers={}
for lib in ('libkernel_web.so','libSceLibcInternal.so','libSceNet.so'):
 providers[lib]=set(line.split()[-1] for line in run('provider-'+lib,[llvm/'llvm-nm','-D','--defined-only',sdk/'target/lib'/lib]).splitlines() if line.strip())
imports=[line.split()[-1] for line in undefined.splitlines() if line.strip()]
mapping={name:[lib for lib,symbols in providers.items() if name in symbols] for name in imports};assert all(mapping.values())
(O/'import-providers.json').write_text(json.dumps(mapping,indent=2)+'\n')
files=[W/'src/OwnedMemory.cpp',W/'include/polyhook2/OwnedMemory.hpp',W/'tests/owned_controls.cpp',W/'tests/native_owned.cpp',W/'build.py']
receipt={'object':str(O/'OwnedMemory.o'),'object_sha256':hashlib.sha256((O/'OwnedMemory.o').read_bytes()).hexdigest(),'object_bytes':(O/'OwnedMemory.o').stat().st_size,'host_provider':str(O/'OwnedMemory-host.o'),'native_compiled':True,'sdk_linked':True,'native_executed':False,'console_contact':False,'host_controls':host.strip(),'dynamic_layout':geometry,'imports':mapping,'source_hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'original_memaccessor_header_changed':False,'native_context_changed':False,'automatic_module_registration':False}
(O/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
