from collections import Counter
import datetime,hashlib,json,re,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
BASE=Path(__file__).resolve().parent
OUT=BASE/'configured-native-1/loopback-download'
AUDIT=BASE/'native-artifact-audit-1';AUDIT.mkdir(exist_ok=False)
manifest_path=OUT/'manifest.json';m=json.loads(manifest_path.read_text())
artifact=OUT/m['artifact']
prior_dir=ROOT/'.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/configured-native-1/loopback-download'
prior_manifest=json.loads((prior_dir/'manifest.json').read_text());prior=prior_dir/prior_manifest['artifact']
failures=[];cache={};counts=Counter();origins=Counter();mismatches=[];host_headers=[]
def digest(path):
 path=Path(path)
 if path not in cache:cache[path]=hashlib.sha256(path.read_bytes()).hexdigest()
 return cache[path]
def require(ok,message):
 if not ok:failures.append(message)
def check_file(path,expected,category):
 path=Path(path);counts[category]+=1
 if not path.is_file() or digest(path)!=expected:mismatches.append({'path':str(path),'category':category})
def include(rows,category):
 if isinstance(rows,dict):
  for values in rows.values():include(values,category)
  return
 for row in rows:
  path=Path(row['path']);check_file(path,row['sha256'],category);origins[row['origin']]+=1
  if row['origin'].startswith('host') or str(path).startswith(('/Applications/','/Library/Developer/','/usr/include/','/opt/homebrew/','/usr/local/')):host_headers.append(str(path))
for name,expected in m['source_sha256'].items():check_file(ROOT/name,expected,'source')
for name,expected in m['object_sha256'].items():check_file(OUT/name,expected,'object')
include(m['include_closure'],'application_header')
include(m['crypto']['include_closure'],'curl_backport_header')
include(m['format_sha1_descriptor']['identity']['include_closure'],'format_sha1_header')
for name,row in m['archive_descriptor']['identity']['inputs'].items():
 check_file(name,row['sha256'],'retained_archive_input');require(Path(name).stat().st_size==row['bytes'],'archive byte count differs')
require(not mismatches,'current source/object/header/dependency closure differs')
require(not host_headers,'native includes host headers')
require(digest(artifact)==m['artifact_sha256'] and artifact.stat().st_size==m['artifact_size'],'artifact identity differs')
require(not m['console_executed'],'manifest claims console execution')
require(all(c['returncode']==0 and not c.get('stderr') for c in m['commands']),'compile/link diagnostic or failure')
fixed=ROOT/'outputs/ps5-mod-bridge/include/modbridge/nexus_requirements_query.h'
require(m['source_sha256'].get(str(fixed.relative_to(ROOT)))==digest(fixed),'fixed query header not identity-bound')
new_units={}
for unit in ('curl_download','nexus_client','nexus','auth','http','jobs','host'):
 rows=m['include_closure'][unit+'.o.d']
 if unit in ('curl_download','nexus_client','nexus'):
  require(any(Path(r['path'])==fixed and r['sha256']==digest(fixed) for r in rows),'fixed query not in actual TU closure: '+unit)
 obj=OUT/(unit+'.o')
 c=[r['argv'] for r in m['commands'] if '-c' in r['argv'] and '-o' in r['argv'] and r['argv'][r['argv'].index('-o')+1]==str(obj)]
 require(len(c)==1,'new unit compile is not unique: '+unit)
 new_units[unit]={'object_sha256':digest(obj),'argv':c[0]}
 require('-D_DARWIN_C_SOURCE=1' not in c[0],'host feature macro in native unit')
producer=new_units['curl_download']['argv']
require('-DMBEDTLS_CONFIG_FILE="tls_config.h"' in producer,'producer does not use selected TLS profile')
require('-nostdinc' in producer and any(str(a).endswith('/tls_effective.h') for a in producer) and any(str(a).endswith('/library/common.h') for a in producer),'producer forced crypto/header contract differs')
LLVM=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
commands=[]
def inspect(name,args):
 tool=LLVM/args[0];require(digest(tool)==m['tool_sha256'][args[0]],'inspection tool identity differs')
 p=subprocess.run([str(tool),*[str(a) for a in args[1:]]],capture_output=True)
 (AUDIT/name).write_bytes(p.stdout)
 commands.append({'argv':[str(tool),*[str(a) for a in args[1:]]],'returncode':p.returncode,'stdout_bytes':len(p.stdout),'stdout_sha256':hashlib.sha256(p.stdout).hexdigest(),'stderr_bytes':len(p.stderr)})
 require(p.returncode==0 and not p.stderr,'inspection failed: '+name)
 return p.stdout.decode()
