#include "PoolAdmissionEngine.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace rc::pooladmission;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x); std::exit(1); } } while(0)
struct FakeOps {
 uint64_t time=0, query_cost=0, oracle_cost=0, shadow_cost=0; std::string events;
 QueryKind query_kind=QueryKind::Available; uint64_t region_start=0x2000, region_end=0x3000;
 uint32_t protection=1, flags=0x10; int probe_result=-1; bool captured=false, fault=false, shadow_enabled=true;
 bool shadow_armed=true; size_t direct_reads=0, shadow_calls=0, probe_calls=0, queries=0, logs=0;
 uint8_t bytes[16]={'N','o','n','e'};
 uint64_t now_ms() const { return time; }
 void wait_ms(uint64_t n) { events+='W'; time+=n; }
 bool known_fault(uint64_t,size_t) { return fault; }
 Query query(uint64_t) { events+='V'; ++queries; time+=query_cost; return {query_kind,region_start,region_end,protection,flags,0}; }
 void log_region(uint64_t,size_t,const Query&) { ++logs; }
 bool capture(uint64_t,size_t len,const uint8_t** p,size_t* n) { events+='C'; *p=bytes; *n=captured?sizeof(bytes):0; return captured&&len<=sizeof(bytes); }
 int probe(uint64_t,size_t,Budget&) { events+='O'; ++probe_calls; time+=oracle_cost; if(probe_result==1) captured=true; return probe_result; }
 bool shadow(Budget&) { events+='S'; ++shadow_calls; time+=shadow_cost; return shadow_armed; }
 void read_direct(uint64_t,size_t n,uint8_t* dest) { events+='R'; ++direct_reads; std::memcpy(dest,bytes,n); }
};
static void primary_vq() { FakeOps o; Budget b(o.now_ms()); Session s; uint8_t dest[6]{}; auto r=read(o,b,s,0x2000,6,dest); REQUIRE(r.verdict==Verdict::Readable); REQUIRE(r.source==Source::VirtualQuery); REQUIRE(o.events=="VR"); REQUIRE(o.logs==1); REQUIRE(o.direct_reads==1); }
static void exact_window() { FakeOps o; o.region_start=0x2003; o.region_end=0x2009; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2003,6,dest).verdict==Verdict::Readable); REQUIRE(o.events=="VR"); }
static void uncontained_region() { FakeOps o; o.region_start=0x2100; Budget b(0); Session s; uint8_t dest[6]{}; auto r=read(o,b,s,0x2000,6,dest); REQUIRE(r.verdict==Verdict::Unavailable); REQUIRE(o.direct_reads==0); REQUIRE(o.events[0]=='V'); REQUIRE(o.probe_calls==1); }
static void unavailable_falls_to_owned_read() { FakeOps o; o.query_kind=QueryKind::Unavailable; o.probe_result=1; Budget b(0); Session s; uint8_t dest[6]{}; auto r=read(o,b,s,0x2000,6,dest); REQUIRE(r.verdict==Verdict::Readable); REQUIRE(r.source==Source::CopyinCapture); REQUIRE(o.events=="VCOC"); REQUIRE(o.direct_reads==0); REQUIRE(std::memcmp(dest,o.bytes,6)==0); }
static void vq_negative_blocks_every_fallback() { for(bool mapped:{false,true}) { FakeOps o; if(mapped)o.protection=0;else o.query_kind=QueryKind::Unmapped; o.captured=true; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative); REQUIRE(o.events=="V"); o.query_kind=QueryKind::Available;o.protection=1; REQUIRE(read(o,b,s,0x2002,4,dest).verdict==Verdict::HardNegative); REQUIRE(o.events=="V"); } }
static void oracle_fault_blocks_shadow_and_future_vq() { FakeOps o; o.query_kind=QueryKind::Unavailable; o.probe_result=0; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative); REQUIRE(o.events=="VCO"); o.query_kind=QueryKind::Available; REQUIRE(read(o,b,s,0x2001,2,dest).verdict==Verdict::HardNegative); REQUIRE(o.direct_reads==0); REQUIRE(o.shadow_calls==0); }
static void prior_bridge_fault_excludes_vq_and_capture() { FakeOps o; o.fault=true;o.captured=true; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative); REQUIRE(o.events.empty()); }
static void identity_never_proves_heap() { FakeOps o; o.query_kind=QueryKind::Unavailable; Budget b(0); Session s; uint8_t dest[6]{}; auto r=read(o,b,s,0x2000,6,dest); REQUIRE(r.verdict==Verdict::Unavailable); REQUIRE(o.events=="VCOCS"); REQUIRE(o.direct_reads==0); REQUIRE(o.shadow_calls==1); REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Unavailable); REQUIRE(o.shadow_calls==1); REQUIRE(o.probe_calls==1); }
static void single_total_deadline() { FakeOps o; o.query_kind=QueryKind::Unavailable; Budget b(0); Session s; uint8_t dest[6]{}; auto r=retry(o,b,[&]{return read(o,b,s,0x2000,6,dest);}); REQUIRE(r.verdict==Verdict::Expired); REQUIRE(o.time==60000); REQUIRE(o.time<300000); REQUIRE(o.shadow_calls==1); REQUIRE(o.probe_calls==1); std::printf("worst_case_fake_elapsed_ms=%llu\n",(unsigned long long)o.time); }
static void operation_cost_and_sleep_share_deadline() { FakeOps o; o.query_kind=QueryKind::Unavailable; o.query_cost=500;o.oracle_cost=3000;o.shadow_cost=2000; Budget b(0); Session s; uint8_t dest[6]{}; auto r=retry(o,b,[&]{return read(o,b,s,0x2000,6,dest);}); REQUIRE(r.verdict==Verdict::Expired); REQUIRE(o.time<=60500); REQUIRE(o.time<300000); }
static void deadline_prevents_following_operations() { FakeOps o; o.query_kind=QueryKind::Unavailable; o.query_cost=60000; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Expired); REQUIRE(o.events=="V"); REQUIRE(o.direct_reads==0); }
static void changed_address_has_no_reused_proof() { FakeOps o; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Readable); o.query_kind=QueryKind::Unavailable; REQUIRE(read(o,b,s,0x4000,6,dest).verdict==Verdict::Unavailable); REQUIRE(o.direct_reads==1); }
static void overflow_never_touches_os() { FakeOps o;Budget b(0);Session s;uint8_t dest[6]{};REQUIRE(read(o,b,s,UINT64_MAX-2,6,dest).verdict==Verdict::Invalid);REQUIRE(o.events.empty()); }
static void later_entry_needs_its_own_proof() { FakeOps o; Budget b(0); Session s; uint8_t dest[6]{}; REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Readable); o.protection=0; REQUIRE(read(o,b,s,0x2ffe,6,dest).verdict==Verdict::HardNegative); REQUIRE(o.direct_reads==1); }
static void expired_at_start_does_no_work() { FakeOps o; o.time=60000;Budget b(0);Session s;uint8_t dest[6]{};REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Expired);REQUIRE(o.events.empty()); }
static void reservation_is_not_readable() { FakeOps o; o.flags=0; Budget b(0);Session s;uint8_t dest[6]{};REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative);REQUIRE(o.events=="V"); }
static void negative_region_remains_excluded() { FakeOps o;o.protection=0;Budget b(0);Session s;uint8_t dest[6]{};REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative);o.protection=1;REQUIRE(read(o,b,s,0x2050,6,dest).verdict==Verdict::HardNegative);REQUIRE(o.events=="V");}

