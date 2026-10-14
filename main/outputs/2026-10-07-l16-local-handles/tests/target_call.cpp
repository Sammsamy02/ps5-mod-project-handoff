#include "NativeFNameAdapter.hpp"
static_assert(sizeof(wchar_t)==2);
static_assert(sizeof(char16_t)==2);
extern "C" uint64_t l15_target_call(rc::fnameabi::Ctor ctor) {
    const auto result=rc::fnameabi::invoke(ctor,u"ByteProperty",0).value;
    return uint64_t(result.index)|(uint64_t(result.number)<<32);
}
