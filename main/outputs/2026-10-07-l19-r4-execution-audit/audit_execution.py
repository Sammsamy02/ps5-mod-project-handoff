"""Read-only independent audit of frozen R4 execution/control inputs."""
from pathlib import Path
import ast,hashlib,json,subprocess,sys,types
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[1];P=W/'outputs/2026-10-07-l19-r4-full-build'
cache={}
def sha(p):
 p=Path(p).resolve()
 if p not in cache:cache[p]=hashlib.sha256(p.read_bytes()).hexdigest()
 return cache[p]
def checked(record):
 p=Path(record['path']);assert p.is_file() and not p.is_symlink() and sha(p)==record['sha256'],p
 return p
def read(p):return json.loads(Path(p).read_text())
v5=P/'parent-review/EXECUTION-ADMISSION.FINAL-INPUTS-PENDING.v5.json'
fix=P/'parent-review/EXECUTION-HOOK-ALIAS-FIX.json'
assert sha(v5)=='40631bcb9eb1555b380b6040f1b39854d78dc532157bff3722c1fa83510ad0a9'
assert sha(fix)=='8ac7c69f50a83d63f2c53e4f19a9b2c1049bcba18bbcad3cda18961780a6f69b'
a=read(v5);f=read(fix);assert f['pass'] and f['actual_behavior_tests']==13
actual={str(p):sha(p) for p in (P/'execution').glob('*.py')}
assert actual==a['execution_source_pins'] and len(actual)==5
frozen=P/'parent-review/FROZEN-PREPARATION.json';fr=read(frozen)
for path,h in fr['pins'].items():assert sha(P/path)==h,path
clone=P/'PIPELINE-CLONE-RECEIPT.json';cl=read(clone)
assert len(cl['byte_identical_root_files'])==14
for path,r in cl['byte_identical_root_files'].items():assert sha(P/path)==r['sha256'],path
sys.path.insert(0,str(P/'execution'))
from admission import Admission,dependencies,validate_recipe_set
from composition import compile_argv
recipes=read(checked(a['affected_recipes']));subs=read(checked(a['input_substitutions']))
validate_recipe_set(recipes,subs)
slots=subs['input_slots'];assert len(slots)==386 and sum(bool(s['replace_from_affected_recipe']) for s in slots)==147
assert [s['position'] for s in slots]==list(range(386))
base=read(checked(recipes['base_receipt']));inputs=read(checked(recipes['base_inputs']))
assert list(inputs)==[s['old_input']['path'] for s in slots]
assert len(base['selected_runtime_units'])==369 and len(set(base['runtime_object_order']))==369
records=read(checked(a['source_manifest']))['selected_sources']
allowed={str(Path(p).resolve()):h for p,h in base['source_hashes'].items()}
allowed.update(a['additional_source_pins'])
for r in records.values():allowed[str(Path(r['path']).resolve())]=r['sha256']
owner_header=checked(recipes['recipes']['program']['required_accepted_header'])
owner=checked(recipes['owner_source']);allowed[str(owner_header)]=sha(owner_header);allowed[str(owner)]=sha(owner)
generated=checked(a['generated_header']);allowed[str(generated)]=sha(generated)
pass1=read(checked(recipes['owner_pass1']));previous={r['name']:r for r in pass1['selected_units']}
reused=0;dep_total=0;dep_unique=set()
for name,r in recipes['recipes'].items():
 if not r['pass1_reusable']:continue
 reused+=1;checked(r['final_source']);checked(r['pass1_object']);dep=checked(r['pass1_dependency'])
 assert r['pass1_argv']==previous[name]['argv'],name
 actual_deps=dependencies(dep);assert str(owner_header) in actual_deps,name
 dep_total+=len(actual_deps);dep_unique.update(actual_deps)
 for path in actual_deps:assert path in allowed and sha(path)==allowed[path],(name,path)
assert reused==144
for path,h in a['tool_pins'].items():checked({'path':path,'sha256':h})
alias_audit=Admission();alias_audit.data=a;alias_audit.pins={'linker':Path(a['linker']['path'])};alias_audit.tool_pins=a['tool_pins'];alias_audit.validate_tool_aliases()
alias_records=a['tool_aliases'];assert len(alias_records)==1
sdk=Path(next(p for p in a['tool_pins'] if p.endswith('/prospero-clang++'))).parent
assert str(sdk/'clang++') in a['tool_pins'] and str(sdk/'prospero-llvm-config') in a['tool_pins']
# Independently examine every actual recipe argument transformation.
obj=types.SimpleNamespace(recipes=recipes,records=records,pins={k:Path(a[k]['path']) for k in ['profile_header','generated_header']})
for name,r in recipes['recipes'].items():
 argv=compile_argv(obj,name,P/'runs/audit-not-created')
 original=r['original_argv'];extra=len(argv)-len(original);restored=[argv[0],*argv[1+extra:]]
 for flag in ['-o','-MF']:restored[restored.index(flag)+1]=original[original.index(flag)+1]
 source=records[{'program':'Program','unreal-unrealinitializer':'initializer','native-layoutbinding':'NativeLayoutBinding.cpp'}[name]]['path'] if name in ['program','unreal-unrealinitializer','native-layoutbinding'] else r['final_source']['path']
 restored[restored.index(source)]=r['old_selected_source']['path'];assert restored==original,name
 assert sum('RC_NATIVE_LAYOUT_DECLARATIONS_HEADER' in x for x in argv)==(1 if name=='program' else 0)
