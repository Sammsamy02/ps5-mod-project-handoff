"""Scoped filename/JSON provenance inventory; never reads .ucas/.utoc/.bin contents."""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import hashlib,json
H=Path(__file__).resolve().parent;R=H.parents[3];B=H.parent.parent
reqpath=B/'cap08/base-identity-audit/base-input-audit.json';vpath=B/'VERIFICATION.json';prior=B/'cap12/local-payload-search.json';audit=json.loads(reqpath.read_text());v=json.loads(vpath.read_text());assert v['schema']==11
requirements=[]
for x in audit['missing_owner_stores']:requirements.append(dict(x['geometry'],role='owner_store'))
for x in audit['required_base_export_inputs']:
 for g in x['captured_owners']:requirements.append(dict(g,role='base_export',package_id=x['package_id']))
required_ids={x['chunk_id'] for x in requirements};wanted_lengths={x['logical_length'] for x in requirements}
# Scope chosen from retained sources in base identity audit and preceding scoped search, not a home-wide discovery.
scopes=['.superpowers/native-observations/unreal-external-dependency-headers-1','.superpowers/native-observations/unreal-external-dependency-headers-2','.superpowers/native-observations/unreal-external-dependency-headers-3','.superpowers/native-observations/townfall-original-container-header-1','.superpowers/native-observations/townfall-main-utoc-2026-10-01','.superpowers/native-dependency-spike/dependency-owner-1','.superpowers/native-dependency-spike/main-container-header-read-plan-1','.superpowers/native-dependency-spike/unreal-dependency-compatibility-1','outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence','outputs/2026-10-12-caps-prep-chatgpt/cap08/base-identity-audit','outputs/2026-10-12-caps-prep-chatgpt/cap08/base-payload-intake']
files=[];metadata=[];hints=[];unparsed=[]
for scope in scopes:
 root=R/scope
 if not root.exists():continue
 for p in sorted(root.rglob('*')):
  if not p.is_file() or p.is_symlink():continue
  if any(q in ('__pycache__','.git','source','upstream','vendor','node_modules') for q in p.relative_to(root).parts):continue
  n=p.stat().st_size;rel=str(p.relative_to(R));row={'path':rel,'bytes':n};files.append(row)
  if p.suffix=='.bin' and n in wanted_lengths:row['required_length_match_only']='QUARANTINED: filename/size is not provenance or whole-IoHash verification'
  if p.suffix!='.json':continue
  if n>2*1024*1024:unparsed.append({'path':rel,'reason':'metadata exceeds2MiB bound'});continue
  raw=p.read_bytes();sha=hashlib.sha256(raw).hexdigest()
  try:d=json.loads(raw)
  except (UnicodeDecodeError,json.JSONDecodeError):unparsed.append({'path':rel,'reason':'not UTF8 JSON'});continue
  metadata.append({'path':rel,'bytes':n,'sha256':sha});encoded=json.dumps(d)
  matched=sorted(s for s in required_ids if s in encoded)
  if matched:hints.append({'path':rel,'required_raw_ids_mentioned':matched,'classification':'metadata reference only; not payload admission'})
# Check current source/test artifact bytes against schema11; never hash payload files.
bind=[]
for x in v['files']:
 p=B/x['path']
 if not x['path'].startswith(('cap07/','cap08/')):continue
 if p.suffix!='.py' and not (p.suffix in ('.log','.txt') and ('verification' in p.name or 'green' in p.name or 'test-results' in p.name)):continue
 assert p.stat().st_size<2*1024*1024
 sha=hashlib.sha256(p.read_bytes()).hexdigest();bind.append({'path':x['path'],'expected_sha256':x['sha256'],'actual_sha256':sha,'matches':sha==x['sha256']})
for pp,sha in v['utility_tests']['external_fixture_sources_matched'].items():
 p=Path(pp);actual=hashlib.sha256(p.read_bytes()).hexdigest();bind.append({'path':str(p.relative_to(R)),'expected_sha256':sha,'actual_sha256':actual,'matches':actual==sha})
assert bind and all(x['matches'] for x in bind),'schema11 source/log binding changed'
composer=B/'cap07/compose_by_owner.py';old=json.loads((B/'cap07/evidence/leon-composition-refusal.json').read_text());cs=hashlib.sha256(composer.read_bytes()).hexdigest();assert cs==old['composer_sha256']
quarantine=[{'path':str(Path(x['header_source']['path']).relative_to(R)),'recorded_bytes':x['header_source']['bytes'],'package_id':x['package_id'],'reason':x['reason'],'admitted':False} for x in audit['quarantined_partial_headers']]
receipt={'label':'Fresh Mac-only bounded metadata inventory; no payload reads or admission','schema_binding':11,'scope':scopes,'scope_exists':{s:(R/s).exists() for s in scopes},'scope_file_count':len(files),'files':files,'json_metadata_checked':len(metadata),'json_metadata':metadata,'unparsed_metadata':unparsed,'raw_id_metadata_hints':hints,'requirements':[{k:x[k] for k in ('role','owner_toc_sha256','owner_container_id_u64','toc_row','chunk_id','logical_length','expected_iohash_20')} for x in requirements],'owner_catalogue_gap_packages':[x['package_id'] for x in audit['required_base_export_inputs'] if not x['captured_owners']],'existing_quarantined_headers':quarantine,'new_inputs_admitted':0,'admission_policy':'No filename/size/reference match admits input. Complete owner TOC/container/row/rawID/full logical length/whole-IoHash plus actual payloadSHA binding required; this inventory never reads or verifies payload bytes.','source_log_bindings':bind,'composer_sha256':cs,'composer_matches_prior_refusal':True,'prior_search_sha256':hashlib.sha256(prior.read_bytes()).hexdigest(),'requirements_audit_sha256':hashlib.sha256(reqpath.read_bytes()).hexdigest(),'schema11_verification_sha256':hashlib.sha256(vpath.read_bytes()).hexdigest(),'historical_tests_rerun':False,'limits':'Only named retained capture/prep directories. Binary and TOC contents not opened. No global absence claim; unlabelled inputs, other directories and omitted/oversized metadata remain unproved.'}
(H/'inventory.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS:',len(scopes),'named scopes,',len(files),'file stats,',len(metadata),'JSON metadata,',len(bind),'schema11 source/log hashes; composer unchanged;0 payload reads;0 inputs admitted')
print('Length-only candidates:',[f['path'] for f in files if 'required_length_match_only' in f]);print('Raw-ID references:',len(hints),'unparsed metadata:',len(unparsed))
