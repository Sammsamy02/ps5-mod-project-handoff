#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,subprocess,struct
w=Path(__file__).resolve().parent; base=w.with_name('ue4ss-native-layout-widths-1'); o=w/'native-1';o.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
facts=json.loads((w/'source-facts.json').read_text())
for source,expected in facts['source_pins'].items(): assert sha(source)==expected, source
assert sha(base/'integration-manifest.json')==facts['base_manifest_sha256']
assert sha(base/'integration-units.json')==facts['base_units_sha256']
c=json.loads((base/'native-3/command.json').read_text()); env=dict(os.environ,LLVM_CONFIG=c['LLVM_CONFIG'])
base_argv=c['argv'][:]
for name,source in [('native-layoutwidths',w/'src/NativeLayoutWidths.cpp'),('focused-controls',w/'tests/native_width_controls.cpp')]:
 a=base_argv[:];a[a.index(str(base/'src/NativeLayoutWidths.cpp'))]=str(source);a[a.index('-o')+1]=str(o/(name+'.o'));a[a.index('-MF')+1]=str(o/(name+'.d'));a[1:1]=['-I',str(w/'include')]
 q=subprocess.run(a,env=env,capture_output=True,text=True,timeout=60)
 (o/(name+'.stdout')).write_text(q.stdout);(o/(name+'.stderr')).write_text(q.stderr);(o/(name+'-command.json')).write_text(json.dumps({'argv':a,'exit':q.returncode,'LLVM_CONFIG':c['LLVM_CONFIG']},indent=2)+'\n')
 assert q.returncode==0,q.stderr
llvm=Path(c['LLVM_CONFIG']).parent; a=[str(llvm/'llvm-readelf'),'-x','.focused_width_facts',str(o/'focused-controls.o')];q=subprocess.run(a,capture_output=True,text=True);assert q.returncode==0,q.stderr
(o/'focused-facts-readelf.stdout').write_text(q.stdout);(o/'extraction-command.json').write_text(json.dumps({'argv':a,'exit':q.returncode},indent=2)+'\n')
parts=[]
for line in q.stdout.splitlines():
 f=line.split()
 if f and f[0].startswith('0x'):
  for v in f[1:5]:
   if len(v)!=8 or any(ch not in '0123456789abcdefABCDEF'for ch in v):break
   parts.append(bytes.fromhex(v))
data=b''.join(parts);(o/'focused-facts.bin').write_bytes(data);assert len(data)==24
assert (o/'focused-controls.o').read_bytes()[5]==1
facts=struct.unpack('<6I',data);member,modern,func,supermodern,superlegacy,superkey=facts
assert member and modern and func==max(member,modern) and supermodern and superlegacy==0 and superkey==0
src=w/'src/NativeLayoutWidths.cpp';obj=o/'native-layoutwidths.o';unit={'name':'native-layoutwidths','source':str(src),'source_sha256':sha(src),'object':str(obj),'object_sha256':sha(obj),'object_bytes':obj.stat().st_size,'exit':0,'command':str(o/'native-layoutwidths-command.json')}
(w/'integration-units.json').write_text(json.dumps({'units':[unit]},indent=2)+'\n')
receipt={'scope':'actual SDK compile/static controls plus own ELF constant extraction; no native execution/game ABI claim', 'UFunction.Func':{'legacy_member_function_pointer_bytes':member,'modern_function_object_pointer_bytes':modern,'conservative_all_alternatives_bytes':func},'UStruct.SuperStruct':{'modern_TObjectPtr_bytes':supermodern,'legacy_TNonAccessTrackedObjectPtr_bytes':None,'result':None,'reason':'legacy alternative remains incomplete; not omitted'},'base_available_keys':303,'successor_available_keys':304,'successor_unavailable_keys':115,'changed_canonical_keys':['UFunction.Func'],'production_object_excludes_focused_controls':True,'source_header_unchanged':sha(w/'include/NativeLayoutWidths.hpp')==sha(base/'include/NativeLayoutWidths.hpp')}
(o/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n'); print(json.dumps({'unit':unit,'receipt':receipt},indent=2))