def calls(path,function):
 tree=ast.parse(path.read_text());fn=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==function)
 return {n.func.id for n in ast.walk(fn) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name)}
driver=P/'build_runtime_composition6_l19.py';launcher=P/'launch19_one_pass.py'
assert {'odr_gate','receipt_preflight','edited_landed_gate','entry_preflight','chain_symbols_gate','build_overlay','package_phase'}<=calls(driver,'main')
assert {'require_ready','verify_units','admission_gate','regen_receipts','preflight_and_verify','the_pass','post_build','determinism'}<=calls(launcher,'main')
assert {'whole_selected_gate','luamod_comparator_gate','consumer_und_gate'}<=calls(launcher,'post_build')
before={str(p) for pattern in ['runs/*','prx-candidate*'] for p in P.glob(pattern)}
cmd=[sys.executable,'-B','-m','unittest','discover','-s',str(P/'tests'),'-p','test_*.py','-v']
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
missing=[sys.executable,'-B',str(P/'execution/full_pipeline.py'),'--admission',str(v5),'--admission-sha256',sha(v5),'--full']
t=subprocess.run(missing,capture_output=True,text=True);assert t.returncode!=0 and '--go' in t.stderr
after={str(p) for pattern in ['runs/*','prx-candidate*'] for p in P.glob(pattern)};assert after==before
(O/'CONTROLS.log').write_text(r.stdout+r.stderr+'\nMISSING GO\n'+t.stdout+t.stderr)
manifest={'source_pins':actual,'control_and_input_pins':{str(p):sha(p) for p in [v5,fix,frozen,clone,P/'parent-review/AFFECTED-RECIPES.json',P/'parent-review/INPUT-SUBSTITUTION-MANIFEST.json',P/'EXECUTION-INTERFACE.md',P/'tests/test_execution_admission.py',P/'tests/test_recipe_execution.py',Path(__file__)]},'delegated_tool_pins':a['tool_pins'],'compiler_aliases':alias_records,'inherited14_verified':{str(P/path):r['sha256'] for path,r in cl['byte_identical_root_files'].items()}}
(O/'SOURCE-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
result={'pass':True,'findings':[],'scope':'Read-only frozen execution code and pending-admission provenance audit; no full build/GO granted','source_manifest_sha256':hashlib.sha256((O/'SOURCE-MANIFEST.json').read_bytes()).hexdigest(),'audited_pending_admission_sha256':sha(v5),'current_code_pins':actual,'behavior_tests':13,'selected_units':369,'ordered_composition_inputs':386,'exact_replacements':147,'pass1_pinned_reuse':144,'pass1_fresh':3,'pass2_fresh':147,'compile_concurrency':2,'reused_dependency_occurrences_verified':dep_total,'reused_unique_dependencies_verified':len(dep_unique),'unique_files_hashed':len(cache),'argument_transformations_verified':147,'missing_GO_refused_without_new_outputs':True,'commands':[{'argv':cmd,'exit':r.returncode},{'argv':missing,'exit':t.returncode}],'controls_log_sha256':hashlib.sha256((O/'CONTROLS.log').read_bytes()).hexdigest(),'original_package_gates_retained':True,'prior_finding_history':[{'status':'RESOLVED','finding':'v2 admitted compiler wrapper/config but omitted delegated SDK scripts and actual compiler; clang++ alias needed exact mapping','original_pending_admission_sha256':'e540e221aeb72be5b1f4789c45ade02e6cf4cc4623690ba9261e869527cfb27e','fix_receipt_sha256':sha(fix),'resolution':'v5 pins both delegation scripts, regular clang-21 and exact clang++ symlink spelling/target/hash; current code checks alias before and after composition'}],'final_READY_and_parent_GO_still_required':True,'compiler_linker_packager_invoked':False,'console_contact':False,'network_contact':False}
(O/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'pass':True,'controls':13,'reused':144,'inputs':386,'replacements':147,'unique_dependencies':len(dep_unique),'audit_sha256':hashlib.sha256((O/'AUDIT.json').read_bytes()).hexdigest()}))