// A read straddling a mapping boundary must prove both containing regions.
struct SplitOps : FakeOps {
 enum class Second { Readable, Unavailable, Gap, NoRead, Uncommitted } second;
 explicit SplitOps(Second s) : second(s) {}
 Query query(uint64_t address) {
  events += 'V'; ++queries;
  if (address==0x2000) return {QueryKind::Available,0x2000,0x2004,1,0x10,0};
  REQUIRE(address==0x2004);
  if (second==Second::Unavailable) return {QueryKind::Unavailable,0,0,0,0,-1};
  return {QueryKind::Available,second==Second::Gap?0x2005u:0x2004u,0x2010,
          second==Second::NoRead?0u:1u,second==Second::Uncommitted?0u:0x10u,0};
 }
};
static void adjacent_regions_cover_exact_range() {
 SplitOps o(SplitOps::Second::Readable); Budget b(0);Session s;uint8_t dest[6]{};
 const auto r=read(o,b,s,0x2000,6,dest);
 REQUIRE(r.verdict==Verdict::Readable);REQUIRE(r.source==Source::VirtualQuery);
 REQUIRE(o.events=="VVR");REQUIRE(o.queries==2);REQUIRE(o.logs==2);REQUIRE(o.probe_calls==0);
}
static void second_region_inconclusive_falls_back(SplitOps::Second second) {
 SplitOps o(second);Budget b(0);Session s;uint8_t dest[6]{};
 REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::Unavailable);
 REQUIRE(o.events=="VVCOCS");REQUIRE(o.direct_reads==0);REQUIRE(o.probe_calls==1);REQUIRE(o.shadow_calls==1);
 REQUIRE(!s.excluded({0x2004,2}));
}
static void second_region_negative_excludes_region(SplitOps::Second second) {
 SplitOps o(second);Budget b(0);Session s;uint8_t dest[6]{};
 REQUIRE(read(o,b,s,0x2000,6,dest).verdict==Verdict::HardNegative);
 REQUIRE(o.events=="VV");REQUIRE(o.direct_reads==0);REQUIRE(o.probe_calls==0);REQUIRE(o.shadow_calls==0);
 o.second=SplitOps::Second::Readable;
 REQUIRE(read(o,b,s,0x2008,2,dest).verdict==Verdict::HardNegative);
 REQUIRE(o.events=="VV");
}
int main() { adjacent_regions_cover_exact_range();second_region_inconclusive_falls_back(SplitOps::Second::Unavailable);second_region_inconclusive_falls_back(SplitOps::Second::Gap);second_region_negative_excludes_region(SplitOps::Second::NoRead);second_region_negative_excludes_region(SplitOps::Second::Uncommitted); negative_region_remains_excluded(); reservation_is_not_readable(); primary_vq();exact_window();uncontained_region();unavailable_falls_to_owned_read();vq_negative_blocks_every_fallback();oracle_fault_blocks_shadow_and_future_vq();prior_bridge_fault_excludes_vq_and_capture();identity_never_proves_heap();single_total_deadline();operation_cost_and_sleep_share_deadline();deadline_prevents_following_operations();changed_address_has_no_reused_proof();overflow_never_touches_os();later_entry_needs_its_own_proof();expired_at_start_does_no_work();std::puts("PASS 22 production admission engine scenarios"); }
