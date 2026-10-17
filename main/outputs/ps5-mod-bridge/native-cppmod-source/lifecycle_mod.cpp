#include <Mod/CppUserModBase.hpp>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include "child_runtime.h"

// A real source-derived UE4SS object. It does not access Unreal objects, input,
// hooks, files, or game memory. The host must provide the genuine base class.
namespace
{
    std::atomic_uint32_t constructed{}, destroyed{}, updated{}, program_started{};
    class LifecycleOnly final : public RC::CppUserModBase
    {
    public:
        LifecycleOnly() { constructed.fetch_add(1, std::memory_order_relaxed); }
        ~LifecycleOnly() override { destroyed.fetch_add(1, std::memory_order_relaxed); }
        void on_update() override { updated.fetch_add(1, std::memory_order_relaxed); }
        void on_program_start() override { program_started.fetch_add(1, std::memory_order_relaxed); }
    };
}

extern "C" __attribute__((visibility("default"))) RC::CppUserModBase* start_mod()
{
    if (mb_cppmod_child_acquire()!=MB_CPPMOD_CHILD_OK) return nullptr;
    try { return new LifecycleOnly; }
    catch (...) { (void)mb_cppmod_child_release(); throw; }
}
extern "C" __attribute__((visibility("default"))) void uninstall_mod(RC::CppUserModBase* mod)
{
    if (!mod) return;
    delete mod;
    (void)mb_cppmod_child_release();
}
// Optional observation export for a future real native loader test. Values are
// copied into caller storage, so no STL or allocation ownership crosses here.
extern "C" __attribute__((visibility("default"))) int modbridge_cppmod_counters(
    uint32_t* output, uint32_t count)
{
    if (!output || count != 4) return -1;
    output[0] = constructed.load(std::memory_order_relaxed);
    output[1] = destroyed.load(std::memory_order_relaxed);
    output[2] = updated.load(std::memory_order_relaxed);
    output[3] = program_started.load(std::memory_order_relaxed);
    return 0;
}
static_assert(std::is_same_v<decltype(&start_mod), RC::CppUserModBase* (*)()>);
static_assert(std::is_same_v<decltype(&uninstall_mod), void (*)(RC::CppUserModBase*)>);
