#!/usr/bin/env python3
"""Execute real initializer admission/FName phase and real bridge FD state."""
from pathlib import Path
from datetime import datetime,timezone
import ast,hashlib,json,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
label=sys.argv[1] if len(sys.argv)>1 else 'GREEN'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-INITIALIZER-'+label+'-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
src=(root/'units/UnrealInitializer.cpp').read_text()
bridge=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text()
fixture=(root/'tests/test_retained.cpp').read_text()
def section(text,a,b):
 start=text.index(a);return text[start:text.index(b,start)]
tree=ast.parse((root/'tests/initializer_integration.py').read_text())
pre=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='pre' for t in n.targets))
pre=pre[:pre.index('extern "C" int bridge_write_copyin_capture')]
pre=pre.replace('#include <chrono>','#include <bit>\n#include "NativeFNameChecks.hpp"\n#include "FNameAbiProfile.hpp"\n#include "CopyinStartup.hpp"\n#include <chrono>',1)
pre=pre.replace('void CheckStartupWait(){','static bool check_exception=false;\nvoid CheckStartupWait(){if(check_exception)throw std::runtime_error("injected cancellation");')
adapter=section(src,'        namespace admission =','        // Retained public UE4SS Function API').replace('std::chrono::steady_clock::now()','FakeClock::now()')
old='''                const auto* source = reinterpret_cast<const volatile uint8_t*>(address);
                for (size_t i = 0; i < length; ++i) bytes[i] = source[i];'''
