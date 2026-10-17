from pathlib import Path
import hashlib,json,subprocess,os
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');HERE=Path(__file__).resolve().parent
# Preserve the earlier focused entry result separately.
if not(HERE/'RESULTS.entry-only.json').exists():(HERE/'RESULTS.entry-only.json').write_bytes((HERE/'RESULTS.json').read_bytes())
entry=HERE/'runtime_module_entry.c';text=entry.read_text();needle='''    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);'''
replacement='''    /* The owning runtime is not the only possible holder of published code.
       Closing publication and testing residence is unconditional, including
       partial-start/no-owner cases. The in-link provider refuses unknown state. */
    int residence = bridge_native_code_residence_required();
    if (residence != 0) {
        diagnostic_event("module_stop_code_resident", (uint32_t)residence);
        return leave_entry(-22);
    }
    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);'''
assert text.count(needle)==1
text=text.replace(needle,replacement).replace('#include "runtime_start_bridge.h"','#include "runtime_start_bridge.h"\n/* Defined by the same composition; not a system import. */\nextern int bridge_native_code_residence_required(void);')
entry.write_text(text)
supplier=ROOT/'outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp'
s=supplier.read_text();old="    owner.reset();  // destructor emits at most one further 'stopped' line (none after stop)";assert s.count(old)==1
s=s.replace(old,'    if (stopped) owner.reset();  // preserve the owner when stop refuses')
(HERE/'runtime_ack_supplier.cpp').write_text(s)
bridge=ROOT/'outputs/2026-10-07-l19-r3-full-build/units/runtime_start_bridge_r14_diag_spanpub.cpp';b=bridge.read_text();old='''extern "C" int modbridge_runtime_stop(void)
{
    auto &slot = owner_slot();''';new='''extern "C" int bridge_native_code_residence_required(void);
extern "C" int modbridge_runtime_stop(void)
{
    // Close hook publication atomically before any owner destruction, even if
    // partial startup has no owner left. Nonzero/unknown means keep residence.
    if (bridge_native_code_residence_required() != 0) return 1;
    auto &slot = owner_slot();''';assert b.count(old)==1;b=b.replace(old,new)
old='''        slot.reset();
        // RuntimeOwner destruction stops/joins the initializer (or terminates
        // rather than returning). Only after that may its probe descriptors close.
        bridge_startup_resources_release();
        return MRS_ERR_STOP_EXN;'''
new='''        // A stop exception is not proof of join, callback quiescence or safe
        // destruction. Keep the owner and its resources for a later stop attempt.
        return MRS_ERR_STOP_EXN;''';assert b.count(old)==1;b=b.replace(old,new)
(HERE/'runtime_start_bridge_r14_diag_spanpub.cpp').write_text(b)
pin=lambda p:{'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
(HERE/'SOURCE-DELTA.json').write_text(json.dumps({'status':'LOCAL_CANDIDATE_PENDING_INTEGRATION','originals':[pin(x)for x in [ROOT/'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/lifecycle/runtime_module_entry.c',supplier,bridge]],'candidates':[pin(HERE/x)for x in ['runtime_module_entry.c','runtime_ack_supplier.cpp','runtime_start_bridge_r14_diag_spanpub.cpp']],'new_in_link_provider':'bridge_native_code_residence_required','new_system_import':False,'native_unload_proven':False},indent=2)+'\n')
# Actual candidate entry control flow, with explicit owner/residence/OS seams.
v1=(HERE/'candidate.c').read_text();start=text.index('int module_stop(size_t argc, const void *args)');end=text.index('\n/* Source-level markers',start)
pre=v1[:v1.index('int module_stop(')]+'static int residence;\nstatic int bridge_native_code_residence_required(void){return residence;}\n'
post=v1[v1.index('\nint main('):]
post=post.replace(' runtime_active=strcmp(argv[1],"inactive")!=0;', ''' if(!strcmp(argv[1],"resident")||!strcmp(argv[1],"resident_inactive")||!strcmp(argv[1],"unknown")){
  residence=!strcmp(argv[1],"unknown")?-1:1;
  runtime_active=strcmp(argv[1],"resident_inactive")!=0;int initial=runtime_active;
  int r=module_stop(0,0);return r<0&&runtime_active==initial&&stops==0&&finis==0&&inputs==0&&channels==0&&entry_guard==0?0:4;
 }
 runtime_active=strcmp(argv[1],"inactive")!=0;''')
unit=HERE/'entry_residence.c';unit.write_text(pre+text[start:end]+post)
cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(unit),'-o',str(HERE/'entry_residence')]
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
rows=[];env=dict(os.environ,ASAN_OPTIONS='abort_on_error=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
for case in ['negative','positive','success','inactive','busy','resident','resident_inactive','unknown']:
 r=subprocess.run([str(HERE/'entry_residence'),case],capture_output=True,text=True,env=env);assert not r.returncode,(case,r.stderr);rows.append({'case':case,'exit':r.returncode})
# Exact original/candidate supplier helper against a declared RuntimeOwner seam.
start=s.index('auto StopRuntimeWithAck(');end=s.index('\n#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST',start)
original=supplier.read_text();o=original[original.index('auto StopRuntimeWithAck('):original.index('\n#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST',original.index('auto StopRuntimeWithAck('))]
pre='''#include <memory>
#include <stdexcept>
#include <string>
static int destructs;
struct RuntimeOwner{int mode;bool stop(){if(mode==2)throw std::runtime_error("stop");return mode==0;}~RuntimeOwner(){++destructs;}};
'''
post='''int main(int argc,char**argv){if(argc!=2)return 99;int mode=std::stoi(argv[1]);auto p=std::make_unique<RuntimeOwner>();p->mode=mode;auto* identity=p.get();if(mode==2){try{StopRuntimeWithAck(p);return 2;}catch(...){return p.get()==identity&&destructs==0?0:3;}}bool r=StopRuntimeWithAck(p);return mode==0?(r&&!p&&destructs==1?0:4):(!r&&p.get()==identity&&destructs==0?0:5);}
'''
for name,body in [('supplier_original',o),('supplier_candidate',s[start:end])]:
 p=HERE/(name+'.cpp');p.write_text(pre+body+post);cmd=['clang++','-std=c++20','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p),'-o',str(HERE/name)];r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stderr
 for mode in range(3):
  r=subprocess.run([str(HERE/name),str(mode)],capture_output=True,text=True,env=env);expected=5 if name=='supplier_original' and mode==1 else 0;assert r.returncode==expected,(name,mode,r.returncode,r.stderr);rows.append({'case':name+':'+str(mode),'exit':r.returncode,'expected':expected})
(HERE/'INTEGRATION-CONTROLS.json').write_text(json.dumps({'status':'PASS_MAC_ONLY_ACTUAL_ENTRY_SUPPLIER_BODIES','entry_cases':8,'supplier_cases_per_variant':3,'original_supplier_refusal_red':True,'bridge_source_guard_added':True,'bridge_actual_body_controls':'pending','rows':rows,'native_execution':False,'limits':['Residence provider and target/full composition still require integration','Does not solve separate Lua uninstall borrower lifetime','No engine quiescence or unload proof']},indent=2)+'\n')
print('entry8 PASS; supplier original-refusal RED / candidate3 PASS; preserved stop owner; no console')
