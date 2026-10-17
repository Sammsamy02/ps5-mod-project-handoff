from pathlib import Path
import argparse,hashlib,json,os,subprocess
H=Path(__file__).resolve().parents[1];W=H/'cross-work';TOOLS=H.parent/'cap10-mono-build/build-tools'
p=argparse.ArgumentParser();p.add_argument('stage',choices=['configure','build']);p.add_argument('--label',default='BUILD');p.add_argument('--part',action='append');args=p.parse_args()
env=os.environ.copy()
for k in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE','CONFIG_SITE','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','LIBS']:env.pop(k,None)
env.update(PATH=str(TOOLS/'bin')+':/usr/bin:/bin:/usr/sbin:/sbin',CC='/usr/bin/clang -arch x86_64',CXX='/usr/bin/clang++ -arch x86_64',CFLAGS='-O1',CXXFLAGS='-O1',LDFLAGS='-arch x86_64',MONO_BRANCH='pinned-public-source',MONO_BUILD_REVISION='0cbf0e290c31adb476f9de0fa44b1d8829affa40')
def run(label,cmd,cwd=W):
 log=H/'logs'/f'{label}.log'
 with log.open('x') as f:result=subprocess.run(cmd,cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT)
 record={'command':cmd,'cwd':str(cwd),'exit_code':result.returncode,'environment':{k:env[k] for k in ['PATH','CC','CXX','CFLAGS','CXXFLAGS','LDFLAGS','MONO_BRANCH','MONO_BUILD_REVISION']},'max_build_jobs':1,'target_execution':False,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()}
 (H/'evidence'/f'{label}.json').write_text(json.dumps(record,indent=2)+'\n');print(label,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
if args.stage=='configure':
 run('AUTOCONF',[str(TOOLS/'bin/autoconf')])
 run('CONFIGURE',['./configure','--prefix='+str(H/'cross-install'),'--build=x86_64-apple-darwin24','--host=x86_64-apple-darwin24','--target=x86_64-sie-ps5','--with-cross-offsets='+str(W/'x86_64-sie-ps5.h'),'--disable-mcs-build','--disable-boehm','--with-sgen=yes','--with-tls=pthread','--disable-btls','--disable-btls-lib','--disable-nls','--disable-support-build','--with-ikvm-native=no','--with-mcs-docs=no','--disable-system-aot','--disable-dtrace','--disable-llvm'])
else:
 for part in (args.part or ['eglib','arch','utils','cil','zlib','sgen','metadata','mini']):run(args.label+'-'+part.upper(),['/usr/bin/make','-j1','V=1'],W/'mono'/part)
