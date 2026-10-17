#include "NativeThreadNameAdapter.hpp"
#include "ExtendedCodeProfile.hpp"
#include "CandidateBindingData.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <future>
using namespace NativeNameThread;using namespace RC::Unreal::Platform;
static thread_local std::int32_t model_current=77;
static std::uint32_t model_game=77,model_render=88;
static std::uintptr_t model_base;
static unsigned getter_calls,name_calls;
static std::function<void()> in_lookup;
namespace NativeNameThread {
// Explicit host API contract fixture; never calls game or target getter/code.
std::int32_t current_native_id(){++getter_calls;return model_current;}
const void* call_native_name(std::uintptr_t entry,std::uint32_t id){++name_calls;if(entry!=model_base+kAccessor)throw 1;if(in_lookup)in_lookup();
 return reinterpret_cast<const void*>(model_base+(id==model_game?kGameNameStorage:id==model_render?kRenderNameStorage:kEmptyNameStorage));}
}
struct Fixture {
 std::shared_ptr<std::array<std::uint8_t,64>> bytes=std::make_shared<std::array<std::uint8_t,64>>();
 std::uint64_t base=reinterpret_cast<std::uintptr_t>(bytes->data());
 std::shared_ptr<const ModuleSnapshot> owner;rc::codecache::Cache cache;
 std::shared_ptr<RC::Unreal::UnrealInitializer::StartupWaitControl> cancel=std::make_shared<RC::Unreal::UnrealInitializer::StartupWaitControl>();
 bool stop=false;Data data=kCandidateBindingData;
 Fixture(const char* image){model_base=base;ModuleInput input{};input.lifetime=bytes;input.monolithic=true;input.images.emplace(RC::ScanTarget::MainExe,ModuleImage{bytes->data(),kImageExtent,{{bytes->data(),bytes->size()}}});owner=ModuleSnapshot::Create(std::move(input));
  auto profile=kExtendedCodeProfile;profile.path=image;profile.base=base;
  struct Ops {int open(const char* p){return ::open(p,O_RDONLY);}std::int64_t pread(int f,void*p,std::size_t n,std::uint64_t o){return ::pread(f,p,n,o);}int close(int f){return ::close(f);}std::uint64_t now_ms(){return 1000;}}ops;
  auto result=cache.prepare(profile,base,base,base+kImageExtent,1,ops);if(!result.ok)throw std::runtime_error(result.reason);
  cancel->IsCancelled=[this]{return stop;};
 }
 void reset(){model_current=77;model_game=77;model_render=88;stop=false;getter_calls=name_calls=0;in_lookup={};}
 Prepared prepare(){return Binding::prepare(owner,&cache.metadata,data,data.layout_digest,2000,true,cancel);}
};
int main(int argc,char** argv){if(argc!=2)return 2;Fixture f(argv[1]);unsigned checks=0,failures=0;
 auto check=[&](const char* name,const std::function<bool()>& fn){f.reset();++checks;bool ok=false;try{ok=fn();}catch(...){}std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;};
 check("actual binder/genuine authenticated three-pin image admit host API model",[&]{auto p=f.prepare();return p.binding&&bool(p.binding->acquire().ticket);});
 check("old two-pin R4 cache refuses before getter",[&]{auto m=f.cache.metadata;m.count=2;auto p=Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel);return !p.binding&&p.refusal==Refusal::CodePin&&getter_calls==0;});
 check("missing import admission refuses before getter",[&]{auto p=Binding::prepare(f.owner,&f.cache.metadata,f.data,f.data.layout_digest,2000,false,f.cancel);return p.refusal==Refusal::Import&&getter_calls==0;});
 check("wrong prefix refuses",[&]{auto m=f.cache.metadata;m.entries[2].bytes[0]^=1;return Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::CodePin;});
 check("wrong named function identity refuses",[&]{auto m=f.cache.metadata;m.entries[2].name="unrelated";return Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::CodePin;});
 check("writable code segment refuses",[&]{auto m=f.cache.metadata;m.segments[m.entries[2].segment].flags|=2;return Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::CodePin;});
 check("wrong SHA refuses",[&]{auto m=f.cache.metadata;m.sha256[0]='0';return Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::Image;});
 check("wrong base refuses",[&]{auto m=f.cache.metadata;++m.base;return Binding::prepare(f.owner,&m,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::Image;});
 check("wrong layout digest refuses",[&]{return Binding::prepare(f.owner,&f.cache.metadata,f.data,"wrong",2000,true,f.cancel).refusal==Refusal::Profile;});
 check("expired initial authentication refuses",[&]{return Binding::prepare(f.owner,&f.cache.metadata,f.data,f.data.layout_digest,61000,true,f.cancel).refusal==Refusal::Image;});
 check("missing owner refuses",[&]{return Binding::prepare({},&f.cache.metadata,f.data,f.data.layout_digest,2000,true,f.cancel).refusal==Refusal::Owner;});
 check("invalid static storage range refuses",[&]{auto d=f.data;d.game_name=d.image_extent-1;return Binding::prepare(f.owner,&f.cache.metadata,d,d.layout_digest,2000,true,f.cancel).refusal==Refusal::Image;});
 check("static storage in executable segment refuses",[&]{auto d=f.data;d.game_name=0x200;return Binding::prepare(f.owner,&f.cache.metadata,d,d.layout_digest,2000,true,f.cancel).refusal==Refusal::Storage;});
 check("globalID0 refuses positive caller without BSS dereference",[&]{auto p=f.prepare();if(!p.binding)return false;model_game=0;return !p.binding->acquire().ticket&&name_calls==1;});
 check("foreign and render ID both refuse",[&]{auto p=f.prepare();if(!p.binding)return false;model_current=88;bool a=!p.binding->acquire().ticket;model_current=99;return a&&!p.binding->acquire().ticket;});
 check("negative getter refuses without accessor call",[&]{auto p=f.prepare();if(!p.binding)return false;model_current=-1;return !p.binding->acquire().ticket&&name_calls==0;});
 check("cancelled getter/accessor lifetime refuses",[&]{auto p=f.prepare();if(!p.binding)return false;f.stop=true;return !p.binding->acquire().ticket&&getter_calls==0;});
 check("lookup exception refuses",[&]{auto p=f.prepare();if(!p.binding)return false;in_lookup=[](){throw 1;};return !p.binding->acquire().ticket;});
 check("startup cache retirement does not invalidate copied binding",[&]{auto p=f.prepare();if(!p.binding)return false;auto m=f.cache.metadata;f.cache.clear();auto a=p.binding->acquire();f.cache.metadata=m;return bool(a.ticket);});
 check("same-thread self wait refuses and cancels",[&]{auto p=f.prepare();if(!p.binding)return false;auto a=p.binding->acquire();return a.ticket&&!p.binding->cancel_and_wait()&&!p.binding->acquire().ticket;});
 check("cancel waits outstanding ticket",[&]{auto p=f.prepare();if(!p.binding)return false;auto a=p.binding->acquire();if(!a.ticket)return false;std::promise<void> began;auto fut=std::async(std::launch::async,[&]{began.set_value();return p.binding->cancel_and_wait();});began.get_future().wait();bool waits=fut.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;a.ticket.reset();return waits&&fut.get();});
 std::cout<<"checks="<<checks<<" failures="<<failures<<" native-execution=0 direct-BSS-reads=0\n";return failures?1:0;
}
