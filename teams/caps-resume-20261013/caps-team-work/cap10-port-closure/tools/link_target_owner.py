#!/usr/bin/env python3
"""Compile/link exact production owner with real target Mono and corlib, NEVER run it.

The observation/provider fixture is derived from the prior host test and proves
only symbol/link completeness. No game readiness or runtime admission is implied.
The admission macro is explicitly confined to this parent-authorized compile probe.
"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess
HERE=Path(__file__).resolve().parents[1];CAP=HERE.parent/'cap10';CROSS=HERE.parent/'cap10-cross-aot';WORK=HERE/'target-work'
local=json.loads((HERE/'LOCAL-INPUTS.json').read_text());sdk=Path(local['sdk']);llvm=Path(local['llvm_bin'])
env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
for key in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
OUT=HERE/'owner-link';OUT.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
runtime=WORK/'mono/mini/.libs/libmonosgen-2.0.a';aot=CROSS/'artifacts/mscorlib.full-interp.ps5.o'
aot_receipt=json.loads((CROSS/'evidence/AOT-ARTIFACT.json').read_text())
assert sha(aot)==aot_receipt['object_sha256']
assert sha(CROSS/'managed-input/mscorlib.dll')==aot_receipt['managed_source_sha256']
# Actual matching IL/config fixture paths, only embedded as strings; never used here.
prefix=OUT/'runtime';(prefix/'lib/mono/4.5').mkdir(parents=True,exist_ok=True);(prefix/'etc/mono').mkdir(parents=True,exist_ok=True)
shutil.copyfile(CROSS/'managed-input/mscorlib.dll',prefix/'lib/mono/4.5/mscorlib.dll')
shutil.copyfile(WORK/'data/config',prefix/'etc/mono/config')
original=CAP/'host-check/MonoHarness.cpp'
harness=OUT/'LinkOnlyObserver.cpp'
prefix_text=original.read_text().split('static void need(',1)[0]
prefix_text=prefix_text.replace('#include "MonoGcRegionApi.hpp"','#include <mono/utils/mono-threads-api.h>')
prefix_text=prefix_text.replace('HOST VALIDATION','TARGET LINK-ONLY FIXTURE').replace('HOST-ONLY observer','LINK-ONLY observer')
harness.write_text('// NEVER EXECUTED. Test-only observation/provider fixture; no target/runtime behavior evidence.\n'+prefix_text+'''
#include "MonoOwner.hpp"
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\\n",tag,value); }
int main()
{
    const auto result=RC::Cap10Host::run_embedded_probe_once();
    return result.completed ? 0 : 1;
}
''')
provider=OUT/'LinkOnlyProvider.c'
provider.write_text('/* NEVER EXECUTED. Target link-only provider fixture, not the game provider. */\n'+(CAP/'host-check/native-mono-test.c').read_text())
sources=[CAP/'native/MonoOwner.cpp',CAP/'native/InvocationBoundary.cpp',CAP/'native/MonoOwner.hpp',CAP/'native/InvocationBoundary.hpp',CAP/'native/Cap10InvocationObservation.hpp',CAP/'native/ProbeImage.hpp',harness,provider]
inputs={str(p):sha(p) for p in sources+[runtime,aot,WORK/'config.h',WORK/'mono/eglib/eglib-config.h',prefix/'lib/mono/4.5/mscorlib.dll',prefix/'etc/mono/config']}
commands=[];logs=[]
receipt=dict(verdict='TARGET_OWNER_LINK_INCOMPLETE',target_execution=False,prx_build=False,production_admission=False,compile_probe_admission_macro=True,test_only_observer=True,input_sha256=inputs,commands=commands)
def save():
 (HERE/'evidence/TARGET-OWNER-LINK.json').write_text(json.dumps(receipt,indent=2)+'\n')
 (HERE/'logs/TARGET-OWNER-LINK.log').write_text(''.join(logs))
def run(name,cmd):
 r=subprocess.run(cmd,cwd=HERE,env=env,text=True,capture_output=True)
 commands.append(dict(name=name,command=cmd,exit_code=r.returncode));logs.append(name+'\n'+r.stdout+r.stderr);save()
 if r.returncode:raise SystemExit(r.returncode)
 return r.stdout
flags=[str(sdk/'bin/prospero-clang++'),'-std=c++17','-O1','-Wall','-Wextra','-Werror','-c','-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1','-DMODBRIDGE_CAP10_CORLIB_DIR='+json.dumps(str(prefix/'lib')),'-DMODBRIDGE_CAP10_CONFIG_DIR='+json.dumps(str(prefix/'etc')),'-I'+str(CAP/'native'),'-I'+str(WORK),'-I'+str(WORK/'mono/eglib')]
objects=[]
for source in [sources[0],sources[1],harness]:
 obj=OUT/(source.stem+'.o');run(source.stem,flags+[str(source),'-o',str(obj)]);objects.append(obj)
obj=OUT/'LinkOnlyProvider.o';run('LinkOnlyProvider',[str(sdk/'bin/prospero-clang'),'-O1','-Wall','-Wextra','-Werror','-c',str(provider),'-o',str(obj)]);objects.append(obj)
exe=OUT/'mono-owner-link-only.elf'
run('link',[str(sdk/'bin/prospero-clang++'),*[str(o) for o in objects],str(aot),str(runtime),'-lpthread','-Wl,--no-undefined','-Wl,-z,defs','-Wl,-Map,'+str(OUT/'link.map'),'-o',str(exe)])
readelf=run('ELF',[str(llvm/'llvm-readelf'),'--file-header','--dynamic-table','--dyn-syms',str(exe)])
(OUT/'ELF-IMPORTS.txt').write_text(readelf)
symbols=run('symbols',[str(llvm/'llvm-nm'),str(exe)]);(OUT/'SYMBOLS.txt').write_text(symbols)
for required in ['mono_aot_module_mscorlib_info','mono_aot_register_module','mono_jit_init_version','mono_threads_enter_gc_unsafe_region','mono_threads_exit_gc_unsafe_region','mono_gchandle_new','mono_gchandle_free','mono_runtime_invoke','mono_dl_fallback_register']:
 assert required in symbols,required
assert 'libmonosgen-2.0.so' not in readelf,'SDK Mono import stub must not be linked'
assert 'Machine:                           Advanced Micro Devices X86-64' in readelf
assert all(sha(Path(p))==digest for p,digest in inputs.items()),'input drift during build'
receipt.update(verdict='TARGET_OWNER_STATIC_CORLIB_LINK_PASS',output=str(exe),output_sha256=sha(exe),output_bytes=exe.stat().st_size,elf_imports_sha256=sha(OUT/'ELF-IMPORTS.txt'),symbol_census_sha256=sha(OUT/'SYMBOLS.txt'),link_map_sha256=sha(OUT/'link.map'),registration_and_rooting_symbols_retained=True,library_order=['owner/boundary/observer/provider objects','genuine target static corlib object','genuine source-built target Mono static archive','pthread','public SDK default C++/C/kernel libraries'])
save();print(receipt['verdict'],sha(exe))
