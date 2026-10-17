"""Pin the owned stage and prove its exact textual patch apply/reverse on a fresh owned copy."""
import difflib,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
OWN=Path(__file__).resolve().parents[1];STAGE=OWN/'derived/target-ready';MAIN=Path('/WORKSPACE/.codex/worktrees/translation-core/what');TEAM=OWN.parent
RT=STAGE/'outputs/2026-10-08-l26-runtime'
def pin(path):
 p=Path(path);return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
def write(path,data):Path(path).write_text(json.dumps(data,indent=2)+'\n')
def main():
 parent=json.loads((TEAM/'cap07-mac-ready/derived/target-ready/INTEGRATION.json').read_text())
 for row in parent['originals']:
  source=Path(row['path']);source=source if source.is_absolute() else MAIN/source
  if pin(source)['sha256']!=row['sha256']:raise ValueError('main original drift; preserve new Claude changes before staging: '+str(source))
 callback=json.loads((OWN/'evidence/CALLBACK-STAGE.json').read_text())
 if pin(callback['source']['path'])['sha256']!=callback['source']['sha256']:raise ValueError('callback input drift')
 textfiles=[];originals=[];diff=[]
 for p in sorted(STAGE.rglob('*')):
  if not p.is_file() or p.name in ('INTEGRATION.json','integration.patch'):continue
  relative=p.relative_to(STAGE);source=MAIN/relative
  try:new=p.read_text(encoding='utf-8')
  except UnicodeDecodeError:continue
  if '\0' in new:continue
  if source.exists():
   old=source.read_text(encoding='utf-8');originals.append(pin(source))
  else:old=''
  if new==old:continue
  textfiles.append((p,source,relative,old,new))
  for line in difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/'+str(relative) if source.exists() else '/dev/null',tofile='b/'+str(relative)):
   diff.append(line if line.endswith('\n') else line+'\n\ No newline at end of file\n')
 patch=STAGE/'integration.patch';patch.write_text(''.join(diff))
 proof=[]
 with tempfile.TemporaryDirectory(prefix='apply-proof-',dir=OWN/'evidence') as d:
  root=Path(d)
  for p,source,relative,old,new in textfiles:
   if source.exists():target=root/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read_bytes())
  for phase,args in [('apply',['--batch','-p1','-i',str(patch)]),('reverse',['--batch','-R','-p1','-i',str(patch)])]:
   result=subprocess.run(['patch',*args],cwd=root,capture_output=True,text=True)
   if result.returncode:raise ValueError(phase+' failed: '+result.stderr)
   for p,source,relative,old,new in textfiles:
    target=root/relative
    if phase=='apply':assert target.read_bytes()==p.read_bytes(),relative
    elif source.exists():assert target.read_bytes()==source.read_bytes(),relative
    else:assert not target.exists(),relative
   proof.append({'phase':phase,'returncode':result.returncode,'files_verified':len(textfiles),'stdout':result.stdout,'stderr':result.stderr})
 record={'status':'EXACT_FRESH_COPY_APPLY_REVERSE_PASS','patch':pin(patch),'proof':proof,'originals_unchanged':all(pin(x['path'])['sha256']==x['sha256'] for x in originals),'source_files':originals,'main_applied':False}
 write(OWN/'evidence/APPLY-REVERSE.json',record)
 files=[{'path':str(p.relative_to(STAGE)),**{k:v for k,v in pin(p).items() if k!='path'}} for p in sorted(STAGE.rglob('*')) if p.is_file() and p.name!='INTEGRATION.json']
 manifest={'label':'Complete #7 target-ready + #10 concrete source/build/package stage; root acceptance pending','parent_stage':pin(TEAM/'cap07-mac-ready/derived/target-ready/INTEGRATION.json'),'source_originals':originals,'files':files,'apply_reverse_proof':pin(OWN/'evidence/APPLY-REVERSE.json'),'binary_copy_inputs':[x for x in files if x['path'].endswith(('.o','.a','.dll'))],'main_applied':False,'game_prx_build':False,'console_contact':False}
 write(STAGE/'INTEGRATION.json',manifest)
 inputs=json.loads((RT/'cap10/INTEGRATION-INPUTS.json').read_text())
 delivered=[]
 for base in ('tools','derived/target-ready'):
  for p in sorted((OWN/base).rglob('*')):
   if p.is_file() and '__pycache__' not in p.parts:delivered.append({'path':str(p.relative_to(OWN)),**{k:v for k,v in pin(p).items() if k!='path'}})
 for p in sorted((OWN/'evidence').glob('*')):
  if p.is_file() and p.suffix not in ('.o',) and p.name!='FINALIZE.log':delivered.append({'path':str(p.relative_to(OWN)),**{k:v for k,v in pin(p).items() if k!='path'}})
 for name in ('README.md','PLAN.md'):delivered.append({'path':name,**{k:v for k,v in pin(OWN/name).items() if k!='path'}})
 handoff={'version':1,'status':'MAC_CONCRETE_INTEGRATION_STAGE_VERIFIED_PENDING_ROOT_REVIEW','files':delivered,'owner_runtime_pair':inputs['owner_runtime_pair'],'stage_manifest':pin(STAGE/'INTEGRATION.json'),'apply_reverse':record['status'],'tests':pin(OWN/'evidence/TESTS.log'),'local_component_object':pin(OWN/'evidence/cap10-with-base-fixed.o'),'compile_jobs':0,'isolated_partial_links':1,'driver_import_or_main_execution':False,'game_prx_build':False,'console_contact':False,'final_provider_contract':'computed only from actual pinned bootstrap/current objects; unresolved provider identity fails','remaining':['Root acceptance of final owner/runtime pair and this integration patch','Claude complete current game compile and independent package twins with emitted exact import/type/constructor contracts','Seven real SceNet provider export identities if still unresolved after actual packaging inputs','Claude real target acceptance: managed Run/exception/recovery, lifecycle/GC/TLS/AOT and #7 engine mount']}
 write(OWN/'HANDOFF-MANIFEST.json',handoff)
 print(json.dumps({'text_files':len(textfiles),'stage_files':len(files),'handoff_files':len(delivered),'patch':pin(patch),'main_unchanged':record['originals_unchanged']}))
if __name__=='__main__':main()
