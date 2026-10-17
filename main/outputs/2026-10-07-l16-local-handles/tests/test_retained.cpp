#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <string>
#include <vector>
#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <cstdarg>
static std::vector<std::pair<std::string,uint32_t>> events;
extern "C" void diagnostic_event(const char *s,uint32_t v){events.emplace_back(s,v);}
static std::string dir;
static bool restored=false;
static int late_opens=0,create_calls=0,write_calls=0,pread_calls=0;
static int create_error=0,fail_create_at=0,read_error=0,close_error=0;
static size_t read_cap=0;
static unsigned delay_us=0;
static uint64_t startup_clock_jump=0;
static uint64_t clock_offset_ms=0;
struct TestClock { static auto now(){return std::chrono::steady_clock::now()+std::chrono::milliseconds(clock_offset_ms);} };
static std::vector<int> live_fds;
static std::vector<int64_t> scripted_writes;
static uint8_t backing[0x3000];
static constexpr uint64_t guest=0x22c0000000ull;
static std::string translate(const char*p){return dir+"/"+std::string(p).substr(std::string(p).find_last_of('/')+1);}
static uid_t test_getuid(){return restored?42:0;}
static uid_t test_geteuid(){return restored?42:0;}
static gid_t test_getgid(){return restored?43:0;}
static gid_t test_getegid(){return restored?43:0;}
#define getuid test_getuid
#define geteuid test_geteuid
#define getgid test_getgid
#define getegid test_getegid
#include "sink_extract.inc"
#undef getuid
#undef geteuid
#undef getgid
#undef getegid
extern "C" int sceKernelIsInSandbox(){return restored?1:0;}
extern "C" int sysctlbyname(const char*,void*p,size_t*n,const void*,size_t){
    if(restored){errno=EPERM;return -1;} if(*n!=sizeof(int))return -1;
    *static_cast<int*>(p)=0;*n=sizeof(int);return 0;
}
extern "C" int sceKernelOpen(const char*p,int flags,uint16_t mode){
    if(restored){++late_opens;return int32_t(0x80020001u);}
    if(flags&O_CREAT){++create_calls;if(startup_clock_jump && create_calls==2)clock_offset_ms+=startup_clock_jump;
        if(create_error && (!fail_create_at||create_calls==fail_create_at))return create_error;}
    const int fd=std::string(p)=="/data/plugins"? ::open(dir.c_str(),flags,mode) : ::open(translate(p).c_str(),flags,mode);
    if(fd>=0)live_fds.push_back(fd);
    return fd;
}
extern "C" int _openat(int fd,const char*p,int flags,...){
    if(restored){++late_opens;errno=EPERM;return -1;}
    va_list ap;va_start(ap,flags);int mode=va_arg(ap,int);va_end(ap);
    const int result=::openat(fd,p,flags,mode);if(result>=0)live_fds.push_back(result);return result;
}
extern "C" int sceKernelClose(int fd){
    live_fds.erase(std::remove(live_fds.begin(),live_fds.end(),fd),live_fds.end());
    const int rc=::close(fd);if(close_error){errno=EIO;return -1;}return rc;
}
extern "C" int64_t sceKernelWrite(int fd,const void*p,size_t n){
    const uint64_t a=reinterpret_cast<uint64_t>(p);++write_calls;if(delay_us)usleep(delay_us);
    int64_t result=write_calls<=int(scripted_writes.size())?scripted_writes[write_calls-1]:int64_t(n);
    if(result<=0||uint64_t(result)>n)return result;
    if(a>=guest&&a-guest+uint64_t(result)<=sizeof(backing))return ::write(fd,backing+(a-guest),size_t(result));
    errno=EFAULT;return -1;
}
extern "C" int64_t sceKernelRead(int fd,void*p,size_t n){return ::read(fd,p,read_cap?std::min(n,read_cap):n);}
extern "C" int64_t sceKernelPread(int fd,void*p,size_t n,int64_t off){
    ++pread_calls;if(read_error)return read_error;
    return ::pread(fd,p,read_cap?std::min(n,read_cap):n,off);
}
static int failures=0;
static void check(bool v,const char*name){std::printf("%s %s\n",v?"PASS":"FAIL",name);if(!v)++failures;}
static uint32_t event(const char*s){for(auto i=events.rbegin();i!=events.rend();++i)if(i->first==s)return i->second;return 0xdeadbeef;}
static void reset(){
    close_error=0;bridge_copyin_directory_release();restored=false;create_error=0;fail_create_at=0;
    events.clear();clock_offset_ms=0;startup_clock_jump=0;g_copyin_faults={};g_copyin_probe_nonce=0;create_calls=0;
    bridge_copyin_directory_prepare();restored=true;
    late_opens=0;write_calls=0;pread_calls=0;scripted_writes.clear();read_error=0;read_cap=0;delay_us=0;
    for(auto&v:g_write_copyin_capture_valid)v=false;
}
namespace modbridge { namespace native {
static int stop_mode=0;static bool quiesced=false;
struct TestOwner { ~TestOwner(){quiesced=true;} };
static bool StopRuntimeWithAck(std::unique_ptr<TestOwner>& slot) {
    if(stop_mode==1)throw 7;
    slot.reset();return stop_mode==0;
}
} }
static std::unique_ptr<modbridge::native::TestOwner>& owner_slot(){static std::unique_ptr<modbridge::native::TestOwner> owner;return owner;}
static constexpr int MRS_ERR_STOP_EXN=5;
#include "stop_extract.inc"
int main(){
    char temp[]="/tmp/l16-retained-tests-XXXXXX";dir=mkdtemp(temp);for(size_t i=0;i<sizeof(backing);++i)backing[i]=uint8_t(i);
    reset();check(bridge_kernel_write_copyin_probe(guest,32)==1,"retained actual sink passes when late create is EPERM");
    const uint8_t* bytes=nullptr;uint32_t count=0;
    check(bridge_write_copyin_capture(guest,&bytes,&count)==1&&count==32&&bytes[31]==31,"fresh positional readback yields exact source bytes");
    check(late_opens==0,"probe performs no late path create or reopen");
    check(std::find(events.begin(),events.end(),std::make_pair(std::string("copyin_uid"),uint32_t(0)))!=events.end(),"startup credential snapshot preserves uid zero observation");
    backing[0]=0x7a;check(bridge_kernel_write_copyin_probe(guest,32)==1&&bridge_write_copyin_capture(guest,&bytes,&count)==1&&bytes[0]==0x7a,"second fresh attempt captures changed live bytes not old cache");backing[0]=0;
    check(event("copyin_cred_phase")==2&&event("copyin_euid")==42&&event("copyin_sandbox_raw")==1,"delayed credentials logged after restoration");
    check(event("copyin_jail_available")==0&&event("copyin_jail_errno")==1,"unsupported jail query reported unknown not false");
    reset();scripted_writes={3,5,6};check(bridge_kernel_write_copyin_probe(guest,14)==1,"partial write loop completes exact length");
    check(bridge_write_copyin_capture(guest+6,&bytes,&count)==1&&count==8&&bytes[0]==6,"interior capture remains exact");
    reset();scripted_writes={64,int64_t(int32_t(0x8002000eu))};check(bridge_kernel_write_copyin_probe(guest,128)==0,"partial then signed SCE EFAULT refuses");
    check(event("pool_copyin_probe_sink")==64&&pread_calls==0&&bridge_write_copyin_capture(guest,&bytes,&count)==0,"fault does not publish transferred prefix");
    const int prior=write_calls;check(bridge_kernel_write_copyin_probe(guest,6)==0&&write_calls==prior,"known fault prevents subsequent copyin");
    reset();scripted_writes={int64_t(0x8002000eu)};check(bridge_kernel_write_copyin_probe(guest,32)==0,"unsigned SCE EFAULT normalized");
    reset();scripted_writes={0};check(bridge_kernel_write_copyin_probe(guest,32)==-1&&write_calls==1,"zero write refuses without loop");
    reset();scripted_writes={33};check(bridge_kernel_write_copyin_probe(guest,32)==-1&&pread_calls==0,"oversized count refuses");
    reset();close_error=1;check(bridge_kernel_write_copyin_probe(guest,32)==-1&&bridge_write_copyin_capture(guest,&bytes,&count)==0,"close error prevents success publication");
    check(event("copyin_file_op")==4&&event("copyin_file_errno")==EIO,"close failure logs path flags error and credentials");
    reset();read_cap=4;check(bridge_kernel_write_copyin_probe(guest,32)==-1&&bridge_write_copyin_capture(guest,&bytes,&count)==0,"short readback never publishes stale data");
    reset();read_error=int32_t(0x80020001u);check(bridge_kernel_write_copyin_probe(guest,32)==-1,"pread SCE EPERM refuses");
    check(event("copyin_file_op")==3&&event("copyin_file_errno")==1&&event("pool_copyin_path_len")>0,"file failure includes operation path flags and credentials");
    reset();check(bridge_kernel_write_copyin_probe_until(guest,32,0)==-1&&write_calls==0&&late_opens==0,"expired shared deadline starts no file operation");
    reset();delay_us=10000;check(bridge_kernel_write_copyin_probe_until(guest,32,bridge_copyin_now_ms()+5)==-1&&pread_calls==0,"late write cannot publish or start readback");
    reset();check(bridge_kernel_write_copyin_probe(UINT64_MAX-7,32)==-1&&write_calls==0,"overflow refuses before source read");
    reset();for(int i=0;i<64;++i)if(bridge_kernel_write_copyin_probe(guest,32)!=1){check(false,"bounded bank handles each of 64 attempts");break;}
    const int before_cap=write_calls;check(bridge_kernel_write_copyin_probe(guest,32)==-1&&write_calls==before_cap,"exhausted bank refuses no late creation");
    reset();const int before_stop=int(live_fds.size());bridge_copyin_directory_release();check(before_stop>1&&live_fds.empty(),"stop closes parent and unused probe handles");
    restored=false;create_error=int32_t(0x80020002u);events.clear();bridge_copyin_directory_prepare();restored=true;
    check(bridge_kernel_write_copyin_probe(guest,32)==-1&&live_fds.empty(),"failed startup allocation refuses without leaked handles");
    check(event("copyin_file_errno")==2&&event("pool_copyin_open_flags")&O_CREAT,"startup ENOENT logs exact creation flags");
    reset();bridge_copyin_directory_release();restored=false;create_calls=0;create_error=int32_t(0x80020002u);fail_create_at=3;
    bridge_copyin_directory_prepare();check(live_fds.empty(),"partial bank acquisition closes all owned descriptors");
    reset();bool owner=false;l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[](){return 2;});check(live_fds.empty(),"failed owner startup cleanup closes complete bank");
    reset();try{l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[]()->int{throw 7;});}catch(int){}check(live_fds.empty(),"exception cleanup closes complete bank");
    reset();const int owned=int(live_fds.size());l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[&](){owner=true;return 0;});check(int(live_fds.size())==owned,"successful runtime keeps descriptors until stop");
    reset();bridge_copyin_directory_release();restored=false;create_calls=0;startup_clock_jump=6000;
    bridge_copyin_directory_prepare();check(live_fds.empty()&&create_calls==2,"startup allocation budget expires before next create and closes bank");
    owner=false;
    reset();owner_slot().reset(new modbridge::native::TestOwner);modbridge::native::quiesced=false;modbridge::native::stop_mode=1;
    check(modbridge_runtime_stop()==5&&modbridge::native::quiesced&&live_fds.empty(),"stop exception releases descriptors after owner destruction");
    reset();owner_slot().reset(new modbridge::native::TestOwner);modbridge::native::quiesced=false;modbridge::native::stop_mode=2;
    check(modbridge_runtime_stop()==1&&modbridge::native::quiesced&&live_fds.empty(),"negative stop with destroyed owner releases descriptors");
    bridge_copyin_directory_release();std::printf("failures=%d\n",failures);return failures?1:0;
}
