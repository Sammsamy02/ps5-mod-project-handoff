import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3]
work=pathlib.Path(__file__).resolve().parent
phase=sys.argv[1]
dest=work/phase;dest.mkdir(exist_ok=True)
project=root/'outputs/ps5-mod-bridge';host=project/'native-host'
profile=root/'.superpowers/native-dependency-spike/native-curl-profile-experiment-1'
previous=root/'.superpowers/native-dependency-spike/native-nexus-provider-requests-1/app-host-2'
records=[]
def run(name,argv):
 r=subprocess.run([str(x) for x in argv],capture_output=True,text=True)
 records.append(dict(name=name,argv=[str(x) for x in argv],returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
 print(name,r.returncode,r.stdout.strip(),r.stderr[-1200:],flush=True)
 return r.returncode
cpp=['c++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
 '-I'+str(host),'-I'+str(project/'include'),'-isystem',project/'third_party']
returns=[]
if not phase.startswith('admission-path-'):
 returns.append(run('core_compile',[*cpp,work/'core_controls.cpp',project/'src/nexus.cpp','-o',dest/'core-controls']))
 if not returns[-1]:returns.append(run('core_run',[dest/'core-controls']))
 returns.append(run('exchange_compile',[*cpp,'-Dclock_gettime=controls_clock_gettime','-c',host/'oauth_code.cpp','-o',dest/'oauth_code.o']))
 if not returns[-1]:
  returns.append(run('exchange_link',[*cpp,work/'exchange_controls.cpp',dest/'oauth_code.o',project/'src/nexus.cpp','-o',dest/'exchange-controls']))
  if not returns[-1]:returns.append(run('exchange_run',[dest/'exchange-controls']))
cc=[profile/'host/cc.py','-std=c11','-Wall','-Wextra','-Werror','-I'+str(host/'download')]
returns.append(run('retained_transport_compile',[*cc,'-c',host/'download/curl_download.c','-o',dest/'curl_download.o']))
if not returns[-1]:
 returns.append(run('retained_admission_link',[*cc,work/'admission_controls.c',dest/'curl_download.o',
  previous/'desktop_fixture-download/owner.o',previous/'desktop_fixture-download/file_sink.o',
  previous/'dependencies-download/libcurl.a',profile/'host/libmbedtls.a',profile/'host/libmbedx509.a',profile/'host/libmbedcrypto.a',
  '-framework','SystemConfiguration','-framework','CoreFoundation','-o',dest/'admission-controls']))
 if not returns[-1]:returns.append(run('retained_admission_run',[dest/'admission-controls']))
sources=[host/'oauth_code.h',host/'oauth_code.cpp',host/'download/oauth_code_transfer.h',host/'download/curl_download.c',
 project/'include/modbridge/nexus.hpp',project/'src/nexus.cpp',work/'core_controls.cpp',work/'exchange_controls.cpp',work/'admission_controls.c',pathlib.Path(__file__)]
artifacts=[previous/'dependencies-download/libcurl.a',profile/'host/libmbedtls.a',profile/'host/libmbedx509.a',profile/'host/libmbedcrypto.a']
receipt=dict(scope='Actual core ASan/UBSan; real exchange with explicit fake provider/renamed clock boundary; retained matching ordinary-host curl admission without runtime/socket/TLS.',
 console_executed=False,native_build=False,network_started=False,phase=phase,passed=all(rc==0 for rc in returns),
 commands=records,source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
 archive_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in artifacts})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if receipt['passed'] else 1)
