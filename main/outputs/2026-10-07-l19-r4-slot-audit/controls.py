#!/usr/bin/env python3
"""Mutation controls for the independent read-only native evidence audit."""
from pathlib import Path
import copy,hashlib,json,subprocess,sys
root=Path.cwd();own=Path(__file__).resolve().parent;candidate=root/'outputs/2026-10-07-l19-r4-engine-slots';fact=candidate/'ENGINE-SLOT-FACTS-v2.json';base=json.loads(fact.read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((candidate/'SOURCE-HASHES.json').read_text())
for relative,pin in manifest['files'].items():assert sha(candidate/relative)==pin,relative
final=json.loads((candidate/'FINAL-RECEIPT.json').read_text())
for relative,pin in final['pins'].items():assert sha(candidate/relative)==pin,relative
mutations={
 'wrong_declared_slot':lambda d:d['facts'][0].update(value='0x500'),
 'wrong_instruction_bytes':lambda d:d['instruction_evidence']['0x2fdb2e3'].update(instruction_bytes='00'+d['instruction_evidence']['0x2fdb2e3']['instruction_bytes'][2:]),
 'wrong_relocation_target':lambda d:d['relocations'][0].update(r_addend='0x384d461'),
 'wrong_native_property_offset':lambda d:d['property_semantics'][0].update(offset='0x2c8'),
 'missing_native_property_identity':lambda d:d['property_semantics'].pop(),
}
results=[];outdir=own/'negative-controls';outdir.mkdir(exist_ok=True)
for name,mutation in mutations.items():
 d=copy.deepcopy(base);mutation(d);p=outdir/(name+'.json');p.write_text(json.dumps(d,indent=2)+'\n')
 r=subprocess.run([sys.executable,str(own/'audit.py'),str(p)],cwd=root,text=True,capture_output=True)
 assert r.returncode!=0 and 'AssertionError' in r.stderr,(name,r.returncode,r.stderr)
 (outdir/(name+'.log')).write_text(r.stderr)
 results.append({'name':name,'refused':True,'exit_code':r.returncode,'mutation_sha256':sha(p),'log_sha256':sha(outdir/(name+'.log'))})
r=subprocess.run([sys.executable,str(own/'audit.py'),str(fact)],cwd=root,text=True,capture_output=True);assert r.returncode==0,r.stderr
(own/'AUDIT-RUN.log').write_text(r.stdout+r.stderr)
result={'pass':True,'original_fact_sha256':sha(fact),'worker_frozen_manifest_sha256':sha(candidate/'SOURCE-HASHES.json'),'worker_manifest_files':len(manifest['files']),'worker_final_receipt_sha256':sha(candidate/'FINAL-RECEIPT.json'),'worker_final_pins_verified':len(final['pins']),'controls':results,'original_after_controls':'PASS','audit_script_sha256':sha(own/'audit.py'),'controls_script_sha256':sha(Path(__file__))}
(own/'CONTROL-RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'pass':True,'negative_controls':len(results),'worker_manifest_files':len(manifest['files']),'worker_final_pins':len(final['pins'])}))
