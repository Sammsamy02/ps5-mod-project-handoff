"""Narrow GNU-linker dispatch audit; reads an existing experimental link only."""
from pathlib import Path
import copy,hashlib,json,struct,subprocess,sys
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
prior=O.parent/'dependency-guard-addendum/AUDIT.json';assert sha(prior)=='7124925db37686351c851fb89c2864bba0b97b51179897bf5aa1afd7c05f3674'
fix=P/'parent-review/GNU-LINKER-INVOCATION-FIX.json';assert sha(fix)=='9efc5faa30de06612f863e936e521e93d8fe8dea24c0d2238afce39228c8a552'
f=read(fix);assert f['pass'] and f['behavior_test_methods']==16 and not f['native_sources_profile_or_inherited_gates_changed'] and not f['full_pipeline_restarted']
old_ad=P/'parent-review/execution-hook-v3.sources/admission.py';new_ad=P/'execution/admission.py'
old_co=P/'parent-review/execution-hook-v3.sources/composition.py';new_co=P/'execution/composition.py'
assert sha(old_ad)=='69d8a0fddf5fc111fc55948d286f5176db7e9e394fd14eaab3021f0ff601d6e8'
assert sha(old_co)=='4159d7c22f186fdad6b78a8d25733f6ec0e51af58b4b5471ab0bec3077ffd75e'
needle="        require(expected in aliases, 'exact delegated clang++ alias mapping required')\n"
addition="""        linker_alias = str(self.pins['linker'].parent / 'ld.lld')
        require(self.data.get('linker_invocation') == linker_alias and linker_alias in aliases,
                'exact GNU ld.lld invocation/alias mapping required')
        require(aliases[linker_alias].get('resolved_path') == str(self.pins['linker']),
                'GNU linker invocation must resolve to pinned linker executable')
"""
assert old_ad.read_text().count(needle)==1 and old_ad.read_text().replace(needle,needle+addition)==new_ad.read_text()
line="        link = [str(a.pins['linker']), '-m', 'elf_x86_64', '-r', *ordered,\n"
assert old_co.read_text().count(line)==1 and old_co.read_text().replace(line,"        link = [a.data['linker_invocation'], '-m', 'elf_x86_64', '-r', *ordered,\n")==new_co.read_text()
for path,h in f['current_execution_source_pins'].items():assert sha(path)==h,path
oldfile=P/'parent-review/EXECUTION-ADMISSION.FINAL.v2.json';newfile=P/'parent-review/EXECUTION-ADMISSION.FINAL.v3.json'
assert sha(newfile)=='7aca76ab4fb738876bd657a33d14ff6be454bd6f063b0c750c7d163d867a44bc'
old=read(oldfile);a=read(newfile)
changed={'ready_prep','edition','execution_source_pins','tool_aliases','tool_pins','linker_invocation'}
assert {k:v for k,v in old.items() if k not in changed}=={k:v for k,v in a.items() if k not in changed}
for key in ['ready_prep','edition']:
 assert Path(old[key]['path']).read_bytes()==Path(a[key]['path']).read_bytes() and sha(a[key]['path'])==a[key]['sha256']
assert all(a['tool_pins'][p]==h for p,h in old['tool_pins'].items())
assert set(a['tool_pins'])-set(old['tool_pins'])=={a['linker']['path']}
assert all(a['tool_aliases'][p]==r for p,r in old['tool_aliases'].items())
alias=Path(a['linker_invocation']);assert alias.name=='ld.lld' and alias.is_symlink() and alias.resolve()==Path(a['linker']['path'])
sys.path.insert(0,str(P/'execution'))
from admission import Admission,Refusal
ad=object.__new__(Admission);ad.data=a;ad.pins={'linker':Path(a['linker']['path'])};ad.tool_pins=a['tool_pins']
ad.validate_tool_aliases();cases=[{'case':'actual_GNU_alias_and_pinned_targets','pass':True}]
for label,change in [('generic_lld_spelling',lambda d:d.update(linker_invocation=a['linker']['path'])),('missing_GNU_alias',lambda d:d['tool_aliases'].pop(str(alias))),('wrong_resolved_target',lambda d:d['tool_aliases'][str(alias)].update(resolved_path=a['nm']['path']))]:
 ad.data=copy.deepcopy(a);change(ad.data)
 try:ad.validate_tool_aliases()
 except Refusal:cases.append({'case':label,'pass':True,'refused':True})
 else:raise AssertionError(label)