assert old in adapter;adapter=adapter.replace(old,'                host_direct(address,length,bytes);')
phase=section(src,'            // This one admission deadline','            // ---- Phase 5: static_construct_object_internal')
phase=phase.replace('std::bit_cast<rc::fnameabi::Ctor>(ctor_runtime)','&fixture_ctor')
fn=src.index('        static auto consume_required_slot_seeds(');first=src.index('{',fn)+1;end=src.index('            const uint64_t base =',first)
scope=src[first:end]
adapters=fixture[:fixture.index('namespace modbridge { namespace native {')]
adapters=adapters.replace('std::chrono::steady_clock::now()+std::chrono::milliseconds(clock_offset_ms)','std::chrono::steady_clock::time_point(std::chrono::milliseconds(fake_ms))')
adapters='#include <cstdint>\nextern uint64_t fake_ms;\n'+adapters
sink=section(bridge,'// ---- HOST-TEST-EXTRACT-BEGIN','// Legacy explicit diagnostics only.').replace('std::chrono::steady_clock::now()','TestClock::now()')
has_hooks='int bridge_copyin_initializer_enter(void)' in bridge
if has_hooks:sink=sink.replace('bridge_copyin_initializer_complete','real_bridge_copyin_initializer_complete')
else:sink+='\nextern "C" int bridge_copyin_initializer_enter(void){return 0;}\nextern "C" void real_bridge_copyin_initializer_complete(void){}\nextern "C" void bridge_copyin_initializer_leave(void){}\n'
(run/'sink_extract.inc').write_text(sink)
shim=r'''
static unsigned completion_calls=0,ctor_calls=0;static bool ctor_bound=false,ctor_throw=false,bad_output=false;
extern "C" void bridge_copyin_initializer_complete(void){++completion_calls;real_bridge_copyin_initializer_complete();}
using CharType=char16_t;using TCHAR=char16_t;
enum EFindName{FNAME_Find=0,FNAME_Add=1};
struct FName{uint32_t index,number;struct Storage{template<class T>void assign_address(T){ctor_bound=true;}void reset_address(){ctor_bound=false;}};static inline Storage ConstructorInternal;};
static rc::fnameabi::Ctor g_native_fname_ctor=nullptr;static bool bSeedBoundFNameCtor=false;
static FName native_fname_thunk(const CharType*,EFindName){return {0,0};}
static void fixture_ctor(rc::fnameabi::Name*out,const char16_t* text,uint32_t mode){++ctor_calls;REQUIRE(mode==0&&std::u16string(text)==u"ByteProperty");if(ctor_throw)throw std::runtime_error("native test boundary exception");*out={bad_output?5u:4u,0};}
extern "C" int bridge_image_range(uint64_t a,size_t n,rc::imagecoverage::Hit*out){
 *out={};rc::imagecoverage::Mapping maps[]={{0x96dc000,0x9a34000,3}};
 *out=rc::imagecoverage::from_profile(rcprofile::kLoadSegments,5,0x400000,0x400000,a,n,maps,1);return out->readable?1:0;
}
static bool heap_unavailable=false;
extern "C" int sceKernelVirtualQuery(const void*p,int flags,L13VqInfo*out,size_t n){
 REQUIRE(flags==0&&n==0x48);++vq_count;uint64_t a=reinterpret_cast<uint64_t>(p);
 if(heap_unavailable&&a>=heap&&a<heap+0x20000)return static_cast<int>(0x80020001u);
 out->flags=0x10;out->protection=a==negative?0:a>=0x400000&&a<0x96dc000?4:1;
 out->start=reinterpret_cast<void*>(a==negative?a:a>=0x400000&&a<0x96dc000?extra_guest:a<heap?0x96dc000:heap);
 out->end=reinterpret_cast<void*>(a==negative?a+4:a>=0x400000&&a<0x96dc000?extra_guest+0x1000:a<heap?0x9a34000:heap+0x20000);return 0;
}
static bool early_failure=false;
void checks(){
SCOPE
 if(early_failure)seed_refused("early phase2 seed refusal");
 uint64_t base=SeedTableNS::kBaseObserved;std::chrono::seconds validation_budget(10);
PHASE
 // This executes at the real Phase5 boundary, while the function is active.
 REQUIRE(live_fds.empty());REQUIRE(completion_calls==1);
}
void put(uint64_t a,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)memory[a+i]=static_cast<uint8_t>(v>>(i*8));}
int main(int argc,char**argv){
 REQUIRE(argc==2);std::string scenario=argv[1];bool expected_failure=scenario!="success";
 char temp[]="/tmp/l17-init-checks-XXXXXX";dir=mkdtemp(temp);
 put(pool+8,0,4);put(pool+12,64,4);put(pool+16,heap,8);put(pool+0x14040,0,4);put(pool+0x14044,4,4);
 put(heap,4u<<6,2);for(unsigned i=0;i<4;++i)memory[heap+2+i]="None"[i];put(heap+8,12u<<6,2);for(unsigned i=0;i<12;++i)memory[heap+10+i]="ByteProperty"[i];
 for(size_t i=0;i<sizeof(backing);++i)backing[i]=memory.count(heap+i)?memory[heap+i]:0;
 extra_guest=0x63d590;std::memcpy(extra_backing,rc::fnameprofile::kPrefix,sizeof(rc::fnameprofile::kPrefix));
 reset();
 if(scenario=="success"){}
 else if(scenario=="prologue_fault")extra_fault=true;
 else if(scenario=="signature_mismatch")extra_backing[0]^=1;
 else if(scenario=="roundtrip_refused")bad_output=true;
 else if(scenario=="native_exception")ctor_throw=true;
 else if(scenario=="admission_refused")negative=pool+8;
 else if(scenario=="early_seed_refused")early_failure=true;
 else if(scenario=="cancelled")check_exception=true;
 else if(scenario=="retry_exhaustion"){heap_unavailable=true;scripted_writes.assign(64,int64_t(int32_t(0x80020001u)));for(int i=0;i<64;++i)bridge_kernel_write_copyin_probe(guest,32);}
 else return 2;
 bool threw=false;std::string error;try{checks();}catch(const std::exception&e){threw=true;error=e.what();}
 check(threw==expected_failure,"actual initializer returns or refuses according to unchanged admission/signature/roundtrip checks");
 check(live_fds.empty()&&completion_calls==1,"actual initializer check scope closes unused bank once on success refusal or exception");
 if(scenario=="success")check(ctor_bound&&bSeedBoundFNameCtor&&ctor_calls==1,"successful checks bind native FName before bank retirement at Phase5 boundary");
 if(scenario=="prologue_fault")check(ctor_calls==0&&!ctor_bound&&bridge_copyin_range_faulted(0x63d590,32)==1,"console-style prologue EFAULT prevents native call and survives descriptor retirement");
 if(scenario=="signature_mismatch"||scenario=="admission_refused")check(ctor_calls==0&&!ctor_bound,"signature or admission refusal makes zero native calls");
 if(scenario=="retry_exhaustion")check(fake_ms==60000&&write_calls==64&&late_opens==0&&ctor_calls==0,"unavailable retry exhaustion respects shared deadline and never acquires after startup");
 bridge_copyin_initializer_leave();bridge_copyin_directory_release();for(const auto&p:std::filesystem::directory_iterator(dir))std::filesystem::remove(p.path());std::filesystem::remove(dir);
 std::cout<<"scenario="<<scenario<<" completion_calls="<<completion_calls<<" ctor_calls="<<ctor_calls<<" elapsed_ms="<<fake_ms<<" error="<<error<<"\n";
 std::cout<<"failures="<<failures<<"\n";return failures?1:0;
}
'''.replace('SCOPE',scope).replace('PHASE',phase)
p=run/'initializer_lifecycle.cpp';p.write_text(adapters+pre+adapter+shim)
rows=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 binary=run/mode;cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-misleading-indentation','-O1',*extra,'-I',str(run),'-I',str(root/'units'),str(p),'-o',str(binary)]
 c=subprocess.run(cmd,capture_output=True,text=True);row={'mode':mode,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr,'cases':[]}
 print(mode,'compile',c.returncode)
 if c.returncode:print(c.stderr)
 else:
  for case in ['success','prologue_fault','signature_mismatch','roundtrip_refused','native_exception','admission_refused','early_seed_refused','cancelled','retry_exhaustion']:
   r=subprocess.run([str(binary),case],capture_output=True,text=True,timeout=30);row['cases'].append({'scenario':case,'argv':[str(binary),case],'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(case,r.returncode,r.stdout.strip(),r.stderr)
 rows.append(row)
receipt={'scope':'actual extracted initializer entry cleanup scope plus admission/signature/native-boundary/roundtrip/bind through Phase5 boundary; actual bridge','sources':{'initializer_sha256':hashlib.sha256(src.encode()).hexdigest(),'bridge_sha256':hashlib.sha256(bridge.encode()).hexdigest()},'native_boundary_replacements':['clock','Sony I/O adapted to real host descriptors','read_direct CPU copy -> fixture map','native constructor function address -> same-ABI host fixture','phase1/2 unchanged code excluded; early refusal injected after actual function-entry scope'],'baseline_missing_hook_stubs':not has_hooks,'runs':rows}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'receipts'/('INITIALIZER-LIFECYCLE-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and len(r['cases'])==9 and all(c['exit']==0 for c in r['cases']) for r in rows) else 1)
