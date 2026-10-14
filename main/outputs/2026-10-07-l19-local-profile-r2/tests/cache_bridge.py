#!/usr/bin/env python3
"""Exercise real extracted bridge cache/I/O/lifecycle with a small pinned ELF."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,struct,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
label=sys.argv[1] if len(sys.argv)>1 else 'GREEN'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-CACHE-BRIDGE-'+label+'-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
bridge=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text();fixture=(root/'tests/test_retained.cpp').read_text()
def section(s,a,b):
 p=s.index(a);return s[p:s.index(b,p)]
sink=section(bridge,'// ---- HOST-TEST-EXTRACT-BEGIN','// Legacy explicit diagnostics only.').replace('std::chrono::steady_clock::now()','TestClock::now()').replace('rc::codeprofile::kProfile','host_profile')
missing='bridge_code_cache_context(' not in sink
if missing:sink+='''\nstatic rc::codecache::Result bridge_code_cache_prepare(uint64_t,uint64_t,uint64_t,uint32_t,bool=false,const rc::imagecoverage::Mapping*=nullptr,size_t=0){return {};}
extern "C" int bridge_code_cache_context(uint64_t,const rc::codecache::Metadata**o){if(o)*o=nullptr;return 0;}
extern "C" int bridge_code_cache_signature(uint64_t,uint64_t,const uint8_t*,size_t){return 0;}
static void bridge_code_cache_clear_after_join(){}
static void bridge_startup_resources_release(){bridge_copyin_directory_release();bridge_code_cache_clear_after_join();}
'''.replace('Mapping*=','Mapping* =')
(run/'sink_extract.inc').write_text(sink)
(run/'stop_extract.inc').write_text(section(bridge,'extern "C" int modbridge_runtime_stop(void)','extern "C" int modbridge_runtime_start_with_activation'))
adapters=fixture[:fixture.index('namespace modbridge { namespace native {')]
adapters=adapters.replace('if(fd>=0)live_fds.push_back(fd);','if(fd>=0){live_fds.push_back(fd);if(!(flags&O_CREAT)&&std::string(p)!="/data/plugins"){image_fd=fd;if(recycle_image)recycle_target=fd;}}',1)
adapters=adapters.replace('    if(close_error){errno=EIO;return -1;}return rc;','    if(fd==image_fd&&image_close_shape){errno=0;return image_close_shape==1?-1:1;}\n    if(close_error){errno=EIO;return -1;}return rc;')
owner='''namespace modbridge {namespace native {
static bool quiesced=false;
struct TestOwner {~TestOwner(){const rc::codecache::Metadata*m=nullptr;quiesced=bridge_code_cache_context(0x400000,&m)==1;}};
static bool StopRuntimeWithAck(std::unique_ptr<TestOwner>&slot){slot.reset();return true;}
}}
static std::unique_ptr<modbridge::native::TestOwner>&owner_slot(){static std::unique_ptr<modbridge::native::TestOwner>o;return o;}
static constexpr int MRS_ERR_STOP_EXN=5;
#include "stop_extract.inc"
'''
main=r'''
int main(int argc,char**argv){if(argc!=3)return 2;char temp[]="/tmp/l18-cache-bridge-XXXXXX";dir=mkdtemp(temp);
 std::filesystem::copy_file(argv[1],dir+"/fixture.elf");std::string image=dir+"/fixture.elf";
 host_profile={image.c_str(),argv[2],0x10bb8,0x4987a0,0x400000,rc::codeprofile::kPins,2};
 restored=false;bridge_copyin_directory_prepare();
 auto r=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2);
 check(r.ok&&live_fds.size()==65&&pread_calls==6,"actual bridge authenticates all known pins and closes image fd before worker");
 restored=true;check(bridge_copyin_initializer_enter()==1,"initializer ownership begins after startup cache preparation");
 const rc::codecache::Metadata*m=nullptr;
 check(bridge_code_cache_context(0x400000,&m)==1&&m&&m->count==2,"actual bridge exposes immutable cache identity to initializer owner");
 check(bridge_code_cache_context(0,&m)==0&&m==nullptr&&bridge_code_cache_context(0x404000,&m)==0,"absent or mismatched module view is refused");
 const auto nonce=g_copyin_probe_nonce;const auto telemetry=g_last_probe_telemetry;const size_t event_count=events.size();const auto fault_count=g_copyin_faults.count;
 std::atomic<bool>foreign_ok{true};std::vector<std::thread>threads;
 for(unsigned i=0;i<4;++i)threads.emplace_back([&]{for(unsigned j=0;j<100;++j){const rc::codecache::Metadata*x=reinterpret_cast<const rc::codecache::Metadata*>(1);if(bridge_code_cache_context(0x400000,&x)!=0||x!=nullptr||bridge_code_cache_signature(0x400000,0x63d590,rc::fnameprofile::kPrefix,32)!=0)foreign_ok=false;}});
 for(auto&t:threads)t.join();
 check(foreign_ok&&events.size()==event_count&&g_copyin_probe_nonce==nonce&&g_copyin_faults.count==fault_count&&std::memcmp(&telemetry,&g_last_probe_telemetry,sizeof(telemetry))==0,"foreign cache access refuses before unsynchronized telemetry/nonce/ledger/cache state");
 check(bridge_code_cache_signature(0x400000,0x63d590,rc::fnameprofile::kPrefix,32)==1&&write_calls==0&&late_opens==0,"authenticated prefix lookup performs no target probe or late file open");
 g_copyin_faults.record(0x63d590,32);
 check(bridge_code_cache_signature(0x400000,0x63d590,rc::fnameprofile::kPrefix,32)==0&&g_copyin_faults.count==fault_count+1,"recorded source fault is not rescued or erased by immutable disk proof");
 bridge_copyin_initializer_complete();
 check(live_fds.empty()&&bridge_code_cache_context(0x400000,&m)==1&&g_copyin_faults.count==fault_count+1,"bank retirement preserves cache identity and fault evidence");
 owner_slot().reset(new modbridge::native::TestOwner);
 check(modbridge_runtime_stop()==0&&modbridge::native::quiesced&&bridge_code_cache_context(0x400000,&m)==0,"supported stop destroys owner before clearing immutable cache");
 bridge_copyin_initializer_leave();
 restored=false;bridge_copyin_directory_prepare();host_profile.sha256="0000000000000000000000000000000000000000000000000000000000000000";
 auto bad=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2);restored=true;bridge_copyin_initializer_enter();
 check(!bad.ok&&bridge_code_cache_context(0x400000,&m)==0,"wrong SHA refuses fresh startup with no stale cache");
 bool started=false;l13_sink::with_startup_cleanup(started,bridge_startup_resources_release,[](){return 1;});
 check(live_fds.empty()&&bridge_code_cache_context(0x400000,&m)==0,"failed startup cleanup closes complete bank and drops cache");
 bridge_copyin_initializer_leave();bridge_startup_resources_release();host_profile.sha256=argv[2];restored=false;
 for(int shape:{1,2}){
  image_close_shape=shape;recycle_image=true;closes.clear();
  auto failed_close=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2);image_close_shape=0;recycle_image=false;
  const int consumed=image_fd;bridge_startup_resources_release();bridge_startup_resources_release();
  check(!failed_close.ok&&!g_code_cache.metadata.ready&&failed_close.close_raw==(shape==1?-1:1)&&failed_close.close_error==EIO&&sentinel_fd==consumed&&::fcntl(sentinel_fd,F_GETFD)!=-1&&closes[consumed]==1,"raw nonzero close consumes image FD once; failed-start/repeated cleanup preserves reused unrelated sentinel");
  ::close(sentinel_fd);sentinel_fd=-1;
 }
 events.clear();auto missing_geometry=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2,true,nullptr,0);
 check(!missing_geometry.ok&&!g_code_cache.metadata.ready&&event("code_image_cache_ok")==0&&event("code_image_reason_len")>0&&event("code_image_path_len")==image.size(),"obtained module-info missing geometry refuses with full configured-path and reason diagnostics");
 rc::imagecoverage::Mapping nonexec[]={{0x400000,0x8987a0,1}};auto contradiction=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2,true,nonexec,1);
 check(contradiction.ok&&rc::codecache::named_entry(g_code_cache.metadata,0x63d590,32)==nullptr,"common identity rejects contradictory obtained non-executable module segment before either signature route");
 rc::imagecoverage::Mapping exec[]={{0x400000,0x8987a0,4}};auto agreement=bridge_code_cache_prepare(0x400000,0x400000,0x8987a0,2,true,exec,1);
 check(agreement.ok&&rc::codecache::named_entry(g_code_cache.metadata,0x63d590,32)!=nullptr,"matching obtained executable module segment preserves authenticated named prefix");
 bridge_startup_resources_release();for(const auto&p:std::filesystem::directory_iterator(dir))std::filesystem::remove(p.path());std::filesystem::remove(dir);
 std::printf("failures=%d\n",failures);return failures?1:0;
}
'''
p=run/'cache_bridge.cpp';p.write_text('#include "CodeSignatureProfile.hpp"\n#include "ImageCoverage.hpp"\nstatic rc::codecache::Profile host_profile{};\nstatic int image_fd=-1,image_close_shape=0;static bool recycle_image=false;\n'+adapters+owner+main)
data=bytearray(0x10bb8);data[:7]=b'\x7fELF\x02\x01\x01';struct.pack_into('<HHI',data,16,0xfe10,62,1);struct.pack_into('<Q',data,32,64);struct.pack_into('<HHH',data,52,64,56,2)
struct.pack_into('<IIQQQQQQ',data,64,1,1,0x400,0x23d290,0,0x8000,0x8000,0x4000)
struct.pack_into('<IIQQQQQQ',data,120,1,1,0x8400,0x4907a0,0,0x8000,0x8000,0x4000)
data[0x700:0x720]=bytes.fromhex('554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690');data[0x8700:0x8719]=bytes.fromhex('554889e54157415641554154534881ec58020000488b054d14')
image=run/'fixture.elf';image.write_bytes(data);sha=hashlib.sha256(data).hexdigest()
rows=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('tsan',['-fsanitize=thread','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 exe=run/mode;cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-O1',*extra,'-I',str(run),'-I',str(root/'units'),str(p),'-pthread','-o',str(exe)]
 c=subprocess.run(cmd,capture_output=True,text=True);row={'mode':mode,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr}
 if c.returncode==0:
  argv=[str(exe),str(image),sha];r=subprocess.run(argv,capture_output=True,text=True,timeout=30);row.update(run_argv=argv,run_exit=r.returncode,stdout=r.stdout,stderr=r.stderr)
 rows.append(row);print(mode,c.returncode,row.get('run_exit'),row.get('stdout',c.stderr),row.get('stderr',''))
receipt={'scope':'real extracted bridge; real host descriptors; small ELF with actual FName32/SCO25 pins and named-address mapping; profile substitution at public boundary','missing_hook_stubs':missing,'runs':rows,'source_sha256':hashlib.sha256(bridge.encode()).hexdigest()}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'reports'/('CACHE-BRIDGE-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and r.get('run_exit')==0 for r in rows) else 1)
