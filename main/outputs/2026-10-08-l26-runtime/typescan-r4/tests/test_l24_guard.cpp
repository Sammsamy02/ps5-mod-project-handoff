// test_l24_guard.cpp — Mac-only host tests for the l24 guard changes.
// No console contact. Covers NativePropertyPointerGuard.hpp/.inl (bootstrap
// class check, refusal collection, scan-mode control) and the l24
// PropertyImageClassRange spanpub-fallback route in NativePropertyReadBackend.inl
// (bridge/admission surface mocked below). Conventions follow actual_getters.cpp:
// assert-only, single main(), byte-memory fixtures via the guard's read callback.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "NativeTypeScanPolicy.hpp"
#include "NativePropertyPointerGuard.hpp"
#define STR(x) x
#define __PROSPERO__ 1

// ---- host mocks of the rc bridge/admission surface used by the l24 backend ----
namespace rc::imagecoverage {
struct Hit { bool readable=false; int index=0; uint64_t start=0,end=0,files_end=0; uint32_t protection=0; const char* source=""; const char* note=""; };
}
namespace rc::pooladmission {
enum class QueryKind { Available, Unavailable };
enum class Verdict { Readable, HardNegative, Unavailable, Expired, Invalid };
inline bool valid_range(uint64_t address,size_t length){ return address&&length&&length<=UINTPTR_MAX-address; }
struct Query { QueryKind kind=QueryKind::Unavailable; uint64_t start=0,end=0; uint32_t protection=0,flags=0; int note=0; };
struct Budget { uint64_t start_ms=0,limit_ms=0; Budget()=default; Budget(uint64_t s,uint64_t l):start_ms(s),limit_ms(l){} bool expired(uint64_t now)const{return now>=start_ms+limit_ms;} };
struct Result { Verdict verdict=Verdict::Invalid; };
// Minimal host model of the budgeted VQ coverage proof: Available VQ region
// covering [address,address+length) with a read permission bit => Readable.
template<class Ops> Result coverage(Ops& ops,Budget& budget,uint64_t address,size_t length) {
    if(budget.expired(ops.now_ms())) return {Verdict::Expired};
    const auto q=ops.query(address);
    if(q.kind!=QueryKind::Available||q.start>address||q.end<=address||length>q.end-address||!(q.protection&4u)) return {Verdict::Unavailable};
    return {Verdict::Readable};
}
}

// ---- engine mocks (actual_getters.cpp conventions) + l24 guard include ----
namespace RC::Unreal {
using CharType=char;using uint64=uint64_t;constexpr int FNAME_Find=0;
enum class EClassCastFlags:uint64_t {CASTCLASS_FProperty=1};
inline std::unordered_map<uint64_t,std::string> g_fname_display{{2,"IntProperty"},{3,"FloatProperty"},{4,"Object"},{5,"Widget"}};
inline uint64_t g_fname_find(const char* name){ if(!name)return 0; for(const auto&[k,v]:g_fname_display) if(v==name)return k; return 0; }
struct FName {
    uint64_t id=0; // raw FName bytes as read from fixture memory
    FName()=default;
    FName(const char* name,int){ id=g_fname_find(name); } // FNAME_Find through the verified pool
    struct Index{uint32_t value;uint32_t ToUnstableInt()const{return value;}};
    Index GetComparisonIndex() const{return {static_cast<uint32_t>(id)};}
    bool operator==(const FName&)const=default;
    std::string ToString()const{ auto it=g_fname_display.find(id); return it==g_fname_display.end()?std::string{}:it->second; }
};
struct FField {inline static std::unordered_map<std::string,int> MemberOffsets;};
struct FProperty:FField {inline static std::unordered_map<std::string,int> MemberOffsets;};
struct FFieldClass {inline static std::unordered_map<std::string,int> MemberOffsets;};
#include "NativePropertyPointerGuard.inl"
}

using namespace RC::Unreal;
using NativePropertyGuard::RefusalLog;
using NativePropertyGuard::RefusalReason;

