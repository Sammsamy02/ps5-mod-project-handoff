#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,sys
HERE=Path(__file__).resolve().parent
CACHE=HERE.parent/'source-cache';CANDIDATE=HERE/'candidate'
OUT=HERE/(sys.argv[1] if len(sys.argv)>1 else 'green');OUT.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def ns(p):s=p.read_text();return s[s.index('namespace RC::Unreal'):]
header=(CACHE/'UFunctionStructs.hpp').read_text();header='\n'.join(x for x in header.splitlines() if not x.startswith('#include <Unreal/'))
helper=(CANDIDATE/'ScriptHookThunks.hpp').read_text();helper='\n'.join(x for x in helper.splitlines() if not x.startswith('#include <Unreal/'))
uf=(CANDIDATE/'UFunction.cpp').read_text()
install=uf[uf.index('    auto UFunction::GetFunctionHookData()'):uf.index('    auto UFunction::RegisterPreHookForInstance')]
retire=uf[uf.index('    auto UFunction::UnregisterAllHooks()'):uf.index('    auto UFunction::GetFunctionHookCallbackData')]
unit=OUT/'actual_candidate.cpp'
unit.write_text('#include "../green_host_model.hpp"\n#include <array>\n#include <mutex>\n#include <algorithm>\n'+header+'\n'+helper+'\n'+ns(CANDIDATE/'UFunctionStructs.cpp')+'\nnamespace RC::Unreal {\n'+install+retire+'\n}\n'+ns(CANDIDATE/'ScriptHookThunks.cpp')+'\n#include "../green_cases.cpp"\n#include "../extra_green_cases.cpp"\n')
command=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(unit),'-o',str(OUT/'green_tests')]
r=subprocess.run(command,capture_output=True,text=True);(OUT/'compile.stderr').write_text(r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
rows=[]
cases=['null_identity','foreign_identity','retired_null_frame','retired_then_registered','reentrant_unregister','known_identity','capacity_no_reuse','null_original','recursive_original','gnatives_refusal','other_owned_thunk_refusal','intervening_replacement','nested_retirement','original_exception_lifetime','gnatives_later_original_only','concurrent_registration_dispatch']
for scenario in cases:
 r=subprocess.run([str(OUT/'green_tests'),scenario],capture_output=True,text=True,env=env,timeout=20)
 (OUT/(scenario+'.stdout')).write_text(r.stdout);(OUT/(scenario+'.stderr')).write_text(r.stderr)
 rows.append({'scenario':scenario,'returncode':r.returncode,'stdout':scenario+'.stdout','stderr':scenario+'.stderr'})
receipt={'status':'GREEN_ISOLATED_STATIC_THUNK_PROTOTYPE' if all(x['returncode']==0 for x in rows) else 'FAIL','scope':'Actual candidate callback/install/retirement and source-compiled static thunk dispatch; explicit HOST storage/engine seams; no selected integration/native argument/liveness claim','candidate_pins':{p.name:sha(p) for p in CANDIDATE.iterdir() if p.is_file()},'unit_sha256':sha(unit),'cases_sha256':sha(HERE/'green_cases.cpp'),'extra_cases_sha256':sha(HERE/'extra_green_cases.cpp'),'seams_sha256':sha(HERE/'green_host_model.hpp'),'command':command,'results':rows,'pass_count':sum(x['returncode']==0 for x in rows),'fail_count':sum(x['returncode']!=0 for x in rows),'capacity':8,'slots_reused':False,'frame_fields_interpreted_by_thunk_dispatch':False,'requires_gnatives_null_on_install':True}
(OUT/'GREEN-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'pass_count':receipt['pass_count'],'fail_count':receipt['fail_count'],'failures':[x['scenario'] for x in rows if x['returncode']!=0]}))
