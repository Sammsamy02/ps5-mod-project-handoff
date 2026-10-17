from pathlib import Path
import hashlib,json
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
for k in ['selection','provider','c_header','handoff','report']:check(f[k])
commands=json.loads((w/'target/commands.json').read_text());assert all(r['exit']==0 for r in commands)
assert 'main-checks=8 failures=0' in (w/'tests/GREEN-controls/run.log').read_text()
assert 'PASS postconstructor call refuses with -1' in (w/'tests/GREEN-controls/run.log').read_text()
assert 'main-checks=8 failures=4' in (w/'tests/RED-controls/run.log').read_text()
assert 'FAIL postconstructor call refuses with -1' in (w/'tests/RED-controls/run.log').read_text()
for name,status in [('DEFINED',' T '),('C-UNDEFINED',' U ')]:
 assert len([l for l in (w/'target'/(name+'.stdout')).read_text().splitlines() if l.startswith('bridge_native_thread_authority_retire'+status)])==1
assert not f['native_execution'] and not f['outer_wiring'] and not f['independent_acceptance_claim'] and not f['selected_source_changes']
print(json.dumps(dict(pass_=True,owned_pins=len(s['owned_files']),external_pins=len(s['external_files']),host_controls=9,target_compiles=2,native_execution=False,outer_wiring=False,independent_acceptance=False)))
