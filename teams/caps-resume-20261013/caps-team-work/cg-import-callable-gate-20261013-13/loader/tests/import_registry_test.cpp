#include "win_api_shim.h"

#include <cstdint>
#include <cstdio>

static int failures = 0;
static cap13::WinApiShim* active = nullptr;
static bool reentered = false;

static void check(bool condition, const char* label)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    std::fflush(stdout);
    if (!condition) ++failures;
}

static bool lookup(cap13::WinModule handle, const char* name, void** out)
{
    if (handle != reinterpret_cast<void*>(0x1234)) return false;
    reentered = active->resolve("KERNEL32.DLL", "Sleep", 0) != nullptr;
    if (name[0] == 'F' && name[1] == 'n' && name[2] == '\0')
    {
        *out = reinterpret_cast<void*>(0x5678);
        return true;
    }
    return false;
}

int main()
{
    cap13::WinApiShim shim;
    active = &shim;
    shim.register_builtin_exports();
    check(shim.resolve("KERNEL32.DLL", "Sleep", 0) == nullptr,
          "raw SysV built-in body is not exposed as a guest import");
    check(!shim.publish_builtin_wrapper("kernel32.dll", "Imaginary", reinterpret_cast<void*>(0x99)) &&
          !shim.publish_builtin_wrapper("kernel32.dll", "Sleep", nullptr),
          "unknown rows and null wrappers cannot be published");
    check(shim.publish_builtin_wrapper("KERNEL32.DLL", "Sleep", reinterpret_cast<void*>(0x9999)) &&
          shim.resolve("kernel32.dll", "Sleep", 0) == reinterpret_cast<void*>(0x9999),
          "a known row resolves only through its published Win64 wrapper");
    shim.register_module("UE4SS.DLL", reinterpret_cast<void*>(0x1234), lookup);
    check(shim.resolve("ue4ss.dll", "Fn", 0) == reinterpret_cast<void*>(0x5678),
          "registered module names use ASCII case-insensitive lookup");
    check(reentered, "module lookup may resolve a built-in import recursively");
    check(shim.resolve("ue4ss.dll", "Missing", 0) == nullptr,
          "missing registered export remains unresolved");
    shim.register_module(nullptr, reinterpret_cast<void*>(0x1234), lookup);
    shim.register_module("", reinterpret_cast<void*>(0x1234), lookup);
    check(shim.resolve(nullptr, "Fn", 0) == nullptr,
          "invalid module names do not create registrations");
    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
