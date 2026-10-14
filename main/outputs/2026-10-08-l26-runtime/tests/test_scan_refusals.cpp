// test_scan_refusals.cpp — l25 host tests for Claude's l25 item 6.
// Mac-only / host-tested: no console contact, no live engine calls.
//
// Proves four behaviors against the shipped l24 typescan-r3 guard
// (NativePropertyPointerGuard.hpp/.inl, included unmodified below):
//   (a) unknown-yet classes pass in bootstrap scan mode — a fixture class whose
//       FName ends in "Property" but is NOT a known name and carries NO
//       CastFlags is accepted during the scan and refused by the full check;
//   (b) a bad class pointer is refused WITH details — reason (enum + exact
//       harvest text), field address, and class pointer all present in the
//       RefusalLog record;
//   (c) a refusal does not stop the scan — a mini multi-object scan modeled on
//       TypeChecker::find_all_property_types keeps scanning later objects after
//       one object's field refused, and the good fields still register;
//   (d) scan mode resets after an exception — an RAII ScanCollectionScope
//       (host-side pattern over Begin/EndScanCollection) restores full-mode
//       checks during unwinding; a counter-control shows plain Begin/End
//       leaves the mode stuck, which is the hazard the scope removes.
//
// Mock conventions follow 2026-10-08-l24-runtime/typescan-r3/tests/test_l24_guard.cpp
// (byte-memory fixtures through the guard's installed read callback).
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "NativeTypeScanPolicy.hpp"
#include "NativePropertyPointerGuard.hpp"
#define STR(x) x

