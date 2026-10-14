#!/usr/bin/env python3
"""Build genuine pinned Mono host runtime. Source modifications are separate."""
from pathlib import Path
import os,subprocess,json,sys
BASE=Path(__file__).resolve().parents[1]
WORK=BASE/'runtime-work'
def environment():
 env=os.environ.copy()
 for key in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
 env.update(PATH=str(BASE/'build-tools/bin')+':/usr/bin:/bin:/usr/sbin:/sbin',CC='/usr/bin/clang -arch x86_64',CXX='/usr/bin/clang++ -arch x86_64',CFLAGS='-O1',CXXFLAGS='-O1',LDFLAGS='-arch x86_64',MONO_BRANCH='pinned-public-source',MONO_BUILD_REVISION='0cbf0e290c31adb476f9de0fa44b1d8829affa40')
 return env
COMMAND=['./configure','--prefix='+str(BASE/'runtime-install'),'--build=x86_64-apple-darwin24','--host=x86_64-apple-darwin24','--disable-mcs-build','--disable-boehm','--disable-btls','--disable-btls-lib','--disable-nls','--disable-support-build','--with-ikvm-native=no','--with-mcs-docs=no','--disable-system-aot','--disable-dtrace','--disable-llvm']
def run(label,command,cwd):
 with (BASE/'logs'/f'{label}.log').open('w') as out:
  rc=subprocess.call(command,cwd=cwd,env=environment(),stdout=out,stderr=subprocess.STDOUT)
 receipt={'command':command,'cwd':str(cwd),'exit_code':rc,'environment':{k:environment()[k] for k in ['CC','CXX','CFLAGS','CXXFLAGS','LDFLAGS','MONO_BRANCH','MONO_BUILD_REVISION']},'max_make_jobs':2}
 (BASE/'evidence'/f'{label}.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(label,rc,flush=True)
 if rc:raise SystemExit(rc)
if __name__=='__main__':
 if len(sys.argv)==1 or sys.argv[1]=='configure':run('CONFIGURE',COMMAND,WORK)
 else:
  for part in ['eglib','arch','utils','cil','zlib','sgen','metadata','mini']:
   run(sys.argv[1]+'-'+part,['/usr/bin/make','-j2','V=1'],WORK/'mono'/part)
