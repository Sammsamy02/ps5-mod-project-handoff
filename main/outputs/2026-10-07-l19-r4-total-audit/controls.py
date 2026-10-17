#!/usr/bin/env python3
"""Refusal controls on copied total-fact receipts; immutable source/native inputs stay untouched."""
from pathlib import Path
import copy,hashlib,json,subprocess,sys
root=Path.cwd();own=Path(__file__).resolve().parent;source=root/'outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence';fact=source/'OFFICIAL-TOTAL-FACTS.json';base=json.loads(fact.read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
controls={
 'wrong_FField_total':lambda d:d['facts'][0].update(value='0x38'),
 'wrong_native_size_return':lambda d:d['instruction_evidence']['0x3701a4'].update(instruction_bytes='b831000000'),
 'wrong_instruction_file_position':lambda d:d['instruction_evidence']['0x3701a4'].update(file_byte_offset='0x3741a5'),
 'wrong_class_counter_geometry':lambda d:d['native_constraints'].update(FFieldClass_Counter=0x3c),
 'wrong_root_size_query_target':lambda d:d['root_vtable_relocation_evidence'][0].update(r_addend='0x3701b0'),
 'omit_measured_size_constraint':lambda d:d['native_constraints'].pop('sizeof_FField'),
}
out=own/'negative-controls';out.mkdir(exist_ok=True);rows=[]
for name,mutate in controls.items():
 d=copy.deepcopy(base);mutate(d);p=out/(name+'.json');p.write_text(json.dumps(d,indent=2)+'\n');r=subprocess.run([sys.executable,str(own/'audit.py'),str(p)],cwd=root,text=True,capture_output=True)
 assert r.returncode!=0 and 'AssertionError' in r.stderr,(name,r.returncode,r.stderr)
 log=out/(name+'.log');log.write_text(r.stderr);rows.append({'name':name,'refused':True,'exit_code':r.returncode,'mutation_sha256':sha(p),'log_sha256':sha(log)})
r=subprocess.run([sys.executable,str(own/'audit.py')],cwd=root,text=True,capture_output=True);assert r.returncode==0,r.stderr
(own/'AUDIT-RUN.log').write_text(r.stdout+r.stderr)
manifest=json.loads((source/'SOURCE-HASHES.json').read_text())
for name,row in manifest['files'].items():assert sha(source/name)==row['sha256'],name
result={'pass':True,'negative_controls':rows,'original_after_controls':'PASS','fact_source_sha256':sha(fact),'producer_frozen_files_unchanged':True,'audit_script_sha256':sha(own/'audit.py'),'controls_script_sha256':sha(__file__)}
(own/'CONTROL-RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':True,'negative_refusals':len(rows),'original_after_controls':'PASS'}))