// ---- guard-test byte-memory fixture (same shape as actual_getters.cpp) ----
std::unordered_map<uintptr_t,std::vector<unsigned char>> memory;
bool image_good=true;
std::map<uintptr_t,size_t> image_set; // addresses admitted by the image-range callback
bool read_bytes(uintptr_t a,size_t n,void*p){ for(auto&[base,v]:memory) if(a>=base&&a-base<=v.size()&&n<=v.size()-(a-base)){ std::memcpy(p,v.data()+a-base,n); return true; } return false; }
bool image_range(uintptr_t a,size_t n){ if(!image_good)return false; auto it=image_set.find(a); return it!=image_set.end()&&n<=it->second; }
void put(uintptr_t a,uint64_t value){ for(auto&[base,v]:memory) if(a>=base&&a+8<=base+v.size()){ std::memcpy(v.data()+a-base,&value,8); return; } }

// ---- mocked bridge with the l24 two-tier image policy: module-info primary,
// spanpub (this-capture VM records) fallback. Mirrors the root-cause scenario:
// module-info never obtained for the needed image range. ----
namespace RC::Unreal { namespace SeedArming {
namespace admission = rc::pooladmission;
struct L13VqInfo { void* start; void* end; int64_t offset; int32_t protection; int32_t memory_type; uint8_t flags; char name[32]; uint8_t gpu_mask_id; };
static_assert(sizeof(L13VqInfo)==0x48,"SceKernelVirtualQueryInfo layout drifted");
inline std::map<uint64_t,uint64_t> g_module_info_mappings; // start -> end (module-info source)
inline std::map<uint64_t,uint64_t> g_spanpub_records;      // start -> end (spanpub VM records)
inline std::map<uint64_t,uint64_t> g_vq_regions;           // readable regions reported by the VQ mock
inline int g_module_info_lookups=0,g_spanpub_lookups=0;
extern "C" int bridge_image_range(uint64_t address,size_t length,rc::imagecoverage::Hit* hit) {
    if(!hit) return 0;
    ++g_module_info_lookups;
    for(const auto&[s,e]:g_module_info_mappings)
        if(address>=s&&address<e&&length<=e-address){ hit->readable=true; hit->start=s; hit->end=e; hit->source="module-info"; return 1; }
    ++g_spanpub_lookups; // fallback engaged: module-info had no mapping
    for(const auto&[s,e]:g_spanpub_records)
        if(address>=s&&address<e&&length<=e-address){ hit->readable=true; hit->start=s; hit->end=e; hit->source="spanpub"; return 1; }
    return 0;
}
extern "C" int bridge_copyin_range_faulted(uint64_t,uint64_t){ return 0; }
extern "C" int sceKernelVirtualQuery(const void* addr,int,L13VqInfo* info,size_t) {
    if(!info) return -1;
    const uint64_t a=reinterpret_cast<uint64_t>(addr);
    for(const auto&[s,e]:g_vq_regions)
        if(a>=s&&a<e){ std::memset(info,0,sizeof(*info)); info->start=reinterpret_cast<void*>(s); info->end=reinterpret_cast<void*>(e); info->protection=4; info->flags=0x10; return 0; }
    return -1;
}
#include "NativePropertyReadBackend.inl"
}}

