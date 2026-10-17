#!/usr/bin/env python3
"""Prepare l23 local packaging only after runtime twins exist; no console actions."""
from pathlib import Path
import copy,hashlib,json,xml.etree.ElementTree as ET
HERE=Path(__file__).resolve().parent
RUNTIME=HERE.parent
ROOT=RUNTIME.parents[1]
BASE=ROOT/'outputs/2026-10-08-l22-package-r2'
BUILD=ROOT/'outputs/2026-10-08-l23-build-r2'
OUT=ROOT/'outputs/2026-10-08-l23-package'
TOOLS=['prep_guard.py','package_prx_l19.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py']
HEADERS={'RuntimeBuildProfile.hpp','CompiledNativeLayoutDeclarations.hpp','JoinedLayoutPreflight.hpp'}
FORBIDDEN={'FMapProperty::KeyProp','FMapProperty::ValueProp','FSetProperty::ElementProp'}
def require(ok,message):
    if not ok:raise ValueError(message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):
    p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def read(p):return json.loads(Path(p).read_text())
def write(p,x):Path(p).write_text(json.dumps(x,indent=2)+'\n')
def check_pin(row):
    p=Path(row['path']);p=p if p.is_absolute() else ROOT/p
    require(sha(p)==row['sha256'],'pin drift: '+str(p));return p

