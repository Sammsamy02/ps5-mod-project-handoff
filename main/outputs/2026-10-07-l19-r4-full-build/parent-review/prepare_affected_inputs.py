"""Read-only R3/pass1 audit and concrete R4 recipes; never compiles or packages."""
import hashlib,json,shlex
from pathlib import Path
HERE=Path(__file__).resolve().parent;OUT=HERE.parent;ROOT=OUT.parents[1]
R3=ROOT/'outputs/2026-10-07-l19-r3-full-build'
WORK=ROOT/'outputs/2026-10-07-l19-r4-work'
LOADER=ROOT/'outputs/2026-10-07-l19-r4-compiled-loader'
OWNER=ROOT/'outputs/2026-10-07-l19-r4-owner-variant'
CENSUS=WORK/'OWNER-HEADER-DEPENDENT-UNITS.json'
PASS1=WORK/'dependent-build-pass1/RECEIPT.json'
BASE=R3/'runs/run-l19-2/receipt.json';INPUTS=R3/'runs/run-l19-2/input-hashes.json'
PENDING={'program','unreal-unrealinitializer','native-layoutbinding'}
cache={}
def sha(p):
    p=Path(p).resolve()
    if p not in cache:cache[p]=hashlib.sha256(p.read_bytes()).hexdigest()
    return cache[p]
def check(p,h):assert sha(p)==h,('hash drift',str(p),h,sha(p))
def pin(p):return {'path':str(Path(p).resolve()),'sha256':sha(p)}
def read(p):return json.loads(Path(p).read_text())
def put(p,x):
    assert Path(p).resolve().is_relative_to(OUT)
    assert not Path(p).exists(),p
    Path(p).write_text(json.dumps(x,indent=2)+'\n')
def deps(p):
    text=Path(p).read_text().replace('\\\n',' ')
    assert ':' in text
    return {str(Path(v).resolve()) for v in shlex.split(text.split(':',1)[1])}

