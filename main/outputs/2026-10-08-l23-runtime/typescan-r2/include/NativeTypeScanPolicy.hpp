#pragma once
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace RC::Unreal::NativeTypeScan {
// The existing generated accessor's exact diagnostic is the only legacy
// missing-key exception category. Never absorb other runtime errors/faults.
inline std::string missing_key(const std::runtime_error& error) {
 constexpr std::string_view prefix="Tried getting member variable '";
 constexpr std::string_view suffix="' that doesn't exist in this engine version.";
 const std::string_view text=error.what();
 if(!text.starts_with(prefix)||!text.ends_with(suffix)) return {};
 const auto key=text.substr(prefix.size(),text.size()-prefix.size()-suffix.size());
 if(key.empty()||key.find("::")==key.npos) return {};
 for(char c:key) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c==':')) return {};
 return std::string(key);
}
struct MissingKeys {
 std::vector<std::string> values;
 template<class F> void attempt(F&& action) {
  try {action();} catch(const std::runtime_error& error) {
   auto key=missing_key(error); if(key.empty()) throw;
   if(std::find(values.begin(),values.end(),key)==values.end()) values.push_back(std::move(key));
  }
 }
};
struct TypeStatus{const char* name;bool core,found;};
inline bool types_ready(std::initializer_list<TypeStatus> types) {
 for(const auto& t:types) if(t.core&&!t.found) return false;
 return true;
}
inline bool add_address(uintptr_t base,int offset,uintptr_t& result) {
 if(!base||offset<0||uintptr_t(offset)>UINTPTR_MAX-base) return false;
 result=base+uintptr_t(offset); return true;
}
// Reader copies only proved exact bytes; known_class validates class identity.
// No raw dereference and no discovery-map mutation anywhere in this policy.
template<class Read,class Known>
uintptr_t checked_pointer(uintptr_t owner,int offset,int class_offset,Read&& read,Known&& known_class) {
 uintptr_t slot=0,target=0,class_slot=0,klass=0;
 if(!add_address(owner,offset,slot)||!read(slot,sizeof(target),&target)||!target||target==owner||target%alignof(void*) ||
    !add_address(target,class_offset,class_slot)||!read(class_slot,sizeof(klass),&klass)||!klass||klass%alignof(void*)||
    !known_class(klass)) return 0;
 return target;
}
}