def verify_profile(changes,profile,baseline_profile,unitdir):
    require(len(changes['files'])==4,'expected exact four profile changes')
    require({Path(r['candidate']).name for r in changes['files']}==HEADERS|{'townfall-build-facts.json'},'unexpected profile change files')
    for r in changes['files']:
        require(sha(r['original'])==r['original_sha256'] and sha(r['candidate'])==r['candidate_sha256'],'profile CHANGES drift')
    require(changes['new_promoted_facts']==0 and changes['prior_facts_retained']==93,'l23 ships retained93 only')
    require(profile['required_members']==baseline_profile['required_members'] and profile['required_vtables']==baseline_profile['required_vtables'],'prior93 fact set drift')
    require(len(profile['required_members'])+len(profile['required_vtables'])==93,'expected93 facts')
    require(not FORBIDDEN&{x['name'] for x in profile['required_members']},'new container offset shipping refused')
    digest=hashlib.sha256(json.dumps(profile,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    strings=[json.loads(x.rstrip(',')) for x in (unitdir/'CompiledNativeLayoutDeclarations.hpp').read_text().splitlines() if x.startswith('"')]
    require(digest==changes['profile_digest']==strings[3] and digest in (unitdir/'RuntimeBuildProfile.hpp').read_text(),'profile/provider digest drift')
    require('[FMapProperty]' not in strings[0] and '[FSetProperty]' not in strings[0],'container declarations must remain absent')
    pre=(unitdir/'JoinedLayoutPreflight.hpp').read_text()
    require('{"FArrayProperty::Inner",Kind::Member,Disposition::Required,true,120,' in pre,'Array Inner must remain startup-required')
    for name in FORBIDDEN:require('{"'+name+'",Kind::Member,Disposition::Conditional,false,0,' in pre,'unproved container must be conditional/unpinned')
    return digest

def merge_overlays(inherited,changes):
    merged={}
    for row in inherited:
        d=row['destination'];require(Path(d).name==d,'unsafe overlay destination')
        require(d not in merged,'duplicate inherited overlay destination');merged[d]=copy.deepcopy(row)
    for row in changes['files']:
        d=Path(row['candidate']).name
        if d in HEADERS:merged[d]={**pin(row['candidate']),'destination':d}
    require(HEADERS<={*merged},'missing copied profile header')
    return [merged[d] for d in sorted(merged)]

def adapt_controller(text):
    old="        # l22: only the reviewed local profile headers overlay the frozen bootstrap.\n        for row in inputs['l22_lineage_overlays']:\n            (stage/row['destination']).write_bytes(pin(row).read_bytes())"
    new="        # l23: one merged overlay set; retained lineage pins remain separately verified.\n        for row in inputs['lineage_overlays']:\n            (stage/row['destination']).write_bytes(pin(row).read_bytes())"
    require(text.count(old)==1,'baseline overlay loop missing/duplicated');text=text.replace(old,new)
    old="    runtime=pin(inputs['runtime_object']);composition=out/'composition';composition.mkdir()"
    new="    runtime_row=inputs['runtime_objects_by_pass'][args.pass_id]\n    runtime=pin(runtime_row);composition=out/'composition';composition.mkdir()"
    require(text.count(old)==1,'runtime selection anchor drift');text=text.replace(old,new)
    text=text.replace("if sha(staged_runtime)!=inputs['runtime_object']['sha256']", "if sha(staged_runtime)!=runtime_row['sha256']")
    # Historical controller set status after try: exceptions earlier could leave status stale0.
    text=text.replace('    except Exception as error:b.gap(str(error))','    except Exception as error:\n        b.gap(str(error));status=1')
    text=text.replace("b.note('Exact combined source/lifecycle/one-import edition; local packaging only.')", "b.note('l23 retained93 profile; l22 admitted207 import identities; local packaging only.')")
    require(text.count("for row in inputs['lineage_overlays']:")==1 and 'l22_lineage_overlays' not in text,'overlay double injection')
    compile(text,'generated-package_combined.py','exec');return text

def offline_config(data):
    root=ET.fromstring(data);sections=root.findall('packageSources')
    require(len(sections)==1 and [x.tag for x in sections[0]]==['clear'],'NuGet must have clear-only offline package sources')

def ready_inputs():
    twin=read(BUILD/'TWIN-RECEIPT.json');require(twin['status']=='MAC_ONLY_IDENTICAL_L23_RUNTIME_PAIR','runtime twin status not ready')
    require(twin.get('new_imports')==[],'runtime import delta requires separate admission')
    objects={str(i):pin(BUILD/f'pass{i}/runtime-l23.o') for i in (1,2)}
    require(objects['1']['sha256']==objects['2']['sha256'] and objects['1']['bytes']==objects['2']['bytes'],'runtime twin hash/size mismatch')
    require(len(twin['passes'])==2,'twin pass count')
    require(check_pin(twin['source_selection']).resolve()==BUILD/'SOURCE-SELECTION.json','runtime source selection mismatch')
    read(BUILD/'CHANGE-SELECTION.json')
    for i,row in enumerate(twin['passes'],1):
        require(check_pin(row['runtime']).resolve()==Path(objects[str(i)]['path']),'twin runtime path mismatch')
        composition_path=check_pin(row['composition'])
        require(composition_path.resolve()==BUILD/f'pass{i}/COMPOSITION-RECEIPT.json','composition path mismatch')
        require(read(composition_path)['status']=='MAC_ONLY_L23_COMPLETE_LINK','composition status mismatch')
    changes=read(RUNTIME/'profile/CHANGES.json')
    digest=verify_profile(changes,read(RUNTIME/'profile/profiles/townfall-build-facts.json'),read(ROOT/'outputs/2026-10-08-l22-runtime/profile/profiles/townfall-build-facts.json'),RUNTIME/'profile/units')
    inputs=read(BASE/'BUILD-INPUTS.json')
    # Verify the retained selected edition before copying/preserving all approved pins.
    for row in inputs['pins']:check_pin(row)
    for row in inputs['lineage_source_pins']:check_pin(row)
    for row in inputs.get('l22_lineage_overlays',[]):check_pin(row)
    require(read(BASE/'pass-1/EXACT-IMPORT-IDENTITIES.json')['count']==207,'baseline import count drift')
    for i in (1,2):
        names=sorted({s.split()[-1] for s in (BUILD/f'pass{i}/undefined.stdout').read_text().splitlines() if s.split() and s.split()[0]=='U'})
        require(names==inputs['runtime_strong_undefined'],'raw runtime import delta in pass'+str(i))
        delta=read(BUILD/f'pass{i}/IMPORT-DELTA.json');require(delta['added']==[] and delta['removed']==[],'runtime import receipt delta')
    offline_config((BASE/'inputs/NuGet.Config').read_bytes())
    script=(BASE/'package_prx_l19.py').read_text()
    require(script.count('"-p:RestoreConfigFile=" + str(Path(__file__).resolve().parent / "inputs/NuGet.Config")')==2,'offline restore argument missing')
    return inputs,changes,objects,digest

def main():
    # All admission happens before destination creation. Existing artifacts are immutable.
    inputs,changes,objects,digest=ready_inputs()
    controller=adapt_controller((BASE/'package_combined.py').read_text())
    require(not OUT.exists(),'refuse to overwrite packaging destination')
    OUT.mkdir();(OUT/'inputs').mkdir()
    (OUT/'inputs/NuGet.Config').write_bytes((BASE/'inputs/NuGet.Config').read_bytes())
    for name in TOOLS:(OUT/name).write_bytes((BASE/name).read_bytes())
    (OUT/'package_combined.py').write_text(controller)
    overlays=merge_overlays(inputs.pop('l22_lineage_overlays',[]),changes)
    inputs.update(runtime_source_selection=pin(BUILD/'SOURCE-SELECTION.json'),runtime_object=objects['1'],runtime_objects_by_pass=objects,runtime_independent_twin_receipt=pin(BUILD/'TWIN-RECEIPT.json'),runtime_composition_receipts=[pin(BUILD/f'pass{i}/COMPOSITION-RECEIPT.json') for i in (1,2)],package_script=pin(OUT/'package_prx_l19.py'),controller=pin(OUT/'package_combined.py'),parent_build_inputs=pin(BASE/'BUILD-INPUTS.json'),lineage_overlays=overlays,changed_runtime_units=read(BUILD/'CHANGE-SELECTION.json'),l23_profile_digest=digest,l23_profile_changes=pin(RUNTIME/'profile/CHANGES.json'),final_import_baseline=pin(BASE/'pass-1/EXACT-IMPORT-IDENTITIES.json'))
    inputs.pop('changed_runtime_unit',None)
    inputs['pins'] += [pin(OUT/'inputs/NuGet.Config')]+[pin(OUT/n) for n in TOOLS]+overlays+[pin(BUILD/'TWIN-RECEIPT.json'),*objects.values(),pin(RUNTIME/'profile/profiles/townfall-build-facts.json'),pin(RUNTIME/'profile/CHANGES.json'),pin(BASE/'pass-1/EXACT-IMPORT-IDENTITIES.json')]
    inputs['native_menu_proven']=False;inputs['console_actions_authorized']=False
    write(OUT/'BUILD-INPUTS.json',inputs)
    write(OUT/'PREPARATION-RECEIPT.json',{'scope':'l23 local-only, retained93, no new container offsets','parent_inputs':pin(BASE/'BUILD-INPUTS.json'),'inputs':pin(OUT/'BUILD-INPUTS.json'),'controller':pin(OUT/'package_combined.py'),'bootstrap_header_overlays':overlays,'runtime_imports_unchanged':True,'expected_final_import_count':207,'profile_digest':digest,'console_contact':False})
    print(json.dumps({'package_directory':str(OUT),'profile_overlays':len(overlays),'imports_unchanged':True}))
if __name__=='__main__':main()
