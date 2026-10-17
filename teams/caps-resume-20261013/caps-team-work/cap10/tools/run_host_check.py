#!/usr/bin/env python3
"""Actual compiled Probe execution on Mac/CoreCLR; never Mono or Townfall proof."""
import sys
sys.dont_write_bytecode=True
import subprocess,json,hashlib
from pathlib import Path
from build_probe import BASE,HOST,environment

def main():
 h=BASE/'host-check';refs=HOST/'packs/Microsoft.NETCore.App.Ref/10.0.12/ref/net10.0'
 args=[str(HOST/'dotnet'),str(HOST/'sdk/10.0.401/Roslyn/bincore/csc.dll'),'/nologo','/noconfig','/nostdlib+','/target:exe','/deterministic+','/debug-','/out:'+str(h/'Harness.dll')]+['/reference:'+str(p) for p in sorted(refs.glob('*.dll'))]+[str(h/'Harness.cs')]
 subprocess.run(args,cwd=BASE,env=environment(),check=True)
 (h/'Harness.runtimeconfig.json').write_text(json.dumps({'runtimeOptions':{'tfm':'net10.0','framework':{'name':'Microsoft.NETCore.App','version':'10.0.12'}}},indent=2)+'\n')
 subprocess.run(['/usr/bin/clang','-dynamiclib','-O2','-o',str(h/'libcap10-host-test.dylib'),str(h/'native-test.c')],cwd=BASE,env=environment(),check=True)
 result=subprocess.run([str(HOST/'dotnet'),str(h/'Harness.dll'),str(BASE/'managed/Probe.dll'),str(h/'libcap10-host-test.dylib')],cwd=BASE,env=environment(),capture_output=True,text=True)
 (BASE/'evidence/HOST-CHECK.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError('real Probe host check failed; see HOST-CHECK.log')
 proof=json.loads(result.stdout)
 proof['harness_sha256']=hashlib.sha256((h/'Harness.dll').read_bytes()).hexdigest();proof['native_test_library_sha256']=hashlib.sha256((h/'libcap10-host-test.dylib').read_bytes()).hexdigest()
 (BASE/'evidence/HOST-CHECK.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
