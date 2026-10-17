"""Preserved R3 profile with its real partial stageable declarations still refuses."""
from pathlib import Path
import hashlib,importlib.util,json,subprocess,sys
O=Path(__file__).resolve().parents[1];W=O.parents[1];R3=W/'outputs/2026-10-07-l19-local-profile-r3'
spec=importlib.util.spec_from_file_location('producer',O/'generate_declarations.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((R3/'profiles/townfall-build-facts.json').read_text());c=json.loads((O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text())
schema=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
result=m.build(old,schema.read_text(),c['observed_widths'])
assert len(result['manifest']['unresolved'])==4 and len(result['manifest']['stageable_subset_excluded_known_members'])==7
H=O/'tests/r3-original-pair';H.mkdir(exist_ok=True)
digest=json.loads((R3/'reports/FINAL-RECEIPT.json').read_text())['profile_digest']
header=H/'provider.hpp';header.write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {inline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{'+','.join(json.dumps(x) for x in [result['stageable_members'],result['vtables'],'compiled-audited-townfall-l19-r4',digest])+'};}\n')
for n in ['RuntimeBuildProfile.hpp','NativeLayoutDeclarations.hpp']:(H/n).write_bytes(((R3 if n=='RuntimeBuildProfile.hpp' else O)/'units'/n).read_bytes())
base=json.loads((O/'reports/R4-INTEGRATION.json').read_text())
cmd=list(next(x['argv'] for x in base['commands'] if x['kind']=='compile' and x['variant']=='r3-baseline'))
cmd=[('-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(header)+'"') if x.startswith('-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER=') else x for x in cmd]
cmd[1:1]=['-I',str(H)];cmd[cmd.index('-o')+1]=str(H/'baseline-san')
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
run=[str(H/'baseline-san'),str(W/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'),'r3_baseline',str(H/'external')]
t=subprocess.run(run,capture_output=True,text=True);assert t.returncode==0,t.stderr
(O/'reports/R3-DECLARATION-BASELINE.json').write_text(json.dumps({'pass':True,'profile_sha256':sha(R3/'profiles/townfall-build-facts.json'),'declaration_members':60,'declaration_vtables':5,'unknown_pins':result['manifest']['unresolved'],'compile':{'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr},'run':{'argv':run,'exit':t.returncode,'stdout':t.stdout,'stderr':t.stderr},'source_pins':{str(f):sha(f) for f in [Path(__file__),O/'generate_declarations.py',R3/'profiles/townfall-build-facts.json',H/'RuntimeBuildProfile.hpp',H/'NativeLayoutDeclarations.hpp',header,O/'tests/r4-host/integration.cpp']},'scope':'R3 partial pair produced by the unchanged actual generic producer; actual parser/binder and preflight refuse before publication/consumption'},indent=2)+'\n')
print(t.stdout.strip())
