#!/usr/bin/env python3
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
label=sys.argv[1] if len(sys.argv)>1 else 'GREEN'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-XOM-'+label+'-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
rows=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 exe=run/mode;cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',*extra,'-I',str(root/'units'),str(root/'tests/test_xom_signature.cpp'),'-o',str(exe)]
 c=subprocess.run(cmd,capture_output=True,text=True);row={'mode':mode,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr}
 if c.returncode==0:
  r=subprocess.run([str(exe)],capture_output=True,text=True);row.update(run_exit=r.returncode,stdout=r.stdout,stderr=r.stderr)
 rows.append(row);print(mode,row.get('run_exit',c.returncode),row.get('stdout',c.stderr).strip())
receipt={'scope':'actual NativeFNameChecks header source selection tests; no target execution','runs':rows,'source_hashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'tests/test_xom_signature.cpp',root/'units/NativeFNameChecks.hpp']}}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'reports'/('XOM-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and r.get('run_exit')==0 for r in rows) else 1)