imports=set(line.split()[-1] for line in inspect('imports.txt',['llvm-nm','--dynamic','--undefined-only',artifact]).splitlines() if line.strip())
old_imports=set(line.split()[-1] for line in inspect('prior-imports.txt',['llvm-nm','--dynamic','--undefined-only',prior]).splitlines() if line.strip())
definitions_text=inspect('definitions.txt',['llvm-nm','--defined-only','--demangle',artifact]);definitions=Counter()
for line in definitions_text.splitlines():
 fields=line.split(maxsplit=2)
 if len(fields)==3:definitions[fields[2]]+=1
retained={}
for name in ('mb_curl_provider_post_transfer_v1','mb_curl_provider_transfer_v1','mb_curl_oauth_code_transfer_v1','mb_curl_download_v1','mb_nexus_client_request_v1','mb_nexus_auth_catalog_v1','mb_nexus_auth_acquisition_plan_v1','mb_nexus_jobs_submit_acquisition_v1','mb_nexus_jobs_resume_v1','mb_nexus_jobs_submit_v1','mb_nexus_jobs_run_one_v1','mb_nexus_generations_submit_v1','mb_nexus_generations_list_v1','mb_nexus_generations_get_v1','mb_iostore_batch_file_job_gated_v1','pm_config_file_job_gated_v2','mb_pak_archive_job_prepare_gated_v1'):
 retained[name]=definitions[name];require(definitions[name]==1,'required body missing/duplicate: '+name)
retained['requirements_writer']=[name for name in definitions if name.startswith('modbridge::nexus_requirements_body(')]
require(len(retained['requirements_writer'])==1,'bounded query writer body missing/duplicate')
retained['file_selector']=[name for name in definitions if name.startswith('modbridge::nexus_select_file(')]
require(len(retained['file_selector'])==1,'typed current-file selector body missing/duplicate')
retained['metadata_decoder']=[name for name in definitions if name.startswith('modbridge::decode_nexus_metadata(')]
require(bool(retained['metadata_decoder']),'owning typed decoder body missing')
for name in ('mbedtls_platform_zeroize','mbedtls_sha1_init','mbedtls_sha1_free','mbedtls_sha1_starts','mbedtls_sha1_update','mbedtls_sha1_finish','mbedtls_sha256_init','mbedtls_sha256_free','mbedtls_sha256_starts','mbedtls_sha256_update','mbedtls_sha256_finish'):
 require(definitions[name]==1,'crypto provider missing/duplicated: '+name)
