#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "NativeTypeScanPolicy.hpp"
#include "NativePropertyPointerGuard.hpp"
#define STR(x) x
#define __PROSPERO__ 1
namespace RC::Unreal {
using CharType=char;using uint64=uint64_t;constexpr int FNAME_Find=0;
namespace File {using StringType=std::string;}
struct BitfieldInfo{};struct FScriptMapLayout{};struct FScriptSetLayout{};
enum EMapPropertyFlags{};enum EArrayPropertyFlags{};
enum class EClassCastFlags:uint64_t {CASTCLASS_FProperty=1};
struct FName {uint64_t id=0;FName()=default;FName(const char* name,int){if(std::strcmp(name,"IntProperty")==0)id=2;}struct Index{uint32_t value;uint32_t ToUnstableInt()const{return value;}};Index GetComparisonIndex() const{return {uint32_t(id)};}bool operator==(const FName&)const=default;
 // [EV 2026-10-09] fixture-contract repair (unrowed fixture follows the rowed
 // source): the l25 NativePropertyPointerGuard.inl calls FName::ToString in
 // fname_ends_in_property (class-identity check); render the stub ids the
 // assertions use — id 2 is the fixture's "IntProperty" (suffix passes), any
 // other id must NOT end in "Property" (line 53's id-3 refusal).
 std::string ToString()const{return id==2?"IntProperty":"?";}};
struct FField {inline static std::unordered_map<std::string,int> MemberOffsets;};
struct FProperty:FField {inline static std::unordered_map<std::string,int> MemberOffsets;};
struct FFieldClass {inline static std::unordered_map<std::string,int> MemberOffsets;};
struct FMapProperty { static std::unordered_map<std::string,int> MemberOffsets;static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;FProperty* GetKeyProp();const FProperty* GetKeyProp()const;FProperty* GetValueProp();const FProperty* GetValueProp()const;};
struct FSetProperty { static std::unordered_map<std::string,int> MemberOffsets;static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;FProperty* GetElementProp();const FProperty* GetElementProp()const;};
struct FArrayProperty { static std::unordered_map<std::string,int> MemberOffsets;static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;FProperty* GetInner();const FProperty* GetInner()const;};
#include "NativePropertyPointerGuard.inl"
#include "actual_pointer_getters.inc"
}
using namespace RC::Unreal;
std::unordered_map<uintptr_t,std::vector<unsigned char>> memory;
unsigned reads=0;bool image_good=true;uintptr_t blocked=0;
bool read_bytes(uintptr_t a,size_t n,void*p){++reads;if(a==blocked)return false;for(auto&[base,v]:memory)if(a>=base&&a-base<=v.size()&&n<=v.size()-(a-base)){std::memcpy(p,v.data()+a-base,n);return true;}return false;}
bool image_range(uintptr_t a,size_t n){return image_good&&a==0x300&&n==32;}
void put(uintptr_t a,uint64_t value){for(auto&[base,v]:memory)if(a>=base&&a+8<=base+v.size()){std::memcpy(v.data()+a-base,&value,8);return;}}
template<class F>bool refused(F f){try{f();}catch(const std::runtime_error&){return true;}return false;}
int main(){
 FField::MemberOffsets={{"ClassPrivate",8},{"Next",24}};FProperty::MemberOffsets={{"UEP_TotalSize",32}};FFieldClass::MemberOffsets={{"UEP_TotalSize",32},{"FName",8},{"CastFlags",16}};
 FMapProperty::MemberOffsets={{"KeyProp",8},{"ValueProp",16}};FSetProperty::MemberOffsets={{"ElementProp",8}};FArrayProperty::MemberOffsets={{"Inner",8}};
 memory[0x100].resize(32);memory[0x200].resize(32);memory[0x300].resize(32);put(0x108,0x200);put(0x110,0x200);put(0x208,0x300);put(0x308,2);put(0x310,1);
 auto* map=reinterpret_cast<FMapProperty*>(0x100);auto* set=reinterpret_cast<FSetProperty*>(0x100);auto* array=reinterpret_cast<FArrayProperty*>(0x100);
 assert(refused([&]{map->GetKeyProp();})&&reads==0);
 NativePropertyGuard::Install(read_bytes,image_range);
 assert(map->GetKeyProp()==reinterpret_cast<FProperty*>(0x200));assert(map->GetValueProp()==map->GetKeyProp());
 assert(std::as_const(*map).GetKeyProp()==map->GetKeyProp());assert(std::as_const(*map).GetValueProp()==map->GetValueProp());
 assert(set->GetElementProp()==map->GetKeyProp());assert(std::as_const(*set).GetElementProp()==set->GetElementProp());
 assert(array->GetInner()==map->GetKeyProp());assert(std::as_const(*array).GetInner()==array->GetInner());
 assert(NativePropertyGuard::ReadClass(reinterpret_cast<FField*>(0x200))==reinterpret_cast<FFieldClass*>(0x300));
 auto before=FMapProperty::MemberOffsets;FMapProperty::MemberOffsets.erase("KeyProp");auto erased=FMapProperty::MemberOffsets.size();assert(refused([&]{map->GetKeyProp();}));assert(FMapProperty::MemberOffsets.size()==erased);FMapProperty::MemberOffsets=before;
 blocked=0x108;reads=0;assert(refused([&]{map->GetKeyProp();})&&reads==1);blocked=0;
 blocked=0x208;assert(refused([&]{map->GetKeyProp();}));blocked=0;
 put(0x208,0x400);assert(refused([&]{map->GetKeyProp();}));put(0x208,0x300);
 image_good=false;assert(refused([&]{map->GetKeyProp();}));image_good=true;
 put(0x308,3);assert(refused([&]{map->GetKeyProp();}));put(0x308,2);
 put(0x310,0);assert(refused([&]{map->GetKeyProp();}));put(0x310,1);
 put(0x108,0);assert(refused([&]{map->GetKeyProp();}));put(0x108,0x200);
 memory[0x200].resize(12);assert(refused([&]{map->GetKeyProp();}));memory[0x200].resize(32);put(0x208,0x300);
 memory[0x400].resize(32);put(0x408,0x300);put(0x218,0x400);
 assert(NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0x200))==reinterpret_cast<FField*>(0x400));
 reads=0;assert(refused([&]{NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0xdead0));})&&reads==1);
 put(0x218,0xdead0);assert(refused([&]{NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0x200));}));
 put(0x218,0);assert(NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0x200))==nullptr);
 put(0x218,0x200);assert(refused([&]{NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0x200));}));
 FField::MemberOffsets.erase("Next");NativeTypeScan::MissingKeys missing;
 missing.attempt([&]{NativePropertyGuard::ReadNext(reinterpret_cast<FField*>(0x200));});assert(missing.values==std::vector<std::string>{"FField::Next"});FField::MemberOffsets["Next"]=24;
 NativePropertyGuard::Revoke();reads=0;assert(refused([&]{map->GetKeyProp();})&&reads==0);
 std::cout<<"actual guarded getters: 28 assertions passed\n";
}
