from pathlib import Path
import os,sys,json,subprocess,hashlib,concurrent.futures,re
w=Path(__file__).resolve().parent;s=w.parent;r=s.parent.parent;b=s/'ue4ss-unreal-native-build-1/source-dependencies';mode=sys.argv[1];o=w/(mode+'-'+sys.argv[2]);o.mkdir(exist_ok=False);sdk=r/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';llvm=r/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
incs=[s/'ue4ss-native-instruction-owned-1/include',w/'include',s/'ue4ss-native-bounded-owned-1/include',s/'ue4ss-polyhook-platform-port-1/patched/include',b/'polyhook',b/'asmjit/src',b/'asmtk/src',b/'zydis/include',b/'zydis/src',b/'zycore/include'];flags=['-O0','-g','-fPIC','-ffunction-sections','-fdata-sections','-DASMJIT_STATIC','-DASMTK_STATIC','-DZYDIS_STATIC_BUILD','-DZYCORE_STATIC_BUILD',*[x for p in incs for x in ['-I',str(p)]]]
if mode=='host':flags+=['-include','sys/mman.h'];flags+=['-arch','x86_64','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DPLH_OWNED_MEMORY_HOST_TEST','-DPLH_OWNED_MEMORY_ALLOCATOR_TEST']
else:flags+=['-fms-extensions','-Wno-unknown-pragmas']
sources=[('polyhook-'+n,w/'port/sources'/f'{n}.cpp')for n in ['x64Detour','ADetour','ZydisDisassembler']]+[('ownedtrampoline',w/'OwnedTrampoline.cpp'),('fixture',w/'owned_detour.cpp')]
if mode=='host':
 sources += [('polyhook-'+n,(s/'ue4ss-native-polyhook-sources-1/port/polyhook/sources'/f'{n}.cpp')if(s/'ue4ss-native-polyhook-sources-1/port/polyhook/sources'/f'{n}.cpp').is_file()else b/'polyhook/sources'/f'{n}.cpp')for n in ['RangeAllocator','FBAllocator','ErrorLog','UID','MemProtector','PolyHookOs']]
 for dep in ['zydis','zycore']:
  names=list(dict.fromkeys(re.findall(r'"(src/[^"\n]+\.c)"',(b/dep/'CMakeLists.txt').read_text())))
  sources += [(dep+'-'+str(Path(n).with_suffix('')).replace('/','-'),b/dep/n)for n in names]
if len(sys.argv)>3:sources=[row for row in sources if row[0] in sys.argv[3:]]
def compile(unit):
 name,source=unit;cpp=source.suffix=='.cpp';compiler=(sdk/'bin/prospero-clang++'if cpp else sdk/'bin/prospero-clang')if mode=='native'else('/usr/bin/clang++'if cpp else'/usr/bin/clang');obj=o/(name+'.o');cmd=[str(compiler),'-std=c++23'if cpp else'-std=c11',*flags,'-MD','-MF',str(o/(name+'.d')),'-c',str(source),'-o',str(obj)];p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120);(o/(name+'.stdout')).write_text(p.stdout);(o/(name+'.stderr')).write_text(p.stderr);row={'name':name,'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'command':cmd,'exit':p.returncode,'object':str(obj)}
 if not p.returncode:row['object_sha256']=hashlib.sha256(obj.read_bytes()).hexdigest()
 print(name,p.returncode,p.stderr[-1400:]if p.returncode else'',flush=True);return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:results=list(pool.map(compile,sources))
(o/'receipt.json').write_text(json.dumps({'units':results,'all_compiled':all(x['exit']==0 for x in results),'mode':mode,'LLVM_CONFIG':env['LLVM_CONFIG'],'include_priority':[str(p)for p in incs],'native_executed':False,'host_executed':False,'console_contact':False,'build_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n');assert all(x['exit']==0 for x in results)
