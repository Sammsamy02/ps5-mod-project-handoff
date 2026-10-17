from pathlib import Path
import datetime,hashlib,json,subprocess,sys
root=Path(__file__).resolve().parents[4]
native=root/'outputs/ps5-mod-bridge/native-host';base=Path(__file__).resolve().parent
mode=sys.argv[1];out=base/mode;out.mkdir(exist_ok=False)
source=base/'baseline/http.c' if mode=='red' else native/'http.c'
argv=['/usr/bin/clang','-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-I'+str(native),str(source),str(base/'framing.c'),str(native/'tests/web_assets_stub.c'),'-o',str(out/'framing')]
build=subprocess.run(argv,cwd=root,capture_output=True,text=True)
run=subprocess.run([str(out/'framing')],cwd=root,capture_output=True,text=True) if build.returncode==0 else None
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt={'schema_version':1,'mode':mode,'scope':'actual HTTP framing/unavailable component; no auth callback, runtime or socket','source_sha256':sha(source),'header_sha256':sha(native/'http.h'),'driver_sha256':sha(base/'framing.c'),'build':{'argv':argv,'exit':build.returncode,'stdout':build.stdout,'stderr':build.stderr},'run':None if run is None else {'exit':run.returncode,'stdout':run.stdout,'stderr':run.stderr},'console_executed':False,'production_account_contacted':False}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'mode':mode,'build_exit':build.returncode,'stdout':run.stdout if run else None,'stderr':build.stderr,'run_exit':run.returncode if run else None}),flush=True)
raise SystemExit(build.returncode if run is None else run.returncode)
