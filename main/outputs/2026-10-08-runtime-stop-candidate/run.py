from pathlib import Path
import json,hashlib,subprocess,os
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');HERE=Path(__file__).resolve().parent
SRC=ROOT/'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/lifecycle/runtime_module_entry.c'
text=SRC.read_text();start=text.index('int module_stop(size_t argc, const void *args)');end=text.index('\n/* Source-level markers',start);body=text[start:end]
old='''        runtime_active = 0; /* the owner is gone (or was never stoppable) */
        if (stop_result != 0)
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop reported failure");'''
new='''        if (stop_result != 0) {
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop refused; preserving runtime and module residence");
            /* Nonzero positive callback values may not refuse unload. Always return
               a negative entry error. Preserve active ownership and skip every
               fini/input/channel release while code can still be referenced. */
            return leave_entry(-21);
        }
        runtime_active = 0; /* stop succeeded: ownership has been retired */'''
assert text.count(old)==1
candidate=text.replace(old,new); (HERE/'runtime_module_entry.c').write_text(candidate)
pre='''#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char entry_guard;
static int runtime_active, rc, stops, finis, inputs, channels;
#define PROBE_FINISHED 5
static struct {int state;} lifecycle;
static int ops;
static void diagnostic_event(const char*t,uint32_t v){(void)t;(void)v;}
static int modbridge_runtime_stop(void){++stops;return rc;}
static void runtime_failure(const char*t,int v,const char*d){(void)t;(void)v;(void)d;}
static int probe_lifecycle_stop(void*s,const void*o){(void)s;(void)o;++finis;lifecycle.state=PROBE_FINISHED;return 0;}
static void probe_runtime_release_input(void){++inputs;}
static int diagnostic_close_channel(void){++channels;return 0;}
static int leave_entry(int result){__atomic_clear(&entry_guard,__ATOMIC_RELEASE);return result;}
'''
post='''
int main(int argc,char**argv){
 if(argc!=2)return 99;
 if(!strcmp(argv[1],"busy")){entry_guard=1;runtime_active=1;int r=module_stop(0,0);return r==-20&&stops==0&&finis==0&&runtime_active==1?0:1;}
 runtime_active=strcmp(argv[1],"inactive")!=0;
 rc=!strcmp(argv[1],"negative")?-8:!strcmp(argv[1],"positive")?1:0;
 int r=module_stop(0,0);
 if(rc){if(!(r<0&&runtime_active==1&&finis==0&&inputs==0&&channels==0&&entry_guard==0))return 1;
   rc=0;r=module_stop(0,0);if(!(r==0&&runtime_active==0&&stops==2&&finis==1&&inputs==1&&channels==1))return 2;
 }else if(!(r==0&&runtime_active==0&&finis==1&&inputs==1&&channels==1))return 3;
 puts("PASS actual module_stop control flow; host owner/finalizer seams");return 0;}
'''
rows=[]
for name,source in [('original',body),('candidate',candidate[start:candidate.index('\n/* Source-level markers',start)])]:
 p=HERE/(name+'.c');p.write_text(pre+source+post)
 cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p),'-o',str(HERE/name)]
 r=subprocess.run(cmd,capture_output=True,text=True);assert not r.returncode,r.stderr
 for case in ['negative','positive','success','inactive','busy']:
  r=subprocess.run([str(HERE/name),case],env=dict(os.environ,ASAN_OPTIONS='abort_on_error=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'),capture_output=True,text=True)
  rows.append({'source':name,'case':case,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
expected={('original','negative'):1,('original','positive'):1}
assert all(row['exit']==expected.get((row['source'],row['case']),0) for row in rows),rows
pin=lambda p:{'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
result={'status':'PASS_MAC_ONLY_ACTUAL_ENTRY_FLOW','original':pin(SRC),'candidate':pin(HERE/'runtime_module_entry.c'),'delta':'Preserve runtime_active and refuse before finalization on any failed stop','actual_body_only':True,'owner_finalizer_sdk_seams':True,'native_unload_proof':False,'results':rows,'limits':['RequiresCodeResidence must be supplied by actual runtime stop path before ownership is destroyed','This entry fix alone does not prove engine callback quiescence','Target compilation/full integration pending']}
(HERE/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print('original2 RED / candidate5 PASS (fatal ASan/UBSan); no console')
