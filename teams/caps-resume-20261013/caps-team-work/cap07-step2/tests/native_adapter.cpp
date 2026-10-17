// Compile the actual adapter against host fixtures for its existing dependencies.
// This does not compile the genuine Unreal/target ABI or emulate engine locks.
#include "../PATCH/RuntimePakMount.hpp"
#include "../PATCH/PakRequestProducer.hpp"
#include "../PATCH/pak_mount_pins.inc"
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
namespace rc::codecache {struct Metadata{bool ready=true;char sha256[65]{};};}
namespace RC::Unreal {struct FString {std::string value;FString()=default;explicit FString(std::string s):value(std::move(s)){};};}
namespace RC::LuaType::LuaFNameBridge {struct Bridge{std::string s;Bridge(const char*p,size_t n):s(p,n){}bool ok(){return true;}RC::Unreal::FString to_fstring(){return RC::Unreal::FString(s);}};}
static int frees=0,calls=0,mode=0;static void* row_address=nullptr;
namespace RC::Unreal {
struct Allocator {void Free(void*p){++frees;std::free(p);}};static Allocator allocator;static Allocator* allocator_ptr=&allocator;static Allocator** GMalloc=&allocator_ptr;
static bool IsInGameThreadRaw(){return true;}
namespace UnrealInitializer::SeedArming {
static uint64_t module_view_base(){return 1;}
static int bridge_code_cache_context(uint64_t,const rc::codecache::Metadata**p){static rc::codecache::Metadata m;*p=&m;return 1;}
static int bridge_code_cache_signature(uint64_t,uint64_t,const uint8_t*,size_t){return 1;}
static bool PropertyReadBytes(uint64_t a,size_t n,void*out){if(mode==2&&a==reinterpret_cast<uint64_t>(row_address))return false;std::memcpy(out,reinterpret_cast<void*>(a),n);return true;}
#include "../PATCH/NativePakMountAdapter.inl"
}}
struct Result{void* data;int32_t count,capacity;};
static void* snapshot(void* out,void*){++calls;auto&r=*static_cast<Result*>(out);r.data=std::malloc(16);row_address=r.data;r.count=1;r.capacity=1;uint64_t id=77;uint32_t order=12;std::memcpy(r.data,&id,8);std::memcpy(static_cast<char*>(r.data)+8,&order,4);if(mode==1)r.count=-1;if(mode==3)throw std::runtime_error("after allocation");return out;}
int main(){
 using namespace ModBridge::PakMount;using RC::Unreal::UnrealInitializer::SeedArming::NativePakMountBridge;
 uint64_t vt[16]{};vt[12]=reinterpret_cast<uint64_t>(&snapshot);uint64_t instance[6]{reinterpret_cast<uint64_t>(vt),0,0,42,0x1780000,0};uint64_t delegate[2]{reinterpret_cast<uint64_t>(instance),4};
 auto p=modbridge_pak_pins_ue561();p.snapshot_delegate=reinterpret_cast<uint64_t>(delegate)-1;p.snapshot_vtable=reinterpret_cast<uint64_t>(vt)-1;p.snapshot_execute=reinterpret_cast<uint64_t>(&snapshot)-1;p.proofs[4].linked=p.snapshot_execute;
 NativePakMountBridge e(p);std::vector<PakInfo> rows;int checks=0,failed=0;
 auto check=[&](bool ok,const char*s){++checks;if(!ok){++failed;std::printf("FAIL %s\n",s);}};
 for(mode=0;mode<=3;++mode){int before=frees;bool ok=false;try{ok=e.snapshot_paks(42,rows);}catch(const std::runtime_error&){}check(frees==before+1,"returned allocation released on all exits");check(ok==(mode==0),"snapshot result reflects validation/read/throw");if(ok)check(rows.size()==1&&rows[0].identity==77&&rows[0].order==12,"copied exact identity/order");}
 int before=calls;mode=0;check(!e.snapshot_paks(43,rows)&&calls==before,"wrong captured receiver refuses before call");
 instance[5]=1;check(!e.snapshot_paks(42,rows)&&calls==before,"nonzero delegate this-adjustor refuses before call");
 instance[5]=0;instance[4]=0;check(!e.snapshot_paks(42,rows)&&calls==before,"null snapshot member target refuses before call");
 instance[4]=0x1780000;check(e.snapshot_paks(42,rows)&&calls==before+1,"restored bound-member identity snapshot proceeds");
 std::printf("native adapter fixture: %d assertions, %d failed\n",checks,failed);return failed?1:0;
}
