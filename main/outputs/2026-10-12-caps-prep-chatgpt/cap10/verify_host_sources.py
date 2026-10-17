"""Read pinned host sources and verify hashes/API text only. No fetching/building."""
from pathlib import Path
import hashlib,json
from probe_preflight import validate_host_plan
base=Path(__file__).resolve().parent
plan=json.loads((base/'HOST-PLAN.json').read_text());receipt=json.loads((base/plan['source_receipt']).read_text());sources={};errors=[]
for name,record in receipt['files'].items():
    data=(base/'upstream'/name).read_bytes()
    if hashlib.sha256(data).hexdigest()!=record['sha256']:errors.append('source hash mismatch '+name)
    sources[name]=data.decode('utf8')
errors+=validate_host_plan(plan,sources)
print(json.dumps({'passed':not errors,'files_checked':len(sources),'required_apis_checked':len(plan['required_apis']),'errors':errors,'label':'Mac-only source integrity/API presence; target host absent'},indent=2))
raise SystemExit(bool(errors))
