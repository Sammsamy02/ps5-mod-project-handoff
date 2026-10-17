#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,hashlib,sys
root=Path(__file__).resolve().parents[1]
text=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text()
a=text.index('// ---- HOST-TEST-EXTRACT-BEGIN');b=text.index('// Legacy explicit diagnostics only.',a)
(root/'tests/sink_extract.inc').write_text(text[a:b].replace('std::chrono::steady_clock::now()', 'TestClock::now()'))
c=text.index('extern \"C\" int modbridge_runtime_stop(void)');d=text.index('extern \"C\" int modbridge_runtime_start_with_activation',c)
(root/'tests/stop_extract.inc').write_text(text[c:d])
rows=[]
for name,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 cmd=['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-O1',*extra,'-I',str(root/'units'),str(root/'tests/test_retained.cpp'),'-o',str(root/'tests'/name)]
 c=subprocess.run(cmd,capture_output=True,text=True)
 r=subprocess.run([str(root/'tests'/name)],capture_output=True,text=True,timeout=30) if c.returncode==0 else c
 print(name,r.returncode);print(r.stdout);print(r.stderr)
 rows.append({'name':name,'compile_argv':cmd,'compile_exit':c.returncode,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'cases':r.stdout.count('PASS ')})
(root/'receipts'/('HOST-'+(sys.argv[1] if len(sys.argv)>1 else 'GREEN')+'.json')).write_text(json.dumps({'source_sha256':hashlib.sha256(text.encode()).hexdigest(),'runs':rows},indent=2))
sys.exit(1 if any(r['exit'] for r in rows) else 0)
