#!/usr/bin/env python3
"""Verify the final real target config/backend selection and compiled TLS object."""
from pathlib import Path
import hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parents[1]
local=json.loads((HERE/'LOCAL-INPUTS.json').read_text())
work=HERE/'target-work';sdk=Path(local['sdk']);llvm=Path(local['llvm_bin'])
env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
probe=HERE/'probe/final-config.c'
probe.write_text('''#include <config.h>
#include <mono/metadata/w32process-unix-internals.h>
#include <pthread.h>
#if !defined(__PROSPERO__) || __FreeBSD__ != 9
#error genuine target required
#endif
#if !defined(TARGET_AMD64) || !defined(DISABLE_JIT) || !defined(ENABLE_COOP_SUSPEND)
#error target build policy mismatch
#endif
#if defined(DISABLE_AOT) || defined(MONO_KEYWORD_THREAD)
#error AOT must remain enabled and compiler TLS disabled
#endif
#if !defined(USE_DEFAULT_BACKEND) || defined(USE_BSD_BACKEND)
#error process diagnostic capability selection mismatch
#endif
#if defined(HAVE_DL_ITERATE_PHDR) || defined(HAVE_KINFO_GETFILE) || HAVE_DECL_PTHREAD_GETNAME_NP
#error current public SDK negative capability probes changed; review source port
#endif
_Static_assert(sizeof(void*)==8 && sizeof(long)==8, "LP64 required");
int final_target_configuration_verified;
''')
commands=[]
def run(label,cmd):
 r=subprocess.run(cmd,env=env,capture_output=True,text=True)
 (HERE/'logs'/f'{label}.stdout').write_text(r.stdout);(HERE/'logs'/f'{label}.stderr').write_text(r.stderr)
 commands.append(dict(name=label,command=cmd,exit_code=r.returncode))
 assert r.returncode==0,(label,r.stderr)
 return r.stdout
run('FINAL-TARGET-CONFIG',[str(sdk/'bin/prospero-clang'),'-std=gnu11','-Werror','-I'+str(work),'-I'+str(work/'mono'),'-I'+str(work/'mono/eglib'),'-c',str(probe),'-o',str(HERE/'probe/final-config.o')])
tls=work/'mono/utils/libmonoutils_la-mono-tls.o'
symbols=run('FINAL-TARGET-TLS-NM',[str(llvm/'llvm-nm'),'-u',str(tls)])
assert all(s in symbols for s in ['pthread_key_create','pthread_key_delete','pthread_getspecific'])
assert '__emutls_' not in symbols
receipt=dict(scope='Actual final target configure/backend and built TLS object; no execution or process capability proof',commands=commands,
 config_sha256=hashlib.sha256((work/'config.h').read_bytes()).hexdigest(),tls_object_sha256=hashlib.sha256(tls.read_bytes()).hexdigest(),
 genuine_target=True,jit_disabled=True,aot_enabled=True,pthread_tls=True,negative_capabilities_checked=True,process_default_backend=True)
(HERE/'evidence/FINAL-TARGET-CONFIGURATION.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('PASS genuine final target configuration and built pthread TLS')