link=next(c['argv'] for c in m['commands'] if '-o' in c['argv'] and c['argv'][c['argv'].index('-o')+1]==str(artifact))
crypto=[Path(a) for a in link if Path(a).name=='libmbedcrypto.a'];require(len(crypto)==1,'multiple crypto owners')
require(str(crypto[0])==m['format_sha1_descriptor']['transport_crypto_archive'] and digest(crypto[0])==m['crypto']['mbedtls_archives_sha256']['libmbedcrypto.a'],'selected crypto owner differs')
format_archive=Path(m['format_sha1_descriptor']['archive']);format_text=inspect('format-sha1.txt',['llvm-nm','--extern-only','--defined-only',format_archive])
require(not re.search(r'\b[TDB] mbedtls_platform_zeroize$',format_text,re.M),'format SHA1 duplicates zeroize')
map_lines=(OUT/'link.map').read_text().splitlines();provider_evidence=[line.strip() for line in map_lines if 'libmbedformatsha1.a(sha1.o):(.text.mbedtls_sha1_' in line or 'libmbedcrypto.a(platform_util.o):(.text.mbedtls_platform_zeroize)' in line or 'libmbedcrypto.a(sha256.o):(.text.mbedtls_sha256_' in line]
require(any('libmbedformatsha1.a(sha1.o)' in line for line in provider_evidence) and any('platform_util.o' in line for line in provider_evidence) and any('sha256.o' in line for line in provider_evidence),'map crypto ownership evidence absent')
data=artifact.read_bytes();require(data[:6]==b'\x7fELF\x02\x01','not little-endian ELF64')
(_,machine,_,_,phoff,shoff,_,_,phsize,phnum,shsize,shnum,_)=struct.unpack_from('<HHIQQQIHHHHHH',data,16);require(machine==62,'not x86_64')
programs=[struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize) for i in range(phnum)]
sections=[struct.unpack_from('<IIQQQQIIQQ',data,shoff+i*shsize) for i in range(shnum)]
ph=[row for row in programs if row[0]==2];sh=[row for row in sections if row[1]==6];require(len(ph)==len(sh)==1,'dynamic cardinality differs')
p,s=ph[0],sh[0];entries=[struct.unpack_from('<qQ',data,p[2]+i) for i in range(0,p[5],16)]
require((p[2],p[3],p[5],p[6])==(s[4],s[3],s[5],s[5]),'PT_DYNAMIC/SHT_DYNAMIC differ')
require(p[5]==336 and p[5]%16==0 and s[9]==16,'dynamic table bytes/entry size differ')
require(p[7]==s[8]==16384 and p[2]%p[7]==0 and p[3]%p[7]==0,'dynamic alignment differs')
require(entries[-1]==(0,0) and all(t!=0 for t,_ in entries[:-1]),'DT_NULL not solely terminal')
dynsym=[r for r in sections if r[1]==11];require(len(dynsym)==1 and dynsym[0][5]%dynsym[0][9]==0,'dynsym shape differs')
dynamic={'bytes':p[5],'entries':len(entries),'entry_size':s[9],'PT_DYNAMIC_matches_SHT_DYNAMIC':(p[2],p[3],p[5],p[6])==(s[4],s[3],s[5],s[5]),'alignment':p[7],'terminal_DT_NULL':entries[-1]==(0,0),'dynsym_entries':dynsym[0][5]//dynsym[0][9]}
receipt={'schema_version':1,'operation':'native_free_grant_resume_composition_static_audit','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':not failures,'failures':failures,
 'artifact':{'path':str(artifact),'bytes':artifact.stat().st_size,'sha256':digest(artifact)},'manifest':{'path':str(manifest_path),'sha256':digest(manifest_path),'build_id':m['build_id']},
 'prior_artifact':{'path':str(prior),'sha256':digest(prior)},'hash_closure':{'counts':dict(counts),'origins':dict(origins),'all_current':not mismatches,'mismatches':mismatches,'host_headers':host_headers},
 'new_unit_contracts':new_units,'fixed_query_header_sha256':digest(fixed),'retained':retained,'crypto_owner_evidence':provider_evidence,'single_zeroize_definition':definitions['mbedtls_platform_zeroize']==1,
 'imports':{'current_count':len(imports),'prior_count':len(old_imports),'added':sorted(imports-old_imports),'removed':sorted(old_imports-imports)},'dynamic':dynamic,'commands':commands,'audit_script_sha256':digest(Path(__file__)),
 'native_executed':False,'console_executed':False,'production_account_contacted':False,'game_files_changed':False,'stopped_actions_retried':False}
(AUDIT/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
report='# Native free-account grant-resume composition audit\n\n'+('PASS' if receipt['passed'] else 'FAIL')+': current actual SDK artifact/static closure inspected without execution.\n\n'+json.dumps({'hash_counts':dict(counts),'imports':receipt['imports'],'dynamic':dynamic,'failures':failures},indent=2)+'\n\nThe new same-ID grant-resume entry point, atomic acquisition submission, retained single-job submission/serial worker, auth-owned acquisition-plan entry point and typed current-file selector remain in this ELF alongside the fixed requirements POST, GET/OAuth/archive APIs, owning query/metadata decoder, authenticated catalog and prior generation/preparation bodies. One selected TLS/PSA crypto owner supplies SHA256 and zeroize; format-only SHA1 remains separately scoped. This is compile/static composition evidence only, not a PS5 provider/runtime/account success claim.\n'
(AUDIT/'report.md').write_text(report)
print(json.dumps({'passed':receipt['passed'],'counts':dict(counts),'imports':receipt['imports'],'dynamic':dynamic,'failures':failures,'receipt':str(AUDIT/'receipt.json')}),flush=True)
raise SystemExit(0 if receipt['passed'] else 1)