def main():
    assert read(OUT/'PREP-MANIFEST.json')['readiness']=='HOLD'
    assert not (OUT/'BUILD-GO.json').exists()
    clone=read(OUT/'PIPELINE-CLONE-RECEIPT.json')
    for name,row in clone['byte_identical_root_files'].items():
        check(row['source'],row['sha256']);check(OUT/name,row['sha256'])
    census=read(CENSUS);receipt=read(PASS1);base=read(BASE);inputs=read(INPUTS)
    check(census['selected_receipt'],census['selected_receipt_sha256'])
    assert Path(census['selected_receipt']).resolve()==BASE.resolve()
    assert census['affected_count']==147 and len(census['affected'])==147
    assert receipt['fresh_compiles']==144 and receipt['affected_total']==147 and receipt['failed']==[]
    assert set(receipt['pending_profile_program_units'])==PENDING
    for p,h in receipt['input_pins'].items():check(p,h)
    assert len(base['selected_runtime_units'])==369 and len(inputs)==386
    old_units=base['selected_runtime_units'];affected={r['name']:r for r in census['affected']}
    assert len(affected)==147 and set(affected)<=old_units.keys()
    fresh={r['name']:r for r in receipt['selected_units']}
    assert len(fresh)==144 and set(fresh)==set(affected)-PENDING
    for p,h in inputs.items():check(p,h)
    for name,row in old_units.items():
        check(row['source'],row['source_sha256']);check(row['object'],row['object_sha256'])
    old_pins={str(Path(p).resolve()):h for p,h in base['source_hashes'].items()}
    for p,h in old_pins.items():check(p,h)
    header=WORK/'dependent-build-pass1/include/Unreal/FField.hpp'
    accepted_header=OWNER/'candidate/FField.hpp';accepted_source=OWNER/'candidate/FField.cpp'
    check(header,sha(accepted_header))
    source_audit=ROOT/'outputs/2026-10-07-l19-r4-owner-variant-review/FINAL-AUDIT.json'
    assert read(source_audit)['pass'] is True
    allowed=dict(old_pins)
    allowed[str(header.resolve())]=sha(accepted_header)
    allowed[str(accepted_source.resolve())]=sha(accepted_source)
    walk={};recipes={}
    for name,row in affected.items():
        old=old_units[name]
        assert row['source_sha256']==old['source_sha256'] and row['object_sha256']==old['object_sha256']
        check(row['dependency'],row['dependency_sha256'])
        prior_deps=deps(row['dependency'])
        for p in prior_deps:
            assert p in old_pins,('unbound R3 dependency',name,p)
            check(p,old_pins[p])
        rec={'name':name,'old_selected_source':pin(row['source']),'old_object':pin(row['object']),
             'old_dependency':pin(row['dependency']),'original_argv':row['argv'],'LLVM_CONFIG':row['LLVM_CONFIG'],
             'required_accepted_header':pin(header),'required_independent_owner_audit':pin(source_audit),
             'include_prefix':['-I',str(header.parent.parent)],'pass2_must_compile_fresh':True,
             'composition_input_index':list(inputs).index(old['object']),
             'permitted_changes':['accepted FField include prefix','exact admitted source path','-o output','-MF dependency output'],
             'pending_final_source_binding':name in PENDING}
        if name in PENDING:
            rec['pass1_reusable']=False
            rec['final_source_role']={'program':'Program','unreal-unrealinitializer':'initializer','native-layoutbinding':'NativeLayoutBinding.cpp'}[name]
            rec['final_source']=pin(LOADER/'units/UE4SSProgram.cpp') if name=='program' else None
            rec['required_profile_include_prefix']=None
            if name=='program':
                rec['required_compiled_declaration_define']='RC_NATIVE_LAYOUT_DECLARATIONS_HEADER'
                rec['required_generated_header']=None
                rec['required_generic_provider']=pin(LOADER/'units/NativeLayoutDeclarations.hpp')
                rec['permitted_changes']+=['exact reviewed profile/provider include prefixes','exact generated-header macro (Program only)']
        else:
            f=fresh[name];assert f['exit']==0 and f['accepted_header_selected'] is True
            assert f['old_object_sha256']==old['object_sha256']
            expected_source=accepted_source if name=='unreal-ffield' else Path(old['source'])
            assert Path(f['source']).resolve()==expected_source.resolve()
            check(f['source'],f['source_sha256']);check(f['object'],f['object_sha256']);check(f['dependency_file'],f['dependency_sha256'])
            actual_deps=deps(f['dependency_file']);assert str(header.resolve()) in actual_deps
            for p in actual_deps:
                assert p in allowed,('unbound current dependency',name,p)
                check(p,allowed[p]);walk[p]=allowed[p]
            rec.update(final_source=pin(expected_source),pass1_reusable=True,
                       pass1_object=pin(f['object']),pass1_dependency=pin(f['dependency_file']),
                       pass1_argv=f['argv'],all_dependency_pins={p:allowed[p] for p in sorted(actual_deps)})
        recipes[name]=rec
    # Preserve original merged Lua source and comparator, including fresh .d evidence.
    lua=fresh['luamod'];oldlua=old_units['luamod']
    assert lua['source_sha256']==oldlua['source_sha256'] and lua['object_sha256']==oldlua['object_sha256']
    slots=[{'position':i,'old_input':pin(p),'unit':next((n for n,u in old_units.items() if u['object']==p),None),
            'replace_from_affected_recipe':next((n for n,u in old_units.items() if u['object']==p and n in affected),None)} for i,p in enumerate(inputs)]
    common={'scope':'LOCAL preparation only; no compilation/link/package/console','readiness':'HOLD',
            'base_receipt':pin(BASE),'base_inputs':pin(INPUTS),'owner_census':pin(CENSUS),'owner_pass1':pin(PASS1),
            'owner_audit':pin(source_audit),'owner_header':pin(accepted_header),'owner_source':pin(accepted_source),
            'loader_freeze':pin(LOADER/'reports/SOURCE-SELECTION.json'),'loader_final_receipt':pin(LOADER/'reports/FINAL-RECEIPT.json'),
            'selected_units':369,'composition_inputs':386,'affected_units':147,'pass1_validated_reusable_units':144,
            'pending_final_units':sorted(PENDING),'pass2_required_fresh_units':147,'no_added_units':True,
            'original_luamod':{'source':pin(oldlua['source']),'object_sha256':oldlua['object_sha256'],
                              'pass1_object':pin(lua['object']),'accepted_header_in_actual_dependency':True,'source_unchanged':True,'object_identical':True},
            'final_profile':None,'final_generated_declarations':None,'final_profile_independent_review':None,'parent_R4_build_GO':None}
    put(HERE/'AFFECTED-RECIPES.json',dict(common,recipes=recipes))
    put(HERE/'INPUT-SUBSTITUTION-MANIFEST.json',dict(common,input_slots=slots,all_checked_dependency_pins=walk))
    put(HERE/'PREPARATION-RECEIPT.json',dict(common,pass_=True,actual_checked_unique_files=len(cache),
        full_144_dependency_walks_verified=True,all_386_base_input_hashes_verified=True,all_369_base_source_object_hashes_verified=True,
        pipeline_byte_exact_files=14,build_executed=False,packaging_executed=False,
        prepared_outputs={str(p.relative_to(OUT)):sha(p) for p in [HERE/'AFFECTED-RECIPES.json',HERE/'INPUT-SUBSTITUTION-MANIFEST.json']}))
    print(json.dumps({'prepared':True,'checked_unique_files':len(cache),'affected':147,'reuse_eligible_pass1':144,'pass2_fresh':147,'inputs':386,'readiness':'HOLD'}))

if __name__=='__main__':main()
