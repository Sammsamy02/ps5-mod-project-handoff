from pathlib import Path
import json,hashlib
w=Path(__file__).resolve().parent
def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def check(p):
 q=Path(p['path']);assert q.stat().st_size==p['size'] and digest(q)==p['sha256'],q
s=json.loads((w/'SOURCE-SELECTION.json').read_text())
for p in s['owned_files']+s['external_files']:check(p)
f=json.loads((w/'FINAL-RECEIPT.json').read_text())
for key in ['selection','handoff','provider','header','sc_proof','target_control']:check(f[key])
h=json.loads((w/'HANDOFF.json').read_text());assert h['c_header']['sha256']==h['identical_prior_header']['sha256']
old=Path(h['supersedes_provider']['path']).read_text();new=Path(h['replacement_provider']['path']).read_text()
expected=old
for order in ['release','acquire','acq_rel']:expected=expected.replace('std::memory_order_'+order,'std::memory_order_seq_cst')
expected=expected.replace('// Trivial, constant-initialized guards outlive the managed owner state.','// Trivial, constant-initialized guards outlive the managed owner state.\n// Both phase and entry count use one SC order; see SC-ORDERING.md.')
assert new==expected
t=json.loads((w/'TARGET-SC-ORDERING.json').read_text());check(t['object']);b=Path(t['object']['path']).read_bytes()
operations=0
for window in t['windows']:
 raw=bytes.fromhex(window['bytes']);assert b[window['file_offset']:window['file_offset']+window['size']]==raw
 for op in window['operations']:
  at=int(op['instruction_offset'],16);assert raw[at:at+5]==bytes.fromhex(op['bytes']) and op['order_value']==5;operations+=1
assert operations==9
assert 'main-checks=8 failures=0' in (w/'tests/GREEN-controls/run.log').read_text()
assert 'PASS postconstructor' in (w/'tests/GREEN-controls/run.log').read_text()
assert all(r['exit']==0 for r in json.loads((w/'target/commands.json').read_text()))
print(json.dumps(dict(pass_=True,owned=len(s['owned_files']),external=len(s['external_files']),host_controls=9,target_compiles=1,sc_gate_operations=9,header_unchanged=True,helper_unchanged=True,independent_acceptance=False,native_execution=False)))
