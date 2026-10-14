#!/usr/bin/env python3
"""Compile and execute real current MonoOwner/InvocationBoundary on the strict host runtime."""
from pathlib import Path
import os,subprocess,hashlib,json,resource
BASE=Path(__file__).resolve().parents[1];CAP=BASE.parent/'cap10';WORK=BASE/'runtime-work'
MDK=BASE.parent/'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def main():
 dest=BASE/'owner-runtime/lib/mono/4.5';lib=WORK/'mono/mini/.libs/libmonosgen-2.0.1.dylib'
 assert sha(dest/'mscorlib.dll')=='86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4'
 aot=json.loads((BASE/'evidence/OWNER-CORLIB-AOT.json').read_text());assert sha(dest/'mscorlib.dll.dylib')==aot['aot_sha256']
 prefix=(CAP/'host-check/MonoHarness.cpp').read_text().split('static void need(',1)[0]
 harness=BASE/'tests/MonoOwnerHarness.cpp'
 harness.write_text(prefix+r'''
#include "MonoOwner.hpp"
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    std::printf("{\"verdict\":\"%s\",\"stage\":%u,\"completed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"aot_interp_mode\":true,\"host_only_observer\":true,\"patched_loader\":true,\"townfall_proof\":false}\n",
        result.completed ? "REAL_MONO_OWNER_STRICT_HOST_PASS" : "REAL_MONO_OWNER_STRICT_HOST_FAIL",
        static_cast<unsigned>(result.stage), result.completed ? "true" : "false",
        unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked),
        unsigned(result.calls.deliberate.had_exception),
        result.calls.first.passed ? "true" : "false", result.calls.deliberate.passed ? "true" : "false",
        result.calls.recovery.passed ? "true" : "false");
    return result.completed ? 0 : 1;
}
''')
 env=os.environ.copy()
 for key in ['MONO_PATH','MONO_ENV_OPTIONS','MONO_AOT_CACHE']:env.pop(key,None)
 env.update(MONO_CFG_DIR=str(MDK/'etc'),MONO_CRASH_NOFILE='1')
 objects=BASE/'owner-objects';objects.mkdir(exist_ok=True)
 commands=[];logs=[];sources=[]
 def compile(command):
  commands.append(command)
  result=subprocess.run(command,cwd=BASE,env=env,text=True,capture_output=True)
  logs.append(result.stdout+result.stderr);(BASE/'logs/OWNER-COMPILE-LINK.log').write_text(''.join(logs))
  if result.returncode:raise RuntimeError('compile/link failed: '+result.stdout+result.stderr)
 flags=['/usr/bin/clang++','-arch','x86_64','-std=c++17','-O1','-Wall','-Wextra','-Werror','-c','-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1','-DMODBRIDGE_CAP10_CORLIB_DIR='+json.dumps(str(BASE/'owner-runtime/lib')),'-DMODBRIDGE_CAP10_CONFIG_DIR='+json.dumps(str(MDK/'etc')),'-I'+str(BASE/'tests'),'-I'+str(CAP/'native'),'-I'+str(WORK),'-I'+str(WORK/'mono/eglib')]
 for name,source in [('MonoOwner',CAP/'native/MonoOwner.cpp'),('InvocationBoundary',CAP/'native/InvocationBoundary.cpp'),('MonoOwnerHarness',harness)]:
  compile(flags+[str(source),'-o',str(objects/(name+'.o'))]);sources.append({'source':str(source),'sha256':sha(source)})
 native=CAP/'host-check/native-mono-test.c';compile(['/usr/bin/clang','-arch','x86_64','-O1','-Wall','-Wextra','-Werror','-c',str(native),'-o',str(objects/'native-observer.o')]);sources.append({'source':str(native),'sha256':sha(native)})
 exe=BASE/'bin/mono-owner-strict'
 compile(['/usr/bin/clang++','-arch','x86_64',*[str(objects/(name+'.o')) for name in ['MonoOwner','InvocationBoundary','MonoOwnerHarness','native-observer']],str(lib),'-Wl,-headerpad_max_install_names','-o',str(exe)])
 subprocess.run(['/usr/bin/install_name_tool','-change',str(BASE/'runtime-install/lib/libmonosgen-2.0.1.dylib'),str(lib),str(exe)],check=True)
 otool=subprocess.run(['/usr/bin/otool','-L',str(exe)],text=True,capture_output=True,check=True)
 (BASE/'logs/OWNER-DEPENDENCIES.log').write_text(otool.stdout+otool.stderr)
 assert str(lib) in otool.stdout,'harness must resolve our genuine patched runtime'
 command=['/usr/bin/arch','-x86_64',str(exe)]
 result=subprocess.run(command,cwd=dest,env=env,text=True,capture_output=True,timeout=60,preexec_fn=no_core)
 (BASE/'logs/OWNER-CHECK.log').write_text(result.stdout+result.stderr)
 proof={'verdict':'REAL_MONO_OWNER_STRICT_HOST_FAIL','exit_code':result.returncode}
 if result.returncode==0:proof.update(json.loads(result.stdout.strip().splitlines()[-1]))
 proof.update({'runtime_diagnostics_clean':"assertion '" not in result.stdout+result.stderr,'commands':commands,'run_command':command,'source_inputs':sources,'runtime_library_sha256':sha(lib),'loader_sha256':sha(WORK/'mono/utils/mono-dl.c'),'config_sha256':sha(WORK/'config.h'),'harness_sha256':sha(exe),'corlib_aot_sha256':sha(dest/'mscorlib.dll.dylib'),'mono_path_override_present':False,'real_internal_headers':True,'host_only':True,'target_proof':False})
 (BASE/'evidence/OWNER-CHECK.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps({k:v for k,v in proof.items() if k not in ['commands']},indent=2))
 if result.returncode or not proof['runtime_diagnostics_clean']:raise SystemExit(1)
if __name__=='__main__':main()
