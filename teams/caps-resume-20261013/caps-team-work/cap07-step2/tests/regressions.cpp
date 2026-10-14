#include "mock_engine.hpp"
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include "../PATCH/pak_mount_pins.inc"
using namespace ModBridge::PakMount;
int checks=0, failures=0;
#define C(x) do{++checks;if(!(x)){++failures;std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#x);}}while(0)
MountRequest request(){MountRequest r;r.pak_path_abs="/owned/mod.pak";r.order=10;return r;}
struct Fault : mock::MockBridge {
 using MockBridge::MockBridge; bool unreadable=false,throws=false;
 bool mounted_pak_count(uint64_t a,uint32_t& o) override {if(unreadable)return false;return MockBridge::mounted_pak_count(a,o);}
 uint64_t call_execute(uint64_t a,uint64_t b,void*c,int32_t d) override {if(throws)throw std::runtime_error("fixture engine throw");return MockBridge::call_execute(a,b,c,d);}
};
int main(){
 {mock::FakeImage i;i.wire();Fault e(i);auto p=modbridge_pak_pins_ue561();p.handle_mount_pak=0;C(mount(p,request(),e).verdict==Verdict::RefusedPins);C(e.execute_calls==0);}
 {mock::FakeImage i;i.wire();Fault e(i);auto r=request();r.pak_path_abs="relative.pak";C(mount(modbridge_pak_pins_ue561(),r,e).verdict==Verdict::RefusedBadRequest);C(e.execute_calls==0);}
 {mock::FakeImage i;i.wire();Fault e(i);e.unreadable=true;C(mount(modbridge_pak_pins_ue561(),request(),e).verdict!=Verdict::Mounted);C(e.execute_calls==0);}
 {mock::FakeImage i;i.wire();Fault e(i);e.throws=true;try{mount(modbridge_pak_pins_ue561(),request(),e);}catch(const std::runtime_error&){} C(e.fstrings_live==0);}
 {mock::FakeImage i;i.wire();Fault e(i);i.mem64[i.delegate_inst+0x18]=i.pak_layer+8;C(mount(modbridge_pak_pins_ue561(),request(),e).verdict!=Verdict::Mounted);C(e.execute_calls==0);}
 {int32_t a=0;uint32_t o=0;C(!choose_order("/owned/a.pak",{{1,3},{1,4}},a,o));}
 {int32_t a=0;uint32_t o=0;C(choose_order("/owned/a_p.pak",{{1,3}},a,o)&&o==104);}
 std::printf("regressions: %d assertions, %d failed\n",checks,failures);return failures?1:0;
}
