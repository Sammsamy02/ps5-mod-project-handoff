#!/usr/bin/env python3
"""One dependency diagnostic of the frozen, already-passing real owner executable."""
from pathlib import Path
import os,hashlib,json,subprocess,resource
BASE=Path(__file__).resolve().parents[1];FROZEN=BASE.parent/'cap10-mono-build'
MDK=BASE.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def main():
 receipt=BASE/'evidence/OWNER-DEPENDENCY-TRACE.json'
 if receipt.exists():raise ValueError('One diagnostic already recorded; preserve it')
 previous=json.loads((FROZEN/'evidence/OWNER-CHECK.json').read_text())
 assert previous['verdict']=='REAL_MONO_OWNER_STRICT_HOST_PASS'
 exe=FROZEN/'bin/mono-owner-strict';runtime=FROZEN/'runtime-work/mono/mini/.libs/libmonosgen-2.0.1.dylib';assemblies=FROZEN/'owner-runtime/lib/mono/4.5'
 required={exe:previous['harness_sha256'],runtime:previous['runtime_library_sha256'],assemblies/'mscorlib.dll.dylib':previous['corlib_aot_sha256'],FROZEN/'runtime-work/config.h':previous['config_sha256'],assemblies/'mscorlib.dll':'86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4'}
 for p,expected in required.items():assert sha(p)==expected,str(p)+' drift'
 env=os.environ.copy()
 for key in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
 overlay={'MONO_CFG_DIR':str(MDK/'etc'),'MONO_CRASH_NOFILE':'1','MONO_LOG_LEVEL':'debug','MONO_LOG_MASK':'dll','DYLD_PRINT_LIBRARIES':'1','DYLD_PRINT_APIS':'1','TMPDIR':str(BASE/'tmp')}
 env.update(overlay)
 # Launch the already-built x86_64 Mach-O directly so a protected intermediary
 # cannot strip DYLD_PRINT_* variables. macOS selects Rosetta automatically.
 command=[str(exe)]
 result=subprocess.run(command,cwd=assemblies,env=env,text=True,capture_output=True,timeout=60,preexec_fn=no_core)
 (BASE/'logs/OWNER-DEPENDENCY-TRACE.stdout.log').write_text(result.stdout)
 (BASE/'logs/OWNER-DEPENDENCY-TRACE.stderr.log').write_text(result.stderr)
 proofs=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{"verdict":')]
 outcome=proofs[-1] if proofs else {}
 combined=result.stdout+result.stderr
 record={'verdict':'REAL_OWNER_DEPENDENCY_DIAGNOSTIC_PASS' if result.returncode==0 and outcome.get('completed') else 'REAL_OWNER_DEPENDENCY_DIAGNOSTIC_FAIL','command':command,'cwd':str(assemblies),'exit_code':result.returncode,'environment_overlay':overlay,'no_mono_path_override':True,'owner_result':outcome,'runtime_diagnostics_clean':"assertion '" not in combined,'dyld_trace_present':'dyld[' in result.stderr,'mono_dll_trace_present':'DllImport' in result.stdout,'source_proof':'../cap10-mono-build/evidence/OWNER-CHECK.json','frozen_inputs':[{'path':str(p),'sha256_before':expected,'sha256_after':sha(p)} for p,expected in required.items()],'config_file':{'path':str(MDK/'etc/mono/config'),'sha256':sha(MDK/'etc/mono/config')},'host_only':True,'target_proof':False}
 receipt.write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps(record,indent=2))
 if result.returncode or not outcome.get('completed'):raise SystemExit(1)
if __name__=='__main__':main()