// ---- engine mocks (actual_getters.cpp / test_l24_guard.cpp conventions) ----
namespace RC::Unreal {
using CharType=char;using uint64=uint64_t;constexpr int FNAME_Find=0;
enum class EClassCastFlags:uint64_t {CASTCLASS_FProperty=1};
inline std::unordered_map<uint64_t,std::string> g_fname_display{
    {2,"IntProperty"},{3,"Object"},{4,"Widget"},{5,"VerseGradientProperty"},{6,"MapProperty"}};
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

// ---- byte-memory fixture (same shape as the l24 guard tests) ----
std::unordered_map<uintptr_t,std::vector<unsigned char>> memory;
bool image_good=true;
std::map<uintptr_t,size_t> image_set; // class addresses admitted by the image-range callback
bool read_bytes(uintptr_t a,size_t n,void*p){ for(auto&[base,v]:memory) if(a>=base&&a-base<=v.size()&&n<=v.size()-(a-base)){ std::memcpy(p,v.data()+a-base,n); return true; } return false; }
bool image_range(uintptr_t a,size_t n){ if(!image_good)return false; auto it=image_set.find(a); return it!=image_set.end()&&n<=it->second; }
void put(uintptr_t a,uint64_t value){ for(auto&[base,v]:memory) if(a>=base&&a+8<=base+v.size()){ std::memcpy(v.data()+a-base,&value,8); return; } }

// ---- host-side RAII scan-collection scope (l25 pattern over the shipped API) ----
struct ScanCollectionScope {
    explicit ScanCollectionScope(RefusalLog* log) { NativePropertyGuard::BeginScanCollection(log); }
    ~ScanCollectionScope() { NativePropertyGuard::EndScanCollection(); }
    ScanCollectionScope(const ScanCollectionScope&) = delete;
    ScanCollectionScope& operator=(const ScanCollectionScope&) = delete;
};

// (c) mini multi-object scan modeled on TypeChecker::find_all_property_types:
// per-field checked reads; a refusal ends that object's chain only; later
// objects still scan; good fields register their class name.
struct ScanStats { int objects=0, fields=0; std::map<std::string,int> registered; };
static void scan_object(const char* /*object_name*/, FField* first, RefusalLog& log, ScanStats& stats) {
    ++stats.objects;
    FField* field = first;
    std::vector<FField*> visited;
    while (field) {
        if (std::find(visited.begin(), visited.end(), field) != visited.end()) return; // cyclic chain guard
        visited.push_back(field);
        FFieldClass* klass = NativePropertyGuard::ReadClassChecked(field, &log);
        if (!klass) break; // refusal recorded; this object's chain ends, the scan driver continues
        ++stats.fields;
        FName name{}; // test-side read of the class FName through the same fixture backend
        read_bytes(reinterpret_cast<uintptr_t>(klass)+8, sizeof(name), &name);
        ++stats.registered[name.ToString()];
        field = NativePropertyGuard::ReadNextChecked(field, &log);
    }
}

int main(){
    // published layout offsets (l23/l24 fixture conventions)
    FField::MemberOffsets={{"ClassPrivate",8},{"Next",24}};
    FProperty::MemberOffsets={{"UEP_TotalSize",32}};
    FFieldClass::MemberOffsets={{"UEP_TotalSize",32},{"FName",8},{"CastFlags",16}};

    // 32-byte fixture regions: fields 0x100..0x2c0, classes 0x300..0xa00, owner 0x600
    for(uintptr_t a:{0x100,0x140,0x180,0x1c0,0x200,0x240,0x280,0x2c0,0x300,0x500,0x700,0x900,0xa00,0x600}) memory[a].assign(32,0);
    // classes: FName id @+8, CastFlags @+16
    put(0x308,2); put(0x310,1); // 0x300 image  IntProperty +FProperty cast flag -> full-mode PASS control
    put(0x508,3); put(0x510,1); // 0x500 image  "Object" (no Property suffix) -> identity refusal both modes
    put(0x708,4); put(0x710,1); // 0x700 image  "Widget" (unknown name) -> identity refusal
    put(0x908,5); put(0x910,0); // 0x900 heap   "VerseGradientProperty", NO CastFlags, NOT a known name -> bootstrap-only (a)
    put(0xa08,2); put(0xa10,1); // 0xa00 heap   IntProperty -> bootstrap-only (readable heap, non-image)
    // fields: ClassPrivate @+8, Next @+24
    put(0x108,0x300); put(0x148,0x500); put(0x188,0x900); put(0x1c8,0); put(0x208,0x301); put(0x248,0x700); put(0x288,0xa00); put(0x2c8,0x300);
    put(0x100+24,0x280);  // B1 -> B2
    put(0x2c0+24,0x1c0);  // C1 -> C2
    image_set={{0x300,32},{0x500,32},{0x700,32}}; // only these class addresses are image
    auto* f_img_int   =reinterpret_cast<FField*>(0x100); // good in both modes
    auto* f_img_object=reinterpret_cast<FField*>(0x140); // identity refusal both modes
    auto* f_verse     =reinterpret_cast<FField*>(0x180); // unknown-yet class (a)
    auto* f_null      =reinterpret_cast<FField*>(0x1c0); // null class pointer
    auto* f_misaligned=reinterpret_cast<FField*>(0x200); // misaligned class pointer 0x301
    auto* f_widget    =reinterpret_cast<FField*>(0x240); // valid shape, unknown name
    NativePropertyGuard::Install(read_bytes,image_range);

    // ---- full-mode PASS control ----
    RefusalLog l0;
    assert(NativePropertyGuard::ReadClassChecked(f_img_int,&l0)==reinterpret_cast<FFieldClass*>(0x300));
    assert(l0.count==0&&l0.dropped==0);

    // ---- (a) unknown-yet class: FName ends in "Property", NO CastFlags, not a known name ----
    RefusalLog la_full;
    assert(NativePropertyGuard::ReadClassChecked(f_verse,&la_full)==nullptr); // full check: no image range, no CastFlags, no exact known-name match -> refuse
    assert(la_full.count==1&&la_full.records[0].reason==RefusalReason::ClassIdentity);
    assert(la_full.records[0].field_address==0x180&&la_full.records[0].class_pointer==0x900);
    {   // bootstrap scan mode accepts it: readable heap + "Property" suffix, no CastFlags needed
        RefusalLog la_scan; ScanCollectionScope scope(&la_scan);
        assert(NativePropertyGuard::ScanModeActive());
        assert(NativePropertyGuard::ReadClassChecked(f_verse,&la_scan)==reinterpret_cast<FFieldClass*>(0x900));
        assert(la_scan.count==0&&la_scan.dropped==0);
    }
    assert(!NativePropertyGuard::ScanModeActive());
    {   // control: a class whose FName does NOT end in "Property" and is not known is refused in bootstrap too
        RefusalLog la2; ScanCollectionScope scope(&la2);
        assert(NativePropertyGuard::ReadClassChecked(f_widget,&la2)==nullptr);
        assert(NativePropertyGuard::ReadClassChecked(f_img_object,&la2)==nullptr);
        assert(la2.count==2&&la2.records[0].reason==RefusalReason::ClassIdentity&&la2.records[1].reason==RefusalReason::ClassIdentity);
    }
    std::cout<<"PASS (a) unknown-yet *Property class passes bootstrap only, no CastFlags needed\n";

    // ---- (b) bad class pointer refused WITH details: reason, field address, class pointer ----
    RefusalLog lb;
    assert(NativePropertyGuard::ReadClassChecked(reinterpret_cast<FField*>(0xdead0),&lb)==nullptr); // unreadable slot
    assert(lb.count==1&&lb.records[0].reason==RefusalReason::SlotRead);
    assert(lb.records[0].field_address==0xdead0&&lb.records[0].class_pointer==0);
    assert(std::strcmp(NativePropertyGuard::reason_text(lb.records[0].reason),"slot read")==0);
    assert(NativePropertyGuard::ReadClassChecked(f_null,&lb)==nullptr); // null class pointer
    assert(lb.count==2&&lb.records[1].reason==RefusalReason::ClassNull);
    assert(lb.records[1].field_address==0x1c0&&lb.records[1].class_pointer==0);
    assert(std::strcmp(NativePropertyGuard::reason_text(lb.records[1].reason),"class null")==0);
    assert(NativePropertyGuard::ReadClassChecked(f_misaligned,&lb)==nullptr); // misaligned class pointer
    assert(lb.count==3&&lb.records[2].reason==RefusalReason::ClassMisaligned);
    assert(lb.records[2].field_address==0x200&&lb.records[2].class_pointer==0x301); // the bad pointer itself is in the record
    assert(std::strcmp(NativePropertyGuard::reason_text(lb.records[2].reason),"class misaligned")==0);
    assert(NativePropertyGuard::ReadClassChecked(f_widget,&lb)==nullptr); // readable, aligned, but not a property class
    assert(lb.count==4&&lb.records[3].reason==RefusalReason::ClassIdentity);
    assert(lb.records[3].field_address==0x240&&lb.records[3].class_pointer==0x700);
    assert(std::strcmp(NativePropertyGuard::reason_text(lb.records[3].reason),"class identity")==0);
    // ReadPointerChecked refusal: target slot misread collapses to SlotRead, but the
    // record still carries the field (owner) address and the key in object_name.
    put(0x608,0x301); // owner 0x600 slot holds a misaligned target
    RefusalLog lb2;
    assert(NativePropertyGuard::ReadPointerChecked(reinterpret_cast<void*>(0x600),8,"FMapProperty::KeyProp",&lb2)==nullptr);
    assert(lb2.count==1&&lb2.records[0].reason==RefusalReason::SlotRead);
    assert(lb2.records[0].field_address==0x600&&lb2.records[0].class_pointer==0);
    assert(std::strcmp(lb2.records[0].object_name,"FMapProperty::KeyProp")==0); // key preserved
    // long keys truncate to 63 chars + NUL inside the record
    const std::string long_key(100,'k');
    RefusalLog lb3;
    assert(NativePropertyGuard::ReadPointerChecked(reinterpret_cast<void*>(0x600),8,long_key.c_str(),&lb3)==nullptr);
    assert(std::strlen(lb3.records[0].object_name)==63&&lb3.records[0].object_name[63]=='\0');
    std::cout<<"PASS (b) refusals carry reason text, field address, class pointer (and key for pointer reads)\n";

    // ---- (c) a refusal does not stop the scan ----
    // Object A first field refused (misaligned class): A registers nothing.
    // Object B is fully scanned AFTER A's refusal (2 fields, heap class needs bootstrap).
    // Object C scans its good head field, then a null-class tail field refuses; C keeps its head registration.
    // Object D's unknown-yet *Property class registers during the scan.
    RefusalLog lc; ScanStats stats;
    {   ScanCollectionScope scope(&lc);
        scan_object("/Script/Fixture.BadOwner",  f_misaligned, lc, stats); // A: refused field
        scan_object("/Script/Fixture.GoodOwner", f_img_int,    lc, stats); // B: B1(0x100)->B2(0x280)
        scan_object("/Script/Fixture.MixedOwner",reinterpret_cast<FField*>(0x2c0),lc,stats); // C: C1(0x2c0)->C2(0x1c0 null)
        scan_object("/Script/Fixture.VerseOwner",f_verse,      lc, stats); // D: unknown-yet class
    }
    assert(stats.objects==4);                        // the scan driver reached every object
    assert(stats.fields==4);                         // B1, B2, C1, D1 registered
    const std::map<std::string,int> expected_registered{{"IntProperty",3},{"VerseGradientProperty",1}};
    assert(stats.registered==expected_registered);
    assert(lc.count==2);                             // exactly the two refusals: A1 misaligned, C2 null
    assert(lc.records[0].reason==RefusalReason::ClassMisaligned&&lc.records[0].field_address==0x200&&lc.records[0].class_pointer==0x301);
    assert(lc.records[1].reason==RefusalReason::ClassNull&&lc.records[1].field_address==0x1c0&&lc.records[1].class_pointer==0);
    assert(!NativePropertyGuard::ScanModeActive());  // driver closed the scan
    std::cout<<"PASS (c) refusal ended only its own chain; all 4 objects scanned, 4 fields registered\n";

    // ---- (d) scan mode resets after an exception (RAII) ----
    RefusalLog ld; bool caught=false;
    try {
        ScanCollectionScope scope(&ld);
        assert(NativePropertyGuard::ScanModeActive());
        assert(NativePropertyGuard::ReadClassChecked(f_verse,&ld)==reinterpret_cast<FFieldClass*>(0x900)); // bootstrap check live
        throw std::runtime_error("scan body failed mid-object");
    } catch (const std::runtime_error& e) { caught=std::strcmp(e.what(),"scan body failed mid-object")==0; }
    assert(caught);
    assert(!NativePropertyGuard::ScanModeActive());                                  // reset by unwinding
    assert(NativePropertyGuard::ReadClassChecked(f_verse,&ld)==nullptr);             // full check back in force
    assert(ld.count==1&&ld.records[0].reason==RefusalReason::ClassIdentity&&ld.records[0].class_pointer==0x900);
    // counter-control: the candidate's plain Begin/End pairing (no RAII) leaves the
    // mode stuck when an exception escapes the scan body — the hazard the scope removes.
    RefusalLog ld2; caught=false;
    NativePropertyGuard::BeginScanCollection(&ld2);
    try { throw std::runtime_error("unwinding without RAII"); } catch (...) { caught=true; }
    assert(caught&&NativePropertyGuard::ScanModeActive());                           // stuck open (documented hazard)
    NativePropertyGuard::EndScanCollection();
    assert(!NativePropertyGuard::ScanModeActive());
    std::cout<<"PASS (d) RAII scope reset scan mode during unwinding; plain Begin/End counter-control stays stuck\n";

    std::cout<<"l25 scan-refusal host tests: (a) bootstrap unknown-yet, (b) detailed refusals, (c) scan continues, (d) RAII reset — all passed\n";
}
