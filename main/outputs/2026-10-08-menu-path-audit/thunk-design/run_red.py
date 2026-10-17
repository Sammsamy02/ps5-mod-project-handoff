#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os
HERE=Path(__file__).resolve().parent
CACHE=HERE.parent/'source-cache'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
header=(CACHE/'UFunctionStructs.hpp').read_text()
header='\n'.join(x for x in header.splitlines() if not x.startswith('#include <Unreal/'))
full=(CACHE/'UFunctionStructs.cpp').read_text();body=full[full.index('namespace RC::Unreal'):]
uf=(CACHE/'UFunction.cpp').read_text()
install=uf[uf.index('    auto UFunction::GetFunctionHookData()'):uf.index('    auto UFunction::RegisterPreHookForInstance')]
retire=uf[uf.index('    auto UFunction::UnregisterAllHooks()'):uf.index('    auto UFunction::GetFunctionHookCallbackData')]
unit=HERE/'actual_wrapper.cpp'
unit.write_text('#include "host_model.hpp"\n'+header+'\n'+body+'\nnamespace RC::Unreal {\n'+install+retire+'\n}\n#include "cases.cpp"\n')
command=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(unit),'-o',str(HERE/'red_tests')]
r=subprocess.run(command,capture_output=True,text=True);(HERE/'compile.stderr').write_text(r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
rows=[]
for scenario in ('null_identity','foreign_identity','retired_null_frame','retired_then_registered','reentrant_unregister','known_identity'):
 r=subprocess.run([str(HERE/'red_tests'),scenario],capture_output=True,text=True,env=env,timeout=15)
 (HERE/(scenario+'.stdout')).write_text(r.stdout);(HERE/(scenario+'.stderr')).write_text(r.stderr)
 rows.append({'scenario':scenario,'returncode':r.returncode,'stdout':scenario+'.stdout','stderr':scenario+'.stderr'})
receipt={'status':'RED_IDENTITY_AND_RETIREMENT_CONTROLS_NO_IMPLEMENTATION','scope':'Actual production methods with explicit HOST engine storage; no native dispatch/liveness claim','source_sha256':sha(CACHE/'UFunctionStructs.cpp'),'header_sha256':sha(CACHE/'UFunctionStructs.hpp'),'install_retire_source_sha256':sha(CACHE/'UFunction.cpp'),'unit_sha256':sha(unit),'cases_sha256':sha(HERE/'cases.cpp'),'seams_sha256':sha(HERE/'host_model.hpp'),'command':command,'results':rows,'pass_count':sum(x['returncode']==0 for x in rows),'fail_count':sum(x['returncode']!=0 for x in rows)}
(HERE/'RED-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'pass_count':receipt['pass_count'],'fail_count':receipt['fail_count'],'failures':[x['scenario'] for x in rows if x['returncode']!=0]}))
