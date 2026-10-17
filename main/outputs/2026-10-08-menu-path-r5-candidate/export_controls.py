#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,hashlib,os
HERE=Path(__file__).resolve().parent
prefix=r"""
#include <stdexcept>
#include <cstdlib>
namespace RC::Unreal::ThunkDetail {
enum class Residence { Unpublished,RequiresCodeResidence };
struct TeardownSnapshot {Residence residence;};
bool ready=false,published=false,throw_on_begin=false;int begin_calls=0;
bool RegistryReady(){return ready;}
TeardownSnapshot BeginTeardown(){++begin_calls;if(!ready||throw_on_begin)throw std::runtime_error("lifetime");return {published?Residence::RequiresCodeResidence:Residence::Unpublished};}
}
"""
controls=r"""
int main(){using namespace RC::Unreal::ThunkDetail;
 if(bridge_native_code_residence_required()!=-1||begin_calls!=0)return 1;
 ready=true;if(bridge_native_code_residence_required()!=0||begin_calls!=1)return 2;
 published=true;if(bridge_native_code_residence_required()!=1||begin_calls!=2)return 3;
 ready=false;if(bridge_native_code_residence_required()!=-1||begin_calls!=2)return 4;
 ready=true;throw_on_begin=true;if(bridge_native_code_residence_required()!=-1||begin_calls!=3)return 5;
 return 0;
}
"""
rows=[]
for label,base in [('export-red','before-readiness'),('export-green','candidate')]:
 out=HERE/label;out.mkdir(exist_ok=True);f=HERE/base/'ScriptHookThunks.cpp';s=f.read_text();method=s[s.rindex('extern "C" int bridge_native_code_residence_required'):]
 unit=out/'actual_export.cpp';unit.write_text(prefix+'\n'+method+'\n'+controls)
 command=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(unit),'-o',str(out/'test')]
 r=subprocess.run(command,capture_output=True,text=True);(out/'compile.stderr').write_text(r.stderr)
 if r.returncode:raise RuntimeError(r.stderr)
 r=subprocess.run([str(out/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=10)
 (out/'run.stderr').write_text(r.stderr);receipt={'scope':'Verbatim actual C export; explicit HOST readiness/BeginTeardown seams count admission into lock-owning function, with no production test knob','source_path':str(f),'source_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'unit_sha256':hashlib.sha256(unit.read_bytes()).hexdigest(),'command':command,'returncode':r.returncode,'controls':5,'cold_or_destroyed_begin_calls_expected':0}
 (out/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');rows.append({'label':label,'returncode':r.returncode})
assert rows==[{'label':'export-red','returncode':1},{'label':'export-green','returncode':0}],rows
print(json.dumps(rows))
