#!/usr/bin/env python3
"""Real matching Mono6.12 public API/boundary test; stock host, not strict owner/PS5 proof."""
import sys
sys.dont_write_bytecode=True
import os,subprocess,json,hashlib
from pathlib import Path
from build_probe import BASE,HOST,environment
ROOT=BASE.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
def main():
 h=BASE/'host-check';env=environment();env.pop('MONO_PATH',None);env.update({'MONO_CFG_DIR':str(ROOT/'etc'),'MONO_GAC_PREFIX':str(ROOT),'MONO_CRASH_NOFILE':'1'})
 config_probe=h/'ConfigProbe.dll'
 subprocess.run([str(HOST/'dotnet'),str(HOST/'sdk/10.0.401/Roslyn/bincore/csc.dll'),'/nologo','/noconfig','/nostdlib+','/langversion:7.3','/deterministic+','/debug-','/target:library','/out:'+str(config_probe),'/reference:'+str(BASE/'dependencies/mono-references/v4.7.2/mscorlib.dll'),str(h/'ConfigProbe.cs')],cwd=BASE,env=env,check=True)
 args=['/usr/bin/clang','-arch','x86_64','-c','-O2','-o',str(h/'native-mono-test.o'),str(h/'native-mono-test.c')]
 subprocess.run(args,cwd=BASE,env=env,check=True)
 compile=['/usr/bin/clang++','-arch','x86_64','-std=c++17','-O1','-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1','-I'+str(BASE/'native'),'-I'+str(h),'-I'+str(BASE/'dependencies/mono-source'),'-I'+str(ROOT/'include/mono-2.0'),'-I/opt/homebrew/include/glib-2.0','-I/opt/homebrew/lib/glib-2.0/include',str(BASE/'native/InvocationBoundary.cpp'),str(h/'MonoHarness.cpp'),str(h/'native-mono-test.o'),str(ROOT/'lib/libmonosgen-2.0.1.dylib'),'-Wl,-export_dynamic','-Wl,-headerpad_max_install_names','-o',str(h/'mono-harness')]
 result=subprocess.run(compile,cwd=BASE,env=env,text=True,capture_output=True);(BASE/'evidence/MONO-HOST-COMPILER.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError('Mono host compiler failed; see receipt')
 subprocess.run(['/usr/bin/install_name_tool','-change','/Library/Frameworks/Mono.framework/Versions/6.12.0/lib/libmonosgen-2.0.1.dylib',str(ROOT/'lib/libmonosgen-2.0.1.dylib'),str(h/'mono-harness')],cwd=BASE,env=env,check=True)
 run=subprocess.run(['/usr/bin/arch','-x86_64',str(h/'mono-harness'),str(ROOT/'lib'),str(ROOT/'etc'),str(config_probe),str(BASE/'managed/Probe.dll')],cwd=BASE,env=env,text=True,capture_output=True,timeout=45)
 (BASE/'evidence/MONO-HOST-CHECK.log').write_text(run.stdout+run.stderr)
 if run.returncode:raise RuntimeError('Real matching Mono check failed; see receipt')
 proof=json.loads(run.stdout.strip().splitlines()[-1]);proof['runtime']='Mono6.12.0.206 / 0cbf0e290c3 amd64 on Mac/Rosetta';proof['harness_sha256']=hashlib.sha256((h/'mono-harness').read_bytes()).hexdigest();proof['compiler_command']=compile
 proof.update({'relocated_config_file_io_pass':True,'mono_path_override_present':False,'assembly_prefix':str(ROOT/'lib'),'config_prefix':str(ROOT/'etc'),'source_sha256':{str(p.relative_to(BASE)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [BASE/'native/InvocationBoundary.cpp',h/'MonoHarness.cpp',h/'ConfigProbe.cs']}})
 (BASE/'evidence/MONO-HOST-CHECK.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
