#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,sys
HERE=Path(__file__).resolve().parent;CANDIDATE=HERE/'candidate'
OUT=HERE/'cold-red';OUT.mkdir(exist_ok=True)
def ns(p):s=p.read_text();return s[s.index('namespace RC::Unreal'):]
header=(CANDIDATE/'include/Unreal/UFunctionStructs.hpp').read_text();header='\n'.join(x for x in header.splitlines() if not x.startswith('#include <Unreal/'))
helper=(CANDIDATE/'ScriptHookThunks.hpp').read_text();helper='\n'.join(x for x in helper.splitlines() if not x.startswith('#include <Unreal/'))
uf=(CANDIDATE/'UFunction.cpp').read_text();methods=uf[uf.index('    auto UFunction::GetFunctionHookData()'):uf.rfind('\n}')]
unit=OUT/'actual_candidate.cpp'
cold='''extern "C" int bridge_native_code_residence_required(void);
inline int cold_result=99;
struct ColdProbe { ColdProbe(){cold_result=bridge_native_code_residence_required();} };
ColdProbe cold_probe;
'''
unit.write_text('#include "../host_model.hpp"\n#include "../candidate/ScriptFunctionStorage.hpp"\n#include <array>\n#include <mutex>\n#include <algorithm>\n'+cold+header+'\n'+helper+'\n'+ns(CANDIDATE/'UFunctionStructs.cpp')+'\nnamespace RC::Unreal {\n'+methods+'\n}\n'+ns(CANDIDATE/'ScriptHookThunks.cpp')+'\n#include <iostream>\nint main(){if(cold_result!=-1)return 1;return bridge_native_code_residence_required()==0?0:2;}\n')
command=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread',str(unit),'-o',str(OUT/'test')]
r=subprocess.run(command,capture_output=True,text=True);(OUT/'compile.stderr').write_text(r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
rows=[]
for scenario in ('cold_residence',):
 r=subprocess.run([str(OUT/'test'),scenario],capture_output=True,text=True,env=env,timeout=20)
 (OUT/(scenario+'.stdout')).write_text(r.stdout);(OUT/(scenario+'.stderr')).write_text(r.stderr)
 rows.append({'scenario':scenario,'returncode':r.returncode})
pins={str(p.relative_to(CANDIDATE)):hashlib.sha256(p.read_bytes()).hexdigest() for p in CANDIDATE.rglob('*') if p.is_file()}
receipt={'scope':'Actual copied candidate methods and source-compiled thunk dispatch; explicit HOST engine storage','candidate_pins':pins,'command':command,'unit_sha256':hashlib.sha256(unit.read_bytes()).hexdigest(),'case_pins':{f:hashlib.sha256((HERE/f).read_bytes()).hexdigest() for f in ['cases.cpp','cases.basic.cpp','cases.extra.cpp','host_model.hpp']},'results':rows,'pass_count':sum(x['returncode']==0 for x in rows),'fail_count':sum(x['returncode']!=0 for x in rows)}
(OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'pass_count':receipt['pass_count'],'fail_count':receipt['fail_count']}))
