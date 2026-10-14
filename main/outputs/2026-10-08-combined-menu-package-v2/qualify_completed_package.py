"""Qualify an existing package after the preserved extended-section parser error.

No compiling, package mutation, console action or review substitution. The exact
original failure must be reproduced, then replaced by a tested complete census
including legacy .ctors/.dtors and duplicate section names.
"""
import argparse,hashlib,importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from prep_guard import require_ready
from extended_sections import array_census
from check_import_identity import verify as verify_imports

def pin(p):
    p=Path(p);return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--pass-id',choices=['1','2'],required=True);a=parser.parse_args()
    inputs=require_ready();out=HERE/('pass-'+a.pass_id);destination=out/'CORRECTED-QUALIFICATION.json'
    if destination.exists():raise ValueError('preserve prior qualification')
    original=json.loads((out/'BUILD-RECEIPT.json').read_text())
    if original['status']!='FAIL' or original['gaps']!=['list index out of range']:
        raise ValueError('not the isolated documented checker failure')
    package=json.loads((out/'PACKAGE-RESULT.json').read_text());checks=json.loads((out/'STATIC-CHECKS.json').read_text())
    if checks['status']!='STATIC_CHECKS_PASSED' or not all(r['pass'] for r in checks['checks'].values()):
        raise ValueError('an original static package check failed')
    spec=importlib.util.spec_from_file_location('preserved_reader',HERE/'package_prx_l19.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    combined=Path(package['combined_object']['path']);wrapped=Path(package['candidate']['path']);bare=out/'bare-sdk'/module.CANDIDATE_BASENAME
    if pin(combined)['sha256']!=package['combined_object']['sha256'] or pin(wrapped)['sha256']!=package['candidate']['sha256']:
        raise ValueError('already-built artifacts drifted')
    cb=combined.read_bytes();bb=bare.read_bytes()
    try:module.parse_sections(cb)
    except IndexError:pass
    else:raise ValueError('original parser failure not reproduced on these exact bytes')
    before=array_census(cb);after=array_census(bb)
    if any(before[n]['bytes']!=after[n]['bytes'] or not before[n]['bytes'] for n in before):
        raise ValueError('constructor/destructor slots not preserved')
    # Boundaries and native entry ownership are checked by the preserved
    # package checks. This confirms all legacy/modern input entries survived.
    census={'status':'PASS_MAC_ONLY_FULL_EXTENDED_AND_LEGACY_CENSUS','combined':before,'packaged':after,
            'canonicalization':'Saved DynamicWriter.cs maps .ctors/.dtors to .init_array/.fini_array. Duplicate sections are all retained in this census.'}
    write(out/'CONSTRUCTOR-ARRAY-CENSUS-CORRECTED.json',census)
    identities=verify_imports(bb,json.loads((out/'supplier-contract.json').read_text()),require_getter=True)
    if identities['count']!=207:raise ValueError('wrong actual import cardinality')
    write(out/'EXACT-IMPORT-IDENTITIES.json',identities)
    d={'status':'MAC_ONLY_COMPLETED_PACKAGE_CHECKS_PASS','pass':int(a.pass_id),
       'candidate':pin(wrapped),'bare':pin(bare),'combined':pin(combined),
       'original_failed_receipt':pin(out/'BUILD-RECEIPT.json'),'original_failure_preserved':True,
       'original_failure_reproduced':'IndexError from extended numbering: ELF64 e_shnum=0/e_shstrndx=65535',
       'original_static_checks':pin(out/'STATIC-CHECKS.json'),'original_static_check_count':len(checks['checks']),
       'corrected_array_census':pin(out/'CONSTRUCTOR-ARRAY-CENSUS-CORRECTED.json'),
       'exact_import_identities':pin(out/'EXACT-IMPORT-IDENTITIES.json'),'actual_import_count':207,
       'source_inputs':pin(HERE/'BUILD-INPUTS.json'),'corrected_checker':pin(__file__),
       'extended_reader':pin(HERE/'extended_sections.py'),'constructor_slots':after['.init_array']['slots'],
       'destructor_slots':after['.fini_array']['slots'],'independent_cleanup_acceptance':False,
       'native_execution':False,'native_menu_proven':False,'console_contact':False,'deployment_approved':False}
    write(destination,d)
    print(json.dumps({k:d[k] for k in ('status','pass','candidate','original_static_check_count','actual_import_count','constructor_slots','destructor_slots')}))
if __name__=='__main__':main()
