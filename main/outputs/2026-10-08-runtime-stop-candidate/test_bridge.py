from pathlib import Path
import json,subprocess,os,hashlib
HERE=Path(__file__).resolve().parent;ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
def body(p):
 s=p.read_text();a=s.index('extern "C" int modbridge_runtime_stop(void)');b=s.index('\nextern "C" int modbridge_runtime_start_with_activation',a);return s[a:b]
def supplier(p):
 s=p.read_text();a=s.index('auto StopRuntimeWithAck(');b=s.index('\n#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST',a);return s[a:b]
pre='''#include <memory>
#include <stdexcept>
#include <string>
static int mode, destructs, releases, residence;
constexpr int MRS_ERR_STOP_EXN=-83;
namespace modbridge::native {struct RuntimeOwner {bool stop(){if(mode==2)throw std::runtime_error("stop");return mode!=1;}~RuntimeOwner(){++destructs;}};}
static std::unique_ptr<modbridge::native::RuntimeOwner> slot;
static auto& owner_slot(){return slot;}
static void bridge_startup_resources_release(){++releases;}
extern "C" int bridge_native_code_residence_required(void){return residence;}
'''
post='''int main(int argc,char**argv){if(argc!=2)return 99;const std::string c=argv[1];mode=c=="exception"?2:c=="refusal"?1:0;residence=c=="resident"||c=="resident_no_owner"?1:c=="unknown"?-1:0;if(c!="resident_no_owner"&&c!="no_owner")slot=std::make_unique<modbridge::native::RuntimeOwner>();auto* identity=slot.get();int r=modbridge_runtime_stop();if(residence)return r!=0&&slot.get()==identity&&destructs==0&&releases==0?0:10;if(mode==1)return r!=0&&slot.get()==identity&&destructs==0&&releases==0?0:11;if(mode==2)return r==MRS_ERR_STOP_EXN&&slot.get()==identity&&destructs==0&&releases==0?0:12;return r==0&&!slot&&releases==1?0:13;}
'''
oldbridge=ROOT/'outputs/2026-10-07-l19-r3-full-build/units/runtime_start_bridge_r14_diag_spanpub.cpp';oldsupplier=ROOT/'outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp'
rows=[];env=dict(os.environ,ASAN_OPTIONS='abort_on_error=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
for kind,bp,sp in [('original',oldbridge,oldsupplier),('candidate',HERE/'runtime_start_bridge_r14_diag_spanpub.cpp',HERE/'runtime_ack_supplier.cpp')]:
 p=HERE/('bridge_'+kind+'.cpp');p.write_text(pre+'namespace modbridge::native {\n'+supplier(sp)+'\n}\n'+body(bp)+post)
 cmd=['clang++','-std=c++20','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p),'-o',str(HERE/('bridge_'+kind))];r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stderr
 for case in ['refusal','exception','resident','resident_no_owner','unknown','success','no_owner']:
  r=subprocess.run([str(HERE/('bridge_'+kind)),case],capture_output=True,text=True,env=env)
  expected=({'refusal':11,'exception':12,'resident':10,'resident_no_owner':10,'unknown':10}.get(case,0)if kind=='original'else 0)
  assert r.returncode==expected,(kind,case,r.returncode,r.stderr);rows.append({'variant':kind,'case':case,'exit':r.returncode,'expected':expected})
(HERE/'BRIDGE-CONTROLS.json').write_text(json.dumps({'status':'PASS_MAC_ONLY_ACTUAL_BRIDGE_AND_SUPPLIER_FLOW','rows':rows,'original_failure_cases':5,'candidate_passes':7,'native_execution':False,'scope':'Exact source bodies; explicit RuntimeOwner/residence/resource seams, not actual initializer lifecycle','unit_pins':[{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}for p in [HERE/'bridge_original.cpp',HERE/'bridge_candidate.cpp']]},indent=2)+'\n');print('bridge original5 RED / candidate7 PASS with fatal sanitizers; no native unload claim')
