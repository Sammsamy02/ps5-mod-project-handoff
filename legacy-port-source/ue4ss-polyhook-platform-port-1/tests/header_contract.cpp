#include <polyhook2/PolyHookOs.hpp>
#include <polyhook2/PolyHookOsIncludes.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

#ifndef __PROSPERO__
#error This control must use the actual Prospero compiler target.
#endif
#ifndef POLYHOOK2_OS_PROSPERO
#error PolyHook did not select the Prospero OS contract.
#endif
#ifndef POLYHOOK2_ARCH_X64
#error PolyHook did not select its x64 architecture.
#endif
#if defined(POLYHOOK2_OS_WINDOWS) || defined(POLYHOOK2_OS_LINUX) || defined(POLYHOOK2_OS_APPLE)
#error Prospero must not impersonate another supported OS.
#endif

static_assert(sizeof(void*) == 8);
static_assert(sizeof(PLH::x64Detour) > 0);
static_assert(sizeof(ZydisDecoder) > 0);
static_assert(ASMJIT_ARCH_X86 == 64);
static_assert(PROT_READ == 1 && PROT_WRITE == 2 && PROT_EXEC == 4);
static_assert(std::is_same_v<decltype(&::mmap), void* (*)(void*, size_t, int, int, int, off_t)>);
static_assert(std::is_same_v<decltype(&::mprotect), int (*)(const void*, size_t, int)>);
static_assert(std::is_same_v<decltype(&::sysconf), long (*)(int)>);

struct OwnedTarget {
    int bias;
    int add(int value) { return bias + value; }
};

using PlainFunction = int (*)(int, int);
using MemberFunction = int (OwnedTarget::*)(int);
using ExplicitReceiver = int (*)(OwnedTarget*, int);
static_assert(sizeof(MemberFunction) == 16);
static_assert(std::is_same_v<PLH::callback_type_t<PlainFunction>, PlainFunction>);
static_assert(std::is_same_v<PLH::callback_type_t<MemberFunction>, ExplicitReceiver>);
static_assert(std::is_same_v<PLH::callback_return_type_t<MemberFunction>, int>);

static int original_sum(int a, int b) { return a + b; }

extern "C" int owned_plain_callback(int a, int b) {
    const auto callback = PLH::make_callback(&original_sum, [](int x, int y) { return x + y + 7; });
    static_assert(std::is_same_v<decltype(callback), PlainFunction const>);
    return callback(a, b);
}

extern "C" int owned_receiver_callback(OwnedTarget* receiver, int value) {
    const auto callback = PLH::make_callback(&OwnedTarget::add,
                                            [](OwnedTarget* object, int x) { return object->bias + x; });
    static_assert(std::is_same_v<decltype(callback), ExplicitReceiver const>);
    return callback(receiver, value);
}

extern "C" int owned_hook_callback_macro(OwnedTarget* receiver, int value) {
    HOOK_CALLBACK(&OwnedTarget::add, target_callback, {
        return GET_ARG(0)->bias + GET_ARG(1);
    });
    static_assert(std::is_same_v<target_callback_t, ExplicitReceiver>);
    return target_callback(receiver, value);
}
