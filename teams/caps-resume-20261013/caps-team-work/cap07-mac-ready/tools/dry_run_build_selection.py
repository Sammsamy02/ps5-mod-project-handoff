"""Execute selected pure driver definitions against a verified owned stage.
Never import a build driver, execute main, inspect object bytes or call a compiler.
"""
import sys
sys.dont_write_bytecode=True
import ast,json,copy,hashlib,shlex,argparse
from pathlib import Path
H=Path(__file__).resolve().parent.parent
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
PREFIX='outputs/2026-10-08-l26-runtime/'
RT=ROOT/PREFIX

def read(p,cap=4*1024*1024):
 with Path(p).open('rb') as f:b=f.read(cap+1)
 if len(b)>cap:raise ValueError('bounded metadata/source limit')
 return b

def selected(source,names):
 t=ast.parse(source);nodes=[n for n in t.body if (isinstance(n,ast.FunctionDef) and n.name in names) or (isinstance(n,ast.Assign) and any(isinstance(v,ast.Name) and v.id in names for v in n.targets))]
 if len(nodes)!=len(names):raise ValueError('definition census')
 return ast.fix_missing_locations(ast.Module(body=nodes,type_ignores=[]))

def load_stage(tree=None):
 tree=Path(tree or H/'derived/review-final').resolve()
 if tree.parent!=(H/'derived').resolve():raise ValueError('owned derived stage required')
 raw=read(tree/'INTEGRATION.json');receipt=json.loads(raw);files={}
 for row in receipt['files']:
  path=(tree/row['path']).resolve()
  if not path.is_relative_to(tree):raise ValueError('staged path outside owned stage')
  b=read(path)
  if len(b)!=row['bytes'] or hashlib.sha256(b).hexdigest()!=row['sha256']:raise ValueError('staged file SHA/size drift: '+row['path'])
  files[row['path']]=b
 patch=read(tree/'integration.patch')
 identity={'path':str(tree),'integration_receipt_sha256':hashlib.sha256(raw).hexdigest(),'integration_patch_sha256':hashlib.sha256(patch).hexdigest(),'verified_files':receipt['files']}
 return tree,receipt,files,identity

