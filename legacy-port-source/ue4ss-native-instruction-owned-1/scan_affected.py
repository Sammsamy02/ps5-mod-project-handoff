from pathlib import Path
import json,subprocess,os,concurrent.futures,hashlib
w=Path(__file__).resolve().parent;b=w.parent;out=w/'affected-1';out.mkdir(exist_ok=False)
r=json.loads((b/'ue4ss-unreal-native-build-1/consolidated-14/result.json').read_text());llvm=str(b.parent.parent/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
def find_commands(x):
 if isinstance(x,dict):
  if isinstance(x.get('argv'),list):yield x
  for v in x.values():yield from find_commands(v)
 elif isinstance(x,list):
  for v in x:yield from find_commands(v)
def check(pair):
 i,u=pair
 if not u['source'].endswith(('.cpp','.cc','.cxx')):return None
 commands=list(find_commands(json.loads(Path(u['command']).read_text())))
 matches=[x for x in commands if u['source'] in x['argv'] and '-c' in x['argv']]
 if not matches:return {'unit':u['name'],'status':'no_compiler_recipe','source':u['source']}
 p=matches[0];a=p['argv'];args=[];skip=False
 for s in a:
  if skip:skip=False;continue
  if s in ('-o','-MF','-MT'):skip=True;continue
  if s in ('-c','-MMD','-MD'):continue
  args.append(s)
 dep=out/(str(i)+'.d');argv=args+['-MM','-MF',str(dep),'-MT','owned-header-consumer']
 result=subprocess.run(argv,env=dict(os.environ,LLVM_CONFIG=p.get('LLVM_CONFIG',llvm)),capture_output=True,text=True,timeout=90)
 (out/(str(i)+'.json')).write_text(json.dumps({'argv':argv,'exit':result.returncode,'stderr':result.stderr},indent=2)+'\n')
 data=dep.read_text() if dep.exists() else ''
 return {'unit':u['name'],'source':u['source'],'index':i,'status':'ok' if result.returncode==0 else 'failed','affected':any(s in data for s in ('/Instruction.hpp','/ZydisDisassembler.hpp')),'dependencies':str(dep)}
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:results=[x for x in pool.map(check,enumerate(r['units'])) if x]
(out/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps({'cpp_units':len(results),'affected':[x['unit'] for x in results if x.get('affected')],'incomplete':[x for x in results if x['status']!='ok']},indent=2))
