"""Narrow actual-dependency regression audit; no build, restart or shared writes."""
from pathlib import Path
import copy,hashlib,json,sys
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
prior=O.parent/'FINAL-AUDIT.json';assert sha(prior)=='c9661b97a67c7e1828e0b241e88b1bd6f7d363039db67329903f770106654a06'
fix=P/'parent-review/BINDER-DEPENDENCY-GUARD-FIX.json';assert sha(fix)=='15f28b7c9b2ac4c3ab885e89460c77f8aa5608ac5d4fe2d0288b976f21c6596b'
f=read(fix);assert f['pass'] and not f['gate_waiver'] and not f['package_invoked'] and not f['restart_executed']
old=P/'parent-review/execution-hook-v2.sources/admission.py';new=P/'execution/admission.py'
assert sha(old)==read(prior)['current_code_pins'][str(new)]
before="""        if name in PENDING:
            require(str(self.pins['profile_header']) in actual, 'selected profile header absent from .d: ' + name)
"""
after="""        if name in {'program', 'unreal-unrealinitializer'}:
            require(str(self.pins['profile_header']) in actual, 'selected profile header absent from .d: ' + name)
        if name == 'native-layoutbinding':
            for role in ('NativeLayoutBinding.hpp', 'NativeLayoutActiveWidths.hpp'):
                require(str(resolved(self.records[role]['path'])) in actual,
                        'selected binder helper absent from .d: ' + role)
"""
assert old.read_text().count(before)==1 and old.read_text().replace(before,after)==new.read_text()
assert sha(new)=='69d8a0fddf5fc111fc55948d286f5176db7e9e394fd14eaab3021f0ff601d6e8'
for p,h in f['current_execution_source_pins'].items():assert sha(p)==h,p
for p,h in read(prior)['current_code_pins'].items():
 if p!=str(new):assert sha(p)==h,p
old_admission=P/'parent-review/EXECUTION-ADMISSION.FINAL.json';new_admission=P/'parent-review/EXECUTION-ADMISSION.FINAL.v2.json'
assert sha(new_admission)=='e9a037ba555ffcac19caba4c36aa34445ab4c8c23e2a4f6e347e5a9f908de880'
oa=read(old_admission);a=read(new_admission)
assert {k:v for k,v in oa.items() if k not in ['execution_source_pins','ready_prep','edition']}=={k:v for k,v in a.items() if k not in ['execution_source_pins','ready_prep','edition']}
assert a['execution_source_pins']==f['current_execution_source_pins']
for key in ['ready_prep','edition']:
 assert Path(oa[key]['path']).read_bytes()==Path(a[key]['path']).read_bytes()
 assert sha(a[key]['path'])==a[key]['sha256']
assert read(a['ready_prep']['path'])['readiness']=='READY'
for key in ['source_manifest','profile_json','profile_header','generated_header','xom_audit','final_review','integration_manifest']:
 assert sha(a[key]['path'])==a[key]['sha256'],key
sys.path.insert(0,str(P/'execution'))
from admission import Admission,Refusal,dependencies,resolved
recipes=read(a['affected_recipes']['path']);base=read(recipes['base_receipt']['path'])
ad=object.__new__(Admission);ad.data=a
ad.allowed={str(resolved(p)):h for p,h in base['source_hashes'].items()};ad.allowed.update(a['additional_source_pins'])
ad.records=read(a['source_manifest']['path'])['selected_sources'];ad.allowed.update({r['path']:r['sha256'] for r in ad.records.values()})
owner=recipes['recipes']['program']['required_accepted_header'];ad.owner_header=Path(owner['path']);ad.allowed[str(ad.owner_header)]=owner['sha256']
ad.pins={k:Path(a[k]['path']) for k in ['profile_header','generated_header','linker']};ad.tool_pins=a['tool_pins'];ad.validate_tool_aliases()
fixtures={};commands={};pins={}
for path,h in f['actual_compiler_dependency_fixtures'].items():
 p=Path(path);assert sha(p)==h;name=p.name[:-2];fixtures[name]=p
 command=P/'failed-attempt-2/run-l19-1'/('compile-'+name+'.command.json');c=read(command);assert c['exit']==0
 recorded=Path(c['argv'][c['argv'].index('-MF')+1]);actual=command.parent/recorded.name;assert actual.read_bytes()==p.read_bytes()
 commands[name]={'path':str(command),'sha256':sha(command),'exit':c['exit'],'actual_dependency_file':str(actual),'actual_dependency_sha256':sha(actual)}
 pins[str(p)]=h
