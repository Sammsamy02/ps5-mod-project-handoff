from pathlib import Path
import json,hashlib,struct
W=Path(__file__).resolve().parent
def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def check(p):
 q=Path(p['path']); assert q.stat().st_size==p['size'] and digest(q)==p['sha256'],q
s=json.loads((W/'SOURCE-SELECTION.json').read_text())
for p in s['owned_files']+s['external_files']:check(p)
f=json.loads((W/'FINAL-RECEIPT.json').read_text())
for k in ['source_selection','facts','report','handoff']:check(f[k])
n=json.loads((W/'NATIVE-NAME-FACTS.json').read_text());check(n['native_evidence'])
ev=json.loads(Path(n['native_evidence']['path']).read_text()); image=Path(n['image']['path'])
for b in n['branches']:
 for key in ['window','initialization_window','manager_window']:
  for address in b.get(key,[]):assert address in ev['instructions'],address
for address in n['caller']['window']:assert address in ev['instructions'],address
assert image.stat().st_size==n['image']['size']
with image.open('rb') as fd:
 header=fd.read(64);assert header[:6]==b'\x7fELF\x02\x01'
 ph=struct.unpack_from('<Q',header,32)[0];pe,pn=struct.unpack_from('<HH',header,54)
 fd.seek(ph);raw=fd.read(pe*pn);ss=[struct.unpack_from('<IIQQQQQQ',raw,i*pe) for i in range(pn)]
 for r in ev['instructions'].values():
  v=int(r['image_vaddr'],16);o=int(r['file_offset'],16);b=bytes.fromhex(r['bytes'])
  hits=[p[2]+v-p[3] for p in ss if p[0]==1 and p[3]<=v and v+len(b)<=p[3]+p[5]]
  assert hits==[o],r;fd.seek(o);assert fd.read(len(b))==b,r
 for d in ev['data']:
  o=int(d['file_offset'],16);b=bytes.fromhex(d['bytes']);fd.seek(o);assert fd.read(len(b))==b
 for seg in ev['segments']:
  v=int(seg['address'],16);m=seg['load'];p=ss[m['index']];assert p[0]==1 and p[1]==m['flags'] and p[3]<=v and v+seg['width']<=p[3]+p[6]
 for a in ['target-wiring-v2/ABI-CENSUS.json','target/ABI-CENSUS.json']:
  assert json.loads((W/a).read_text())['values']==[4,4,8,8,16,8,0,1,33,3]
assert 'checks=20 failures=0' in (W/'tests/GREEN-controls/run.log').read_text()
assert 'checks=21 failures=0' in (W/'tests/BINDING-v2/run.log').read_text()
assert 'kExactPostpackageImportAdmitted=false' in (W/'candidate/ThreadNamePackageAdmission.hpp').read_text()
cpp=(W/'candidate/NativeThreadNameAdapter.cpp').read_text()
assert 'cancellation_->IsCancelled()' in cpp and 'ThrowIfStoppedAt' not in cpp and 'Deadline' not in cpp
assert all(not f[k] for k in ['native_execution','final_package_import_admitted','selected_source_changes','queue_delivery_proved','menu_success_claim'])
print(json.dumps(dict(pass_=True,owned_pins=len(s['owned_files']),external_pins=len(s['external_files']),native_windows=len(ev['instructions']),policy_checks=20,binding_checks=21,target_distinct_tus=8,selected_mutations=0,native_execution=False,import_hold=True)))
