from pathlib import Path
import json,subprocess,os,hashlib
w=Path(__file__).resolve().parent;s=w.parent;r=s.parent.parent;o=w/'build-1';o.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
profile=json.loads((s/'ue4ss-native-instruction-owned-1/native-consumers-1/unreal-unrealinitializer/command.json').read_text());env=dict(os.environ,LLVM_CONFIG=profile['LLVM_CONFIG'],RUSTC_BOOTSTRAP='1')
commands=[]
def run(name,cmd):
 cmd=list(map(str,cmd));p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120);(o/(name+'.stdout')).write_text(p.stdout);(o/(name+'.stderr')).write_text(p.stderr);commands.append({'name':name,'command':cmd,'exit':p.returncode});(o/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');print(name,p.returncode,p.stdout[-900:],p.stderr[-1300:]if p.returncode else'',flush=True);assert p.returncode==0;return p
native=profile['argv'][:];native=native[:native.index('-c')];native[1:1]=['-I',str(w/'include')]
sdk=r/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
for name,source in [('native-c-layout','layout_controls.c'),('native-cpp-layout','layout_controls.cpp'),('native-original-type','native_original_type_controls.cpp'),('native-caller','caller_controls.cpp')]:
 cmd=native[:]
 if source.endswith('.c'):
  cmd[0]=str(sdk/'bin/prospero-clang');cmd[cmd.index('-std=c++23')]='-std=c11'
 run(name,[*cmd,'-c',w/source,'-o',o/(name+'.o')])
hostincs=[w/'include',s/'ue4ss-unreal-native-build-1/port/unreal/include',s/'ue4ss-unreal-native-build-1/port/ue4ss/include',s/'ue4ss-unreal-native-build-1/port/first/include',s/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include',s/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include']
host=['/usr/bin/clang++','-std=c++23','-O0','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DRC_HELPERS_BUILD_STATIC','-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC',*[v for p in hostincs for v in ['-I',str(p)]]]
run('host-c-layout',['/usr/bin/clang','-std=c11','-I',w/'include','-c',w/'layout_controls.c','-o',o/'host-c-layout.o'])
run('host-cpp-layout',[*host,'-c',w/'layout_controls.cpp','-o',o/'host-cpp-layout.o'])
run('host-caller-build',[*host,w/'caller_controls.cpp','-o',o/'caller-controls'])
run('host-caller-execute',[o/'caller-controls'])
rust=s/'unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/bin/rustc'
run('host-rust-controls',[rust,'--edition=2021','--crate-type=staticlib','-C','opt-level=1',w/'rust_controls.rs','-o',o/'librust-controls.a'])
run('host-cross-language-build',[*host,w/'cross_language.cpp',o/'librust-controls.a','-o',o/'cross-language'])
run('host-cross-language-execute',[o/'cross-language'])
crate=s/'ue4ss-native-engine-resolvers-integrated-1';deps=crate/'target/x86_64-sie-ps5-core/release/deps'
core=next(deps.glob('libcore-*.rlib'));builtins=next(deps.glob('libcompiler_builtins-*.rlib'))
run('native-rust-layout',[crate/'tools/rustc-native','--edition=2021','--crate-type=rlib','--emit=obj','-C','opt-level=1','-C','panic=abort','--target',crate/'x86_64-sie-ps5-core.json','-L','dependency='+str(deps),'--extern','core='+str(core),'--extern','compiler_builtins='+str(builtins),w/'rust_layout_native.rs','-o',o/'native-rust-layout.o'])
files=[w/'build.py',*w.glob('*.rs'),*w.glob('*.c'),*w.glob('*.cpp'),*w.glob('include/*'),w/'expected-layout.json',w/'slots.json',core,builtins,crate/'x86_64-sie-ps5-core.json']
for p in ['port/unreal/src/UnrealInitializer.cpp','source-dependencies/ue4ss/deps/first/patternsleuth_bind/src/lib.rs','port/unreal/include/Unreal/PlatformModuleSnapshot.hpp','port/ue4ss/include/Platform/NativeProgramContext.hpp','source-dependencies/ue4ss/deps/first/String/include/String/StringType.hpp']:
 files.append(s/'ue4ss-unreal-native-build-1'/p)
receipt={'all_controls_passed':True,'SDK_compiled':True,'native_executed':False,'console_contact':False,'game_memory_access':False,'resolver_provider_defined':False,'LLVM_CONFIG':env['LLVM_CONFIG'],'RUSTC_BOOTSTRAP':'1','layout_checks_per_language':sum(2+len(v[2])for v in json.loads((w/'expected-layout.json').read_text()).values()),'commands':commands,'inputs':{str(p):sha(p)for p in files},'objects':{str(p):sha(p)for p in o.glob('*.o')}}
(o/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS all ABI/caller/UTF16 controls and native C/C++/Rust compilation',flush=True)