ad.data=a
experiment=Path(f['one_actual_experimental_link']['path']);assert sha(experiment)==f['one_actual_experimental_link']['sha256']
e=read(experiment);assert e['pass'] and e['actual_inputs']==386 and e['affected_object_slots']==147
target=Path(e['runtime']['path']);assert target.stat().st_size==e['runtime']['bytes']==118473352 and sha(target)==e['runtime']['sha256']
header=target.read_bytes()[:20];assert header[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<HH',header,16)==(1,62)
cmdfile=Path(e['command']['path']);assert sha(cmdfile)==e['command']['sha256'];command=read(cmdfile);argv=command['argv'];assert command['exit']==0
assert argv[:4]==[str(alias),'-m','elf_x86_64','-r'] and argv[-2:]==['-o',str(target)]
inputsfile=Path(f['actual_input_hashes_copy']['path']);assert sha(inputsfile)==f['actual_input_hashes_copy']['sha256']==e['input_hashes']['sha256']
inputs=read(inputsfile);assert len(inputs)==386 and argv[4:-3]==list(inputs)
slots=read(a['input_substitutions']['path'])['input_slots'];assert len(slots)==386 and sum(bool(s['replace_from_affected_recipe']) for s in slots)==147
for slot,path in zip(slots,inputs):
 if not slot['replace_from_affected_recipe']:assert path==slot['old_input']['path']
 else:assert Path(path).name==slot['replace_from_affected_recipe']+'.o'
commands=[]
def run_readonly(label,argv):
 r=subprocess.run(argv,capture_output=True,text=True);commands.append({'case':label,'argv':argv,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
nm=Path(a['nm']['path']);assert not nm.is_symlink() and nm.name=='llvm-nm' and sha(nm)==a['nm']['sha256']
r=run_readonly('typed_undefined_existing_experiment',[str(nm),'--extern-only','--undefined-only','--format=posix',str(target)]);assert r.returncode==0
current={p[0]:p[1] for p in (line.split() for line in r.stdout.splitlines()) if len(p)>=2}
recipes=read(a['affected_recipes']['path']);baseline=read(recipes['base_receipt']['path'])['undefined_delta']['current'];assert current==baseline and len(current)==210
config=Path(next(p for p in a['tool_pins'] if Path(p).name=='llvm-config'));assert not config.is_symlink() and sha(config)==a['tool_pins'][str(config)]
r=run_readonly('llvm_config_bindir',[str(config),'--bindir']);assert r.returncode==0 and Path(r.stdout.strip()).resolve()==alias.parent
r=run_readonly('GNU_alias_version',[str(alias),'--version']);assert r.returncode==0 and 'LLD' in r.stdout
r=run_readonly('generic_lld_version_refuses',[str(ad.pins['linker']),'--version']);assert r.returncode!=0 and 'generic driver' in r.stderr
# Compiler recipes retain raw wrapper invocation; canonical pins are not used as argv.
wrapper=Path(next(p for p in a['tool_pins'] if Path(p).name=='prospero-clang++'));assert not wrapper.is_symlink() and sha(wrapper)==a['tool_pins'][str(wrapper)]
assert all(row['original_argv'][0]==str(wrapper) for row in recipes['recipes'].values())
delegate=wrapper.parent/'clang++';config_delegate=wrapper.parent/'prospero-llvm-config'
assert not delegate.is_symlink() and not config_delegate.is_symlink()
assert sha(delegate)==a['tool_pins'][str(delegate)] and sha(config_delegate)==a['tool_pins'][str(config_delegate)]
assert '"${SCRIPT_DIR}/clang++"' in wrapper.read_text() and '"${LLVM_BINDIR}/${SCRIPT_NAME}"' in delegate.read_text()
compiler_alias=alias.parent/'clang++';assert compiler_alias.is_symlink() and str(compiler_alias) in a['tool_aliases']
ad.validate_tool_aliases()
try:Admission.load(newfile,sha(newfile),None,None)
except Refusal as ex:assert 'GO' in str(ex)
else:raise AssertionError('missing GO accepted')
pins={str(p):sha(p) for p in [Path(__file__),prior,fix,old_ad,new_ad,old_co,new_co,oldfile,newfile,experiment,target,cmdfile,inputsfile,P/'parent-review/GNU-LINKER-INVOCATION-FIX.patch',Path(f['focused_controls_log']['path'])]}
receipt={'pass':True,'findings':[],'scope':'NEW narrow GNU linker dispatch/alias acceptance; experimental link read only; prior audits unchanged','new_admission':{'path':str(newfile),'sha256':sha(newfile)},'current_execution_source_pins':f['current_execution_source_pins'],'exact_minimal_delta_verified':True,'independent_alias_cases':cases,'retained_behavior_methods':16,'actual_experimental_link':{'receipt':{'path':str(experiment),'sha256':sha(experiment)},'output':e['runtime'],'ordered_inputs_verified':386,'affected_slots_verified':147,'exact_raw_typed_undefined_count':210,'typed_undefined_delta_vs_R3':{'added':[],'removed':[],'changed_bindings':{}}},'dispatch_boundary_checks':{'GNU_ld_lld_name_preserved_and_alias_pre_post_validated':True,'llvm_nm_regular_name_preserved_and_actual_object_read_passed':True,'llvm_config_regular_name_preserved_and_bindir_passed':True,'compiler_raw_SDK_wrapper_and_CXX_alias_preserved':True},'read_only_tool_commands':commands,'recipe_source_profile_inherited_gate_and_reuse_policy_changed':False,'GO_created':False,'compile_or_link_or_package_invoked_by_auditor':False,'full_restart_invoked':False,'console_contact':False,'source_pins':pins}
(O/'AUDIT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'pass':True,'admission':sha(newfile),'ordered_inputs':386,'typed_UND':210,'audit_sha256':sha(O/'AUDIT.json')}))
