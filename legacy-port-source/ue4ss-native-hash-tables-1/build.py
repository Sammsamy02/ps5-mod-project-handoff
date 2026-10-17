from pathlib import Path
import json,subprocess,os,hashlib,sys
W=Path(__file__).resolve().parent;B=W.parent/'ue4ss-unreal-native-build-1';O=W/sys.argv[1];O.mkdir(exist_ok=False)
r=json.loads((B/'consolidated-9/result.json').read_text());u=next(u for u in r['units'] if u['name']=='unreal-uobject'); profile=json.loads(Path(u['command']).read_text())
units=[]
for name in ['UObjectHashTables','UObjectHashTablesNativeUnavailable']:
 src=W/'source'/f'{name}.cpp'; obj=O/f'{name}.o'; argv=profile['argv'][:];argv[argv.index('-c')+1]=str(src);argv[argv.index('-o')+1]=str(obj)
 p=subprocess.run(argv,env=dict(os.environ,LLVM_CONFIG=profile['LLVM_CONFIG']),capture_output=True,text=True,timeout=180)
 (O/f'{name}.stdout.txt').write_text(p.stdout);(O/f'{name}.stderr.txt').write_text(p.stderr)
 cmd=O/f'{name}.command.json';cmd.write_text(json.dumps(dict(argv=argv,exit=p.returncode,LLVM_CONFIG=profile['LLVM_CONFIG']),indent=2)+'\n')
 item=dict(name='unreal-'+name.lower(),source=str(src),source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),object=str(obj),exit=p.returncode,command=str(cmd),warning_count=p.stderr.count('warning:'),error_count=p.stderr.count('error:'))
 if p.returncode==0:item.update(object_bytes=obj.stat().st_size,object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest())
 units.append(item);print(name,p.returncode,item['error_count'])
(O/'result.json').write_text(json.dumps(dict(units=units,all_compiled=all(u['exit']==0 for u in units),linked=False,executed=False,console_contact=False,public_headers_changed=False,engine_hash_services_available=False),indent=2)+'\n')
