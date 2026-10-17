#include "NativeThreadAuthority.hpp"
#include <polyhook2/MemAccessor.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <future>
#include <chrono>
#include <iostream>
using namespace NativeThreadIntegration;
using namespace RC::Unreal::Platform;
using namespace std::chrono_literals;
static std::atomic<unsigned> getter_calls{};
static thread_local std::int32_t native_id=77;
static std::function<void()> getter_action;
extern "C" std::int32_t scePthreadGetthreadid(void){++getter_calls;if(getter_action)getter_action();return native_id;}
struct Fixture {
    std::shared_ptr<PLH::OwnedMemory::OwnedMapping> mapping=std::make_shared<PLH::OwnedMemory::OwnedMapping>(64);
    std::uint8_t* data=static_cast<std::uint8_t*>(mapping->data());
    std::uint64_t base=reinterpret_cast<std::uintptr_t>(data)-0x9358a98;
    std::shared_ptr<const ModuleSnapshot> owner;
    rc::codecache::Cache cache;
    Profile profile{"3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc","0a09270e315de7f955b2f90514881fc9c0ac2d992d57d76dc7463a04891204bc","c75fd1f25d0e9d44abc4d2621800452853506425821626c2e21579ed25146590",194241424,197526216,0x9358a98,0x9358aa4,"scePthreadGetthreadid","EI-5-jlq2dE","libkernel"};
    std::atomic<bool> stop=false;
    ImportProof imports{true,"scePthreadGetthreadid","EI-5-jlq2dE","libkernel",std::string(64,'d')};
    MappingLifetimeProof lifetime;
    Fixture(const char* image) {
        reset();ModuleInput input{};input.lifetime=mapping;input.monolithic=true;
        input.images.emplace(RC::ScanTarget::MainExe,ModuleImage{reinterpret_cast<const std::uint8_t*>(base),profile.image_extent,{{data,mapping->size()}}});owner=ModuleSnapshot::Create(std::move(input));
        const std::uint8_t getter_prefix[]={0xff,0x25,0xf2,0x01,0xcc,0x01,0x68,0x5a,0,0,0,0xe9,0x40,0xfa,0xff,0xff};
        const rc::codecache::Pin pin{1,"native-current-ID-PLT",0x74723a0,getter_prefix,sizeof(getter_prefix)};
        const rc::codecache::Profile p{image,profile.image_sha,profile.image_size,profile.image_extent,base,&pin,1};
        struct Ops {int open(const char* p){return ::open(p,O_RDONLY);}std::int64_t pread(int fd,void* p,std::size_t n,std::uint64_t o){return ::pread(fd,p,n,o);}int close(int fd){return ::close(fd);}std::uint64_t now_ms(){return 1000;}}ops;
        const auto r=cache.prepare(p,base,base,base+profile.image_extent,1,ops);if(!r.ok)throw std::runtime_error(r.reason);
        lifetime={true,mapping,[this]{return stop.load();}};
    }
    void reset(){data[0]=77;data[1]=data[2]=data[3]=0;data[12]=1;native_id=77;getter_action={};stop=false;}
    Prepared prepare(){return Authority::prepare(owner,&cache.metadata,profile,profile.layout_digest,2000,imports,lifetime);}
};
int main(int argc,char** argv){
 if(argc!=2)return 2;Fixture f(argv[1]);unsigned checks=0,failures=0;
 auto check=[&](const char* name,const std::function<bool()>& run){f.reset();++checks;bool ok=false;try{ok=run();}catch(...){}std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;};
 check("real authenticated disk and registered owned BSS sample admit",[&]{auto p=f.prepare();if(!p.authority)return false;auto a=p.authority->acquire();return a.lease&&a.refusal==Refusal::None;});
 check("missing import certificate refuses before getter",[&]{auto i=f.imports;i.independently_verified=false;auto count=getter_calls.load();auto p=Authority::prepare(f.owner,&f.cache.metadata,f.profile,f.profile.layout_digest,2000,i,f.lifetime);return !p.authority&&p.refusal==Refusal::ImportAuthority&&getter_calls==count;});
 check("wrong import NID refuses",[&]{auto i=f.imports;i.nid="different";auto p=Authority::prepare(f.owner,&f.cache.metadata,f.profile,f.profile.layout_digest,2000,i,f.lifetime);return p.refusal==Refusal::ImportAuthority;});
 check("wrong import library refuses",[&]{auto i=f.imports;i.library="different";return Authority::prepare(f.owner,&f.cache.metadata,f.profile,f.profile.layout_digest,2000,i,f.lifetime).refusal==Refusal::ImportAuthority;});
 check("metadata-only mapping lifetime refuses",[&]{auto l=f.lifetime;l.independently_verified=false;return Authority::prepare(f.owner,&f.cache.metadata,f.profile,f.profile.layout_digest,2000,f.imports,l).refusal==Refusal::MappingLifetime;});
 check("missing owning module refuses",[&]{return Authority::prepare({},&f.cache.metadata,f.profile,f.profile.layout_digest,2000,f.imports,f.lifetime).refusal==Refusal::ImageOwner;});
 check("missing fresh cache refuses",[&]{return Authority::prepare(f.owner,nullptr,f.profile,f.profile.layout_digest,2000,f.imports,f.lifetime).refusal==Refusal::Metadata;});
 check("expired initializer authentication refuses",[&]{return Authority::prepare(f.owner,&f.cache.metadata,f.profile,f.profile.layout_digest,61000,f.imports,f.lifetime).refusal==Refusal::Metadata;});
 check("wrong layout digest refuses",[&]{return Authority::prepare(f.owner,&f.cache.metadata,f.profile,"different",2000,f.imports,f.lifetime).refusal==Refusal::Profile;});
 check("wrong image SHA refuses",[&]{auto m=f.cache.metadata;m.sha256[0]='0';return Authority::prepare(f.owner,&m,f.profile,f.profile.layout_digest,2000,f.imports,f.lifetime).refusal==Refusal::Metadata;});
 check("source/load-base disagreement refuses",[&]{auto m=f.cache.metadata;++m.base;return Authority::prepare(f.owner,&m,f.profile,f.profile.layout_digest,2000,f.imports,f.lifetime).refusal==Refusal::Base;});
 check("file-backed field cannot masquerade as BSS",[&]{auto p=f.profile;p.id_offset=0x200;return Authority::prepare(f.owner,&f.cache.metadata,p,p.layout_digest,2000,f.imports,f.lifetime).refusal==Refusal::ReadableBss;});
 check("current registry denial refuses without fallback",[&]{auto p=f.prepare();if(!p.authority)return false;PLH::MemAccessor access;bool ok=false;access.mem_protect(reinterpret_cast<std::uintptr_t>(f.data),f.mapping->size(),PLH::W,ok);if(!ok)return false;auto a=p.authority->acquire();access.mem_protect(reinterpret_cast<std::uintptr_t>(f.data),f.mapping->size(),PLH::R|PLH::W,ok);return ok&&!a.lease&&a.refusal==Refusal::RegistryRead;});
 check("readiness0 refuses current native thread",[&]{auto p=f.prepare();if(!p.authority)return false;f.data[12]=0;auto a=p.authority->acquire();return !a.lease&&p.authority->known_affinity()==std::thread::id{};});
 check("equal negative getter/global error refuses",[&]{auto p=f.prepare();if(!p.authority)return false;f.data[0]=f.data[1]=f.data[2]=f.data[3]=0xff;native_id=-1;return !p.authority->acquire().lease;});
 check("foreign native ID refuses before affinity publication",[&]{auto p=f.prepare();if(!p.authority)return false;native_id=78;auto a=p.authority->acquire();return !a.lease&&p.authority->known_affinity()==std::thread::id{};});
 check("readiness transition during getter refuses",[&]{auto p=f.prepare();if(!p.authority)return false;getter_action=[&]{f.data[12]=0;};return !p.authority->acquire().lease;});
 check("engine ID transition during getter refuses",[&]{auto p=f.prepare();if(!p.authority)return false;getter_action=[&]{f.data[0]=78;};return !p.authority->acquire().lease;});
 check("immutable metadata copy survives startup cache retirement",[&]{auto p=f.prepare();if(!p.authority)return false;auto saved=f.cache.metadata;f.cache.clear();auto a=p.authority->acquire();f.cache.metadata=saved;return bool(a.lease);});
 check("opaque affinity refuses another thread with equal native ID",[&]{auto p=f.prepare();if(!p.authority)return false;auto first=p.authority->acquire();if(!first.lease)return false;bool rejected=false;std::thread t([&]{native_id=77;rejected=!p.authority->acquire().lease;});t.join();return rejected;});
 check("external cancellation refuses without publication",[&]{auto p=f.prepare();if(!p.authority)return false;f.stop=true;auto a=p.authority->acquire();return !a.lease&&a.refusal==Refusal::Cancelled&&p.authority->known_affinity()==std::thread::id{};});
 check("self cancellation refuses deadlocking wait",[&]{auto p=f.prepare();if(!p.authority)return false;auto a=p.authority->acquire();return a.lease&&!p.authority->cancel_and_wait()&&!p.authority->acquire().lease;});
 check("cancellation waits actual callback lifetime ticket",[&]{auto p=f.prepare();if(!p.authority)return false;auto a=p.authority->acquire();if(!a.lease)return false;std::promise<void> began;auto fut=std::async(std::launch::async,[&]{began.set_value();return p.authority->cancel_and_wait();});began.get_future().wait();bool waits=fut.wait_for(20ms)==std::future_status::timeout;a.lease.reset();return waits&&fut.get()&&!p.authority->acquire().lease;});
 getter_action={};std::cout<<"checks="<<checks<<" failures="<<failures<<" genuine-game-execution=0\n";return failures?1:0;
}
