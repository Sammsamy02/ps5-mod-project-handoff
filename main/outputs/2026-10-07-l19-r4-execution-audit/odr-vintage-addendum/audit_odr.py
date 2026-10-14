"""Independent bounded ODR generation audit; no compile/link/package/restart."""
from pathlib import Path
import copy,difflib,hashlib,json,os,subprocess,sys,types
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
def rec(p):return {'path':str(p),'sha256':sha(p)}
def pinned(r):assert sha(r['path'])==r['sha256'],r['path'];return Path(r['path'])
fpath=P/'parent-review/ODR-GENERATION-CLASSIFICATION-FIX.json';assert sha(fpath)=='053c8f7daf713bfbea47c87f37da27085a6020001aff8f4162eec74a62bb81f9';f=read(fpath)
assert f['pass'] and not f['full_pair_or_twin_PASS_claimed'] and not f['source_or_profile_changes'] and not f['inherited_pipeline_files_edited']
for path,h in f['current_execution_source_pins'].items():assert sha(path)==h
old_code=P/'parent-review/execution-hook-v5.sources/full_pipeline.py';new_code=P/'execution/full_pipeline.py'
patch=pinned(f['execution_diff'])
assert ''.join(difflib.unified_diff(old_code.read_text().splitlines(True),new_code.read_text().splitlines(True),fromfile='prior/full_pipeline.py',tofile='new/full_pipeline.py'))==patch.read_text()
original=pinned(f['original_checker']);derivative=pinned(f['derivative_checker']);checker_patch=pinned(f['checker_diff'])
assert ''.join(difflib.unified_diff(original.read_text().splitlines(True),derivative.read_text().splitlines(True),fromfile='original/check_odr_emitters_l19.py',tofile='new/check_odr_emitters_r4_vintage_v2.py'))==checker_patch.read_text()
new_ad=pinned(f['new_admission']);assert sha(new_ad)=='4508068d108ea07d270f2fdb588d1f8b1eec0eff901ce22cd69b6d27a60bb42f'
old_ad=P/'parent-review/EXECUTION-ADMISSION.FINAL.v4.json';a=read(new_ad);old=read(old_ad)
changes={'ready_prep','edition','execution_source_pins','additional_source_pins','odr_generation_checker','odr_baseline_receipt'}
assert {k:v for k,v in old.items() if k not in changes}=={k:v for k,v in a.items() if k not in changes}
for key in ['ready_prep','edition']:
 assert Path(old[key]['path']).read_bytes()==pinned(a[key]).read_bytes()
assert all(a['additional_source_pins'][p]==h for p,h in old['additional_source_pins'].items())
assert set(a['additional_source_pins'])-set(old['additional_source_pins'])=={str(derivative)}
assert a['odr_generation_checker']==f['derivative_checker']
sys.path[:0]=[str(P/'execution'),str(P)]
from admission import Admission,Refusal,ROLE
from full_pipeline import load_odr_generation
from composition import compile_argv
genfile=pinned(f['actual_generation_provenance']);g=read(genfile);run=Path(g['run_directory'])
assert g['baseline_ODR']==a['odr_baseline_receipt'] and g['affected_recipes']==a['affected_recipes'] and g['source_manifest']==a['source_manifest']
base=read(pinned(g['baseline_composition']));current_path=pinned(g['current_composition']);current=read(current_path)
recipes=read(pinned(g['affected_recipes']))['recipes'];records=read(pinned(g['source_manifest']))['selected_sources']
R=set(recipes);B=set(base['freshly_compiled_units']);assert len(R)==147 and len(B)==26 and len(R&B)==4 and len(R|B)==169
assert len(current['freshly_compiled_units'])==3 and len(current['r4_reused_accepted_pass1_units'])==144
assert g['actual_this_pass_fresh_compiles']==current['freshly_compiled_units'] and g['actual_prior_accepted_reuse']==current['r4_reused_accepted_pass1_units']
context=types.SimpleNamespace(records=records,recipes={'recipes':recipes},pins={'generated_header':Path(a['generated_header']['path']),'profile_header':Path(a['profile_header']['path'])})
paths=load_odr_generation(genfile,sha(genfile),run);assert len(paths)==169 and sum(p.startswith(str(run)+'/') for p in paths)==3
inputsfile=run/'input-hashes.json';inputs=list(read(inputsfile));slots=read(pinned(a['input_substitutions']))['input_slots'];assert len(inputs)==len(slots)==386
checked=[]
for index,slot in enumerate(slots):
 name=slot['replace_from_affected_recipe']
 if not name:continue
 unit=current['selected_runtime_units'][name];recipe=recipes[name]
 assert inputs[index]==unit['object'] and recipe['composition_input_index']==index
 assert sha(unit['dependency_file'])==unit['dependency_sha256']
 assert unit['source_sha256']==(records[ROLE[name]] if name in ROLE else recipe['final_source'])['sha256']
 if unit['r4_pass1_reused']:
  assert unit['argv']==recipe['pass1_argv'] and unit['object_sha256']==recipe['pass1_object']['sha256'] and unit['dependency_sha256']==recipe['pass1_dependency']['sha256']
 else:
  assert unit['argv']==compile_argv(context,name,run)
 checked.append({'name':name,'input_index':index,'source_sha256':unit['source_sha256'],'object_sha256':unit['object_sha256'],'dependency_sha256':unit['dependency_sha256'],'actual_this_pass_compile':not unit['r4_pass1_reused']})
