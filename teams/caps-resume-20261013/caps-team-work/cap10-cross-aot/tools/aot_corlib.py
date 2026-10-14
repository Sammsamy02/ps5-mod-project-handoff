"""Single-thread cross-AOT corlib, then genuine public target assembly to ELF."""
from pathlib import Path
import hashlib,json,os,resource,shutil,subprocess
H=Path(__file__).resolve().parents[1];W=H/'cross-work'
source=H.parent/'cap10/host-check/owner-runtime/lib/mono/4.5/mscorlib.dll'
expected='86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4'
compiler=W/'mono/mini/mono-sgen';dest=H/'managed-input';dest.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
if sha(source)!=expected:raise ValueError('corlib source hash drift')
managed=dest/'mscorlib.dll'
if managed.exists():raise ValueError('existing managed input preserved')
shutil.copyfile(source,managed)
env=os.environ.copy()
for key in ['MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
mdk=H.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
env.update(MONO_PATH=str(dest),MONO_CFG_DIR=str(mdk/'etc'),MONO_CRASH_NOFILE='1')
assembly=H/'artifacts/mscorlib.full-interp.ps5.s';obj=H/'artifacts/mscorlib.full-interp.ps5.o'
if assembly.exists() or obj.exists():raise ValueError('existing AOT outputs preserved')
commands=[]
def run(label,command,extra=None):
 with (H/'logs'/f'{label}.log').open('x') as f:r=subprocess.run(command,cwd=dest,env=env,stdout=f,stderr=subprocess.STDOUT,preexec_fn=no_core)
 record={'stage':label,'command':command,'exit_code':r.returncode};commands.append(record)
 (H/'evidence/AOT-PROGRESS.json').write_text(json.dumps({'commands':commands,'compiler_sha256':sha(compiler),'managed_source_sha256':sha(managed),'max_aot_threads':1,'target_execution':False},indent=2)+'\n')
 print(label,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
run('CROSSLIB-AOT',['/usr/bin/arch','-x86_64',str(compiler),'--aot=full,interp,static,asmonly,outfile='+str(assembly),str(managed)])
inv=json.loads((H.parent/'cap10-port-research/LOCAL-INVENTORY.json').read_text());cc=Path(inv['files'][0]['path']);env['LLVM_CONFIG']=str(Path(inv['llvm_bin'])/'llvm-config')
run('TARGET-ASSEMBLE',[str(cc),'-c',str(assembly),'-o',str(obj)])
with (H/'evidence/AOT-ARTIFACT.json').open('x') as f:
 json.dump({'commands':commands,'source_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','compiler_sha256':sha(compiler),'config_sha256':sha(W/'config.h'),'offsets_sha256':sha(W/'x86_64-sie-ps5.h'),'managed_source_sha256':sha(managed),'assembly_sha256':sha(assembly),'object_sha256':sha(obj),'object_bytes':obj.stat().st_size,'max_aot_threads':1,'target_execution':False,'prx_build_or_package':False},f,indent=2);f.write('\n')
