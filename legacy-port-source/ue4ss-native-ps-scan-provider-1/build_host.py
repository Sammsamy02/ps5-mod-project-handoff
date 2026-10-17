from pathlib import Path
import subprocess,json,sys,hashlib
w=Path(__file__).resolve().parent;b=w.parent;out=w/sys.argv[1];out.mkdir(exist_ok=False)
rows=json.loads((b/'ue4ss-native-read-lease-1/caller-2/commands.json').read_text());base=rows[0]['argv'];base=base[:base.index(str(b/'ue4ss-native-read-lease-1/tests/caller_controls.cpp'))]
base[1:1]=[v for p in [w/'include',b/'ue4ss-native-mapped-collector-1/include',b/'ue4ss-native-mapped-collector-1/tests',b/'ue4ss-native-mapped-image-1/include'] for v in ['-I',str(p)]]
cmd=base+[str(w/'tests/provider_controls.cpp'),str(w/'src/OwnedMappedScanSource.cpp'),str(b/'ue4ss-native-mapped-collector-1/src/OwnedMappedResolverCollector.cpp'),str(b/'ue4ss-native-read-lease-1/src/OwnedMemory.cpp'),str(b/'ue4ss-native-mapped-image-1/build-1/libpatternsleuth-owned-mapped-host.a'),'-o',str(out/'provider-controls')]
records=[]
for name,argv in [('host-build',cmd),('host-run',[str(out/'provider-controls'),str(b/'ue4ss-native-mapped-collector-1/tests/fixtures/mapped.elf')])]:
 p=subprocess.run(argv,capture_output=True,text=True,timeout=120);records.append({'name':name,'argv':argv,'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr});(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(name,p.returncode,p.stdout,p.stderr[-3000:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
