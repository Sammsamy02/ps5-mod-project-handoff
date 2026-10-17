#!/usr/bin/env python3
"""Copy-on-write l24 profile. Promotes FMap/FSet container member offsets and
class totals on Track-B generic-platform compiler agreement plus static image
evidence. Mac-only, no console contact; l23 originals stay frozen and read-only."""
from pathlib import Path
import json,hashlib,runpy,copy
root=Path(__file__).resolve().parent;ws=root.parents[2]
old=ws/'outputs/2026-10-08-l23-runtime/profile';prior=ws/'outputs/2026-10-07-l19-local-profile-r4'
generic=ws/'outputs/2026-10-08-l23-layout-generic'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
IMAGE='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
COMMIT='6978b63c8951e57d97048d8424a0bebd637dde1d'
NEW=('FMapProperty::KeyProp','FMapProperty::ValueProp','FSetProperty::ElementProp')
TOTALS={'FMapProperty::UEP_TotalSize':160,'FSetProperty::UEP_TotalSize':144}
# Existing profile totals the generic compile must reproduce before any promotion.
ANCHORS={'FArrayProperty::UEP_TotalSize':128,'FProperty::UEP_TotalSize':112,'FBoolProperty::UEP_TotalSize':120,'FEnumProperty::UEP_TotalSize':128,'FStructProperty::UEP_TotalSize':120}
def join(image,receipt,sizes):
 if image['image']['sha256']!=IMAGE:raise ValueError('wrong image identity')
 im={x['name']:x for x in image['facts']}
 if len(im)!=len(image['facts']):raise ValueError('duplicate image facts')
 if im.get('FArrayProperty::Inner',{}).get('offset')!=120:raise ValueError('image calibration missing or conflicting')
 if receipt is None:return []
 if receipt.get('status')!='GENERIC_COMPILER_MEASURED' or receipt.get('target')!='x86_64-sie-ps5' or receipt.get('official_commit')!=COMMIT:raise ValueError('generic compiler identity/status refused')
 if not receipt.get('source_pins') or not receipt.get('compiler_receipt'):raise ValueError('generic compiler provenance missing')
 rm={x['name']:x for x in receipt['facts']}
 if len(rm)!=len(receipt['facts']):raise ValueError('duplicate receipt facts')
 for name in (*NEW,'FArrayProperty::Inner'):
  if name not in rm or name not in im:raise ValueError('missing two-source fact '+name)
  if rm[name]['offset']!=im[name]['offset'] or rm[name]['width']!=im[name]['width']:raise ValueError('conflicting two-source fact '+name)
 alloc={b['class']:b['allocation_size_context_only'] for b in image['bindings']}
 for name,total in TOTALS.items():
  cls=name.split('::')[0]
  if sizes.get(cls)!=total or alloc.get(cls)!=total:raise ValueError('class total not agreed by generic compile + image allocation context: '+name)
 return [{k:im[n][k] for k in ('name','offset','width')} for n in NEW]+[{'name':n,'offset':t,'width':None} for n,t in TOTALS.items()]
def anchors_agree(p):
 have={x['name']:x['offset'] for x in p['required_members']}
 return all(have.get(n)==v for n,v in ANCHORS.items())
