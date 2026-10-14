from pathlib import Path
import struct,json,hashlib,subprocess,importlib.machinery,os
import sys
sys.dont_write_bytecode=True
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent
def pin(p):
 p=Path(p);return dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
img=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';b=img.read_bytes();assert hashlib.sha256(b).hexdigest()=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def fo(a):return next(o+a-v for t,f,o,v,p,fs,ms,al in ss if t==1 and v<=a<v+fs)
refs=json.loads((OUT/'THREAD-CONTRACT-REFS.json').read_text());tags={int(x['tag'],16):int(x['value'],16) for x in refs['dynamic']};rel_alias=[]
for i in range(tags[8]//24):
 a,info,add=struct.unpack_from('<QQq',b,fo(tags[7])+i*24)
 if add in (0x9358a98,0x9358aa4):rel_alias.append(dict(offset=hex(a),info=hex(info),addend=hex(add),file_offset=hex(fo(tags[7])+i*24)))
regions=[]
for a,w in [(0x9358a98,4),(0x9358aa4,1)]:
 for t,f,o,v,p,fs,ms,al in ss:
  if t==1 and v<=a and a+w<=v+ms:regions.append(dict(image_vaddr=hex(a),width_bytes=w,load_segment_vaddr=hex(v),file_offset=hex(o),filesz=hex(fs),memsz=hex(ms),flags=f,file_backed=a+w<=v+fs,interpretation='loader BSS storage; no saved live value'))
sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';nm=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm';argv=[str(nm),'--dynamic',str(sdk/'target/lib/libkernel.so')];z=subprocess.run(argv,text=True,capture_output=True,cwd=ROOT);assert z.returncode==0
(OUT/'sdk-libkernel-symbols.stdout').write_text(z.stdout);(OUT/'sdk-libkernel-symbols.stderr').write_text(z.stderr)
nid=importlib.machinery.SourceFileLoader('support_nid',str(sdk/'bin/prospero-nid')).load_module();matches=[]
for line in z.stdout.splitlines():
 if not line.strip():continue
 name=line.split()[-1]
 if nid.nid_encode(name)=='EI-5-jlq2dE':matches.append(name)
assert matches==['scePthreadGetthreadid'],matches
pub=ROOT/'outputs/2026-10-07-l16-local-handles/receipts/public-api/OpenOrbis-libkernel.h';prov=ROOT/'outputs/2026-10-07-l16-local-handles/receipts/public-api/PROTOTYPES.json';pj=json.loads(prov.read_text());assert pin(pub)['sha256']==pj['sha256'];assert pub.read_text().splitlines()[572]=='int32_t scePthreadGetthreadid(void);'
pins=[pub,prov,nm,sdk/'target/lib/libkernel.so',sdk/'bin/prospero-nid',ROOT/'outputs/2026-10-08-menu-path-audit/source-cache/LuaMod.cpp',ROOT/'outputs/2026-10-07-l19-local-profile-r4/units/UnrealInitializer.cpp',ROOT/'.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp']
cmd=json.loads((OUT/'target-thread-census.command.json').read_text());a=cmd['argv'];src=OUT/'policy/candidate/NativeIdGetterAdapter.cpp'
for flag,val in [('-c',str(src)),('-o',str(OUT/'policy/target-native-id-adapter.o')),('-MF',str(OUT/'policy/target-native-id-adapter.d'))]:a[a.index(flag)+1]=val
env=os.environ.copy();env['LLVM_CONFIG']=cmd['LLVM_CONFIG'];c=subprocess.run(a,text=True,capture_output=True,cwd=ROOT,env=env)
(OUT/'policy/target-native-id-adapter.stdout').write_text(c.stdout);(OUT/'policy/target-native-id-adapter.stderr').write_text(c.stderr);assert c.returncode==0,c.stderr[-2000:]
(OUT/'policy/target-native-id-adapter.command.json').write_text(json.dumps(dict(argv=a,LLVM_CONFIG=cmd['LLVM_CONFIG'],exit_code=c.returncode,scope='compile-only candidate typed getter adapter; no engine execution'),indent=2)+'\n')
deppins=[]
for path in [OUT/'target-thread-census.d',OUT/'policy/target-native-id-adapter.d']:
 text=path.read_text().replace('\\\n','');tokens=text.split(':',1)[1].split();deppins.extend(pin(ROOT/x if not Path(x).is_absolute() else x) for x in tokens)
deppins=list({p['path']:p for p in deppins}.values())
(OUT/'THREAD-SUPPORT.json').write_text(json.dumps(dict(image=pin(img),relocation_address_aliases=rel_alias,relocation_scope='all standard DT_RELA addends exactly equal to two global addresses; computed aliases and non-relative relocation expressions unclosed',storage_regions=regions,SDK_export_unique_NID_match=matches,SDK_symbol_capture_command=argv,SDK_symbol_capture=pin(OUT/'sdk-libkernel-symbols.stdout'),source_inputs=[pin(p) for p in pins],public_prototype=dict(exact_line=573,declaration='int32_t scePthreadGetthreadid(void);',source_url=pj['url'],classification='saved third-party OpenOrbis PS4 prototype; PS5 ABI corroboration only'),target_adapter_command=pin(OUT/'policy/target-native-id-adapter.command.json'),compiler_dependency_pins=deppins),indent=2)+'\n');print(json.dumps(dict(exact_relocation_aliases=len(rel_alias),SDK_NID_matches=matches,public_prototype_sha256=pj['sha256'],target_adapter_exit=c.returncode,dependency_pins=len(deppins))))
