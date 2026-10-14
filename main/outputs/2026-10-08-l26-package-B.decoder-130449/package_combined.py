"""l24 edition of the measured source/lifecycle/import combined packager.

Copied from the proven l23 controller; the pipeline, gates and provenance
discipline are unchanged. Only the packaging inputs move to the l24 runtime
twin build (typescan-r3 refusal-rework edition) and the pass notes/labels say
l24. No console/network operations. Does not promote the PRX or override
independent review. Two separate local passes and explicit exact new import
admission only.
"""
import argparse,hashlib,importlib.util,json,shutil,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.path.insert(0,str(HERE));from prep_guard import require_ready,pin
sys.path.insert(0,str(ROOT/'outputs/2026-10-08-thread-import-admission'))
import import_admission as admission
from check_import_identity import verify as verify_identities
from extended_sections import array_census

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,data):path.write_text(json.dumps(data,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--pass-id',required=True,choices=['1','2']);args=parser.parse_args()
    inputs=require_ready();out=HERE/('pass-'+args.pass_id)
    if out.exists():raise ValueError('refuse to overwrite prior pass: '+str(out))
    out.mkdir()
    spec=importlib.util.spec_from_file_location('copied_package',pin(inputs['package_script']))
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    for key,row in inputs['historical_receipts'].items():m.PIN[key]=pin(row)
    evidence=json.loads(pin(inputs['import_evidence']).read_text());admission.validate_evidence(evidence)
    m.EXTENDED_IMPORTS['scePthreadGetthreadid']='sceKernelOpen'
    m.EXTENDED_IMPORT_EVIDENCE['scePthreadGetthreadid']='sdknm-libkernel.so.stdout'
    m.ALL_EXTENDED_IMPORTS={**m.EXTENDED_IMPORTS,**m.BOOTSTRAP_ONLY_IMPORTS}
    # Preserve the historical typed table order and append the one new getter.
    old_order=[n for n in m.ALL_EXTENDED_IMPORTS if n!='scePthreadGetthreadid']
    order=old_order+['scePthreadGetthreadid'];m.ALL_EXTENDED_IMPORTS={n:m.ALL_EXTENDED_IMPORTS[n] for n in order}
    m.ALL_EXTENDED_IMPORT_EVIDENCE={**m.EXTENDED_IMPORT_EVIDENCE,**{n:'sdknm-libkernel.so.stdout' for n in m.BOOTSTRAP_ONLY_IMPORTS}}
    generated=admission.generate(order)
    if pin(inputs['import_check']).read_text()!=generated['source']:
        raise ValueError('generated owned relocation check does not match actual shim order')

    # Same frozen source paths for both independent bootstrap compiles: avoid
    # changing source-location strings merely because an output directory differs.
    stage=HERE/'lineage'
    if not stage.exists():
        # The anchor is a symlink to the frozen parent file; its resolved
        # parent is not the selected R4 overlay. Use the explicit directory.
        source_lineage=Path(inputs['lineage_root'])
        expected=ROOT/'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2'
        if source_lineage!=expected:raise ValueError('unexpected selected lineage directory')
        shutil.copytree(source_lineage,stage)
        (stage/'lifecycle/runtime_module_entry.c').write_bytes(pin(inputs['outer_entry']).read_bytes())
        (stage/'lifecycle/runtime_start_bridge.cpp').write_bytes(pin(inputs['outer_bridge']).read_bytes())
        for row in inputs['outer_headers']:(stage/'lifecycle'/Path(row['path']).name).write_bytes(pin(row).read_bytes())
        (stage/'native_thread_import_check.c').write_bytes(pin(inputs['import_check']).read_bytes())
        # l24: the retained l23 overlay set (profile unchanged in l24); retained
        # lineage pins remain separately verified.
        for row in inputs['lineage_overlays']:
            (stage/row['destination']).write_bytes(pin(row).read_bytes())
        write(HERE/'STAGED-SOURCE-PINS.json',{'inputs_sha256':sha(HERE/'BUILD-INPUTS.json'),
            'pins':[{'path':str(p),'sha256':sha(p)} for p in sorted(stage.rglob('*')) if p.is_file()]})
    staged=json.loads((HERE/'STAGED-SOURCE-PINS.json').read_text())
    if staged['inputs_sha256']!=sha(HERE/'BUILD-INPUTS.json'):raise ValueError('staged source edition mismatch')
    for row in staged['pins']:pin(row)
    runtime_row=inputs['runtime_objects_by_pass'][args.pass_id]
    runtime=pin(runtime_row);composition=out/'composition';composition.mkdir()
    staged_runtime=composition/'coherent-tick-file-witness-runtime.o';shutil.copyfile(runtime,staged_runtime)
    if sha(staged_runtime)!=runtime_row['sha256']:raise ValueError('runtime copy drift')
    for name,row in inputs['SDK_captures'].items():shutil.copyfile(pin(row),composition/name)
    m.PIN.update(lineage=stage,composition_dir=composition,composition_object_name=staged_runtime.name,
                 composition_object_sha256=sha(staged_runtime),composition_object_bytes=staged_runtime.stat().st_size,
                 composition_checkpoint='LOCAL_COMBINED_SOURCE_SELECTION_NOT_NATIVE_PROOF')
    # The historical frozen-entry compile is replaced only by the new exact
    # actual entry; all other bootstrap order and source provenance is retained.
    units=list(m.BOOTSTRAP_UNITS);units[units.index('lifecycle/module_entry.c')]='lifecycle/runtime_module_entry.c'
    units[units.index('diagnostic.c')]=str(pin(inputs['diagnostic_source']))
    units+=['lifecycle/runtime_start_bridge.cpp','native_thread_import_check.c']
    m.BOOTSTRAP_UNITS[:]=units
    old_shim=m.supplier_import_shim
    def shim():
        text=old_shim()
        return text+f'''.section .rodata.modbridge_supplier_import_count,"a",@progbits
.p2align 2
.globl modbridge_supplier_import_count
.hidden modbridge_supplier_import_count
.type modbridge_supplier_import_count,@object
modbridge_supplier_import_count:
.long {generated['count']}
.size modbridge_supplier_import_count, 4
'''
    m.supplier_import_shim=shim
    old_contract=m.derive_contract
    def contract(b,path):
        result=old_contract(b,path)
        rows=[r for r in result['contract']['imports'] if r['ImportSymbol']['Name']=='scePthreadGetthreadid']
        if len(rows)!=1:raise ValueError('new getter row missing/ambiguous')
        row=rows[0]
        if {k:row['ImportSymbol'][k] for k in admission.EXPECTED}!=admission.EXPECTED:
            raise ValueError('new getter provider identity mismatch')
        row['evidence'].append({'source':str(pin(inputs['import_evidence'])),'sha256':inputs['import_evidence']['sha256'],
            'qualification':'Exact saved native NID/module/library versions and SDK declaration. Final package checked independently; no native resolution claim.'})
        result['combined_thread_import']={'exact_evidence':inputs['import_evidence'],'typed_slot':generated['index'],
            'typed_count':generated['count'],'runtime_owned_relocation_check':inputs['import_check'],
            'native_resolution_proven':False}
        write(path,result);return result
    m.derive_contract=contract
    old_run=m.Build.run
    def run(self,argv,**kw):
        a=list(map(str,argv))
        if a[0].endswith('clang++') and '-c' in a and '-std=c++20' not in a:a.insert(1,'-std=c++20')
        return old_run(self,a,**kw)
    m.Build.run=run
    b=m.Build(out,True)
    b.note('l26 variant B UNSAFE DIAGNOSTIC (delete-listener registration compiled out via -DMODBRIDGE_L25_SKIP_DELETE_LISTENER; stale object pointers not cleared); profile lineage carries the l25 container promotion; l23-admitted 207 import identities; local packaging only; no console contact.')
    write(out/'INPUT-SELECTION.json',inputs)
    status=1;packaging={};verification={}
    try:
        # Exact new source admission replaces the obsolete composition-2 delta
        # checkpoint test. No failing old result is erased or mislabeled.
        strong,weak,defined=m.readelf_undefined(m.PIN['llvm']/'llvm-readelf',staged_runtime)
        if sorted(strong)!=inputs['runtime_strong_undefined'] or sorted(weak)!=inputs['runtime_weak_undefined']:
            raise ValueError('new runtime raw symbol census drift')
        write(composition/'undefined-delta.json',{'current':{n:'actual measured strong UND' for n in sorted(strong)},
            'current_count':len(strong),'scope':'Actual new combined-runtime census; not a relabeled historical delta.'})
        packaging=m.link_pipeline(b)
        if not b.gaps:
            verification=m.verify_candidate(b,packaging)
            combined_blob=Path(packaging['combined_object']['path']).read_bytes()
            packaged_blob=(out/'bare-sdk'/m.CANDIDATE_BASENAME).read_bytes()
            before=array_census(combined_blob);after=array_census(packaged_blob)
            if any(before[n]['bytes']!=after[n]['bytes'] or not before[n]['bytes'] for n in before):
                raise ValueError('constructor/destructor slots not preserved')
            write(out/'CONSTRUCTOR-ARRAY-CENSUS.json',{'status':'PASS_MAC_ONLY_FULL_EXTENDED_AND_LEGACY_CENSUS',
                'combined':before,'packaged':after,
                'qualification':'Complete legacy/modern entry preservation; not native execution.'})
            contract_data=json.loads((out/'supplier-contract.json').read_text())
            exact=verify_identities(packaged_blob,contract_data,require_getter=True)
            if exact['count']!=207:raise ValueError('expected exactly original 206 plus one declared getter')
            write(out/'EXACT-IMPORT-IDENTITIES.json',exact)
        status=int(bool(b.gaps))
    except Exception as error:
        b.gap(str(error));status=1
    write(out/'PACKAGE-RESULT.json',packaging);write(out/'STATIC-CHECKS.json',verification)
    write(out/'BUILD-RECEIPT.json',{'status':'MAC_PACKAGE_CHECKS_PASS' if not status else 'FAIL',
        'gaps':b.gaps,'commands':b.records,'log':b.log,'inputs_sha256':sha(HERE/'BUILD-INPUTS.json'),
        'source_provenance_verified_by_new_manifest':True,'historical_verify_lineage_not_relabelled':True,
        'native_resolution_proven':False,'native_execution':False,'console_contact':False,'deployment_approved':False})
    return status

if __name__=='__main__':raise SystemExit(main())
