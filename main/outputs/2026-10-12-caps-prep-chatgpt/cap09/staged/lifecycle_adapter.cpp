// STAGED SOURCE ONLY. Not compiled, linked, deployed or applied to runtime.
// Uses the existing genuine child_runtime.h interface. No engine or GUI access.
// Use ONLY the staged renamed original TU. Global export-renaming defines
// are prohibited: they also rewrite included headers. Serialize lifecycle/stop.
#include <Mod/CppUserModBase.hpp>
#include "child_runtime.h"

extern "C" RC::CppUserModBase* mb_cppmod_source_start_mod();
extern "C" void mb_cppmod_source_uninstall_mod(RC::CppUserModBase*);

extern "C" __attribute__((visibility("default")))
RC::CppUserModBase* start_mod()
{
    if (mb_cppmod_child_acquire() != MB_CPPMOD_CHILD_OK) return nullptr;
    try
    {
        auto* mod = mb_cppmod_source_start_mod();
        if (!mod) (void)mb_cppmod_child_release();
        return mod;
    }
    catch (...)
    {
        (void)mb_cppmod_child_release();
        throw;
    }
}

extern "C" __attribute__((visibility("default")))
void uninstall_mod(RC::CppUserModBase* mod)
{
    if (!mod) return;
    // A throwing original uninstall retains the child lease. The existing host
    // keeps its mod pointer on that path; do not report successful retirement.
    mb_cppmod_source_uninstall_mod(mod);
    (void)mb_cppmod_child_release();
}
