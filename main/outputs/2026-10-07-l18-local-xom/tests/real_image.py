#!/usr/bin/env python3
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1];workspace=root.parents[1]
image=workspace/'outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-REAL-IMAGE-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
digest=hashlib.sha256()
with image.open('rb') as f:
 while chunk:=f.read(65536):digest.update(chunk)
assert digest.hexdigest()=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
rows=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 exe=run/mode;cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',*extra,'-I',str(root/'units'),str(root/'tests/test_real_image.cpp'),'-o',str(exe)]
 c=subprocess.run(cmd,capture_output=True,text=True);row={'mode':mode,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr}
 if c.returncode==0:
  argv=[str(exe),str(image)];r=subprocess.run(argv,capture_output=True,text=True,timeout=60);row.update(run_argv=argv,run_exit=r.returncode,stdout=r.stdout,stderr=r.stderr)
 rows.append(row);print(mode,row.get('run_exit',c.returncode),row.get('stdout',c.stderr),row.get('stderr',''))
receipt={'scope':'actual retained image through production C++ Cache::prepare with host public file boundary; Mac timings only','independent_python_sha256':digest.hexdigest(),'image_size':image.stat().st_size,'runs':rows}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'receipts/REAL-IMAGE.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and r.get('run_exit')==0 for r in rows) else 1)
