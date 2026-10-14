#include "REVIEWED-NativeFNameChecks.hpp"
#include <cstdio>
namespace a=rc::fnameabi; namespace p=rc::pooladmission;
struct Ops {
 unsigned probes=0; unsigned direct=0;
 uint8_t cached[4]={0x55,0x48,0x89,0xe5};
 uint64_t now_ms(){return 0;}
 bool known_fault(uint64_t,size_t){return false;}
 p::Query query(uint64_t){return {p::QueryKind::Available,0x1000,0x2000,4,0x10,0};}
 bool capture(uint64_t,size_t,const uint8_t** b,size_t* n){*b=cached;*n=4;return true;}
 int probe(uint64_t,size_t,p::Budget&){++probes;return -1;}
};
static unsigned calls=0;
static void ctor(a::Name* out,const char16_t*,uint32_t){++calls;out->index=3;}
int main(){Ops o;p::Budget b(0);const uint8_t expected[]={0x55,0x48,0x89,0xe5};
 const auto gate=a::signature(o,b,0x1000,expected,4);
 if(gate.ok)(void)a::invoke(ctor,u"ByteProperty",0);
 std::printf("cached capture, unavailable fresh probe: admitted=%d probes=%u ctor_calls=%u\n",gate.ok,o.probes,calls);
 return (gate.ok || calls)!=0 ? 1 : 0;
}
