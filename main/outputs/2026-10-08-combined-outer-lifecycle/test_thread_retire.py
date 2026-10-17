"""Actual outer entry/bridge bodies with explicit host owner/finalizer seams."""
from pathlib import Path
import hashlib,json,subprocess,sys

HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
source=Path(sys.argv[1]).resolve();kind=sys.argv[2];out=HERE/sys.argv[3];out.mkdir(exist_ok=False)
text=source.read_text();signature='int module_stop(' if kind=='entry' else 'extern "C" int modbridge_runtime_stop('
start=text.index(signature);begin=text.index('{',start);end=begin+1;level=1
while level:
    if text[end]=='{':level+=1
    elif text[end]=='}':level-=1
    end+=1
body=text[start:end]
if kind=='entry':
    pre=(ROOT/'outputs/2026-10-08-runtime-stop-candidate/entry_residence.c').read_text().split('int module_stop(')[0]
    pre+='''
static int retire_result,retire_checks,late_refusal;
static int bridge_native_pad_code_residence_required(void){return 0;}
static int bridge_native_thread_authority_retire(void){++retire_checks;return late_refusal&&retire_checks>1?1:retire_result;}
'''
    main='''
int main(int argc,char**argv){
 if(argc!=2)return 99;
 runtime_active=strcmp(argv[1],"refuse-inactive")!=0;int before=runtime_active;
 late_refusal=!strcmp(argv[1],"late-refusal");
 retire_result=!strcmp(argv[1],"unknown")?-1:(!strcmp(argv[1],"success")||late_refusal)?0:1;
 int r=module_stop(0,0);
 if(retire_result){if(!(r<0&&retire_checks==1&&runtime_active==before&&stops==0&&finis==0&&inputs==0&&channels==0&&entry_guard==0))return 1;}
 else if(late_refusal){if(!(r<0&&retire_checks==2&&stops==1&&finis==0&&inputs==0&&channels==0))return 3;}
 else if(!(r==0&&retire_checks==2&&stops==1&&finis==1&&inputs==1&&channels==1))return 2;
 return 0;
}
''';ext='c';compiler='clang';standard='-std=c11'
else:
    pre='''#include <memory>
#include <string>
#include <cstring>
static int retire_result,retire_checks,release_checks,stops,late_refusal;
static int bridge_native_code_residence_required(void){return 0;}
static int bridge_native_thread_authority_retire(void){++retire_checks;return late_refusal&&retire_checks>1?1:retire_result;}
static void bridge_startup_resources_release(){++release_checks;}
static constexpr int MRS_ERR_STOP_EXN=-30;
struct Owner{};static std::unique_ptr<Owner> owner;
static auto& owner_slot(){return owner;}
namespace modbridge::native{bool StopRuntimeWithAck(std::unique_ptr<Owner>&o){++stops;o.reset();return true;}}
'''
    main='''
int main(int argc,char**argv){
 if(argc!=2)return 99;
 bool active=std::strcmp(argv[1],"refuse-inactive")!=0;if(active)owner=std::make_unique<Owner>();
 late_refusal=!std::strcmp(argv[1],"late-refusal");
 retire_result=!std::strcmp(argv[1],"unknown")?-1:(!std::strcmp(argv[1],"success")||late_refusal)?0:1;
 int r=modbridge_runtime_stop();
 if(retire_result){if(!(r!=0&&retire_checks==1&&static_cast<bool>(owner)==active&&stops==0&&release_checks==0))return 1;}
 else if(late_refusal){if(!(r!=0&&retire_checks==2&&!owner&&stops==1&&release_checks==0))return 3;}
 else if(!(r==0&&retire_checks==2&&!owner&&stops==1&&release_checks==1))return 2;
 return 0;
}
''';ext='cpp';compiler='clang++';standard='-std=c++20'
tu=out/('actual.'+ext);tu.write_text(pre+body+main);rows=[]
for variant,flags in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
    exe=out/variant
    built=subprocess.run([compiler,standard,*flags,str(tu),'-o',str(exe)],capture_output=True,text=True)
    if built.returncode:raise RuntimeError(built.stderr)
    for case in ['refuse','refuse-inactive','unknown','success','late-refusal']:
        result=subprocess.run([str(exe),case],capture_output=True,text=True)
        rows.append({'variant':variant,'case':case,'exit':result.returncode,'output':result.stdout+result.stderr})
receipt={'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,
         'kind':kind,'scope':'Actual entry/bridge control flow; explicit host provider seams; no native teardown acceptance.'}
(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'kind':kind,'runs':len(rows),'passed':sum(r['exit']==0 for r in rows)}))
raise SystemExit(any(r['exit'] for r in rows))