assert len(checked)==147 and sum(c['actual_this_pass_compile'] for c in checked)==3
cases=[{'case':'exact169_generation_and147_recipe_slot_provenance','pass':True}]
def mutated(label,edit,currentedit=None):
 d=copy.deepcopy(g)
 if currentedit:
  c=copy.deepcopy(current);currentedit(c);p=O/(label+'-current.json');p.write_text(json.dumps(c));d['current_composition']=rec(p)
 edit(d);p=O/(label+'-generation.json');p.write_text(json.dumps(d))
 try:load_odr_generation(p,sha(p),run)
 except Refusal as ex:cases.append({'case':label,'pass':True,'refused':True,'reason':str(ex)})
 else:raise AssertionError(label)
mutated('wrong_object',lambda d:None,lambda c:c['selected_runtime_units']['native-loadercontext'].update(object_sha256='0'*64))
mutated('wrong_source',lambda d:None,lambda c:c['selected_runtime_units']['native-loadercontext'].update(source_sha256='0'*64))
mutated('missing_provider',lambda d:None,lambda c:c['selected_runtime_units'].pop('native-loadercontext'))
mutated('extra_R3_provider',lambda d:d['eligible_R3_generation_units'].append('unreal-ffield'))
mutated('map_drift',lambda d:d['actual_map'].update(sha256='0'*64))
mutated('dependency_drift',lambda d:None,lambda c:c['selected_runtime_units']['unreal-ffield'].update(dependency_sha256='0'*64))
mutated('missing_replacement',lambda d:d['eligible_replacement_units'].remove('unreal-ffield'))
mutated('R3_ODR_hash_drift',lambda d:d['baseline_ODR'].update(sha256='0'*64))
commands=[]
def odr(label,obj,expect):
 out=O/(label+'.json');argv=[sys.executable,str(derivative),str(run),'--object',str(obj),'--require-program-fresh','--generation-receipt',str(genfile),'--generation-receipt-sha256',sha(genfile),'--json',str(out)]
 r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'));assert r.returncode==expect,r.stderr
 result=read(out);commands.append({'label':label,'argv':argv,'exit':r.returncode,'receipt':rec(out)})
 return result
positive=odr('INDEPENDENT-ODR-POSITIVE',current['runtime_object'],0)
assert positive['result']=='PASS' and len(positive['checks'])==19 and all(c['ok'] for c in positive['checks'].values())
assert positive['units']['fresh']==169 and len(positive['R4_generation_classification']['actual_this_pass_object_paths'])==3
faults_path=pinned(f['actual_composed_ELF_fault_controls']);faults=read(faults_path);assert faults['pass']
for name,fault in faults['controls'].items():
 obj=pinned(fault['mutated_ELF']);pinned(fault['original_check_receipt'])
 negative=odr('INDEPENDENT-'+name.upper(),obj,1)
 assert negative['result']=='FAIL' and [k for k,v in negative['checks'].items() if not v['ok']]==['C5_nms_composed_single_and_body_equal']
old26=pinned(f['prior_exact26_refusal_preserved']);assert read(old26)['result']=='FAIL'
control_log=pinned(f['focused_controls_log']);assert 'Ran 9 tests' in control_log.read_text() and control_log.read_text().rstrip().endswith('OK')
try:Admission.load(new_ad,sha(new_ad),None,None)
except Refusal as ex:assert 'GO' in str(ex)
else:raise AssertionError('GO bypass')
down=O/'downstream';fin=read(down/'FINALIZER-RECEIPT.json');gate=read(down/'GATE-V2-RECEIPT.json');access=read(down/'ACCESSOR-CONTROL.json')
assert gate['failed_checks']==['D1'] and access['pass']
files=[Path(__file__),fpath,old_code,new_code,patch,original,derivative,checker_patch,new_ad,old_ad,genfile,current_path,inputsfile,faults_path,old26,control_log,Path(f['focused_control_source']['path']),Path(f['actual_fault_controls_source']['path']),O/'DIAGNOSIS.json',down/'FINALIZER-RECEIPT.json',down/'GATE-V2-RECEIPT.json',down/'ACCESSOR-CONTROL.json']
assert sha(f['focused_control_source']['path'])==f['focused_control_source']['sha256'] and sha(f['actual_fault_controls_source']['path'])==f['actual_fault_controls_source']['sha256']
out={'pass':True,'findings':[],'scope':'Precise ODR generation classifier derivative only; no native sources/profile/inherited predicates changed','admission':rec(new_ad),'execution_source_pins':f['current_execution_source_pins'],'original_predicate_diff_verified_exact':True,'derivative':rec(derivative),'generation':rec(genfile),'set_geometry':{'replacement147':147,'R3_generation26':26,'overlap4':4,'union169':169,'outside_replacements22':22},'actual_compiles':3,'actual_accepted_reuse':144,'pass2_required_actual_compiles':147,'exact147_source_object_dependency_recipe_and386_slot_provenance':checked,'independent_membership_cases':cases,'independent_original_native_predicate_count':19,'independent_actual_ELF_body_and_symbol_faults_refuse_C5':True,'original26_refusal_preserved':rec(old26),'ODR_only_commands':commands,'downstream_consumers':{'original_finalizer_exit':0,'original_accessor_verifier':'PASS all3 stages/0errors','original_gate_v2':'all pass except first-candidate D1 pending real twin'},'existing_b4b_advisory_policy_changed':False,'GO_missing_refuses':True,'compile_link_package_restart_console_or_GO_by_auditor':False,'producer_control_count':9,'pins':{str(p):sha(p) for p in files}}
(O/'AUDIT.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'pass':True,'generation_union':169,'actual_compiles':3,'actual_reuse':144,'membership_cases':len(cases),'native_predicates':19,'real_ELF_negative_controls':2,'audit_sha256':sha(O/'AUDIT.json')}))
