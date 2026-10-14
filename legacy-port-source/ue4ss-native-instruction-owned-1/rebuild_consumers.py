from pathlib import Path
import json,subprocess,hashlib,os,sys,concurrent.futures
w=Path(__file__).resolve().parent;b=w.parent;label=sys.argv[1];includes=[Path(x).resolve() for x in sys.argv[2:]];out=w/label;out.mkdir(exist_ok=False)
r=json.loads((b/'ue4ss-unreal-native-build-1/consolidated-14/result.json').read_text());names={'unreal-unrealinitializer','unreal-hooks'}
def build(u):
 p=json.loads(Path(u['command']).read_text());argv=p['argv'][:];d=out/u['name'];d.mkdir();obj=d/Path(u['object']).name
 prefix=[]
 for inc in [w/'include']+includes:prefix+=['-I',str(inc)]
 argv[1:1]=prefix;argv[argv.index('-o')+1]=str(obj);argv+=['-MMD','-MF',str(d/'dependencies.d')]
 v=subprocess.run(argv,env=dict(os.environ,LLVM_CONFIG=p['LLVM_CONFIG']),capture_output=True,text=True,timeout=180)
 (d/'stdout.txt').write_text(v.stdout);(d/'stderr.txt').write_text(v.stderr);cmd=d/'command.json';cmd.write_text(json.dumps({'argv':argv,'exit':v.returncode,'LLVM_CONFIG':p['LLVM_CONFIG']},indent=2)+'\n')
 item=dict(u,command=str(cmd),exit=v.returncode)
 if v.returncode==0:item.update(object=str(obj),object_bytes=obj.stat().st_size,object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest())
 return item
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:units=list(pool.map(build,[x for x in r['units'] if x['name'] in names]))
(out/'result.json').write_text(json.dumps({'units':units,'all_compiled':all(x['exit']==0 for x in units),'include_roots':[str(x) for x in includes]},indent=2)+'\n')
print(json.dumps({'all_compiled':all(x['exit']==0 for x in units),'units':[{'name':u['name'],'exit':u['exit']} for u in units]}));sys.exit(0 if all(x['exit']==0 for x in units) else 1)
