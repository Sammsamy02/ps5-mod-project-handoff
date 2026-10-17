from pathlib import Path
import subprocess,json,os,sys,hashlib
r=Path(__file__).resolve().parent;w=r.parent/'ue4ss-unreal-native-build-1';label=sys.argv[1];out=r/label;out.mkdir(exist_ok=False)
units=json.loads((w/'consolidated-13/result.json').read_text())['units'];u=next(x for x in units if x['source'].endswith('/USMapGenerator/Generator.cpp'));profile=json.loads(Path(u['command']).read_text());argv=profile['argv'];argv=argv[:argv.index('-c')]
argv=argv[:1]+['-I',str(r/'include'),'-I',str(r.parent/'ue4ss-native-owned-memory-1/include')]+argv[1:]+['-c',str(r/'src/JMapGenerator.cpp'),'-o',str(out/'JMapGenerator.o')]
p=subprocess.run(argv,env=dict(os.environ,LLVM_CONFIG=profile['LLVM_CONFIG']),capture_output=True,text=True,timeout=120)
(out/'stderr.txt').write_text(p.stderr);(out/'stdout.txt').write_text(p.stdout);record=dict(argv=argv,exit=p.returncode,LLVM_CONFIG=profile['LLVM_CONFIG'],source=str(r/'src/JMapGenerator.cpp'),source_sha256=hashlib.sha256((r/'src/JMapGenerator.cpp').read_bytes()).hexdigest(),executed=False)
if p.returncode==0:record.update(object=str(out/'JMapGenerator.o'),object_sha256=hashlib.sha256((out/'JMapGenerator.o').read_bytes()).hexdigest(),object_bytes=(out/'JMapGenerator.o').stat().st_size)
(out/'command.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({k:v for k,v in record.items()if k not in ['argv','LLVM_CONFIG']},indent=2));print('\n'.join(x for x in p.stderr.splitlines()if 'error:' in x)[:4000]);sys.exit(p.returncode)