results=[]
def case(name,dep,expect=None):
 try:r=ad.check_dependencies(name,dep);error=None
 except Refusal as e:r=None;error=str(e)
 assert (error is not None)==(expect is not None),(name,error,expect)
 if expect:assert expect in error,(expect,error)
 results.append({'unit':name,'fixture':str(dep),'expected_refusal':expect,'pass':True,'detail':error,'actual_dependencies':len(r) if r else None})
 return r
for name,p in fixtures.items():
 actual=case(name,p)
 if name=='native-layoutbinding':assert str(ad.pins['profile_header']) not in actual
 else:assert str(ad.pins['profile_header']) in actual
negative=[('native-layoutbinding',ad.records['NativeLayoutBinding.hpp']['path'],'binder helper'),('native-layoutbinding',ad.records['NativeLayoutActiveWidths.hpp']['path'],'binder helper'),('program',str(ad.pins['profile_header']),'profile header'),('unreal-unrealinitializer',str(ad.pins['profile_header']),'profile header'),('program',str(ad.pins['generated_header']),'compiled declaration provider/header'),('native-layoutbinding',str(ad.owner_header),'accepted FField header')]
for i,(name,path,error) in enumerate(negative):
 text=fixtures[name].read_text();assert text.count(path)==1,(name,path)
 dep=O/('missing-'+str(i)+'.d');dep.write_text(text.replace(path,''));case(name,dep,error)
missing=ad.records['NativeLayoutActiveWidths.hpp']['path'];h=ad.allowed.pop(missing)
case('native-layoutbinding',fixtures['native-layoutbinding'],'unqueried/unpinned actual dependency');ad.allowed[missing]=h
assert len(results)==10
try:Admission.load(new_admission,sha(new_admission),None,None)
except Refusal as e:assert 'GO' in str(e)
else:raise AssertionError('missing GO accepted')
pinfiles=[prior,fix,old,new,old_admission,new_admission,Path(__file__),P/'parent-review/BINDER-DEPENDENCY-GUARD-FIX.patch',P/'tests/test_execution_admission.py']
pins.update({str(p):sha(p) for p in pinfiles})
receipt={'pass':True,'findings':[],'scope':'Narrow dependency-guard correction acceptance only; prior audit and failed attempt preserved','prior_audit':{'path':str(prior),'sha256':sha(prior)},'new_admission':{'path':str(new_admission),'sha256':sha(new_admission)},'changed_code':{'path':str(new),'sha256':sha(new)},'exact_minimal_delta_verified':True,'other_four_execution_files_unchanged':True,'actual_compile_receipts':commands,'focused_actual_dependency_cases':results,'focused_case_count':10,'full_behavior_test_methods_retained_receipt':f['actual_behavior_test_methods'],'profile_sources_headers_and_inherited_gates_changed':False,'byte_identical_READY_metadata_new_paths':True,'owner_unpinned_profile_provider_guards_preserved':True,'original_147_recipes_386_order_144_reuse_policy_unchanged':True,'missing_GO_refused':True,'GO_created':False,'compiler_linker_packager_invoked':False,'restart_invoked':False,'console_contact':False,'network_contact':False,'source_pins':pins}
(O/'AUDIT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'pass':True,'focused_cases':10,'new_admission':sha(new_admission),'audit_sha256':sha(O/'AUDIT.json')}))
