#!/usr/bin/env python3
"""R4 data-only four-fact promotion; immutable audit identities are mandatory."""
import hashlib,importlib.util,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];W=ROOT.parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
total=W/'outputs/2026-10-07-l19-r4-total-audit/AUDIT.json';slots=W/'outputs/2026-10-07-l19-r4-slot-audit/AUDIT.json'
assert sha(total)=='14e6b434c755d7a297f867f76c47032055f0f15ae37cd3395e20a4d7a01fb834'
assert sha(slots)=='8d94ce15249ad99997df76bb57d1a1c6ccd524694defd0c3c03b8f8d9bd1f1dc'
audits=[json.loads(p.read_text()) for p in (total,slots)]
assert all(a['pass'] for a in audits)
profile_path=ROOT/'profiles/townfall-build-facts.json'
baseline=W/'outputs/2026-10-07-l19-local-profile-r3/profiles/townfall-build-facts.json'
assert sha(baseline)=='66dc76fd41c96dc2faf9bc02bc098aaace20d05a0793fd633a81d977588c29af'
p=json.loads(baseline.read_text())
image_path=W/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
image=image_path.read_bytes();assert hashlib.sha256(image).hexdigest()==p['image']['sha256'] and len(image)==p['image']['size']
phoff=struct.unpack_from('<Q',image,32)[0];entry,count=struct.unpack_from('<HH',image,54)
loads=[struct.unpack_from('<IIQQQQQQ',image,phoff+i*entry) for i in range(count)]
evidence=[];promoted=[]
expected={'FField.UEP_TotalSize':'0x30','FFieldClass.UEP_TotalSize':'0x40','UEngine.LoadMap.vtable_byte_offset':'0x4f8','AGameModeBase.InitGameState.vtable_byte_offset':'0x7b8'}
for audit,path,sourcekey in zip(audits,(total,slots),('accepted_fact_source','fact_receipt')):
    source=audit[sourcekey];receipt=Path(source['path'])
    if not receipt.is_absolute():receipt=W/receipt
    assert sha(receipt)==source['sha256'];facts=json.loads(receipt.read_text())
    assert facts['image']['sha256']==p['image']['sha256']
    for r in facts.get('instruction_evidence',{}).values():
        addr=int(r['elf_image_vaddr'],0);off=int(r['file_byte_offset'],0);raw=bytes.fromhex(r['instruction_bytes'])
        assert image[off:off+len(raw)]==raw
        found=[s for s in loads if s[0]==1 and s[3]<=addr and addr+len(raw)<=s[3]+s[5]]
        assert len(found)==1 and off==found[0][2]+addr-found[0][3]
    for fact in audit['accepted_facts']:
        assert expected[fact['key']]==fact['value'] and fact['status']=='validated-static' and not fact['live_validation']
        kind='required_vtables' if fact['key'].endswith('.vtable_byte_offset') else 'required_members'
        name=fact['key'].removesuffix('.vtable_byte_offset').replace('.', '::',1)
        pin=next(x for x in p[kind] if x['name']==name)
        assert pin['offset'] is None and not pin['source']
        pin.update(offset=int(fact['value'],0),width=fact['width_bytes'],source=str(receipt.relative_to(W))+':'+fact['key'])
        promoted.append({'name':name,'value':pin['offset'],'width':pin['width'],'confidence':fact['confidence'],'audit_path':str(path),'audit_sha256':sha(path),'fact_receipt_path':str(receipt),'fact_receipt_sha256':sha(receipt)})
    evidence.append({'audit_path':str(path),'audit_sha256':sha(path),'fact_receipt_path':str(receipt),'fact_receipt_sha256':sha(receipt)})
assert len(promoted)==4 and len(p['required_members'])==69 and len(p['required_vtables'])==7
assert all(r['offset'] is not None and r['source'] for k in ('required_members','required_vtables') for r in p[k])
assert not p['unknown_required_static_facts']
p['evidence']['r4_accepted_native_fact_audits']=evidence
p['evidence']['r4_total_confidence']='high-confidence validated-static source-contract-correlated inference; exact game source/platform preset qualification preserved; no direct native allocation/live sizeof measurement'
profile_path.write_text(json.dumps(p,indent=2)+'\n')
spec=importlib.util.spec_from_file_location('typed_profile_generator',ROOT/'profiles/generate_profile.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
digest=module.generate(p)
result={'pass':True,'scope':'Audited data-only four-fact promotion, authenticated saved image; no runtime or live game proof','profile_sha256':sha(profile_path),'profile_digest':digest,'profile_header_sha256':sha(ROOT/'units/RuntimeBuildProfile.hpp'),'required_members':69,'known_members':69,'required_vtables':7,'known_vtables':7,'unknown_required_static_facts':[],
        'promoted_facts':promoted,'image_sha256':p['image']['sha256'],'Owner_native_storage_and_reviewed_read_bytes':8,'Owner_public_binding_footprint_bytes':16,'width_provider_changed':False,
        'source_hashes':{str(q):sha(q) for q in (Path(__file__),ROOT/'profiles/generate_profile.py',baseline,total,slots)}}
(ROOT/'reports/AUDITED-PROFILE-GREEN.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('profile_sha256','profile_digest','known_members','known_vtables','unknown_required_static_facts')}))
