#!/usr/bin/env python3
"""Compile actual full wrapper/callbacks and real install methods; HOST engine seams."""
from pathlib import Path
import hashlib,json,subprocess,os,sys
HERE=Path(__file__).resolve().parent
CACHE=HERE.parent/'source-cache'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 mode=sys.argv[1]
 source=CACHE/'UFunctionStructs.cpp' if mode=='original' else HERE/'candidate'/'UFunctionStructs.cpp'
 out=HERE/(sys.argv[2] if len(sys.argv)>2 else mode); out.mkdir(exist_ok=True)
 header=(CACHE/'UFunctionStructs.hpp').read_text()
 header='\n'.join(x for x in header.splitlines() if not x.startswith('#include <Unreal/'))
 full=source.read_text(); body=full[full.index('namespace RC::Unreal'):]
 uf=(CACHE/'UFunction.cpp').read_text(); start=uf.index('    auto UFunction::GetFunctionHookData()'); end=uf.index('    auto UFunction::RegisterPreHookForInstance',start)
 install=uf[start:end]
 unit=out/'actual_wrapper.cpp'
 unit.write_text('#include "../host_model.hpp"\n'+header+'\n'+body+'\nnamespace RC::Unreal {\n'+install+'\n}\n#include "../cases.cpp"\n')
 command=['clang++','-std=c++20','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Werror=return-type',str(unit),'-o',str(out/'wrapper_test')]
 build=subprocess.run(command,capture_output=True,text=True); (out/'compile.stderr').write_text(build.stderr)
 if build.returncode: print(build.stderr); raise SystemExit(build.returncode)
 env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 cases=['null_null','null_bogus','unknown_bogus','unknown_foreign','known_null_original','known_self_original','known_null_code','known_code_without_gnatives','known_conversion','known_conversion_legacy','pre_exception','script_prefix_delivery']
 results=[]
 for case in cases:
  r=subprocess.run([str(out/'wrapper_test'),case],capture_output=True,text=True,env=env,timeout=15)
  (out/(case+'.stdout')).write_text(r.stdout); (out/(case+'.stderr')).write_text(r.stderr)
  results.append({'case':case,'returncode':r.returncode,'stdout':case+'.stdout','stderr':case+'.stderr'})
 receipt={'scope':'HOST MODEL; actual complete wrapper/callbacks and actual UFunction install methods, fake engine storage/dispatch only; no native ABI/liveness or installation safety claim','mode':mode,'source':str(source),'source_sha256':sha(source),'actual_header_sha256':sha(CACHE/'UFunctionStructs.hpp'),'actual_install_source_sha256':sha(CACHE/'UFunction.cpp'),'harness_sha256':sha(HERE/'cases.cpp'),'seams_sha256':sha(HERE/'host_model.hpp'),'extraction_sha256':sha(unit),'command':command,'build_returncode':build.returncode,'results':results,'pass_count':sum(x['returncode']==0 for x in results),'fail_count':sum(x['returncode']!=0 for x in results)}
 (out/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps({'mode':mode,'pass_count':receipt['pass_count'],'fail_count':receipt['fail_count'],'failures':[x['case'] for x in results if x['returncode']!=0]}))
if __name__=='__main__': main()
