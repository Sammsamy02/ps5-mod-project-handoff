#!/usr/bin/env python3
"""Actual Mono CLI --interpreter proof of real Probe; not production AOT-only mode."""
import sys
sys.dont_write_bytecode=True
import subprocess,json,hashlib
from build_probe import BASE,HOST,environment
from run_mono_host_check import ROOT

def main():
 h=BASE/'host-check';env=environment();env.update({'MONO_PATH':str(ROOT/'lib/mono/4.5'),'MONO_CFG_DIR':str(ROOT/'etc'),'MONO_GAC_PREFIX':str(ROOT),'MONO_CRASH_NOFILE':'1'})
 library=h/'libcap10-mono-test.dylib'
 subprocess.run(['/usr/bin/clang','-arch','x86_64','-dynamiclib','-O2','-o',str(library),str(h/'native-mono-test.c')],cwd=BASE,env=env,check=True)
 exe=h/'MonoInterpreterHarness.exe';ref=BASE/'dependencies/mono-references/v4.7.2/mscorlib.dll'
 subprocess.run([str(HOST/'dotnet'),str(HOST/'sdk/10.0.401/Roslyn/bincore/csc.dll'),'/nologo','/noconfig','/nostdlib+','/langversion:7.3','/deterministic+','/debug-','/target:exe','/out:'+str(exe),'/reference:'+str(ref),str(h/'MonoInterpreterHarness.cs')],cwd=BASE,env=env,check=True)
 (BASE/'managed/Probe.dll.config').write_text('<configuration><dllmap dll="__Internal" target="'+str(library)+'" /></configuration>\n')
 (h/'MonoInterpreterHarness.exe.config').write_text('<configuration><dllmap dll="cap10-host-test" target="'+str(library)+'" /></configuration>\n')
 args=['/usr/bin/arch','-x86_64',str(ROOT/'bin/mono-sgen64'),'--interpreter',str(exe),str(BASE/'managed/Probe.dll')]
 result=subprocess.run(args,cwd=BASE,env=env,capture_output=True,text=True,timeout=45)
 (BASE/'evidence/MONO-INTERPRETER-CHECK.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError('actual Mono interpreter check failed; see receipt')
 proof=json.loads(result.stdout.strip().splitlines()[-1]);proof.update({'command':args,'assembly_sha256':hashlib.sha256((BASE/'managed/Probe.dll').read_bytes()).hexdigest(),'harness_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'native_library_sha256':hashlib.sha256(library.read_bytes()).hexdigest()})
 (BASE/'evidence/MONO-INTERPRETER-CHECK.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
