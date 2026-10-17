#!/usr/bin/env python3
"""Copy-on-write l23 profile. New image facts require separately accepted source compiler receipt."""
from pathlib import Path
import json,hashlib,runpy,argparse,copy
root=Path(__file__).resolve().parent;ws=root.parents[2]
old=ws/'outputs/2026-10-08-l22-runtime/profile';prior=ws/'outputs/2026-10-07-l19-local-profile-r4'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
IMAGE='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
NEW=('FMapProperty::KeyProp','FMapProperty::ValueProp','FSetProperty::ElementProp')
def join(image,official):
 if image['image']['sha256']!=IMAGE:raise ValueError('wrong image identity')
 im={x['name']:x for x in image['facts']}
 if len(im)!=len(image['facts']):raise ValueError('duplicate image facts')
 if im.get('FArrayProperty::Inner',{}).get('offset')!=120:raise ValueError('image calibration missing or conflicting')
 if official is None:return []
 if official.get('status')!='TARGET_COMPILER_MEASURED' or official.get('target')!='x86_64-sie-ps5' or official.get('official_commit')!='6978b63c8951e57d97048d8424a0bebd637dde1d':raise ValueError('official compiler identity/status refused')
 if not official.get('source_pins') or not official.get('compiler_receipt'):raise ValueError('official compiler provenance missing')
 om={x['name']:x for x in official['facts']}
 if len(om)!=len(official['facts']):raise ValueError('duplicate official facts')
 for name in (*NEW,'FArrayProperty::Inner'):
  if name not in om or name not in im:raise ValueError('missing two-source fact '+name)
  if om[name]['offset']!=im[name]['offset'] or om[name]['width']!=im[name]['width']:raise ValueError('conflicting two-source fact '+name)
 return [im[name] for name in NEW]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--official-receipt',type=Path);args=ap.parse_args()
 imagepath=root/'evidence/image-measurements.json';image=json.loads(imagepath.read_text())
 official=json.loads(args.official_receipt.read_text()) if args.official_receipt else None
 promoted=join(image,official)
 p=json.loads((old/'profiles/townfall-build-facts.json').read_text());baseline=copy.deepcopy(p)
 assert p['image']['sha256']==IMAGE and len(p['required_members'])+len(p['required_vtables'])==93
 ev={'image_evidence_path':str(imagepath.relative_to(ws)),'image_evidence_sha256':sha(imagepath),'image_sha256':IMAGE,'new_offsets_status':'TWO_SOURCE_AGREEMENT' if promoted else 'UNPROMOTED_OFFICIAL_COMPILER_MEASUREMENT_REQUIRED','array_inner_disposition':'startup-required','nested_map_set_disposition':'conditional_optional_nested_discovery','official_receipt':str(args.official_receipt.resolve()) if official else None,'official_receipt_sha256':sha(args.official_receipt) if official else None,'promoted_names':[x['name'] for x in promoted],'no_live_success_claimed':True}
 p['evidence']['l23_containers']=ev
 for x in promoted:p['required_members'].append({k:x[k] for k in ('name','offset','width')}|{'source':f"l23 two-source agreement: {ev['image_evidence_path']} and {ev['official_receipt']}; hashes {ev['image_evidence_sha256']} / {ev['official_receipt_sha256']}"})
 assert p['required_members'][:69]==baseline['required_members'] and p['required_vtables']==baseline['required_vtables']
 g=runpy.run_path(str(prior/'profiles/generate_profile.py'));g['generate'].__globals__['root']=root;digest=g['generate'](p)
 (root/'profiles/townfall-build-facts.json').write_text(json.dumps(p,indent=2)+'\n')
 strings=[json.loads(x.rstrip(',')) for x in (old/'units/CompiledNativeLayoutDeclarations.hpp').read_text().splitlines() if x.startswith('"')]
 for cls in ('FMapProperty','FSetProperty'):
  group=[x for x in promoted if x['name'].startswith(cls+'::')]
  if group:strings[0]+='\n['+cls+']\n'+''.join(x['name'].split('::')[1]+'='+hex(x['offset'])+'\n' for x in group)
 strings[2]='compiled-audited-townfall-l23';strings[3]=digest
 (root/'units/CompiledNativeLayoutDeclarations.hpp').write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {\ninline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{\n'+',\n'.join(json.dumps(s) for s in strings)+'\n};\n}\n')
 h=(old/'units/JoinedLayoutPreflight.hpp').read_text().replace('// Exact63 selected startup/menu/spawn dependencies from the pinned l21 join.','// Prior 63 startup/menu/spawn dependencies plus three optional container discovery keys.')
 h=h.replace('{"FArrayProperty::Inner",Kind::Member,Disposition::Conditional,true,120,"spawn_explicit_array_conversion"}','{"FArrayProperty::Inner",Kind::Member,Disposition::Required,true,120,"startup_core_array_inner"}')
 pins={x['name']:x for x in promoted}
 rows=[]
 for name in NEW:
  x=pins.get(name);rows.append(' {'+','.join([json.dumps(name),'Kind::Member','Disposition::Conditional','true' if x else 'false',str(x['offset'] if x else 0),json.dumps('optional_nested_discovery_two_source_proven' if x else 'optional_nested_discovery_two_source_unresolved')])+'}')
 h=h.replace('\n};\nstatic_assert',',\n'+',\n'.join(rows)+'\n};\nstatic_assert',1).replace('==63);','==66);')
 # A pinned conditional can be absent; a conflicting present declaration must refuse.
 h=h.replace('const bool accepted=!required||matches;','const bool accepted=required?matches:(!entry||matches);')
 h=h.replace('if(!accepted){result.ok=false;++result.missing_required;}','if(!accepted){result.ok=false;if(required)++result.missing_required;}')
 (root/'units/JoinedLayoutPreflight.hpp').write_text(h)
 files=[]
 for rel in ('profiles/townfall-build-facts.json','units/RuntimeBuildProfile.hpp','units/CompiledNativeLayoutDeclarations.hpp','units/JoinedLayoutPreflight.hpp'):
  a=old/rel;b=root/rel;assert sha(a)==sha(root/'original'/rel)
  files.append({'original':str(a),'candidate':str(b),'original_sha256':sha(a),'candidate_sha256':sha(b)})
 changes={'scope':'l23 local profile overlay; no console/native invocation; old files retained','files':files,'added_headers':[],'added_header_hashes':{},'profile_digest':digest,'image_sha256':IMAGE,'required_members':len(p['required_members']),'required_vtables':len(p['required_vtables']),'prior_facts_retained':93,'new_promoted_facts':len(promoted),'joined_counts':{'total':66,'startup_required':50,'conditional_maps':14,'runtime_deferred':2},'conditional_legacy_absence_exceptions':['FProperty::GetMinAlignment',*NEW],'initializer_modified':False,'official_status':ev['new_offsets_status'],'regenerate':'python3 '+str(Path(__file__).resolve())+(' --official-receipt '+str(args.official_receipt) if official else ''),'test_commands':['python3 '+str(root/'tests/run_tests.py')]}
 (root/'CHANGES.json').write_text(json.dumps(changes,indent=2)+'\n')
 print(json.dumps({'digest':digest,'promoted':len(promoted),'joined':66}))
if __name__=='__main__':main()
