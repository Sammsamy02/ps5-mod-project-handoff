#!/usr/bin/env python3
"""Build public GNU tooling into this experiment only, at most two make jobs."""
from pathlib import Path
import os, subprocess, json, time
BASE=Path(__file__).resolve().parents[1]
prefix=BASE/'build-tools'
env=os.environ.copy(); env['PATH']=str(prefix/'bin')+':/usr/bin:/bin:/usr/sbin:/sbin'
records=[]
for name in ['autoconf-2.69','automake-1.16.5','libtool-2.4.7']:
 src=BASE/'sources'/name
 for label,cmd in [('configure',['./configure','--prefix='+str(prefix)]),('make',['/usr/bin/make','-j2']),('install',['/usr/bin/make','install'])]:
  log=BASE/'logs'/(name+'-'+label+'.log')
  with log.open('w') as out:
   rc=subprocess.call(cmd,cwd=src,env=env,stdout=out,stderr=subprocess.STDOUT)
  records.append({'dependency':name,'step':label,'command':cmd,'cwd':str(src),'exit_code':rc,'log':str(log.relative_to(BASE))})
  (BASE/'evidence/BUILD-TOOLS.json').write_text(json.dumps(records,indent=2)+'\n')
  print(name,label,rc,flush=True)
  if rc: raise SystemExit(rc)
