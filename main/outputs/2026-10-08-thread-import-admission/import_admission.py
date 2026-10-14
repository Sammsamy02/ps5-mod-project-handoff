"""Exact local import contract + runtime-owned relocation readiness.

Does not assert target provider availability. Final package identity verification
is mandatory; startup checks only its OWN relocation table, not provider memory.
There is no source boolean that purports to know a future artifact hash.
"""
import hashlib, json, pathlib

HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
EVIDENCE=ROOT/'outputs/2026-10-08-native-thread-import-evidence/IMPORT-EVIDENCE.json'
NAME='scePthreadGetthreadid'
EXPECTED={'Name':NAME,'ModuleName':'libkernel','ModuleVersion':257,
          'LibraryName':'libkernel','LibraryVersion':1,'Soname':'libkernel.prx','PublishedName':NAME}

def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def validate_evidence(evidence):
    if (evidence.get('new_imports')!=[NAME] or evidence.get('identity_donor')!='sceKernelOpen'
        or evidence.get('identity')!=EXPECTED or evidence.get('nid')!='EI-5-jlq2dE'
        or evidence.get('symbol_type')!='FUNC' or evidence.get('binding')!='GLOBAL'):
        raise ValueError('exact native thread import identity mismatch')
    for key in ('provider','SDK_export_capture','nid_encoder','base_contract','game_image','thread_audit'):
        record=evidence[key]
        if sha(record['path'])!=record['sha256']:
            raise ValueError('import evidence pin drift: '+key)
    contract=json.loads(pathlib.Path(evidence['base_contract']['path']).read_text())
    rows=[r['ImportSymbol'] for r in contract['contract']['imports'] if r['ImportSymbol']['Name']=='sceKernelOpen']
    if len(rows)!=1:raise ValueError('exact donor absent/ambiguous')
    donor={key:rows[0][key] for key in EXPECTED}
    donor['Name']=donor['PublishedName']=NAME
    if donor!=EXPECTED:raise ValueError('accepted libkernel donor mismatch')
    text=pathlib.Path(evidence['SDK_export_capture']['path']).read_text()
    matches=[line.strip() for line in text.splitlines() if line.split() and line.split()[0]==NAME]
    if matches!=['scePthreadGetthreadid T 1218c 0']:
        raise ValueError('exact SDK declaration mismatch')
    image=pathlib.Path(evidence['game_image']['path'])
    with image.open('rb') as stream:
        for offset_key,bytes_key in (('relocation_file_offset','relocation_bytes'),('symbol_file_offset','symbol_bytes')):
            record=evidence['native_import'];wanted=bytes.fromhex(record[bytes_key])
            stream.seek(int(record[offset_key],16))
            if stream.read(len(wanted))!=wanted:raise ValueError('native symbol/relocation pin drift')
        stream.seek(int(evidence['native_import']['name_file_offset'],16))
        name=evidence['native_import']['name'].encode()+b'\0'
        if stream.read(len(name))!=name:raise ValueError('native NID declaration mismatch')
    return EXPECTED.copy()

def generate(names):
    if len(set(names))!=len(names) or names.count(NAME)!=1:
        raise ValueError('typed shim must contain exactly one getter row')
    count=len(names);index=names.index(NAME)
    source=f'''/* Generated from exact declared import table order; no target-memory read. */
#include <stdint.h>
extern const uintptr_t modbridge_supplier_import_table[];
extern const uint32_t modbridge_supplier_import_count;
extern void diagnostic_event(const char *, uint32_t);
__attribute__((visibility("hidden")))
int bridge_native_thread_import_ready(void) {{
    const uint32_t count=modbridge_supplier_import_count;
    diagnostic_event("native_thread_import_count", count);
    if(count!={count}u){{
        diagnostic_event("native_thread_import_count_ok",0u);
        return 0;
    }}
    diagnostic_event("native_thread_import_count_ok",1u);
    const int bound=modbridge_supplier_import_table[{index}u]!=0;
    diagnostic_event("native_thread_import_slot_ok",(uint32_t)bound);
    return bound;
}}
'''
    header='''#pragma once
extern "C" int bridge_native_thread_import_ready(void);
namespace NativeNameThread {
// Final packaging independently verifies NID and module/library identities.
// This checks only the owned typed relocation row at startup. No claim that
// local SDK declarations alone prove resolution or native engine execution.
inline bool exact_import_admitted() noexcept { return bridge_native_thread_import_ready()==1; }
}
'''
    return {'names':names,'count':count,'index':index,'source':source,'header':header,
            'new_identity':EXPECTED.copy(),'nid':'EI-5-jlq2dE','native_resolution_proven':False}

if __name__=='__main__':
    validate_evidence(json.loads(EVIDENCE.read_text()))
    print('Exact local getter contract validated; final package/startup resolution remains pending.')
