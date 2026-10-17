#!/usr/bin/env python3
"""Real C# compile using pinned Mono-selected framework reference; no target build."""
import sys
sys.dont_write_bytecode=True
import os,json,hashlib,subprocess
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
HOST=Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet')
def environment():
 env=os.environ.copy()
 env.update({'DOTNET_ROOT':str(HOST),'DOTNET_CLI_HOME':str(BASE/'cli-home'),'NUGET_PACKAGES':str(BASE/'nuget-cache'),'NUGET_HTTP_CACHE_PATH':str(BASE/'nuget-cache/http'),'XDG_CACHE_HOME':str(BASE/'cli-home/cache'),'XDG_CONFIG_HOME':str(BASE/'cli-home/config'),'TMPDIR':str(BASE/'tmp'),'TMP':str(BASE/'tmp'),'TEMP':str(BASE/'tmp'),'DOTNET_CLI_TELEMETRY_OPTOUT':'1','DOTNET_SKIP_FIRST_TIME_EXPERIENCE':'1','DOTNET_NOLOGO':'1','DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE':'true','DOTNET_CLI_DO_NOT_USE_MSBUILD_SERVER':'1'})
 for d in ['cli-home','nuget-cache','tmp']:(BASE/d).mkdir(exist_ok=True)
 return env
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 compiler=HOST/'sdk/10.0.401/Roslyn/bincore/csc.dll'; ref=BASE/'dependencies/mono-references/v4.7.2/mscorlib.dll';source=BASE/'managed/Probe.cs';output=BASE/'managed/Probe.dll'
 expected=json.loads((BASE/'dependencies/references-receipt.json').read_text())['files'][0]['sha256']
 if sha(ref)!=expected:raise ValueError('Mono-selected reference drift')
 args=[str(HOST/'dotnet'),str(compiler),'/nologo','/noconfig','/nostdlib+','/target:library','/langversion:7.3','/optimize+','/deterministic+','/debug-','/platform:anycpu','/out:'+str(output),'/reference:'+str(ref),str(source)]
 result=subprocess.run(args,cwd=BASE,env=environment(),text=True,capture_output=True)
 (BASE/'evidence/COMPILER.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError('C# compiler failed; see COMPILER.log')
 import assembly_preflight
 contract=assembly_preflight.inspect(output.read_bytes())
 receipt={'label':'actual Roslyn compilation; Mono-selected .NET Framework4.7.2 reference; no target runtime execution','source_sha256':sha(source),'compiler_sha256':sha(compiler),'reference_sha256':sha(ref),'compiled_sha256':sha(output),'compiled_bytes':output.stat().st_size,'compiler_command':args,'contract':contract,'reference_submodule_commit':'e68046d5106aa0349c23f95821456955fc15b96b','mono_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','target_execution':False}
 (BASE/'evidence/COMPILE.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
