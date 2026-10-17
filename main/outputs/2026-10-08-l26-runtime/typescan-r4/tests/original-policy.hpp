#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <initializer_list>
namespace RC::Unreal::NativeTypeScan {
struct MissingKeys {std::vector<std::string> values; template<class F> void attempt(F f){f();}};
struct TypeStatus{const char*name;bool core,found;};
inline bool types_ready(std::initializer_list<TypeStatus>){return true;}
template<class R,class K> uintptr_t checked_pointer(uintptr_t a,int o,int,R r,K){uintptr_t p=0;r(a+o,sizeof(p),&p);return p;}
}
