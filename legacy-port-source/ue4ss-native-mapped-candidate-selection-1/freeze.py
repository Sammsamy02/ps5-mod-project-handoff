from pathlib import Path
import hashlib,json,shlex
r=Path(__file__).resolve().parent;out=r/'build-1';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs=set()
def pin(p):
 p=Path(p).resolve();assert p.is_file(),p;inputs.add(p)
for name in ['DESIGN.md','REPORT.md','build.py','inventory.py','verify_host.py','freeze.py','affected-unit-inventory.json']:pin(r/name)
for d in ['include','src','tests']:
 for p in (r/d).rglob('*'):
  if p.is_file():pin(p)
for p in out.rglob('*'):
 if p.is_file():pin(p)
for name in ['Cargo.toml','Cargo.lock','build.rs','src/lib.rs','libselection_controls.a','collector.o','selection.o','source.o','memory.o','controls.o']:pin(r/'host-harness-1'/name)
n=json.loads((out/'native-consumers.json').read_text());inv=json.loads((r/'affected-unit-inventory.json').read_text());host=json.loads((out/'host-controls-final.json').read_text());assert host['all_passed'] and inv['affected_count']==3 and n['all_compiled'];assert n['single_rust_object']['exports'] and len(n['single_rust_object']['exports'])==17
for u in n['units']:
 assert sha(u['source'])==u['source_sha256'] and sha(u['object'])==u['object_sha256'];pin(u['source']);pin(u['object']);pin(u['dependency_file'])
 text=Path(u['dependency_file']).read_text().replace('\\\n',' ');deps=shlex.split(text.partition(':')[2])
 for p in deps:pin(p)
for p in [inv['baseline'],n['single_rust_object']['object'],r.parent/'ue4ss-native-mapped-candidates-1/integration-manifest.json',r.parent/'ue4ss-native-mapped-collector-properties-1/native-1/OwnedMappedResolverCollector.o'] :pin(p)
# Frozen ABI/report and original production collector body are byte-identical.
comparisons=[]
for rel in ['include/OwnedMappedResolverCollector.hpp','src/OwnedMappedResolverCollector.cpp']:
 a=r/rel;b=r.parent/'ue4ss-native-mapped-collector-properties-1'/rel;assert sha(a)==sha(b);pin(b);comparisons.append({'successor':str(a),'original':str(b),'sha256':sha(a),'unchanged':True})
assert sha(inv['baseline'])==inv['baseline_sha256'];assert sha(n['single_rust_object']['object'])==n['single_rust_object']['sha256']
pins=[{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)}for p in sorted(inputs)]
manifest={'scope':'local owned mapped-address candidate selection only; no identity/ABI or native execution claim','default_policy':'OriginalOnly','explicit_policy':'SelectUniqueAfterNoMatch','original_preserved':True,'original_config_report_wire_unchanged':comparisons,'eligibility':'original slot NoMatch/status3 and image_error false; never ambiguity/invalid/incomplete override','source_policy_immutable':True,'provenance_callbacks_after_lease':True,'host':{'result':str(out/'host-controls-final.json'),'assertions':87,'genuine_runtime':True,'fixtures':8,'no_fixture_code_execution':True},'native':{'units':n['units'],'old_consumers_rebuilt':3,'new_units':1,'single_rust_object':n['single_rust_object'],'focused_partial':str(out/'native/candidate-source-single-runtime.partial.o'),'focused_partial_sha256':sha(out/'native/candidate-source-single-runtime.partial.o'),'future_total_cpp_c_units':350,'full_composition_performed':False,'execution':False},'inventory':{'path':str(r/'affected-unit-inventory.json'),'baseline':inv['baseline'],'baseline_sha256':inv['baseline_sha256'],'all_units':349,'all_actual_sdk_dependency_probes_passed':True,'affected_units':inv['affected_units']},'include_first':[str(r/'include'),str(r.parent/'ue4ss-native-mapped-candidates-1/include')],'no_game_or_console_access':True,'no_default_enable':True,'pins':pins}
p=r/'integration-manifest.json';p.write_text(json.dumps(manifest,indent=2)+'\n');print('manifest',sha(p),'pins',len(pins));print('partial',manifest['native']['focused_partial_sha256'])
