from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys,time
root=Path(__file__).resolve().parents[3]
base=Path(__file__).resolve().parent
mode,generation=sys.argv[1:]
if mode not in ('host','native') or not generation.isdecimal():raise SystemExit('host/native generation required')
argv=json.loads((base/('build-'+mode+'-argv.json')).read_text())
argv[argv.index('--output-root')+1]=str(base/('configured-'+mode+'-'+generation))
out=base/('build-'+mode+'-'+generation);out.mkdir(exist_ok=False)
project=root/'outputs/ps5-mod-bridge'
tracked=['native-host/build.py','native-host/jobs.cpp','native-host/jobs.h','native-host/host.c','native-host/download/provider_transfer.h','native-host/download/curl_download.c','native-host/http.c','native-host/http.h','native-host/auth.cpp','native-host/auth.h','native-host/nexus_client.cpp','native-host/nexus_client.h','native-host/nexus_bridge.cpp','native-host/nexus_bridge.h','src/nexus.cpp','include/modbridge/nexus.hpp','include/modbridge/nexus_requirements_query.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs={name:sha(project/name) for name in tracked}
started=time.monotonic()
p=subprocess.run(argv,cwd=root,capture_output=True,text=True,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
receipt={'schema_version':1,'argv':argv,'exit':p.returncode,'seconds':time.monotonic()-started,'stdout':p.stdout,'stderr':p.stderr,
 'phase_source_sha256':inputs,'sources_unchanged':inputs=={name:sha(project/name) for name in tracked},'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'console_executed':False,'native_executed':False}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'exit':p.returncode,'seconds':round(receipt['seconds'],3),'stdout_tail':p.stdout[-2000:],'stderr_tail':p.stderr[-6500:],'sources_unchanged':receipt['sources_unchanged'],'receipt':str(out/'receipt.json')}),flush=True)
raise SystemExit(p.returncode)
