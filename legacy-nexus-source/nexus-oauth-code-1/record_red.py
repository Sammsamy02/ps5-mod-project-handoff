import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3]
work=pathlib.Path(__file__).resolve().parent
dest=work/'red'
dest.mkdir(exist_ok=True)
project=root/'outputs/ps5-mod-bridge'
host=project/'native-host'
profile=root/'.superpowers/native-dependency-spike/native-curl-profile-experiment-1'
previous=root/'.superpowers/native-dependency-spike/native-nexus-provider-requests-1/app-host-2'
records=[]
def run(name,argv):
 result=subprocess.run([str(x) for x in argv],capture_output=True,text=True)
 records.append(dict(name=name,argv=[str(x) for x in argv],returncode=result.returncode,
                     stdout=result.stdout,stderr=result.stderr))
 print(name,result.returncode,result.stderr[-1800:])
 return result
r=run('retained_transport_compile',[profile/'host/cc.py','-std=c11','-Wall','-Wextra','-Werror',
 '-I'+str(host/'download'),'-c',host/'download/curl_download.c','-o',dest/'curl_download.o'])
linked=run('missing_public_and_post_api_link',['c++','-std=c++20','-I'+str(host),
 work/'missing_api.cpp',dest/'curl_download.o',previous/'desktop_fixture-download/owner.o',
 previous/'desktop_fixture-download/file_sink.o',previous/'dependencies-download/libcurl.a',
 profile/'host/libmbedtls.a',profile/'host/libmbedx509.a',profile/'host/libmbedcrypto.a',
 '-framework','SystemConfiguration','-framework','CoreFoundation','-o',dest/'missing-api']) if r.returncode==0 else None
missing=['mb_nexus_oauth_write_code_form_v1','mb_nexus_oauth_exchange_code_v1','mb_curl_oauth_code_transfer_v1']
observed=linked is not None and linked.returncode!=0 and all(s in linked.stderr for s in missing)
receipt=dict(scope='Missing actual public writer/exchange and private POST symbols; fresh existing transport with retained matching ordinary-host curl/MbedTLS. No runtime/socket/TLS started.',
 console_executed=False,native_build=False,network_started=False,red_observed=observed,
 commands=records,source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in [host/'oauth_code.h',host/'download/oauth_code_transfer.h',host/'download/curl_download.c',work/'missing_api.cpp']})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if observed else 1)
