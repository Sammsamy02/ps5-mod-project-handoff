"""Focused historical receipt/NM capture audit; no compile/link/package/GO."""
from pathlib import Path
import copy,difflib,hashlib,json,os,subprocess,sys,types
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
def record(path):return {'path':str(path),'sha256':sha(path)}
prior=O.parent/'linker-dispatch-addendum/AUDIT.json'
assert sha(prior)=='2b2006e70292ad64f01b1a7e208461ed1c3d561f3095dcda311d5d0ffca44b42'
fix=P/'parent-review/LINEAGE-RECEIPT-COMPATIBILITY-FIX.v2.json'
assert sha(fix)=='3890f13a1a6862fa74a5a86def6533197912a667908616bbd4adab2f697f42c0'
f=read(fix);assert f['pass'] and f['changed_code_files']==['composition.py'] and f['focused_control_methods']==5
assert not f['source_or_profile_edits'] and not f['inherited_pipeline_files_edited'] and not f['full_package_PASS_claimed']
old_dir=Path(f['previous_execution_sources']);old_co=old_dir/'composition.py';new_co=P/'execution/composition.py'
assert sha(old_co)=='999d55bc7dcf3c25ce8cd1d167ba40bb5ecdb8fe9a47350b0101cc5c3cc3dcb4'
assert sha(new_co)=='64dfae7f45074237e1e8e95333494dc5fac4fd83c0e495614211f905da1d7020'
patch=Path(f['exact_code_diff']['path']);assert sha(patch)==f['exact_code_diff']['sha256']
actual=''.join(difflib.unified_diff(old_co.read_text().splitlines(True),new_co.read_text().splitlines(True),fromfile='prior/composition.py',tofile='new/composition.py'))
assert actual==patch.read_text()
for path,h in f['current_execution_source_pins'].items():
 assert sha(path)==h
 if Path(path).name!='composition.py':assert Path(path).read_bytes()==(old_dir/Path(path).name).read_bytes()
oldfile=P/'parent-review/EXECUTION-ADMISSION.FINAL.v3.json';newfile=Path(f['new_admission']['path'])
assert sha(oldfile)=='7aca76ab4fb738876bd657a33d14ff6be454bd6f063b0c750c7d163d867a44bc'
assert sha(newfile)==f['new_admission']['sha256']=='e54ff544894a743d3b5cceeafd02f158ebb775768f20f5849c4dce193aacdd7f'
old=read(oldfile);ad=read(newfile);changed={'ready_prep','edition','execution_source_pins'}
assert {k:v for k,v in old.items() if k not in changed}=={k:v for k,v in ad.items() if k not in changed}
for key in ['ready_prep','edition']:
 assert Path(old[key]['path']).read_bytes()==Path(ad[key]['path']).read_bytes() and sha(ad[key]['path'])==ad[key]['sha256']