def main():
 imagepath=old/'evidence/image-measurements.json';receiptpath=generic/'CONTAINER-RECEIPT.json';sizespath=generic/'CLASS-SIZES.json'
 image=json.loads(imagepath.read_text());receipt=json.loads(receiptpath.read_text());sizes=json.loads(sizespath.read_text())
 promoted=join(image,receipt,sizes)
 p=json.loads((old/'profiles/townfall-build-facts.json').read_text());baseline=copy.deepcopy(p)
 assert p['image']['sha256']==IMAGE and len(p['required_members'])+len(p['required_vtables'])==93
 if not anchors_agree(p):raise ValueError('generic compile disagrees with retained profile anchor totals')
 ev={'promotion_basis':'l24 design-approved promotion on Track-B generic-platform compiler agreement + static image measurement','generic_receipt_path':str(receiptpath.relative_to(ws)),'generic_receipt_sha256':sha(receiptpath),'generic_receipt_status':receipt['status'],'class_sizes_path':str(sizespath.relative_to(ws)),'class_sizes_sha256':sha(sizespath),'image_evidence_path':str(imagepath.relative_to(ws)),'image_evidence_sha256':sha(imagepath),'image_sha256':IMAGE,'provenance_limitation':receipt['provenance_limitation'],'official_ps5_receipt':None,'new_offsets_status':'PROMOTED_ON_GENERIC_PLATFORM_COMPILER_AGREEMENT; OFFICIAL_PS5_COMPILER_MEASUREMENT_STILL_PENDING','array_inner_disposition':'startup-required','nested_map_set_disposition':'startup-required (promoted from conditional)','promoted_names':[x['name'] for x in promoted],'promoted_totals':dict(TOTALS),'ini_total_size_guard':'NativeLayoutBinding::Prepare rejects populated sections without positive UEP_TotalSize; both new INI sections carry it and every member offset stays below it','no_live_success_claimed':True}
 p['evidence']['l24_containers']=ev
 src=f"l24 generic-platform agreement: {ev['image_evidence_path']} + {ev['generic_receipt_path']} + {ev['class_sizes_path']}; hashes {ev['image_evidence_sha256']} / {ev['generic_receipt_sha256']} / {ev['class_sizes_sha256']}; {ev['provenance_limitation']}"
 for x in promoted:p['required_members'].append({k:x[k] for k in ('name','offset','width')}|{'source':src})
 assert p['required_members'][:69]==baseline['required_members'] and p['required_vtables']==baseline['required_vtables'] and len(p['required_members'])==74
 for rel in ('profiles/townfall-build-facts.json','units/RuntimeBuildProfile.hpp','units/CompiledNativeLayoutDeclarations.hpp','units/JoinedLayoutPreflight.hpp'):
  (root/'original'/rel).parent.mkdir(parents=True,exist_ok=True);(root/'original'/rel).write_bytes((old/rel).read_bytes())
 (root/'profiles').mkdir(exist_ok=True);(root/'units').mkdir(exist_ok=True)
 g=runpy.run_path(str(prior/'profiles/generate_profile.py'));g['generate'].__globals__['root']=root;digest=g['generate'](p)
 (root/'profiles/townfall-build-facts.json').write_text(json.dumps(p,indent=2)+'\n')
 strings=[json.loads(x.rstrip(',')) for x in (old/'units/CompiledNativeLayoutDeclarations.hpp').read_text().splitlines() if x.startswith('"')]
 off={x['name']:x['offset'] for x in promoted}
 for cls in ('FMapProperty','FSetProperty'):
  keys=sorted(n.split('::')[1] for n in (*NEW,*TOTALS) if n.startswith(cls+'::'))
  strings[0]+='\n['+cls+']\n'+''.join(k+'='+hex(off[cls+'::'+k])+'\n' for k in keys)
 strings[2]='compiled-audited-townfall-l24';strings[3]=digest
 (root/'units/CompiledNativeLayoutDeclarations.hpp').write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {\ninline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{\n'+',\n'.join(json.dumps(s) for s in strings)+'\n};\n}\n')
 h=(old/'units/JoinedLayoutPreflight.hpp').read_text().replace('// Prior 63 startup/menu/spawn dependencies plus three optional container discovery keys.','// Prior 63 startup/menu/spawn dependencies plus l24 container promotion: map/set keys are startup-required.')
 for name,offset in (('FMapProperty::KeyProp',112),('FMapProperty::ValueProp',120),('FSetProperty::ElementProp',112)):
  old_row=' {'+','.join([json.dumps(name),'Kind::Member','Disposition::Conditional','false','0',json.dumps('optional_nested_discovery_two_source_unresolved')])+'}'
  new_row=' {'+','.join([json.dumps(name),'Kind::Member','Disposition::Required','true',str(offset),json.dumps('startup_core_map_set_members')])+'}'
  assert old_row in h;h=h.replace(old_row,new_row)
 (root/'units/JoinedLayoutPreflight.hpp').write_text(h)
 rows=[l for l in h.splitlines() if l.startswith(' {"')]
 counts={d:sum(('Disposition::'+d) in l for l in rows) for d in ('Required','Conditional','Deferred')}
 assert len(rows)==66 and counts=={'Required':53,'Conditional':11,'Deferred':2},counts
 files=[]
 for rel in ('profiles/townfall-build-facts.json','units/RuntimeBuildProfile.hpp','units/CompiledNativeLayoutDeclarations.hpp','units/JoinedLayoutPreflight.hpp'):
  a=old/rel;b=root/rel;assert sha(a)==sha(root/'original'/rel)
  files.append({'original':str(a),'candidate':str(b),'original_sha256':sha(a),'candidate_sha256':sha(b)})
 changes={'scope':'l24 local profile overlay: FMap/FSet container member offsets + class totals promoted on Track-B generic-platform compiler agreement with static image evidence; Mac-only host work, no console contact; l23 originals retained unchanged','files':files,'added_headers':[],'added_header_hashes':{},'include_directories':[str(root/'units')],'profile_digest':digest,'image_sha256':IMAGE,'required_members':len(p['required_members']),'required_vtables':len(p['required_vtables']),'prior_facts_retained':93,'new_promoted_facts':len(promoted),'joined_counts':{'total':66,'startup_required':53,'conditional_maps':11,'runtime_deferred':2},'conditional_legacy_absence_exceptions':['FProperty::GetMinAlignment'],'initializer_modified':False,'official_status':ev['new_offsets_status'],'provenance_limitation':receipt['provenance_limitation'],'regenerate':'python3 '+str(Path(__file__).resolve()),'test_commands':['python3 '+str(root/'tests/run_tests.py')]}
 (root/'CHANGES.json').write_text(json.dumps(changes,indent=2)+'\n')
 print(json.dumps({'digest':digest,'promoted':len(promoted),'joined':{'total':66,'required':53,'conditional':11,'deferred':2}}))
if __name__=='__main__':main()