int main(){
 // published layout offsets (l23 fixture conventions)
 FField::MemberOffsets={{"ClassPrivate",8},{"Next",24}};
 FProperty::MemberOffsets={{"UEP_TotalSize",32}};
 FFieldClass::MemberOffsets={{"UEP_TotalSize",32},{"FName",8},{"CastFlags",16}};

 // 32-byte fixture regions: fields 0x100..0x2c0, classes 0x300..0x700
 for(uintptr_t a:{0x100,0x140,0x180,0x1c0,0x240,0x280,0x2c0,0x300,0x400,0x500,0x600,0x700}) memory[a].assign(32,0);
 // classes: FName id @+8, CastFlags @+16
 put(0x308,2); put(0x310,1); // 0x300 image  IntProperty  +FProperty cast flag -> full-mode PASS control
 put(0x408,3); put(0x410,0); // 0x400 heap   FloatProperty, NO CastFlags -> bootstrap-only
 put(0x508,4); put(0x510,1); // 0x500 image  "Object" (no Property suffix) -> refused in both modes
 put(0x608,2); put(0x610,1); // 0x600 heap   IntProperty, readable non-image -> bootstrap-only
 put(0x708,5); put(0x710,1); // 0x700 image  "Widget": valid class, unknown name -> identity refusal
 // fields: ClassPrivate @+8
 put(0x108,0x400); put(0x148,0x500); put(0x188,0x600); put(0x1c8,0x300);
 put(0x248,0);     put(0x288,0x301); put(0x2c8,0x700);
 image_set={{0x300,32},{0x500,32},{0x700,32}}; // only these class addresses are image
 auto* f_heap_float=reinterpret_cast<FField*>(0x100);
 auto* f_img_object=reinterpret_cast<FField*>(0x140);
 auto* f_heap_int=reinterpret_cast<FField*>(0x180);
 auto* f_img_int=reinterpret_cast<FField*>(0x1c0);
 auto* f_null=reinterpret_cast<FField*>(0x240);
 auto* f_misaligned=reinterpret_cast<FField*>(0x280);
 auto* f_identity=reinterpret_cast<FField*>(0x2c0);
 NativePropertyGuard::Install(read_bytes,image_range);

 // ---- 0. full-mode PASS control: image + CastFlags + exact known name ----
 RefusalLog l0;
 assert(NativePropertyGuard::ReadClassChecked(f_img_int,&l0)==reinterpret_cast<FFieldClass*>(0x300));
 assert(l0.count==0&&l0.dropped==0);

 // ---- 1. bootstrap class check vs full check ----
 // 1a. FName ends in "Property" (FloatProperty) but NO CastFlags, heap (non-image)
 RefusalLog la_full,la_boot;
 assert(NativePropertyGuard::ReadClassChecked(f_heap_float,&la_full)==nullptr); // full: no CastFlags (and non-image) -> refuse
 assert(la_full.count==1&&la_full.records[0].reason==RefusalReason::ClassIdentity);
 assert(la_full.records[0].class_pointer==0x400&&la_full.records[0].field_address==0x100);
 NativePropertyGuard::BeginScanCollection(&la_boot);
 assert(NativePropertyGuard::ScanModeActive());
 assert(NativePropertyGuard::ReadClassChecked(f_heap_float,&la_boot)==reinterpret_cast<FFieldClass*>(0x400)); // bootstrap: readable + suffix, no CastFlags needed
 assert(la_boot.count==0);
 NativePropertyGuard::EndScanCollection();
 assert(!NativePropertyGuard::ScanModeActive());
 // 1b. FName does NOT end in "Property" ("Object", image + CastFlags) -> refused in both modes
 RefusalLog lb_full,lb_boot;
 assert(NativePropertyGuard::ReadClassChecked(f_img_object,&lb_full)==nullptr); // full: exact-name match fails
 assert(lb_full.count==1&&lb_full.records[0].reason==RefusalReason::ClassIdentity);
 NativePropertyGuard::BeginScanCollection(&lb_boot);
 assert(NativePropertyGuard::ReadClassChecked(f_img_object,&lb_boot)==nullptr); // bootstrap: no suffix, no known-name match
 assert(lb_boot.count==1&&lb_boot.records[0].reason==RefusalReason::ClassIdentity);
 NativePropertyGuard::EndScanCollection();
 // 1c. readable non-image class address (heap IntProperty) -> bootstrap PASS (readable is enough), full FAIL (image-only)
 RefusalLog lc_full,lc_boot;
 assert(NativePropertyGuard::ReadClassChecked(f_heap_int,&lc_full)==nullptr); // full: image_class_range refuses non-image
 assert(lc_full.count==1&&lc_full.records[0].reason==RefusalReason::ClassIdentity);
 NativePropertyGuard::BeginScanCollection(&lc_boot);
 assert(NativePropertyGuard::ReadClassChecked(f_heap_int,&lc_boot)==reinterpret_cast<FFieldClass*>(0x600)); // bootstrap reads through read_bytes, not image range
 NativePropertyGuard::EndScanCollection();

 // ---- 2. refusal collection ----
 // 2a. unreadable field slot -> SlotRead
 RefusalLog l2a;
 assert(NativePropertyGuard::ReadClassChecked(reinterpret_cast<FField*>(0xdead0),&l2a)==nullptr);
 assert(l2a.count==1&&l2a.records[0].reason==RefusalReason::SlotRead&&l2a.records[0].field_address==0xdead0&&l2a.records[0].class_pointer==0);
 // 2b. null class pointer -> ClassNull
 RefusalLog l2b;
 assert(NativePropertyGuard::ReadClassChecked(f_null,&l2b)==nullptr);
 assert(l2b.count==1&&l2b.records[0].reason==RefusalReason::ClassNull&&l2b.records[0].class_pointer==0);
 // 2c. misaligned class pointer -> ClassMisaligned
 RefusalLog l2c;
 assert(NativePropertyGuard::ReadClassChecked(f_misaligned,&l2c)==nullptr);
 assert(l2c.count==1&&l2c.records[0].reason==RefusalReason::ClassMisaligned&&l2c.records[0].class_pointer==0x301);
 // 2d. valid class (readable, aligned, image, CastFlags) but unknown name -> ClassIdentity
 RefusalLog l2d;
 assert(NativePropertyGuard::ReadClassChecked(f_identity,&l2d)==nullptr);
 assert(l2d.count==1&&l2d.records[0].reason==RefusalReason::ClassIdentity&&l2d.records[0].class_pointer==0x700);
 // 2e. 120 defective fields -> capacity 50, 70 dropped
 RefusalLog l2e;
 for(int i=0;i<120;++i) assert(NativePropertyGuard::ReadClassChecked(reinterpret_cast<FField*>(0xdead0),&l2e)==nullptr);
 assert(l2e.count==RefusalLog::kCapacity&&l2e.dropped==70);
 assert(l2e.records[49].reason==RefusalReason::SlotRead);
 // 2f. exactly 50 defective fields -> full, none dropped
 RefusalLog l2f;
 for(int i=0;i<50;++i) assert(NativePropertyGuard::ReadClassChecked(f_null,&l2f)==nullptr);
 assert(l2f.count==RefusalLog::kCapacity&&l2f.dropped==0);
 assert(l2f.records[49].reason==RefusalReason::ClassNull);

 // ---- 3. scan-mode control ----
 RefusalLog l3;
 NativePropertyGuard::BeginScanCollection(&l3);
 assert(NativePropertyGuard::ReadClassChecked(f_heap_int,&l3)==reinterpret_cast<FFieldClass*>(0x600)); // bootstrap check applies during scan
 assert(l3.count==0);
 NativePropertyGuard::EndScanCollection();
 assert(NativePropertyGuard::ReadClassChecked(f_heap_int,&l3)==nullptr); // same fixture now uses the full check
 assert(l3.count==1&&l3.records[0].reason==RefusalReason::ClassIdentity);
 // reason_text: exact harvest strings
 assert(std::strcmp(NativePropertyGuard::reason_text(RefusalReason::SlotRead),"slot read")==0);
 assert(std::strcmp(NativePropertyGuard::reason_text(RefusalReason::ClassNull),"class null")==0);
 assert(std::strcmp(NativePropertyGuard::reason_text(RefusalReason::ClassMisaligned),"class misaligned")==0);
 assert(std::strcmp(NativePropertyGuard::reason_text(RefusalReason::ClassIdentity),"class identity")==0);

 // ---- 4. spanpub fallback in PropertyImageClassRange ----
 // module-info knows an unrelated image range only; the needed image range is
 // covered exclusively by spanpub VM records; VQ reports all three readable.
 SeedArming::g_module_info_mappings={{0x900000,0x901000}};
 SeedArming::g_spanpub_records={{0x800000,0x801000}};
 SeedArming::g_vq_regions={{0x800000,0x801000},{0x900000,0x901000},{0xA00000,0xA01000}};
 rc::imagecoverage::Hit hit{};
 const int spanpub_before=SeedArming::g_spanpub_lookups;
 assert(SeedArming::bridge_image_range(0x800800,64,&hit)==1&&hit.readable); // address NOT in module-info mappings
 assert(std::strcmp(hit.source,"spanpub")==0); // proof it resolved via the fallback
 assert(SeedArming::PropertyImageClassRange(0x800800,64)); // succeeds via spanpub fallback
 assert(SeedArming::g_spanpub_lookups>spanpub_before); // fallback actually engaged
 assert(!SeedArming::PropertyImageClassRange(0xA00800,64)); // readable VQ region but no image proof anywhere -> fail closed
 assert(SeedArming::PropertyImageClassRange(0x900800,64)); // control: module-info primary path still works

 // ---- revoke control: enabled gate closes ----
 RefusalLog l9;
 NativePropertyGuard::Revoke();
 assert(NativePropertyGuard::ReadClassChecked(f_img_int,&l9)==nullptr);
 assert(l9.count==1&&l9.records[0].reason==RefusalReason::SlotRead);

 std::cout<<"l24 guard host tests: 46 assertions passed\n";
}
