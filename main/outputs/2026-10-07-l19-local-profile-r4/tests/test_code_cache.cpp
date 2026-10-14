#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#if __has_include("CodeImageCache.hpp")
#include "CodeImageCache.hpp"
#else
// RED only: missing feature is a refusal, so assertions compile and fail.
namespace rc::codecache {
struct Pin{uint32_t id;const char*name;uint64_t linked;const uint8_t*bytes;size_t size;};
struct Profile{const char*path;const char*sha256;uint64_t size,extent,base;const Pin*pins;size_t count;};
struct Result{bool ok=false;};struct Metadata{bool ready=false;uint64_t base=0,start_ms=0,deadline_ms=0;size_t count=0;};
struct Cache{Metadata metadata{};template<class Ops>Result prepare(const Profile&,uint64_t,uint64_t,uint64_t,uint64_t,Ops&){return {};}void clear(){}bool identity(uint64_t)const{return false;}const void*prefix(uint64_t,const uint8_t*,size_t)const{return nullptr;}};
}
#endif
namespace c=rc::codecache;
static unsigned checks=0,failures=0;
static void check(bool value,const char*label){++checks;if(!value){++failures;std::printf("FAIL: %s\n",label);}}
struct Ops{
 std::vector<uint8_t> bytes;uint64_t clock=10,read_cost=0;size_t max_read=SIZE_MAX;unsigned opens=0,reads=0,closes=0;int fail_read=0;int open_result=7,close_result=0;bool throw_read=false;bool open_fd=false;
 int open(const char*){++opens;open_fd=open_result>=0;return open_result;}
 int64_t pread(int fd,void*out,size_t n,uint64_t off){++reads;if(throw_read)throw std::runtime_error("read exception");if(fd!=7||!open_fd)std::abort();clock+=read_cost;if(fail_read==int(reads))return int64_t(0x80020005u);if(off>=bytes.size())return 0;size_t take=bytes.size()-size_t(off);if(take>n)take=n;if(take>max_read)take=max_read;std::memcpy(out,bytes.data()+off,take);return int64_t(take);}
 int close(int fd){if(fd!=7||!open_fd)std::abort();++closes;open_fd=false;errno=0;return close_result;}
 uint64_t now_ms(){return clock;}
};
int main(int argc,char**argv){if(argc!=4)return 2;std::ifstream f(argv[1],std::ios::binary);Ops o;o.bytes.assign(std::istreambuf_iterator<char>(f),{});std::string scenario=argv[3];
 const uint8_t a[]={0x55,0x48,0x89,0xe5},b[]={0x41,0x57,0x41,0x56,0x41};
 c::Pin pins[]={{1,"fname-test",0x300,a,sizeof(a)},{2,"sco-test",0x8300,b,sizeof(b)}};
 c::Profile profile{argv[1],argv[2],0x10bb8,0x10000,0x400000,pins,2};
 uint64_t observed=profile.base,resolved=profile.base,end=profile.base+profile.extent;bool expected=true;
 if(scenario=="good"){}
 else if(scenario=="short_reads")o.max_read=127;
 else if(scenario=="wrong_sha"){profile.sha256="0000000000000000000000000000000000000000000000000000000000000000";expected=false;}
 else if(scenario=="zero_base"){observed=0;expected=false;}
 else if(scenario=="resolved_base"){resolved+=0x4000;expected=false;}
 else if(scenario=="wrong_base"){observed+=0x4000;resolved=observed;expected=false;}
 else if(scenario=="wrong_extent"){end+=0x4000;expected=false;}
 else if(scenario=="open_error"){o.open_result=int32_t(0x80020002u);expected=false;}
 else if(scenario=="read_error_first"){o.fail_read=1;expected=false;}
 else if(scenario=="read_error_middle"){o.fail_read=3;expected=false;}
 else if(scenario=="read_error_eof"){o.fail_read=6;expected=false;}
 else if(scenario=="no_progress"){o.max_read=0;expected=false;}
 else if(scenario=="close_reuse"){o.close_result=int32_t(0x80020005u);expected=false;}
 else if(scenario=="close_minus1_errno0"){o.close_result=-1;expected=false;}
 else if(scenario=="close_positive"){o.close_result=1;expected=false;}
 else if(scenario=="deadline"){o.read_cost=20000;expected=false;}
 else if(scenario=="read_exception"){o.throw_read=true;expected=false;}
 else if(scenario=="duplicate_pin"){pins[1]=pins[0];expected=false;}
 else expected=false; // parser/truncation/extra/prefix mutations generated independently
 c::Cache cache;bool threw=false;bool ok=false;try{ok=cache.prepare(profile,observed,resolved,end,42,o).ok;}catch(const std::runtime_error&){threw=true;}
 check(ok==expected,"preparation accepts authenticated complete image only");
 check(cache.metadata.ready==expected,"no partial cache published on refusal");
 if(expected){
  check(cache.metadata.base==profile.base&&cache.metadata.start_ms==10&&cache.metadata.deadline_ms==60010&&cache.metadata.count==2,"immutable identity carries original shared deadline and accounts every pin");
  check(cache.identity(profile.base)&&!cache.identity(0)&&!cache.identity(profile.base+1),"module view must be nonzero actual base equal to startup and profile");
  check(cache.prefix(profile.base+pins[0].linked,a,sizeof(a))&&cache.prefix(profile.base+pins[1].linked,b,sizeof(b)),"named prefixes map through PT_LOAD file offsets");
  uint8_t wrong[]={0,0,0,0};check(!cache.prefix(profile.base+pins[0].linked,wrong,4)&&!cache.prefix(profile.base+pins[0].linked+1,a,4),"cache cannot supply arbitrary/interior/mismatching signature");
 }
 const bool before_open=scenario=="zero_base"||scenario=="resolved_base"||scenario=="wrong_base"||scenario=="wrong_extent"||scenario=="duplicate_pin";
 if(before_open||scenario=="open_error")check(o.closes==0,"pre-open or failed-open refusal has no close obligation");
 else check(o.closes==1&&!o.open_fd,"image fd closes exactly once on every prepared or failed exit");
 if(scenario=="deadline")check(o.clock==60010&&o.reads==3,"expiration stops before a fourth read with unchanged absolute deadline");
 if(scenario=="read_exception")check(threw&&o.closes==1,"exception cleanup consumes local fd once");
 cache.clear();cache.clear();check(!cache.metadata.ready&&!cache.identity(profile.base)&&o.closes<=1,"clear drops memory proof and never retries close against reused numeric fd");
 std::printf("scenario=%s checks=%u failures=%u reads=%u closes=%u elapsed_ms=%llu\n",argv[3],checks,failures,o.reads,o.closes,(unsigned long long)o.clock);return failures?1:0;
}
