import hashlib
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
work=Path(__file__).resolve().parent
phase=sys.argv[1]
dest=work/phase
dest.mkdir(exist_ok=True)
project=root/'outputs/ps5-mod-bridge'
host=project/'native-host'
spike=root/'.superpowers/native-dependency-spike'
profile=spike/'native-curl-profile-experiment-1'
wrapper=profile/'host/cc.py'
app=spike/'native-nexus-provider-requests-1/app-host-2/desktop_fixture-download'
archives=[spike/'native-nexus-provider-requests-1/app-host-2/dependencies-download/libcurl.a',
          profile/'host/libmbedtls.a',profile/'host/libmbedx509.a',profile/'host/libmbedcrypto.a']
records=[]
def run(name,argv):
    result=subprocess.run([str(x) for x in argv],capture_output=True,text=True)
    record={'name':name,'argv':[str(x) for x in argv], 'returncode':result.returncode,
            'stdout':result.stdout,'stderr':result.stderr}
    records.append(record)
    print(name,result.returncode,result.stderr[-1000:])
    return result.returncode

core=['c++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g',
      '-fsanitize=address,undefined','-fno-omit-frame-pointer',
      '-I'+str(project/'include'),'-isystem',project/'third_party',work/'controls.cpp',
      project/'src/nexus.cpp','-o',dest/'core-controls']
core_rc=run('core_compile',core)
core_run=run('core_run',[dest/'core-controls']) if core_rc==0 else None
transport_rc=run('transport_compile',[wrapper,'-std=c11','-Wall','-Wextra','-Werror',
    '-I'+str(host/'download'),'-c',host/'download/curl_download.c','-o',dest/'curl_download.o'])
query_rc=run('query_link',[wrapper,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(host/'download'),
    work/'query_controls.c',dest/'curl_download.o',app/'owner.o',app/'file_sink.o',*archives,
    '-framework','SystemConfiguration','-framework','CoreFoundation','-o',dest/'query-controls']) if transport_rc==0 else None
query_run=run('query_run',[dest/'query-controls']) if query_rc==0 else None
sources=[project/'include/modbridge/nexus.hpp',project/'src/nexus.cpp',host/'download/curl_download.c',
         work/'controls.cpp',work/'query_controls.c',Path(__file__)]
receipt={'scope':'ordinary-host actual core ASan/UBSan and retained curl provider admission; no runtime/socket/TLS or live provider requests',
         'console_executed':False,'network_started':False,'native_build':False,
         'phase':phase,'source_sha256':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},
         'commands':records,'core_compile_return':core_rc,'core_run_return':core_run,
         'query_compile_return':query_rc,'query_run_return':query_run,
         'passed':core_rc==0 and core_run==0 and query_rc==0 and query_run==0}
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if receipt['passed'] else 1)
