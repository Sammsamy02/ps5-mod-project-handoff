#!/usr/bin/env python3
"""The complete isolated l17 host verification command; no console/network."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-ALL-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
commands=[['run_tests.py','GREEN'],['run_host_tests.py'],['initializer_integration.py'],['lifecycle_tests.py','GREEN'],['initializer_lifecycle.py','GREEN']]
rows=[]
for script,*args in commands:
 cmd=[sys.executable,str(root/'tests'/script),*args]
 r=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
 rows.append({'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(script,r.returncode)
 if r.returncode:print(r.stdout,r.stderr)
hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ['units','tests'] for p in (root/d).iterdir() if p.is_file() and p.suffix in ('.cpp','.hpp','.h','.py')}
old=json.loads((root/'receipts/INPUTS.json').read_text())['frozen_inputs']
frozen=all(hashlib.sha256((root.parents[1]/p).read_bytes()).hexdigest()==h for p,h in old.items())
passed=all(r['exit']==0 for r in rows) and frozen
receipt={'pass':passed,'scope':'all isolated l17 host suites, actual extracted initializer and selected owner; no target execution or console contact','frozen_inputs_unchanged':frozen,'source_hashes':hashes,'runs':rows}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'receipts/HOST-ALL.json').write_text(json.dumps(receipt,indent=2)+'\n');print('all_pass',passed,'frozen_inputs_unchanged',frozen);sys.exit(0 if passed else 1)
