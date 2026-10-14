
#include <atomic>
#include <bit>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstdint>
#include <memory>
#include <thread>
namespace RC::Unreal {
class UObject {}; class FFrame {};
using UnrealScriptFunction=void(*)(UObject*,FFrame&,void*);
class UFunction { public:
 using Storage=void*;
 alignas(std::atomic_ref<Storage>::required_alignment) Storage func{};
 Storage& GetFunc(){return func;}
 UnrealScriptFunction GetFuncPtr(); void SetFuncPtr(UnrealScriptFunction);
};
}

namespace RC::Unreal {
    auto UFunction::GetFuncPtr() -> UnrealScriptFunction
    {
        return std::bit_cast<UnrealScriptFunction>(GetFunc());
    }

    auto UFunction::SetFuncPtr(UnrealScriptFunction NewFuncPtr) -> void
    {
        GetFunc() = std::bit_cast<std::remove_reference_t<decltype(std::declval<UFunction>().GetFunc())>>(NewFuncPtr);
    }


}

using namespace RC::Unreal;
void original(UObject*,FFrame&,void*){} void replacement(UObject*,FFrame&,void*){}
int main(){ UFunction f;f.SetFuncPtr(original);std::atomic<bool> ready{false};
 std::thread reader([&]{ready.store(true,std::memory_order_release);for(int i=0;i<200000;++i){auto p=f.GetFuncPtr();if(p!=original&&p!=replacement)std::abort();}});
 while(!ready.load(std::memory_order_acquire)){}
 for(int i=0;i<200000;++i)f.SetFuncPtr(i%2?original:replacement);
 reader.join();return 0;
}
