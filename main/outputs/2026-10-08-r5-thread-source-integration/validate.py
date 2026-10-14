#!/usr/bin/env python3
from pathlib import Path
import json,hashlib
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((HERE/'SOURCE-SELECTION.json').read_text())
for group in ['source_pins','receipt_pins','artifact_pins']:
 for r in manifest[group]:assert sha(r['path'])==r['sha256'],r['path']
receipt=json.loads((HERE/'COMPOSITION-RECEIPT.json').read_text());compile_receipt=json.loads((HERE/'COMPILE-RECEIPT.json').read_text());closure=json.loads((HERE/'CLOSURE.json').read_text())
assert receipt['status']=='EXPERIMENTAL_FULL_R5_STOP_PAD_THREAD_LINK_PASS'
assert (receipt['selected_count'],receipt['full_link_inputs'],receipt['fresh_existing_units'],receipt['new_provider_units'])==(374,391,2,4)
assert compile_receipt['compiled_units']==6 and not compile_receipt['failures']
assert closure['units_scanned']==370 and not closure['Config_changed']
assert receipt['actual_undefined_added']==['scePthreadGetthreadid'] and receipt['actual_undefined_removed']==[]
assert not receipt['thread_import_admission'] and not receipt['thread_retire_runtime_wired'] and not receipt['outer_entry_bridge_selected']
print(json.dumps({'status':'FROZEN_EXPERIMENT_VALIDATION_PASS','source_pins':len(manifest['source_pins']),'receipt_pins':len(manifest['receipt_pins']),'artifact_pins':len(manifest['artifact_pins']),'selected_units':374,'fresh_compiles':6,'UND_delta':['scePthreadGetthreadid'],'package_admission':False}))
