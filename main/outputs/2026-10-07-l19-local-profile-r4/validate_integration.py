"""Read-only reproducible verification of the sealed R4 source/evidence/receipt graph."""
from pathlib import Path
import hashlib,json,re
O=Path(__file__).resolve().parent;W=O.parents[1]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
s=json.loads((O/'reports/SOURCE-SELECTION.json').read_text());r=json.loads((O/'reports/FINAL-RECEIPT.json').read_text())
assert r['pass'] and r['source_selection_sha256']==sha(O/'reports/SOURCE-SELECTION.json')
for path,pin in s['source_files'].items():assert sha(O/path)==pin,path
for path,pin in s['evidence_files'].items():assert sha(W/path)==pin,path
for name,pin in r['receipt_hashes'].items():assert sha(O/'reports'/name)==pin,name
p=json.loads((O/'profiles/townfall-build-facts.json').read_text());d=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
assert sha(O/'profiles/townfall-build-facts.json')==r['profile_sha256'] and d==r['profile_digest']
assert re.search(r'kDigest="([0-9a-f]+)"',(O/'units/RuntimeBuildProfile.hpp').read_text())[1]==d
assert d in (O/'units/CompiledNativeLayoutDeclarations.hpp').read_text()
assert len(p['required_members'])==69 and len(p['required_vtables'])==7 and all(x['offset'] is not None for x in p['required_members']+p['required_vtables'])
c=json.loads((O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text());assert c['exit']==0 and len(c['observed_widths'])==54 and c['observed_widths']['FField::Owner']==16 and c['accepted_owner_header_dependency_verified']
for path,pin in json.loads((O/'reports/TARGET-DEPENDENCY-MANIFEST.json').read_text())['files'].items():assert sha(path)==pin,path
print(json.dumps({'pass':True,'source_files':len(s['source_files']),'evidence_files':len(s['evidence_files']),'members':69,'vtables':7,'actual_public_widths':54,'final_sha256':sha(O/'reports/FINAL-RECEIPT.json')}))
