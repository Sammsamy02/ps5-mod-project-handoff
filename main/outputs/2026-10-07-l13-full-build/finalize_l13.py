#!/usr/bin/env python3
"""Local evidence aggregation. No console contact or executable changes."""
from pathlib import Path
import hashlib,json
from datetime import datetime, timezone

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
LOCAL=ROOT/'outputs/2026-10-07-l13-local-admission'
L12=ROOT/'outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l12-2'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p): return json.loads(Path(p).read_text())
def save(p,d): p.write_text(json.dumps(d,indent=2)+'\n')
def artifact(p): return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def contracts(p): return {r['ImportSymbol']['Name']:r['ImportSymbol'] for r in read(p/'supplier-contract.json')['contract']['imports']}

g=read(LOCAL/'receipts/LATEST-GATES.json')
assert g['status']=='PASS_MAC_ONLY'
for rel,h in g['source_hashes'].items(): assert sha(LOCAL/rel)==h,rel
for rec in g['preserved']: assert sha(ROOT/rec['path'])==rec['expected'],rec['path']
ed=read(OUT/'EDITIONS-l13.json')
for rec in ed['units']+[ed['bridge_cli']]: assert sha(ROOT/rec['file'])==rec['sha256'],rec['file']

passes=[]
for number in (1,2):
    c=OUT/f'prx-candidate-l13-{number}'
    r=OUT/f'runs/run-l13-{number}'
    static=read(c/'verification-evidence/static-checks.json')
    assert len(static['checks'])==20 and all(v['pass'] for v in static['checks'].values())
    access=read(OUT/f'receipts/accessor-verification-prx-candidate-l13-{number}.json')
    assert access['verdict']=='PASS'
    final=c/'modbridge_supplier_composition.prx'
    assert final.read_bytes()[:4].hex()=='4f153d1d'
    assert read(c/'artifacts-receipt.json')['artifacts']['final_prx']==artifact(final)
    gate_path=OUT/('gate-receipt-l13-1-final.json' if number==1 else 'gate-receipt-l11-prx-candidate-l13-2.json')
    gate=read(gate_path)
    assert gate['failed_checks']==[]
    archive=OUT/f'receipts/prx-candidate-l13-{number}'
    assert (archive/'driver/transport-ledger.composition6-mirror.json').is_file()
    finalization=read(archive/'change-manifest.finalized.json.receipt.json')
    assert not finalization.get('divergences',[]),finalization.get('divergences')
    passes.append({'number':number,'prx':artifact(final),
        'runtime_object':artifact(r/'coherent-tick-file-witness-runtime.o'),
        'combined_object':artifact(c/'build/combined-supplier.o'),
        'static_checks_passed':len(static['checks']),
        'gate_receipt':str(gate_path),
        'gate_ids':[v['id'] for v in gate['checks']],
        'failed_checks':gate['failed_checks'],'receipt_archive':str(archive)})
for kind in ('prx','runtime_object','combined_object'):
    assert passes[0][kind]['sha256']==passes[1][kind]['sha256'],kind
old,new=contracts(L12),contracts(OUT/'prx-candidate-l13-2')
added=sorted(new.keys()-old.keys());removed=sorted(old.keys()-new.keys())
assert added==['_openat','sceKernelRead'] and not removed
assert len(new)==200
assert all(old[n]==new[n] for n in old),'Existing import identity drift'
assert all(new[n]['Soname']=='libkernel.prx' for n in added)
src={str(LOCAL/rel):h for rel,h in g['source_hashes'].items()}
receipt={'status':'PASS_MAC_ONLY_UPLOAD_HOLD','created_utc':datetime.now(timezone.utc).isoformat(),
    'console_contact':False,'console_success':False,'passes':passes,
    'determinism':{'prx':True,'runtime_object':True,'combined_object':True},
    'sdk':'0x11000000','magic':'4f153d1d','final_strong_import_count':200,
    'import_delta_vs_l12':{'added':added,'removed':removed,'retained_identities_unchanged':True},
    'new_import_identities':{n:new[n] for n in added},
    'local_source_and_frozen_checks':'PASS','approved_source_hashes':src,
    'source_gate_receipt':str(LOCAL/'receipts/LATEST-GATES.json'),
    'input_plan':str(LOCAL/'reports/INPUT-NEXT-BUILD.md'),
    'not_run_gate_ids':['B1','B2','B3','D2'],
    'not_run_reason':'These optional sections were absent from the inherited l12 manifest; not claimed passed.',
    'first_pass_note':'Provisional D1 kept as history. After twin existed, independent compare-to-2 receipt passed 11/11.',
    'pipeline_corrections':['Typed shim precedes bridge so new native imports remain FUNC, strict harness unchanged.',
      'Manifest adapter points to generated extra-unit receipt. No runtime source change.'],
    'scope':'Two fresh l12-derived composition/package passes with pinned retained dependencies; host evidence only.'}
save(OUT/'receipts/FINAL-GATES.json',receipt)
save(OUT/'deployment/candidate-manifest.json',{'schema':1,'remote_path':'/data/plugins/modbridge_supplier_composition.prx',
    'candidate':passes[1]['prx'],
    'expected_remote_sha256':'024a4143bb84333e4552814fd68e67f6f72f7b04630dcd2b4a512b44d8bc34b4'})
save(OUT/'SOURCE-MANIFEST.json',{'files':{**src,**{str(p):sha(p) for p in OUT.glob('*.py')},
    str(OUT/'EDITIONS-l13.json'):sha(OUT/'EDITIONS-l13.json'),
    str(OUT/'integration/change-manifest.finalized.json'):sha(OUT/'integration/change-manifest.finalized.json')},
    'runtime_input_pins':ed['units'],'artifact':passes[1]['prx']})
print(json.dumps({k:receipt[k] for k in ('status','determinism','final_strong_import_count','import_delta_vs_l12')},indent=2))
print(json.dumps(passes[1]['prx'],indent=2))