def audit_originals(receipt):
 rows=[]
 for row in receipt['originals']:
  path=ROOT/row['path']
  try:
   b=read(path);actual={'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
  except FileNotFoundError:actual={'missing':True}
  rows.append({'path':row['path'],'expected':{'bytes':row['bytes'],'sha256':row['sha256']},'actual':actual,'matches':actual=={'bytes':row['bytes'],'sha256':row['sha256']}})
 return {'root':str(ROOT),'all_match':all(row['matches'] for row in rows),'files':rows,'policy':'Read-only audit; drift requires explicit rebind before integration, never silent rebase.'}

def dry_run(tree=None):
 TREE,receipt,files,identity=load_stage(tree)
 pins={x['path']:x['sha256'] for x in receipt['files']}
 def sha(p):
  path=Path(p);rel=str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else None
  b=files[rel] if rel in files else read(path)
  return hashlib.sha256(b).hexdigest()
 basepath=ROOT/'outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json';raw=read(basepath,8*1024*1024);base=json.loads(raw)
 source=files[PREFIX+'build_variants.py'];env={'HERE':RT,'Path':Path,'copy':copy,'sha':sha}
 exec(compile(selected(source,{'NEW_UNITS','make_new_units','compute_selection'}),'<verified driver definitions>','exec'),env)
 units=env['make_new_units'](base['selected_units'],1,{},None)
 assert len(units)==5 and sum(u['name']=='pakmount' for u in units)==1
 pak=next(u for u in units if u['name']=='pakmount');assert pak['argv'].count(str(RT/'typescan-r4/candidate/src/RuntimePakMount.cpp'))==1
 try:env['compute_selection'](None,incremental=True)
 except RuntimeError as e:assert 'full header-consumer rebuild' in str(e)
 else:raise ValueError('unsafe incremental admitted')
 profile=json.loads(files[PREFIX+'profile/CHANGES.json']);types=json.loads(files[PREFIX+'typescan-r4/CHANGES.json'])
 for rel in pins:
  if rel.endswith(('UnrealInitializer.cpp','LuaMod.cpp')):
   row=next(x for x in types['files'] if x['candidate']==str(ROOT/rel));assert row['candidate_sha256']==pins[rel]
 for name in ['RuntimePakMount.hpp','PakRequestProducer.hpp','NativePakMountAdapter.inl','NativePakMountEntry.hpp','pak_mount_pins.inc']:
  path=str(RT/'typescan-r4/include'/name);assert path in types['added_headers'];assert types['added_header_hashes'][path]==sha(path)
 # Real packaging helper on the staged mirror, with the same header pin relocated
 # solely to that mirror. Its hard pin/missing checks run unchanged.
 pack=files[PREFIX+'l26_packaging_driver.py'];e={'L26RUN':TREE/PREFIX,'Path':Path,'sha':sha};exec(compile(selected(pack,{'pakmount_profile_overlay'}),'<verified packaging helper>','exec'),e)
 pp=str(RT/'profile/units/PakMountCodeProfile.hpp');mirror=str(TREE/(PREFIX+'profile/units/PakMountCodeProfile.hpp'))
 prof=copy.deepcopy(profile);prof['added_headers']=[mirror if p==pp else p for p in prof['added_headers']];prof['added_header_hashes'][mirror]=prof['added_header_hashes'].pop(pp)
 overlay=e['pakmount_profile_overlay'](prof);assert overlay['destination']=='lifecycle/PakMountCodeProfile.hpp'
 # Actual BASE compiler dependency metadata only; never read its SDK/source/object targets.
 checked=0;bytes_read=0;consumers=[];missing=[]
 for u in base['selected_units']:
  p=Path(u['dependency'])
  if not p.is_file():missing.append({'unit':u['name'],'reason':'metadata missing'});continue
  if p.stat().st_size>1024*1024 or bytes_read+p.stat().st_size>64*1024*1024:
   missing.append({'unit':u['name'],'reason':'bounded dependency census64MiB cap; no content read'});continue
  b=read(p,1024*1024);bytes_read+=len(b)
  checked+=1;paths=shlex.split(b.decode().replace('\\\n',' '))[1:]
  if any(Path(p).name=='CodeImageCache.hpp' for p in paths):consumers.append(u['name'])
 assert checked>0
 out={'label':'Metadata-only exact driver-function dry run; not a full --plan/build','verified_driver_definitions':['NEW_UNITS','make_new_units','compute_selection incremental refusal','pakmount_profile_overlay'],'new_units':[u['name'] for u in units],'pakmount_source':pak['source'],'template':'unreal-unrealinitializer','expected_link_inputs':391+len(units),'source_rows_refreshed':2,'new_runtime_header_pins':5,'required_profile_overlay':overlay,'cache_layout':'kMaxPins16 via promoted candidate plus packaging overlay; full build required','dependency_metadata_checked':checked,'dependency_metadata_bytes':bytes_read,'base_code_cache_consumers':consumers,'unread_dependency_metadata':missing,'dependency_census_complete':not missing,'base_receipt_sha256':hashlib.sha256(raw).hexdigest(),'compiler_or_target_execution':False,'binary_reads':False,'full_compute_selection_run':False,'limits':['Existing BASE dependency metadata describes prior compilation only; actual new deps/header-consumer cascade and packaging compilation remain next-build gates.','The packaging helper ran on a staged mirror with its original pin value preserved; no package tree or main output was written.']}
 out['stage']=identity
 out['original_input_audit']=audit_originals(receipt)
 return out

def fresh_receipt(path):
 path=Path(path).resolve()
 if path.parent!=(H/'evidence').resolve() or path.exists():raise ValueError('fresh owned evidence receipt required')
 return path

def write_receipt(path,value):
 path=fresh_receipt(path)
 with path.open('x') as f:json.dump(value,f,indent=2);f.write('\n')

def main(argv=None):
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--stage',type=Path,default=H/'derived/review-final',help='Owned stage (default: authoritative review-final)')
 parser.add_argument('--out',type=Path,required=True,help='Fresh receipt under owned evidence/')
 parser.add_argument('--audit-out',type=Path,help='Optional separate live-original audit under owned evidence/')
 args=parser.parse_args(argv)
 out=fresh_receipt(args.out)
 audit_out=fresh_receipt(args.audit_out) if args.audit_out else None
 if audit_out==out:raise ValueError('separate audit receipt required')
 result=dry_run(args.stage)
 write_receipt(out,result)
 if audit_out:write_receipt(audit_out,{'stage':result['stage'],'original_input_audit':result['original_input_audit']})
 print('PASS metadata driver functions:',len(result['stage']['verified_files']),'verified staged files;',result['dependency_metadata_checked'],'dependency metadata files; no compiler/object reads')
 if not result['original_input_audit']['all_match']:print('ORIGINAL INPUT DRIFT: explicit rebind required before integration; see receipt')

if __name__=='__main__':main()
