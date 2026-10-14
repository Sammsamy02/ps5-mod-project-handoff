#!/usr/bin/env python3
"""Real configured Mono loader regression: same native tests before/after exact patch."""
from pathlib import Path
import hashlib,json,subprocess,sys,os,resource,shutil
BASE=Path(__file__).resolve().parents[1];WORK=BASE/'runtime-work'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def main(label):
 strict=label=='PATCHED'; records=[]; commands=[]
 archive=(WORK/'mono/mini/.libs/libmonosgen-2.0.a') if strict else (BASE/'baseline-runtime/libmonosgen-2.0.a')
 loader_source=(WORK/'mono/utils/mono-dl.c') if strict else (BASE/'sources/mono-0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/utils/mono-dl.c')
 if not archive.is_file():raise ValueError('Full genuine runtime archive required')
 binary=BASE/'bin'/('loader-'+label.lower())
 def compile(command):
  commands.append(command)
  result=subprocess.run(command,cwd=BASE,text=True,capture_output=True)
  with (BASE/'logs'/('LOADER-LINK-'+label+'.log')).open('a') as out:out.write(result.stdout+result.stderr)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 fixture=BASE/'fixtures/ordinary-ambient.dylib'
 compile(['/usr/bin/clang','-arch','x86_64','-Wall','-Wextra','-Werror','-dynamiclib',str(BASE/'tests/ambient-library.c'),'-o',str(fixture)])
 reserved=BASE/'fixtures/modbridge-owned:ambient.dylib'
 prefixed=BASE/'fixtures/libmodbridge-owned:ambient.so'
 shutil.copyfile(fixture,reserved);shutil.copyfile(fixture,prefixed)
 la=BASE/'fixtures/modbridge-owned:libtool.la'
 la.write_text("dlname='ordinary-ambient.dylib'\ninstalled=yes\nlibdir='"+str(BASE/'fixtures')+"'\n")
 la_base=BASE/'fixtures/libmodbridge-owned:implicit'
 Path(str(la_base)+'.la').write_text(la.read_text())
 compile(['/usr/bin/clang','-arch','x86_64','-std=gnu99','-Wall','-Wextra','-Werror','-I'+str(WORK),'-I'+str(WORK/'mono/eglib'),str(BASE/'tests/strict-loader.c'),str(archive),'-lstdc++','-liconv','-lz','-framework','CoreFoundation','-framework','Security','-framework','Foundation','-o',str(binary)])
 cases=[('direct-owned','deny',reserved),('prefixed-owned','deny',prefixed),('libtool-owned','deny',la),('implicit-libtool-owned','deny',la_base),('ordinary-control','normal',fixture),('provider-priority','provider',reserved),('unregister-head','unregister',BASE/'fixtures/modbridge-owned:missing-unregister')]
 for case,mode,path in cases:
  marker=BASE/'fixtures'/(label+'-'+case+'.marker')
  if marker.exists():marker.unlink()
  env=os.environ.copy();env.update(CAP10_AMBIENT_MARKER=str(marker),MONO_CRASH_NOFILE='1')
  for key in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
  command=['/usr/bin/arch','-x86_64',str(binary),mode,str(path)]
  result=subprocess.run(command,cwd=BASE,env=env,text=True,capture_output=True,timeout=20,preexec_fn=no_core)
  out=result.stdout+result.stderr
  (BASE/'logs'/(label+'-'+case+'.log')).write_text(out)
  expected=(result.returncode==0) if strict or mode=='normal' else (result.returncode!=0)
  records.append({'case':case,'mode':mode,'command':command,'exit_code':result.returncode,'expected_outcome_observed':expected,'ambient_constructor_ran':marker.exists(),'stdout':result.stdout,'stderr':result.stderr})
  print(label,case,result.returncode,'expected='+str(expected),flush=True)
 receipt={'verdict':label+'_REAL_RUNTIME_LOADER_'+('PASS' if all(x['expected_outcome_observed'] for x in records) else 'FAIL'),'strict_patch_applied':strict,'upstream_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','genuine_full_runtime_archive':str(archive),'archive_sha256':sha(archive),'loader_source_sha256':sha(loader_source),'config_sha256':sha(WORK/'config.h'),'test_source_sha256':sha(BASE/'tests/strict-loader.c'),'fixture_source_sha256':sha(BASE/'tests/ambient-library.c'),'test_executable_sha256':sha(binary),'compile_commands':commands,'cases':records,'target_proof':False,'host_only':True,'mock_or_internal_header_stub':False}
 (BASE/'evidence'/('LOADER-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
 if not all(x['expected_outcome_observed'] for x in records):raise SystemExit(1)
if __name__=='__main__':main(sys.argv[1])
