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

static bool helper_lookup(cap13::WinModule handle, const char* name, void** out)
{
    if (handle != reinterpret_cast<void*>(0x4321) ||
        name[0] != 'H' || name[1] != '\0') return false;
    *out = reinterpret_cast<void*>(0x8765);
    return true;
}

static bool lookup(cap13::WinModule handle, const char* name, void** out)
{
    if (handle != reinterpret_cast<void*>(0x1234)) return false;
    reentered = active->resolve("HELPER.DLL", "H", 0) ==
                reinterpret_cast<void*>(0x8765);
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
    shim.register_module("Helper.DLL", reinterpret_cast<void*>(0x4321), helper_lookup);
    shim.register_module("UE4SS.DLL", reinterpret_cast<void*>(0x1234), lookup);
    check(shim.resolve("ue4ss.dll", "Fn", 0) == reinterpret_cast<void*>(0x5678),
          "registered module names use ASCII case-insensitive lookup");
    check(reentered, "module lookup may resolve another registered module recursively");
    check(shim.resolve("ue4ss.dll", "Missing", 0) == nullptr,
          "missing registered export remains unresolved");
    shim.register_module(nullptr, reinterpret_cast<void*>(0x1234), lookup);
    shim.register_module("", reinterpret_cast<void*>(0x1234), lookup);
    check(shim.resolve(nullptr, "Fn", 0) == nullptr,
          "invalid module names do not create registrations");
    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
