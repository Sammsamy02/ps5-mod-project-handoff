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
#include <cstdarg>
#include <algorithm>
static std::vector<std::pair<std::string,uint32_t>> events;
extern "C" void diagnostic_event(const char *s,uint32_t v) { events.emplace_back(s,v); }
static std::string dir, opened_path;
static std::vector<int64_t> writes;
static std::vector<uint64_t> cursors;
static int open_error=0, calls=0, open_calls=0, open_flags=0;
static size_t read_cap=0;
static bool closed=false;
static bool sandboxed=false;
static bool read_open_before_close=false;
static unsigned write_delay_us=0;
static int relative_opens=0;
static uint8_t backing[0x3000];
static constexpr uint64_t guest=0x22c0000000ull;
static std::string translate(const char*p){return dir+"/"+std::string(p).substr(std::string(p).find_last_of('/')+1);}
static FILE *host_fopen(const char*p,const char*m){return std::fopen(translate(p).c_str(),m);}
static size_t host_fread(void*b,size_t s,size_t n,FILE*f){return std::fread(b,s,read_cap?std::min(n,read_cap):n,f);}
static int host_unlink(const char*p){return ::unlink(translate(p).c_str());}
#define fopen host_fopen
#define fread host_fread
#define unlink host_unlink
#include "sink_extract.inc"
#undef fopen
#undef fread
#undef unlink
extern "C" int sceKernelOpen(const char*p,int f,uint16_t m){++open_calls;opened_path=p;open_flags=f;if(open_error)return open_error;if(sandboxed){return int32_t(0x80020002u);}closed=false;if(std::string(p)=="/data/plugins")return ::open(dir.c_str(),f,m);return ::open(translate(p).c_str(),f,m);}
extern "C" int _openat(int fd,const char*p,int f,...){va_list ap;va_start(ap,f);int m=va_arg(ap,int);va_end(ap);++relative_opens;if(!(f&O_CREAT)&&!closed)read_open_before_close=true;opened_path=std::string("/data/plugins/")+p;if(f&O_CREAT)open_flags=f;if(open_error)return open_error;return ::openat(fd,p,f,m);}
extern "C" int64_t sceKernelRead(int fd,void*p,size_t n){return ::read(fd,p,read_cap?std::min(n,read_cap):n);}
extern "C" int sceKernelClose(int fd){closed=true;return ::close(fd);}
extern "C" int64_t sceKernelWrite(int fd,const void*p,size_t n){
    const uint64_t a=reinterpret_cast<uint64_t>(p);cursors.push_back(a);++calls;if(write_delay_us)usleep(write_delay_us);
    int64_t r=calls<=int(writes.size())?writes[calls-1]:int64_t(n);
    // A repeated-zero bug must end in a test failure instead of hanging the suite.
    if(calls>100){errno=EIO;return -1;}
    if(r<=0||uint64_t(r)>n)return r;
    if(a>=guest && a-guest+uint64_t(r)<=sizeof(backing))return ::write(fd,backing+a-guest,size_t(r));
    return ::write(fd,p,size_t(r));
}
static int fail=0;
static void check(bool v,const char*s){std::printf("%s %s\n",v?"PASS":"FAIL",s);if(!v)++fail;}
static uint32_t event(const char*s){for(auto i=events.rbegin();i!=events.rend();++i)if(i->first==s)return i->second;return 0xdeadbeef;}
static void reset(){
#if __has_include("CopyinSink.hpp")
bridge_copyin_directory_release();g_copyin_faults={};sandboxed=false;open_error=0;bridge_copyin_directory_prepare();
#endif
events.clear();writes.clear();cursors.clear();open_error=0;calls=0;open_calls=0;read_cap=0;relative_opens=0;read_open_before_close=false;write_delay_us=0;errno=99;for(auto &v:g_write_copyin_capture_valid)v=false;}
int main(){char temp[]="/tmp/l13-sink-tests-XXXXXX";dir=mkdtemp(temp);for(size_t i=0;i<sizeof(backing);++i)backing[i]=uint8_t(i);
    reset();open_error=int32_t(0x80020002u);check(bridge_kernel_write_copyin_probe(guest,14)==-1,"SCE ENOENT open unavailable");check(event("pool_copyin_probe_errno")==2,"SCE signed ENOENT normalized before negation");check(event("pool_copyin_probe_rc")==0x80020002u,"raw SCE open bits retained");check(calls==0,"open error does not touch target");
    reset();writes={int64_t(int32_t(0x8002000eu))};check(bridge_kernel_write_copyin_probe(guest,14)==0,"signed SCE EFAULT unreadable");check(event("pool_copyin_probe_errno")==14,"signed SCE EFAULT normalized");
    reset();writes={int64_t(0x8002000eu)};check(bridge_kernel_write_copyin_probe(guest,14)==0,"unsigned SCE EFAULT unreadable");check(event("pool_copyin_probe_errno")==14,"unsigned SCE EFAULT normalized");
    reset();writes={64,int64_t(int32_t(0x8002000eu))};check(bridge_kernel_write_copyin_probe(guest,128)==0,"partial64 then signed EFAULT unreadable");check(event("pool_copyin_probe_sink")==64,"partial EFAULT counts only transferred prefix");check(event("pool_copyin_probe_fault_off")==64,"partial EFAULT records exact offset");check(cursors.size()==2&&cursors[1]==guest+64,"partial cursor advances by actual count");
    reset();writes={64,int64_t(0x8002000eu)};check(bridge_kernel_write_copyin_probe(guest,128)==0,"partial64 then unsigned EFAULT unreadable");check(event("pool_copyin_probe_sink")==64,"unsigned EFAULT never counted as billions of bytes");
    reset();writes={3,5,6};check(bridge_kernel_write_copyin_probe(guest,14)==1,"partial counts3+5+6 complete14");check(event("pool_copyin_probe_sink")==14,"full transfer counts14");check((open_flags&(O_CREAT|O_TRUNC|O_WRONLY|O_NOFOLLOW))==(O_CREAT|O_TRUNC|O_WRONLY|O_NOFOLLOW),"probe creates unique sink with required flags");check(opened_path.rfind("/data/plugins/modbridge-copyin-p",0)==0,"probe uses same directory as ControlD");
    const uint8_t *data=nullptr;uint32_t n=0;check(bridge_write_copyin_capture(guest+6,&data,&n)==1&&n==8&&data[0]==6,"capture lookup supports interior address");
    reset();writes={0,0,0};check(bridge_kernel_write_copyin_probe(guest,14)==-1,"zero progress unavailable");check(calls==1,"zero progress stops immediately");
    reset();writes={15};check(bridge_kernel_write_copyin_probe(guest,14)==-1,"oversized positive result unavailable");check(event("pool_copyin_probe_sink")==0,"oversized result never advances accounting");
    reset();check(bridge_kernel_write_copyin_probe(UINT64_MAX-7,14)==-1&&open_calls==0,"overflow range rejected before IO");
    reset();read_cap=4;check(bridge_kernel_write_copyin_probe(guest,14)==-1,"short readback unavailable");check(closed&&!read_open_before_close,"write handle closes before readback");
#if __has_include("CopyinSink.hpp")
    reset();const auto nonce_before=g_copyin_probe_nonce;check(calls==0&&relative_opens==0,"directory setup performs no target writes or sink creates");
    check(bridge_kernel_write_copyin_probe(guest,14)==1,"sink available for exact diagnostics");std::string decoded;for(const auto&e:events)if(e.first=="pool_copyin_path_word")for(unsigned j=0;j<4;++j){char c=char(e.second>>(8*j));if(c)decoded.push_back(c);}check(decoded==opened_path&&event("pool_copyin_path_len")==decoded.size(),"diagnostics losslessly encode exact pathname");check(event("pool_copyin_open_flags")==uint32_t(O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_EXCL)&&event("pool_copyin_open_mode")==0600,"diagnostics expose exact create flags and mode");check(g_copyin_probe_nonce==nonce_before+1,"one nonce consumed per lazy sink creation");
    reset();write_delay_us=10000;const uint64_t deadline=bridge_copyin_now_ms()+5;check(bridge_kernel_write_copyin_probe_until(guest,14,deadline)==-1&&relative_opens==1&&closed,"write returning after deadline closes and skips readback");check(bridge_write_copyin_capture(guest,&data,&n)==0,"late result cannot publish capture");
    reset();check(bridge_kernel_write_copyin_probe(guest,14)==1,"initial capture before fault exists");writes={int64_t(int32_t(0x8002000eu))};calls=0;check(bridge_kernel_write_copyin_probe(guest,14)==0,"new fault supersedes old capture");check(bridge_write_copyin_capture(guest,&data,&n)==0,"prior capture invalidated by fault");int prior_calls=calls;check(bridge_kernel_write_copyin_probe(guest,6)==0&&calls==prior_calls,"known fault prevents later probe from overriding provenance");
    reset();check(l13_sink::error(int64_t(0x80020002u),0)==2,"unsigned SCE ENOENT decoder");check(l13_sink::error(-1,EFAULT)==14,"POSIX errno decoder");check(l13_sink::error(-14,0)==14,"direct negative errno decoder");
    reset();check(bridge_copyin_range_faulted(guest,128)==0,"fresh range has no fault provenance");writes={64,int64_t(0x8002000eu)};check(bridge_kernel_write_copyin_probe(guest,128)==0,"fault captured for range");check(bridge_copyin_range_faulted(guest+6,6)==1,"fault history excludes entire originally requested range conservatively");check(bridge_write_copyin_capture(guest,&data,&n)==0,"capture cannot override fault history");
    reset();check(bridge_kernel_write_copyin_probe_until(guest,14,0)==-1&&open_calls==0&&calls==0,"expired deadline prevents all sink work");
    reset();sandboxed=true;check(sceKernelOpen("/data/plugins/absolute-missing",O_CREAT|O_WRONLY,0600)==int32_t(0x80020002u),"fixture rejects absolute lookup after sandbox restore");relative_opens=0;check(bridge_kernel_write_copyin_probe(guest,14)==1&&relative_opens==2,"retained parent creates and reads back after sandbox restore");
    check(bridge_write_copyin_capture(guest,&data,&n)==1&&n==14&&data[13]==13,"retained-parent readback returns actual source bytes");
    reset();bridge_copyin_directory_release();open_error=int32_t(0x80020002u);bridge_copyin_directory_prepare();open_error=0;check(bridge_kernel_write_copyin_probe(guest,14)==-1&&calls==0,"missing retained parent fails closed without target reads");
#else
    check(false,"retained parent, deadline and fault ledger capabilities required");
#endif
#if __has_include("CopyinSink.hpp")
    reset();uint32_t old_nonce=g_copyin_probe_nonce;g_copyin_probe_nonce=64;check(bridge_kernel_write_copyin_probe(guest,14)==-1&&relative_opens==0,"process sink cap stops new file creation");g_copyin_probe_nonce=old_nonce;
    l13_sink::FaultLedger ledger;for(uint64_t i=0;i<65;++i)ledger.record(0x10000+i*0x1000,14);check(ledger.overlaps(0x900000,14),"fault ledger saturation cannot discard fault history");
#endif
    reset();g_copyin_probe_nonce=0;check(bridge_kernel_write_copyin_probe(guest,14)==1,"first generation creates sink");std::string generation_one_path=opened_path;
    reset();g_copyin_probe_nonce=0;check(bridge_kernel_write_copyin_probe(guest,14)==1,"second generation creates sink");check(opened_path!=generation_one_path,"same address and reset counter have distinct startup-generation names");
    const std::string collision_path=opened_path;g_copyin_probe_nonce=0;calls=0;backing[0]=0xff;check(bridge_kernel_write_copyin_probe(guest,14)==-1&&calls==0,"existing exact basename collision refuses before target write");FILE *prior=std::fopen(translate(collision_path.c_str()).c_str(),"rb");int first_byte=prior?std::fgetc(prior):-1;if(prior)std::fclose(prior);check(first_byte==0,"existing evidence bytes survive collision unchanged");backing[0]=0;
#if __has_include("CopyinStartup.hpp")
    reset();int failed_fd=g_copyin_parent_fd;bool owner=false;int refused=l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[](){return 2;});errno=0;check(refused==2&&g_copyin_parent_fd==-1&&fcntl(failed_fd,F_GETFD)==-1&&errno==EBADF,"failed startup closes retained directory without module_stop");
    reset();failed_fd=g_copyin_parent_fd;try{l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[]()->int{throw 7;});}catch(int){}errno=0;check(g_copyin_parent_fd==-1&&fcntl(failed_fd,F_GETFD)==-1&&errno==EBADF,"startup exception closes retained directory without module_stop");
    reset();int live_fd=g_copyin_parent_fd;l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[&](){owner=true;return 0;});check(g_copyin_parent_fd==live_fd&&fcntl(live_fd,F_GETFD)!=-1,"successful owner startup retains directory while init may run");
    reset();live_fd=g_copyin_parent_fd;owner=true;try{l13_sink::with_startup_cleanup(owner,bridge_copyin_directory_release,[]()->int{throw 7;});}catch(int){}check(g_copyin_parent_fd==live_fd&&fcntl(live_fd,F_GETFD)!=-1,"existing owner protects directory even on exception");
#else
    check(false,"startup scope must close parent on failure without closing under live owner");
#endif
    std::printf("failures=%d\n",fail);return fail?1:0;
}
