#include "NativeFNameChecks.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
namespace a=rc::fnameabi;namespace p=rc::pooladmission;
struct Ops {
    uint64_t clock=0; unsigned direct=0,probes=0;bool fault=false,have=true;
    bool capture_after_probe=false; int probe_rc=1;uint64_t probe_cost=0;
    bool shadow_enabled=false;std::vector<uint8_t> code{0x55,0x48,0x89,0xe5};
    std::map<uint64_t,std::vector<uint8_t>> memory;
    p::Query q{p::QueryKind::Available,0x1000,0x2000,5,0x10,0};
    bool code_identity(uint64_t,size_t){return true;}
    bool cached_code_signature(uint64_t,const uint8_t*,size_t){return false;}
    void log_code_segment(uint64_t,size_t,const char*){}
    uint64_t now_ms(){return clock;}
    bool known_fault(uint64_t,size_t){return fault;}
    p::Query query(uint64_t addr){if(addr>=0x4000)return {p::QueryKind::Available,addr,addr+0x100,1,0x10,0};return q;}
    bool capture(uint64_t,size_t,const uint8_t**b,size_t*n){*b=have?code.data():nullptr;*n=have?code.size():0;return have;}
    int probe(uint64_t,size_t,p::Budget&){++probes;clock+=probe_cost;if(capture_after_probe)have=true;return probe_rc;}
    bool shadow(p::Budget&){assert(false);return false;}
    void log_region(uint64_t,size_t,const p::Query&){}
    void log_image(uint64_t,size_t,const rc::imagecoverage::Hit&){}
    rc::imagecoverage::Hit image_range(uint64_t addr,size_t n){return {true,addr,addr+n,addr+n,6,7,"test","test"};}
    bool is_image(uint64_t addr){return addr<0x100000;}
    void read_direct(uint64_t addr,size_t n,uint8_t*b){++direct;const auto i=memory.find(addr);assert(i!=memory.end() && i->second.size()>=n);for(size_t j=0;j<n;++j)b[j]=i->second[j];}
};
static void word(Ops&o,uint64_t addr,uint64_t value,unsigned n){auto &v=o.memory[addr];for(unsigned i=0;i<n;++i)v.push_back(static_cast<uint8_t>(value>>(8*i)));}
int main(){
    const uint8_t sig[]={0x55,0x48,0x89,0xe5};
    {Ops o;p::Budget b(0);auto r=a::signature(o,b,0x1000,sig,4);assert(r.ok && o.direct==0 && o.probes==1);}
    {Ops o;o.code[0]=0x90;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && o.direct==0);}
    {Ops o;o.have=false;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && o.direct==0 && o.probes==1);}
    {Ops o;o.code.resize(2);p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok);}
    {Ops o;o.fault=true;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && !o.probes);}
    {Ops o;o.clock=60000;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && !o.probes);}
    {Ops o;o.q.protection=1;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok);}
    {Ops o;o.q.start=0x1001;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok);}
    {Ops o;o.q.kind=p::QueryKind::Unavailable;p::Budget b(0);assert(a::signature(o,b,0x1000,sig,4).ok);}
    {Ops o;o.have=false;o.capture_after_probe=true;o.probe_rc=1;p::Budget b(0);assert(a::signature(o,b,0x1000,sig,4).ok && o.probes==1 && o.direct==0);}
    {Ops o;o.have=false;o.probe_rc=0;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && o.direct==0);}
    {Ops o;o.have=false;o.capture_after_probe=true;o.probe_rc=1;o.probe_cost=60000;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && o.direct==0);}
    {Ops o;o.probe_rc=-1;p::Budget b(0);assert(!a::signature(o,b,0x1000,sig,4).ok && o.probes==1 && o.direct==0);}
    Ops o;word(o,0x4010,0x100000,8);word(o,0x100006,12<<6,2);
    for(char c:std::string("ByteProperty"))o.memory[0x100008].push_back(static_cast<uint8_t>(c));
    p::Budget b(0);p::Session s;auto r=a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,0},u"ByteProperty");assert(r.ok);
    assert(!a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,1},u"ByteProperty").ok);
    assert(!a::roundtrip(o,b,s,0x4000,0,100,a::Name{0x10003,0},u"ByteProperty").ok);
    o.memory[0x100008][0]='X';assert(!a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,0},u"ByteProperty").ok);
    o.memory[0x100006].clear();word(o,0x100006,(12<<6)|1,2);o.memory[0x100008].clear();
    for(char16_t c:std::u16string(u"ByteProperty")){o.memory[0x100008].push_back(c&255);o.memory[0x100008].push_back(c>>8);}
    assert(a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,0},u"ByteProperty").ok);
    assert(!a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,0},u"BytePropertyX").ok);
    o.fault=true;const auto reads=o.direct;assert(!a::roundtrip(o,b,s,0x4000,0,100,a::Name{3,0},u"ByteProperty").ok && o.direct==reads);
    std::puts("PASS: 13 signature cases; 7 pool roundtrip cases (ASCII/UTF16/id/number/text/length/fault)");
}
