from pathlib import Path
import subprocess,os,json,hashlib
r=Path(__file__).resolve().parent;w=r.parents[2];d=r/'build-1';d.mkdir(exist_ok=True)
llvm=w/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin';sdk=w/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin';e=os.environ.copy();e['LLVM_CONFIG']=str(llvm/'llvm-config')
def call(name,c):
 p=subprocess.run(list(map(str,c)),env=e,capture_output=True,text=True);(d/(name+'.json')).write_text(json.dumps({'command':list(map(str,c)),'LLVM_CONFIG':e['LLVM_CONFIG'],'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr},indent=2));assert p.returncode==0,(name,p.stderr);return p.stdout
exports=['ps_pattern_scan','ps_xref_scan','ps_engine_version_file_image','ps_guobject_array_file_image','ps_fname_ctor_wchar_file_image','ps_fname_to_string_file_image','ps_ugameengine_tick_file_image','ps_guobject_array_cfg_file_image','ps_fname_ctor_call_file_image','ps_gmalloc_file_image','ps_static_construct_object_file_image','ps_gmalloc_deletefile_candidate_file_image','ps_resolve_mapped_core_v1','ps_resolve_mapped_property_v1','ps_guobject_array_cfg_mapped_v1','ps_fname_ctor_call_mapped_v1','ps_gmalloc_deletefile_candidate_mapped_v1']
obj=d/'patternsleuth-native-owned-candidates-resolvers.o';cmd=[llvm/'ld.lld','-r','--gc-sections'];
for x in exports:cmd+=['-u',x]
cmd += [r/'target/x86_64-sie-ps5-core/release/libpatternsleuth_native_engine_version.a','-o',obj];call('native-link',cmd)
defs=call('native-defined',[llvm/'llvm-nm','--defined-only',obj]);actual=sorted(x.split()[-1] for x in defs.splitlines() if x.split()[-1].startswith('ps_'));assert actual==sorted(exports),actual
undef=call('native-undefined',[llvm/'llvm-nm','--undefined-only',obj]);print('exports',actual);print('undefined',undef)
call('native-c-consumer',[sdk/'prospero-clang','-std=c11','-Wall','-Wextra','-Werror','-fPIC','-c',r/'native_mapped_probe.c','-o',d/'mapped-consumer.native.o'])
call('native-consumer-partial-link',[llvm/'ld.lld','-r',obj,d/'mapped-consumer.native.o','-o',d/'mapped-consumer.partial.o'])
(d/'native-summary.json').write_text(json.dumps({'object':str(obj),'bytes':obj.stat().st_size,'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'exports':actual,'undefined_symbols':undef.splitlines(),'sdk_consumer_compile':True,'sdk_partial_link':True,'native_execution':False},indent=2))
print((d/'native-summary.json').read_text())
