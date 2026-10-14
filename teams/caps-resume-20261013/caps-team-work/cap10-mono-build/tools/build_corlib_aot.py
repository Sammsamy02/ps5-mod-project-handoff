#!/usr/bin/env python3
"""Use the newly built genuine patched host compiler on a hash-bound MDK corlib copy."""
from pathlib import Path
import os,subprocess,shutil,hashlib,json,resource
BASE=Path(__file__).resolve().parents[1]
MDK=BASE.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def main():
 source=MDK/'lib/mono/4.5/mscorlib.dll'; expected='86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4'
 assert sha(source)==expected
 dest=BASE/'owner-runtime/lib/mono/4.5';dest.mkdir(parents=True,exist_ok=True)
 target=dest/'mscorlib.dll';aot=dest/'mscorlib.dll.dylib'
 if aot.exists():raise ValueError('existing AOT output preserved')
 shutil.copyfile(source,target)
 env=os.environ.copy();env.update(MONO_PATH=str(dest),MONO_CFG_DIR=str(MDK/'etc'),MONO_CRASH_NOFILE='1')
 env.pop('MONO_ENV_OPTIONS',None);env.pop('MONO_AOT_CACHE',None)
 exe=BASE/'runtime-work/mono/mini/mono-sgen'
 command=['/usr/bin/arch','-x86_64',str(exe),'--aot=full,interp',str(target)]
 with (BASE/'logs/OWNER-CORLIB-AOT.log').open('w') as out: result=subprocess.run(command,cwd=dest,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=180,preexec_fn=no_core)
 receipt={'command':command,'exit_code':result.returncode,'compiler_sha256':sha(exe),'source_sha256':sha(source),'copied_source_sha256':sha(target),'aot_sha256':sha(aot) if aot.exists() else None,'aot_bytes':aot.stat().st_size if aot.exists() else 0,'max_aot_threads':1,'darwin_host_only':True}
 (BASE/'evidence/OWNER-CORLIB-AOT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
 if result.returncode or not aot.exists():raise SystemExit(1)
if __name__=='__main__':main()
