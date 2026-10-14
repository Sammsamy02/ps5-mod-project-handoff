"""Authenticate the complete R4 declaration inputs and call the unchanged producer."""
from pathlib import Path
import hashlib, json, subprocess, sys

O=Path(__file__).resolve().parent; W=O.parents[1]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
write=lambda p,x:Path(p).write_text(json.dumps(x,indent=2)+'\n')
profile=O/'profiles/townfall-build-facts.json'
census=O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json'
schema=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
p=json.loads(profile.read_text()); c=json.loads(census.read_text())
assert c['exit']==0 and not c['missing_provider'] and not c['narrower_than_measured']
assert c['accepted_owner_header_dependency_verified'] is True
assert c['observed_widths']['FField::Owner']==16
assert all(x['offset'] is not None for x in p['required_members']+p['required_vtables'])
promotion=json.loads((O/'reports/AUDITED-PROFILE-GREEN.json').read_text())
assert promotion['pass'] and promotion['profile_sha256']==sha(profile)
sources={str(f.relative_to(O)):sha(f) for d in ['units','profiles'] for f in (O/d).rglob('*') if f.is_file() and f.suffix in ('.hpp','.cpp','.inc','.json','.py')}
sources.update({n:sha(O/n) for n in ['generate_declarations.py','prepare_declarations.py','tests/required_width_census.py','tests/required-width-target/required_census.cpp']})
evidence={str(schema.relative_to(W)):sha(schema)}
for path,pin in c['source_hashes'].items():
 f=Path(path); assert sha(f)==pin
 if not f.is_relative_to(O):evidence[str(f.relative_to(W))]=pin
for row in promotion['promoted_facts']:
 for pathkey,shakey in [('audit_path','audit_sha256'),('fact_receipt_path','fact_receipt_sha256')]:
  f=Path(row[pathkey]); assert sha(f)==row[shakey]; evidence[str(f.relative_to(W))]=row[shakey]
selection=O/'reports/DECLARATION-INPUT-SELECTION.json'
write(selection,{'scope':'Authenticated input selection for the generic declaration producer; not the final integration receipt','source_files':dict(sorted(sources.items())),'evidence_files':dict(sorted(evidence.items()))})
receipt=O/'reports/DECLARATION-INPUT-RECEIPT.json'
write(receipt,{'pass':True,'scope':'All declaration input source/evidence pins and actual target width census verified; integration execution is separately tested','source_selection_sha256':sha(selection),'profile_sha256':sha(profile),'profile_digest':promotion['profile_digest'],'receipt_hashes':{census.name:sha(census)}})
cmd=[sys.executable,str(O/'generate_declarations.py'),'--root',str(W),'--profile',str(profile),'--selection',str(selection),'--receipt',str(receipt),'--census',str(census),'--schema',str(schema),'--out',str(O/'generated'),'--require-complete']
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
m=O/'generated/known-declarations/MemberVariableLayout.ini';v=O/'generated/known-declarations/VTableLayout.ini'
identifier='compiled-audited-townfall-l19-r4'
header=O/'units/CompiledNativeLayoutDeclarations.hpp'
header.write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {\ninline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{\n'+',\n'.join(json.dumps(s) for s in [m.read_text(),v.read_text(),identifier,promotion['profile_digest']])+'\n};\n}\n')
manifest=json.loads((O/'generated/UNRESOLVED.json').read_text())
assert manifest['ready'] and manifest['known_members']==69 and manifest['known_vtables']==7 and not manifest['unresolved']
write(O/'reports/COMPILED-DECLARATIONS.json',{'pass':True,'command':cmd,'stdout':r.stdout,'source_pins':{str(f.relative_to(O)):sha(f) for f in [profile,selection,receipt,census,m,v,header,O/'units/RuntimeBuildProfile.hpp',O/'generate_declarations.py']},'profile_digest':promotion['profile_digest'],'selection_source':identifier,'members':69,'vtables':7,'owner_public_footprint':16,'owner_native_read_bytes':8,'published':False,'selected_only_with_macro':'RC_NATIVE_LAYOUT_DECLARATIONS_HEADER'})
print(r.stdout.strip())
