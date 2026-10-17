"""Explain the eight skipped methods in a separate diagnostic AOT receipt."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
H=Path(__file__).resolve().parents[1];obj=H/'artifacts/mscorlib.full-interp.ps5.o';compiler=H/'cross-work/mono/mini/mono-sgen';managed=H/'managed-input/mscorlib.dll';assembly=H/'artifacts/mscorlib.skip-audit.ps5.s'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
primary=sha(obj);assert not assembly.exists()
env=os.environ.copy()
for k in ['MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(k,None)
env.update(MONO_PATH=str(managed.parent),MONO_CFG_DIR=str(H.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0/etc'),MONO_CRASH_NOFILE='1')
cmd=['/usr/bin/arch','-x86_64',str(compiler),'--aot=full,interp,static,asmonly,print-skipped-methods,stats,outfile='+str(assembly),str(managed)]
with (H/'logs/AOT-SKIPPED-METHODS.log').open('x') as f:r=subprocess.run(cmd,cwd=managed.parent,env=env,stdout=f,stderr=subprocess.STDOUT,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
assert r.returncode==0,r.returncode
log=(H/'logs/AOT-SKIPPED-METHODS.log').read_text();skipped=[line for line in log.splitlines() if line.startswith('Skip (')]
assert sha(obj)==primary
record={'command':cmd,'exit_code':r.returncode,'compiler_sha256':sha(compiler),'managed_sha256':sha(managed),'primary_object_sha256':primary,'primary_object_preserved':True,'diagnostic_assembly_sha256':sha(assembly),'skipped_methods':skipped,'counts':[line for line in log.splitlines() if line.startswith('Compiled:')],'target_execution':False}
with (H/'evidence/AOT-SKIPPED-METHODS.json').open('x') as f:json.dump(record,f,indent=2);f.write('\n')
print(json.dumps({'skipped_methods':skipped,'counts':record['counts']},indent=2))
