#include "PsScanAbi.h"
#include <cassert>
#include <cstdio>
#include <cstring>
extern "C" size_t ps_abi_layout_probe(size_t);
extern "C" bool ps_abi_utf16_callbacks(const PsCtx*);
extern "C" bool ps_abi_result_fields(PsScanResults*,uint8_t*);
static unsigned calls;
static void receive(PsChar16* message){
 const uint16_t expected[]={0x0041,0x03a9,0xd83d,0xde42,0};
 for(size_t i=0;i<5;++i)assert(static_cast<uint16_t>(message[i])==expected[i]);
 ++calls;
}
int main(){
 const size_t expected[]={13,1,0,1,2,3,4,5,6,7,8,9,10,11,12,56,8,0,8,16,24,32,40,4,2,0,2,96,8,0,8,16,24,32,40,48,56,64,72,80,88,92,32,8,0,8,16,24,28,16,8,0,8,88,8,0,4,8,16,24,32,40,56,72};
 for(size_t i=0;i<sizeof(expected)/sizeof(*expected);++i)assert(ps_abi_layout_probe(i)==expected[i]);
 assert(ps_abi_layout_probe(sizeof(expected)/sizeof(*expected))==SIZE_MAX);
 PsCtx c{receive,receive,receive,receive,receive,{true,false,true,false,true,false,true,false,true,false,true,false,true}};
 assert(ps_abi_utf16_callbacks(&c));assert(calls==5);
 uint8_t owned_storage[10]{};PsScanResults r{};
r.g_uobject_array=owned_storage+0;
r.fname_tostring_fstring=owned_storage+1;
r.fname_ctor_wchar=owned_storage+2;
r.gmalloc=owned_storage+3;
r.static_construct_object_internal=owned_storage+4;
r.ftext_fstring=owned_storage+5;
r.fuobject_hash_tables_get=owned_storage+6;
r.gnatives=owned_storage+7;
r.console_manager_singleton=owned_storage+8;
r.gameengine_tick=owned_storage+9;
r.engine_version={5,3};r.build_configuration=2;r.stats=1;
 assert(ps_abi_result_fields(&r,owned_storage));assert(r.engine_version.minor==7&&r.build_configuration==3&&r.stats==2);
 assert(!ps_abi_utf16_callbacks(nullptr));assert(!ps_abi_result_fields(nullptr,owned_storage));
 std::printf("PASS C++/Rust %zu layout checks,13 bool slots,10 owned-pointer fields,5 exact UTF16 callbacks including surrogate pair\n",sizeof(expected)/sizeof(*expected));
}
