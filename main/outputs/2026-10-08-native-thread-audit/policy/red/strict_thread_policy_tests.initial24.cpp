#include <StrictNativeThreadPolicy.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <functional>
using namespace NativeThreadAudit;
struct Input {
    EngineSample first{1,77}, second{1,77};
    std::int32_t current=77;
    bool first_ok=true, second_ok=true, current_ok=true;
    unsigned reads=0;
    StrictThreadPolicy* cancel_in_current=nullptr;
    StrictThreadPolicy* cancel_in_first=nullptr;
    static bool snapshot(void* p,EngineSample& out) {
        auto& x=*static_cast<Input*>(p); const bool first=x.reads++==0;
        if(first&&x.cancel_in_first) x.cancel_in_first->cancel();
        out=first?x.first:x.second; return first?x.first_ok:x.second_ok;
    }
    static bool id(void* p,std::int32_t& out) {
        auto& x=*static_cast<Input*>(p);
        if(x.cancel_in_current) x.cancel_in_current->cancel();
        out=x.current; return x.current_ok;
    }
    ReadOps ops() { reads=0; return {this,snapshot,id}; }
};
static Admission call(StrictThreadPolicy& p,Input& i) {return p.admit(i.ops());}
static bool retry_after_refusal(Input i,Admission expected) {
    StrictThreadPolicy p; if(call(p,i)!=expected)return false;
    Input good; return call(p,good)==Admission::Accepted;
}
int main() {
    std::vector<std::pair<const char*,std::function<bool()>>> tests{
        {"positive native ID77 admits",[]{StrictThreadPolicy p;Input i;return call(p,i)==Admission::Accepted;}},
        {"repeat same engine and opaque affinity admits",[]{StrictThreadPolicy p;Input i;return call(p,i)==Admission::Accepted&&call(p,i)==Admission::Accepted;}},
        {"readiness0 refuses and does not bind",[]{Input i;i.first.ready=0;return retry_after_refusal(i,Admission::NotReady);}},
        {"readiness2 refuses",[]{Input i;i.first.ready=2;return retry_after_refusal(i,Admission::NotReady);}},
        {"zero stored ID refuses",[]{Input i;i.first.game_id=0;return retry_after_refusal(i,Admission::InvalidId);}},
        {"zero current ID refuses",[]{Input i;i.current=0;return retry_after_refusal(i,Admission::InvalidId);}},
        {"shared high-bit error value refuses",[]{Input i;i.first.game_id=i.second.game_id=0x80020001U;i.current=static_cast<std::int32_t>(0x80020001U);return retry_after_refusal(i,Admission::InvalidId);}},
        {"shared negative error value refuses",[]{Input i;i.first.game_id=i.second.game_id=0xffffffffU;i.current=-1;return retry_after_refusal(i,Admission::InvalidId);}},
        {"foreign native ID refuses before affinity binding",[]{Input i;i.current=78;return retry_after_refusal(i,Admission::NativeForeign);}},
        {"first snapshot failure refuses",[]{Input i;i.first_ok=false;return retry_after_refusal(i,Admission::ReadFailed);}},
        {"getter failure refuses",[]{Input i;i.current_ok=false;return retry_after_refusal(i,Admission::ReadFailed);}},
        {"second snapshot failure refuses",[]{Input i;i.second_ok=false;return retry_after_refusal(i,Admission::ReadFailed);}},
        {"readiness1 to0 refuses",[]{Input i;i.second.ready=0;return retry_after_refusal(i,Admission::SnapshotChanged);}},
        {"readiness0 to1 does not authorize preinit",[]{Input i;i.first.ready=0;i.second.ready=1;return retry_after_refusal(i,Admission::NotReady);}},
        {"ID77 to78 snapshot transition refuses",[]{Input i;i.second.game_id=78;return retry_after_refusal(i,Admission::SnapshotChanged);}},
        {"ID77 to0 snapshot transition refuses",[]{Input i;i.second.game_id=0;return retry_after_refusal(i,Admission::SnapshotChanged);}},
        {"conflicting later engine initialization refuses",[]{StrictThreadPolicy p;Input i;if(call(p,i)!=Admission::Accepted)return false;i.first.game_id=i.second.game_id=78;i.current=78;return call(p,i)==Admission::InitConflict;}},
        {"same native ID on foreign opaque thread refuses",[]{StrictThreadPolicy p;Input i;if(call(p,i)!=Admission::Accepted)return false;Admission result=Admission::Accepted;std::thread t([&]{Input j;result=call(p,j);});t.join();return result==Admission::AffinityForeign&&call(p,i)==Admission::Accepted;}},
        {"cancel before snapshot refuses",[]{StrictThreadPolicy p;Input i;p.cancel();return call(p,i)==Admission::Cancelled;}},
        {"cancel during getter refuses",[]{StrictThreadPolicy p;Input i;i.cancel_in_current=&p;return call(p,i)==Admission::Cancelled;}},
        {"cancel during first snapshot refuses",[]{StrictThreadPolicy p;Input i;i.cancel_in_first=&p;return call(p,i)==Admission::Cancelled;}},
        {"positive signed32 maximum admits",[]{StrictThreadPolicy p;Input i;i.first.game_id=i.second.game_id=0x7fffffffU;i.current=0x7fffffff;return call(p,i)==Admission::Accepted;}},
        {"missing reader seam refuses",[]{StrictThreadPolicy p;return p.admit({})==Admission::ReadFailed;}},
        {"eight concurrent equal IDs bind one opaque affinity",[]{StrictThreadPolicy p;std::vector<std::thread> ts;std::atomic<unsigned> accepted=0,foreign=0;std::atomic<bool> go=false;for(int n=0;n<8;++n)ts.emplace_back([&]{while(!go.load())std::this_thread::yield();Input i;auto r=call(p,i);if(r==Admission::Accepted)++accepted;else if(r==Admission::AffinityForeign)++foreign;});go=true;for(auto& t:ts)t.join();return accepted==1&&foreign==7;}},
    };
    unsigned failures=0;
    for(auto& [name,test]:tests) {const bool ok=test();std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;}
    std::cout<<"tests="<<tests.size()<<" failures="<<failures<<'\n';return failures?1:0;
}
