#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,filecmp
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
s=json.loads((HERE/'SOURCE-SELECTION.json').read_text())
for group in ['source_pins','receipt_pins','artifact_pins']:
 for r in s[group]:assert sha(r['path'])==r['sha256'],r['path']
a=json.loads((HERE/'pass1/COMPOSITION-RECEIPT.json').read_text());b=json.loads((HERE/'pass2/COMPOSITION-RECEIPT.json').read_text());assert a['pass']==1 and b['pass']==2
assert a['selected_count']==b['selected_count']==374 and a['full_link_inputs']==b['full_link_inputs']==391
assert a['object']['path']!=b['object']['path'] and a['object']['sha256']==b['object']['sha256'];assert filecmp.cmp(a['object']['path'],b['object']['path'],shallow=False)
aa={r['name']:r for r in a['selected_units']};bb={r['name']:r for r in b['selected_units']};assert set(aa)==set(bb)
for n in aa:
 assert aa[n]['source_sha256']==bb[n]['source_sha256'];assert aa[n]['object_sha256']==bb[n]['object_sha256']
c=json.loads((HERE/'pass2-build/COMPILE-RECEIPT.json').read_text());assert c['status']=='FRESH_COMBINED_TARGET_COMPILE_PASS' and {r['name'] for r in c['results']}=={'unreal-unrealinitializer','native-thread-name-threadnameruntime'}
assert all(r['returncode']==0 for r in c['results']);assert a['actual_undefined_added']==b['actual_undefined_added']==['bridge_native_thread_import_ready'];assert a['actual_undefined_removed']==b['actual_undefined_removed']==[]
print(json.dumps({'status':'FROZEN_CORRECTED_TWIN_RUNTIME_VALIDATION_PASS','selected_units':374,'full_inputs':391,'source_object_pairs_equal':374,'runtime_bytes_equal':True,'runtime_sha256':a['object']['sha256'],'fresh_pass2_TUs':2,'source_pins':len(s['source_pins']),'receipt_pins':len(s['receipt_pins']),'artifact_pins':len(s['artifact_pins']),'package_verified':False,'console_contact':False}))
