#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,sys,json,hashlib
HERE=Path(__file__).resolve().parent; OUT=HERE/sys.argv[1];OUT.mkdir(exist_ok=True)
s=(HERE/'candidate/UFunction.cpp').read_text(); start=s.index('    auto UFunction::GetFuncPtr()');end=s.index('    //auto UFunction::GetNumParms()',start)
methods=s[start:end]
prefix=r"""
#include <atomic>
#include <bit>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstdint>
#include <memory>
#include <thread>
namespace RC::Unreal {
class UObject {}; class FFrame {};
using UnrealScriptFunction=void(*)(UObject*,FFrame&,void*);
class UFunction { public:
 using Storage=void*;
 alignas(std::atomic_ref<Storage>::required_alignment) Storage func{};
 Storage& GetFunc(){return func;}
 UnrealScriptFunction GetFuncPtr(); void SetFuncPtr(UnrealScriptFunction);
};
}
"""
helper=HERE/'candidate/ScriptFunctionStorage.hpp'
if helper.exists():prefix+='\n#include "'+str(helper)+'"\n'
source=OUT/'actual_accessors.cpp';source.write_text(prefix+'\nnamespace RC::Unreal {\n'+methods+'\n}\n'+r"""
using namespace RC::Unreal;
void original(UObject*,FFrame&,void*){} void replacement(UObject*,FFrame&,void*){}
int main(){ UFunction f;f.SetFuncPtr(original);std::atomic<bool> ready{false};
 std::thread reader([&]{ready.store(true,std::memory_order_release);for(int i=0;i<200000;++i){auto p=f.GetFuncPtr();if(p!=original&&p!=replacement)std::abort();}});
 while(!ready.load(std::memory_order_acquire)){}
 for(int i=0;i<200000;++i)f.SetFuncPtr(i%2?original:replacement);
 reader.join();return 0;
}
""")
command=['/usr/bin/clang++','-std=c++20','-O1','-g','-fsanitize=thread','-fno-omit-frame-pointer','-pthread',str(source),'-o',str(OUT/'test')]
r=subprocess.run(command,capture_output=True,text=True);(OUT/'compile.stderr').write_text(r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(OUT/'test')],capture_output=True,text=True,env=dict(os.environ,TSAN_OPTIONS='halt_on_error=1:exitcode=66'),timeout=45)
(OUT/'run.stdout').write_text(r.stdout);(OUT/'run.stderr').write_text(r.stderr)
receipt={'scope':'Verbatim candidate GetFuncPtr/SetFuncPtr with explicit HOST storage seam; port accesses only, not native engine read contract','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'candidate_source_sha256':hashlib.sha256((HERE/'candidate/UFunction.cpp').read_bytes()).hexdigest(),'command':command,'returncode':r.returncode,'race_reported':'WARNING: ThreadSanitizer: data race' in r.stderr}
(OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'returncode':r.returncode,'race_reported':receipt['race_reported']}))
