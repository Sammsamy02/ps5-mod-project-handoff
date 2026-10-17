#include "NameThreadPolicy.hpp"
#include <functional>
#include <iostream>
using namespace NativeNameThread;
struct Fixture {
 std::int32_t id=77,after=77;const void* game=reinterpret_cast<const void*>(0x19402ac8);
 const void* returned=game;bool getter_ok=true,name_ok=true;unsigned gets=0,names=0;
 std::function<void()> on_name;
 static bool get(void* p,std::int32_t& o){auto& f=*static_cast<Fixture*>(p);o=f.gets++?f.after:f.id;return f.getter_ok;}
 static bool name(void* p,std::uint32_t id,const void*& o){auto& f=*static_cast<Fixture*>(p);++f.names;if(id!=static_cast<std::uint32_t>(f.id))return false;if(f.on_name)f.on_name();o=f.returned;return f.name_ok;}
 Ops ops(){return {this,get,name};}
 void reset(){id=after=77;returned=game;getter_ok=name_ok=true;gets=names=0;on_name={};}
};
int main(){Fixture f;unsigned checks=0,failures=0;
 auto check=[&](const char* name,const std::function<bool()>& fn){f.reset();++checks;bool ok=false;try{ok=fn();}catch(...){}std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;};
 check("positive native ID and exact Game address admits",[&]{Policy p;return p.authenticate(f.ops(),f.game)==Result::Accepted;});
 check("Game identity publishes only after return",[&]{Policy p;bool success=false;f.on_name=[&]{success=true;};return p.authenticate(f.ops(),f.game)==Result::Accepted&&success;});
 check("global0/preinitialization cannot match positive ID model",[&]{Policy p;f.returned=reinterpret_cast<const void*>(0x1932a5a0);return p.authenticate(f.ops(),f.game)==Result::NotNamedGameThread;});
 check("negative ID refuses before engine accessor",[&]{Policy p;f.id=f.after=-1;return p.authenticate(f.ops(),f.game)==Result::InvalidId&&f.names==0;});
 check("zero ID refuses before engine accessor",[&]{Policy p;f.id=f.after=0;return p.authenticate(f.ops(),f.game)==Result::InvalidId&&f.names==0;});
 check("getter failure refuses",[&]{Policy p;f.getter_ok=false;return p.authenticate(f.ops(),f.game)==Result::Unavailable&&f.names==0;});
 check("missing callback refuses",[&]{Policy p;auto o=f.ops();o.name=nullptr;return p.authenticate(o,f.game)==Result::Unavailable;});
 check("no expected address refuses",[&]{Policy p;return p.authenticate(f.ops(),nullptr)==Result::Unavailable;});
 check("null returned address refuses",[&]{Policy p;f.returned=nullptr;return p.authenticate(f.ops(),f.game)==Result::NotNamedGameThread;});
 check("render storage refuses",[&]{Policy p;f.returned=reinterpret_cast<const void*>(0x19402ae0);return p.authenticate(f.ops(),f.game)==Result::NotNamedGameThread;});
 check("heap/string contents are not identity",[&]{Policy p;f.returned=reinterpret_cast<const void*>(0x123);return p.authenticate(f.ops(),f.game)==Result::NotNamedGameThread;});
 check("lookup failure refuses",[&]{Policy p;f.name_ok=false;return p.authenticate(f.ops(),f.game)==Result::Unavailable;});
 check("lookup exception refuses",[&]{Policy p;f.on_name=[](){throw 5;};return p.authenticate(f.ops(),f.game)==Result::Unavailable;});
 check("current ID transition refuses",[&]{Policy p;f.after=78;return p.authenticate(f.ops(),f.game)==Result::IdChanged;});
 check("second negative ID refuses",[&]{Policy p;f.after=-1;return p.authenticate(f.ops(),f.game)==Result::IdChanged;});
 check("initialization conflict refuses",[&]{Policy p;if(p.authenticate(f.ops(),f.game)!=Result::Accepted)return false;f.gets=0;f.id=f.after=78;return p.authenticate(f.ops(),f.game)==Result::NativeInitConflict;});
 check("same native ID on foreign opaque thread refuses",[&]{Policy p;if(p.authenticate(f.ops(),f.game)!=Result::Accepted)return false;Result result{};std::thread t([&]{f.gets=0;result=p.authenticate(f.ops(),f.game);});t.join();return result==Result::AffinityForeign;});
 check("cancellation before getter refuses",[&]{Policy p;p.cancel();return p.authenticate(f.ops(),f.game)==Result::Cancelled&&f.gets==0;});
 check("cancellation inside accessor refuses",[&]{Policy p;f.on_name=[&]{p.cancel();};return p.authenticate(f.ops(),f.game)==Result::Cancelled;});
 check("repeated positive identity on same thread admits",[&]{Policy p;if(p.authenticate(f.ops(),f.game)!=Result::Accepted)return false;f.gets=0;return p.authenticate(f.ops(),f.game)==Result::Accepted;});
 std::cout<<"checks="<<checks<<" failures="<<failures<<" native-execution=0\n";return failures?1:0;
}