assert all(h==ad['execution_source_pins'][path] for path,h in old['execution_source_pins'].items() if Path(path).name!='composition.py')
sys.path.insert(0,str(P));sys.path.insert(0,str(P/'execution'))
from admission import Admission,Refusal,pin
from composition import Composer,historical_delta,sdk_nm_evidence
import package_prx_l19 as package
recipes_path=pin(ad['affected_recipes'],'unchanged recipe receipt');recipes=read(recipes_path)
base_path=pin(recipes['base_receipt'],'actual R3 receipt');base=read(base_path)
hist_path=pin({'path':base['base_receipt'],'sha256':base['base_receipt_sha256']},'historical receipt');hist=read(hist_path)
assert sha(base_path)=='a08978798e18c1104255a580f5f7d2db202b889e9c895ceb0cb34aa3d3b3c3d7'
assert sha(hist_path)=='bcd23a2a4abef518d8f220786e80cb131036e351b2218a322d26aee6adca8384'
pf_path=pin(f['actual_no_compiler_lineage_preflight'],'actual preflight');pf=read(pf_path)
assert sha(pf_path)=='cc2be886fb5018abb66849cdd4ac8f64426a89e1267bb258e2863dd6bbb81c00'
assert pf['pass'] and not pf['full_packager_lineage_or_package_PASS_claimed']
repaired_path=pin(pf['repaired_test_receipt'],'repaired actual receipt');repaired=read(repaired_path)
assert sha(repaired_path)=='57609a525cdea2e74f3f266db662310dfd6c263b353e7dc924d50a5a0a8695be'
assert repaired_path.name.casefold()!=pf_path.name.casefold() and repaired_path.stat().st_ino!=pf_path.stat().st_ino
assert f['distinct_fixture_summary_control']['pass'] and f['distinct_fixture_summary_control']['fixture_hash_after_summary_write']==sha(repaired_path)
original_path=pin(pf['original_composition_receipt'],'preserved actual composition receipt');original=read(original_path)
permitted={'base_receipt','base_receipt_sha256','r4_recomposition_base_receipt','r4_sdk_provider_evidence','runtime_object','undefined_delta'}
assert {k:v for k,v in repaired.items() if k not in permitted}=={k:v for k,v in original.items() if k not in permitted}
assert repaired['base_receipt']==base['base_receipt'] and repaired['base_receipt_sha256']==base['base_receipt_sha256']
assert repaired['r4_recomposition_base_receipt']==recipes['base_receipt']
target=pin(pf['actual_runtime'],'preserved actual output');assert target.stat().st_size==118473352
nm=pin(ad['nm'],'actual NM provider');env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1')
def command(name,argv,run,env):
 assert run.resolve()==O.resolve() and argv[0]==str(nm)
 r=subprocess.run(argv,env=env,capture_output=True,text=True)
 assert r.returncode==0,r.stderr
 log={'name':name,'argv':argv,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
 (run/(name+'.command.json')).write_text(json.dumps(log,indent=2)+'\n')
 return log
result=command('independent-actual-undefined',[str(nm),'--extern-only','--undefined-only','--format=posix',str(target)],O,env)
current={r[0]:r[1] for r in (line.split() for line in result['stdout'].splitlines()) if len(r)>=2}
assert current==base['undefined_delta']['current'] and len(current)==210 and len(hist['undefined_delta']['current'])==197
delta=historical_delta(base,current);assert delta==repaired['undefined_delta'] and len(delta['added'])==13
assert not delta['removed'] and not delta['changed_bindings']
assert repaired['r4_actual_undefined_delta_vs_r3']['added']==[] and repaired['r4_actual_undefined_delta_vs_r3']['removed']==[] and repaired['r4_actual_undefined_delta_vs_r3']['changed_bindings']=={}
assert read(repaired_path.parent/'undefined-delta.json')==delta
res=set(delta['expected_bootstrap_resolutions']);checkpoint={'sceKernelGetModuleInfo','sceKernelVirtualQuery'}
allowed={'bridge_image_range','bridge_capture_envelope','bridge_readable_span_at','bridge_readable_span_count','diagnostic_event','bridge_kernel_range_verdict','bridge_kernel_write_copyin_probe','bridge_write_copyin_capture','bridge_kernel_write_copyin_probe_until','bridge_copyin_range_faulted','bridge_copyin_initializer_enter','bridge_copyin_initializer_leave','bridge_copyin_initializer_complete','bridge_code_cache_context','bridge_code_cache_signature','scePadGetHandle'}
assert res and res<=allowed and set(delta['added'])==checkpoint|res
raw_path=pin(pf['original_raw_lineage'],'original raw lineage');raw=read(raw_path)
assert len(raw['checks'])==35 and sum(row['pass'] for row in raw['checks'].values())==34
assert [n for n,row in raw['checks'].items() if not row['pass']]==['undefined_delta_added_matches_checkpoint_claim']
assert raw['status']=='LINEAGE_GAPS' and raw['checks']['composition_receipt_base_matches_pinned']['pass']
assert raw['checks']['sdk_nm_evidence_for_all_extended_imports']['pass']
cases=[{'case':'actual197_to210_cumulative_delta','pass':True}]
def refuse(label,fn):
 try:fn()
 except Refusal:cases.append({'case':label,'pass':True,'refused':True})
 else:raise AssertionError(label)
bad=copy.deepcopy(base);bad['base_receipt_sha256']='0'*64
refuse('historical_hash_drift',lambda:historical_delta(bad,current))
bad=copy.deepcopy(base);bad['undefined_delta']['added']=[]
refuse('fabricated_empty_historical_claim',lambda:historical_delta(bad,current))
badcurrent=dict(current);badcurrent['malloc']='w'
refuse('actual_binding_drift',lambda:historical_delta(base,badcurrent))
badcurrent=dict(current);badcurrent.pop('malloc')
refuse('actual_symbol_removal',lambda:historical_delta(base,badcurrent))
bad=copy.deepcopy(base);lib=next(p for p in bad['undefined_delta']['provider_library_hashes'] if Path(p).name=='libkernel.so');bad['undefined_delta']['provider_library_hashes'][lib]='0'*64
calls=[];refuse('provider_drift_before_any_command',lambda:sdk_nm_evidence(bad,nm,lambda *a:calls.append(a),O,env));assert not calls
fresh=sdk_nm_evidence(base,nm,command,O,env);assert len(fresh)==3
required=set(package.ALL_EXTENDED_IMPORT_EVIDENCE.values());assert {Path(r['path']).name for r in fresh.values()}==required
for name,r in fresh.items():
 saved=pf['fresh_actual_SDK_nm_evidence'][name]
 for k in ['provider','command']:
  if k=='provider':assert r[k]==saved[k]
  pin(saved[k],'producer SDK '+k)
 savedfile=pin({'path':saved['path'],'sha256':saved['sha256']},'producer stdout')
 captured=read(r['command']['path']);oldcaptured=read(saved['command']['path'])
 assert captured['argv']==oldcaptured['argv']==[str(nm),'-D','--defined-only','--format=posix',r['provider']['path']]
 assert captured['exit']==oldcaptured['exit']==0
 assert Path(r['path']).read_bytes()==savedfile.read_bytes()==captured['stdout'].encode()==oldcaptured['stdout'].encode()
 assert base['undefined_delta']['provider_library_hashes'][r['provider']['path']]==r['provider']['sha256']
cases.append({'case':'three_fresh_actual_sdk_captures_original_flags_and_stdout','pass':True})
for symbol,filename in package.ALL_EXTENDED_IMPORT_EVIDENCE.items():
 assert any(parts[0]==symbol for parts in (line.split() for line in (O/filename).read_text().splitlines()) if parts)
try:Admission.load(newfile,sha(newfile),None,None)
except Refusal as ex:assert 'GO' in str(ex)
else:raise AssertionError('missing GO accepted')
controls=pin(f['focused_controls'],'owner controls');assert 'Ran 5 tests' in controls.read_text() and controls.read_text().rstrip().endswith('OK')
blocker=O/'BLOCKER.v1.json';assert sha(blocker)=='11e5a5885ef18b2f6101072c87956b35a6bc178f262d0a18e074ba22af74b621'
files=[Path(__file__),prior,fix,patch,old_co,new_co,oldfile,newfile,recipes_path,base_path,hist_path,pf_path,repaired_path,original_path,raw_path,controls,Path(f['focused_test_source']['path']),Path(f['preflight_script']['path']),target,blocker]
assert sha(f['focused_test_source']['path'])==f['focused_test_source']['sha256']
assert sha(f['preflight_script']['path'])==f['preflight_script']['sha256']
out={'pass':True,'findings':[],'scope':'Narrow receipt historical lineage and actual SDK NM captures; no full lineage/package success claim','admission':record(newfile),'current_execution_source_pins':f['current_execution_source_pins'],'exact_one_file_diff_verified':True,'historical_baseline':record(hist_path),'immediate_R3_base':record(base_path),'actual_typed_counts':{'historical':197,'R3':210,'R4':210},'cumulative_added':delta['added'],'cumulative_removed':[],'cumulative_binding_changes':{},'immediate_R4_vs_R3_delta_empty':True,'historical_fields_preserved':True,'producer_raw_checks':{'passed':34,'preserved_discrepancy':['undefined_delta_added_matches_checkpoint_claim'],'status':'LINEAGE_GAPS'},'unchanged_stage_aware_predicate_matches':True,'stage_aware_fresh_compile_and_package':'PENDING actual inherited pipeline; not performed or waived','fresh_independent_SDK_nm_evidence':fresh,'producer_captures_byte_equal_to_fresh_and_command_stdout':True,'independent_cases':cases,'owner_focused_controls':5,'missing_GO_refuses':True,'source_profile_recipes_or_inherited_gates_changed':False,'compile_link_package_restart_or_console_by_auditor':False,'GO_created':False,'pins':{str(p):sha(p) for p in files}}
out.update(resolved_findings=[{'finding':'case-insensitive receipt.json/RECEIPT.json collision in prior V2 diagnostic','preserved_blocker':record(blocker),'resolution':'NEW V3 fixture receipt.json and PREFLIGHT-SUMMARY.json have distinct names/inodes and verified post-summary fixture hash','pass':True}],repaired_actual_composition_only_permitted_fields_changed=True,distinct_fixture_summary_control={'pass':True,'fixture':record(repaired_path),'summary':record(pf_path),'distinct_inodes':True})
(O/'AUDIT.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'pass':True,'independent_cases':len(cases),'fresh_sdk_captures':3,'historical_counts':'197->210','raw_checks_passed':34,'raw_checkpoint_discrepancy_preserved':True,'audit_sha256':sha(O/'AUDIT.json')}))
