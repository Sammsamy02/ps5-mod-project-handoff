from pathlib import Path
import json,subprocess,os,sys,hashlib
w=Path(__file__).resolve().parent;b=w.parent;out=w/sys.argv[1];out.mkdir(exist_ok=False)
p=json.loads((w/'abi-native-1/command.json').read_text());args=p['argv'];args=args[:args.index('-c')]
incs=[w/'include',b/'ue4ss-native-mapped-collector-1/include',b/'ue4ss-native-mapped-image-1/include',b/'ue4ss-native-read-lease-1/include',b/'ue4ss-native-ps-scan-abi-1/include']
args[1:1]=[v for path in incs for v in ['-I',str(path)]];env=dict(os.environ,LLVM_CONFIG=p['LLVM_CONFIG']);rows=[]
for name,source in [('OwnedMappedScanSource',w/'src/OwnedMappedScanSource.cpp'),('PsScanBinding',w/'src/PsScanBinding.cpp'),('OwnedMappedResolverCollector',b/'ue4ss-native-mapped-collector-1/src/OwnedMappedResolverCollector.cpp')]:
 cmd=args+['-MD','-MF',str(out/(name+'.d')),'-c',str(source),'-o',str(out/(name+'.o'))];q=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120)
 row={'name':name,'argv':cmd,'exit':q.returncode,'LLVM_CONFIG':p['LLVM_CONFIG'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'stdout':q.stdout,'stderr':q.stderr};rows.append(row);(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,q.returncode,q.stderr[-1800:]if q.returncode else '',flush=True)
 if q.returncode:raise SystemExit(q.returncode)
