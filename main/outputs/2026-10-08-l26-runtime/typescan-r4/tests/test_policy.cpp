#include "NativeTypeScanPolicy.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace RC::Unreal::NativeTypeScan;
int main() {
 MissingKeys keys; int visits=0;
 keys.attempt([&]{++visits; throw std::runtime_error("Tried getting member variable 'FMapProperty::KeyProp' that doesn't exist in this engine version.");});
 keys.attempt([&]{++visits; throw std::runtime_error("Tried getting member variable 'FMapProperty::ValueProp' that doesn't exist in this engine version.");});
 keys.attempt([&]{++visits; throw std::runtime_error("Tried getting member variable 'FMapProperty::KeyProp' that doesn't exist in this engine version.");});
 assert(visits==3 && keys.values.size()==2);
 bool preserved=false; try { keys.attempt([]{throw std::runtime_error("unrelated");}); } catch (const std::runtime_error&) {preserved=true;} assert(preserved);
 preserved=false; try { keys.attempt([]{throw std::bad_alloc{};}); } catch(const std::bad_alloc&) {preserved=true;} assert(preserved);
 assert(types_ready({{"optional",false,false},{"core",true,true}}));
 assert(!types_ready({{"optional",false,true},{"core",true,false}}));
 std::map<uintptr_t,std::vector<unsigned char>> mem;
 int reads=0; auto reader=[&](uintptr_t a,size_t n,void*out){++reads;auto it=mem.find(a);if(it==mem.end()||it->second.size()<n)return false;std::memcpy(out,it->second.data(),n);return true;};
 auto put=[&](uintptr_t a,uintptr_t v){mem[a].resize(sizeof(v));std::memcpy(mem[a].data(),&v,sizeof(v));};
 put(0x108,0x200); put(0x208,0x300);
 auto known=[](uintptr_t c){return c==0x300;};
 assert(checked_pointer(0x100,8,8,reader,known)==0x200);
 reads=0; assert(checked_pointer(0,8,8,reader,known)==0 && reads==0);
 assert(checked_pointer(0x100,-1,8,reader,known)==0);
 mem.erase(0x108); reads=0; assert(checked_pointer(0x100,8,8,reader,known)==0&&reads==1);
 put(0x108,0x200);mem.erase(0x208);assert(checked_pointer(0x100,8,8,reader,known)==0);
 put(0x208,0x400);assert(checked_pointer(0x100,8,8,reader,known)==0);
 put(0x208,0);assert(checked_pointer(0x100,8,8,reader,known)==0);
 put(0x108,0);reads=0;assert(checked_pointer(0x100,8,8,reader,known)==0&&reads==1);
 put(0x108,0x200);mem[0x108].resize(4);assert(checked_pointer(0x100,8,8,reader,known)==0);
 assert(checked_pointer(UINTPTR_MAX-3,8,8,reader,known)==0);
 put(0x108,0x100);assert(checked_pointer(0x100,8,8,reader,known)==0);
 std::cout<<"policy: 17 assertions passed\n";
}
