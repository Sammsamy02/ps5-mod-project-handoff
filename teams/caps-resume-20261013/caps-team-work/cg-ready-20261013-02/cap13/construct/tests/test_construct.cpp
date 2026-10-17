#include "fake_engine.hpp"
#include <stdexcept>
#include <atomic>
#include <thread>
using namespace RC::Platform::NativeMods;
static int checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line=%d: %s\n",__LINE__,#x);}}while(0)
struct RecordingEngine : FakeEngine
{
 int creates{}; mbm_object last_cls{},last_outer{}; int sentinel{};
 mbm_status result{MBM_OK}; bool throws{},partial{},blocked{};
 std::atomic<bool> entered{false},release{false};
 auto construct_object(mbm_object cls,mbm_object outer,mbm_object*out)->mbm_status override
 {
  ++creates;last_cls=cls;last_outer=outer;
  if(blocked){entered=true;while(!release)std::this_thread::yield();}
  if(throws)throw std::runtime_error("backend");
  *out=partial?reinterpret_cast<void*>(1):&sentinel;
  return result;
 }
};
static mbm_mod_v1 mod;
static bool destroyed;
static mbm_status destroy(void*){destroyed=true;return MBM_OK;}
static mbm_status entry(const mbm_host_v1*,uint64_t,const mbm_mod_v1**out)
{
 mod={};mod.magic=MBM_ABI_MAGIC;mod.abi_version=MBM_ABI_VERSION;
 mod.struct_size=sizeof(mod);mod.name="construction-service-test";mod.destroy=destroy;
 *out=&mod;return MBM_OK;
}
int main()
{
 static_assert(offsetof(mbm_host_v1,construct_object)==184,"old x64 ABI prefix must remain byte-compatible");
 RecordingEngine e;Limits limits;limits.drain_timeout=std::chrono::milliseconds{1};Host h(e,"test",limits);uint64_t token{};
 CHECK(h.attach("test",entry,&token)==MBM_OK);
 const auto*t=h.table();CHECK(t->struct_size==sizeof(mbm_host_v1));CHECK(t->abi_version==1);
 alignas(void*) unsigned char old_table[184]{};
 std::memcpy(old_table,t,sizeof(old_table));
 uint32_t old_size=184;std::memcpy(old_table+8,&old_size,sizeof(old_size));
 auto*old= reinterpret_cast<mbm_host_v1*>(old_table);
 CHECK(!mbm_host_has_construct_object(old));CHECK(mbm_host_has_construct_object(t));
 int cls{},outer{};mbm_object out=reinterpret_cast<void*>(1);
 CHECK(t->construct_object(t->ctx,token,&cls,&outer,&out)==MBM_OK);
 CHECK(out==&e.sentinel && e.creates==1 && e.last_cls==&cls && e.last_outer==&outer);
 CHECK(t->construct_object(t->ctx,token,nullptr,&outer,&out)==MBM_E_INVALID);CHECK(out==nullptr);
 CHECK(t->construct_object(t->ctx,token,&cls,&outer,nullptr)==MBM_E_INVALID);
 int before=e.creates;CHECK(t->construct_object(t->ctx,999,&cls,&outer,&out)==MBM_E_INVALID);CHECK(e.creates==before && out==nullptr);
 e.game_thread=false;CHECK(t->construct_object(t->ctx,token,&cls,&outer,&out)==MBM_E_STATE);CHECK(e.creates==before);e.game_thread=true;
 e.result=MBM_E_TYPE;e.partial=true;CHECK(t->construct_object(t->ctx,token,&cls,&outer,&out)==MBM_E_TYPE);CHECK(out==nullptr);
 e.result=MBM_OK;e.partial=false;e.throws=true;CHECK(t->construct_object(t->ctx,token,&cls,&outer,&out)==MBM_E_INTERNAL);CHECK(out==nullptr);e.throws=false;
 e.blocked=true;destroyed=false;
 std::thread worker([&]{mbm_object v{};CHECK(t->construct_object(t->ctx,token,&cls,&outer,&v)==MBM_OK);});
 while(!e.entered)std::this_thread::yield();
 // A bounded stop must retain code rather than destroy under construction.
 auto stopped=h.stop(token);
 CHECK(stopped==ModState::Retained);CHECK(!destroyed);
 e.release=true;worker.join();
 before=e.creates;CHECK(t->construct_object(t->ctx,token,&cls,&outer,&out)==MBM_E_STATE);CHECK(e.creates==before && out==nullptr);
 FakeEngine fallback;Host fh(fallback,"default-backend");uint64_t ft{};
 CHECK(fh.attach("fallback",entry,&ft)==MBM_OK);
 CHECK(fh.table()->construct_object(fh.table()->ctx,ft,&cls,nullptr,&out)==MBM_E_UNAVAILABLE);CHECK(out==nullptr);
 printf("construct checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
