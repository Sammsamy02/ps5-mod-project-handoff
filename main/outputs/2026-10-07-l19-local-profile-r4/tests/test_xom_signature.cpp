#include "NativeFNameChecks.hpp"
#include <cstdio>
#include <cstring>
namespace a=rc::fnameabi;namespace p=rc::pooladmission;
static unsigned checks=0, failures=0;
static void check(bool value,const char* label){++checks;if(!value){++failures;std::printf("FAIL: %s\n",label);}}
struct Ops {
 uint64_t clock=0;unsigned queries=0,probes=0,captures=0,cache_lookups=0,logs=0;bool fault=false,identity=true,cache=true;
 uint8_t live[4]={0x55,0x48,0x89,0xe5},disk[4]={0x55,0x48,0x89,0xe5};int probe_result=1;
 p::Query q{p::QueryKind::Available,0x1000,0x2000,4,0x10,0};
 uint64_t now_ms(){return clock;}bool known_fault(uint64_t,size_t){return fault;}
 bool code_identity(uint64_t,size_t){return identity;}
 bool cached_code_signature(uint64_t,const uint8_t* expected,size_t n){++cache_lookups;return cache && n==4 && std::memcmp(disk,expected,n)==0;}
 p::Query query(uint64_t){++queries;return q;}
 void log_region(uint64_t,size_t,const p::Query&){++logs;}
 void log_code_segment(uint64_t,size_t,const char*){++logs;}
 int probe(uint64_t,size_t,p::Budget&){++probes;return probe_result;}
 bool capture(uint64_t,size_t,const uint8_t**b,size_t*n){++captures;*b=live;*n=4;return true;}
};
int main(){const uint8_t expected[4]={0x55,0x48,0x89,0xe5};
 {Ops o;p::Budget b(0);check(a::signature(o,b,0x1000,expected,4).ok && o.cache_lookups==1 && o.probes==0 && o.captures==0,"positive committed XOM uses authenticated disk prefix before any live probe");}
 {Ops o;o.disk[0]^=1;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && o.probes==0,"XOM disk mismatch blocks call without copyin fallback");}
 {Ops o;o.cache=false;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && o.probes==0,"missing XOM cache blocks native call");}
 {Ops o;o.identity=false;o.q.protection=5;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.queries && !o.probes,"wrong image identity blocks READ path before VQ and live probe");}
 {Ops o;o.identity=false;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.queries && !o.cache_lookups,"wrong actual base or image identity blocks XOM before cache lookup");}
 {Ops o;o.q.protection=5;p::Budget b(0);check(a::signature(o,b,0x1000,expected,4).ok && o.probes==1 && o.cache_lookups==0,"READ executable range still demands a fresh live prefix");}
 {Ops o;o.q.protection=5;o.probe_result=0;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && o.cache_lookups==0,"readable EFAULT never uses matching disk cache");}
 {Ops o;o.q.kind=p::QueryKind::Unavailable;p::Budget b(0);check(a::signature(o,b,0x1000,expected,4).ok && o.probes==1 && o.cache_lookups==0,"unavailable VQ retains live-only route");}
 {Ops o;o.q.kind=p::QueryKind::Unmapped;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.cache_lookups && !o.probes,"unmapped range refuses both routes");}
 {Ops o;o.q.end=0x1002;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.cache_lookups,"partial VQ prefix coverage cannot use disk");}
 {Ops o;o.q.flags=0;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.cache_lookups,"uncommitted prefix cannot use disk");}
 {Ops o;o.q.protection=0;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.cache_lookups,"non-executable prefix cannot use disk");}
 {Ops o;o.q.protection=6;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.cache_lookups,"writable non-readable code cannot be presented as immutable XOM");}
 {Ops o;o.fault=true;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.queries && !o.cache_lookups,"recorded fault remains terminal before any new proof");}
 {Ops o;o.clock=60000;p::Budget b(0);check(!a::signature(o,b,0x1000,expected,4).ok && !o.queries && !o.cache_lookups,"original deadline governs cached signatures");}
 std::printf("checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
