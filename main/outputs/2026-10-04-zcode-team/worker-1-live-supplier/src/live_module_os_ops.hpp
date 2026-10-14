// live_module_os_ops.hpp
//
// Host-injectable OS boundary for the live module facts supplier. The generic
// core (live_module_facts.cpp) sees ONLY this table; host tests inject fakes;
// live_module_os_ops_sce.cpp is the only TU binding pinned SDK symbols.
//
// Contract for every callback (mirrors the trusted-provider rules of the
// project's native-module-metadata adapter):
//   - synchronous, bounded, returns 0 on success, nonzero provider code on
//     failure (the core records the code verbatim);
//   - writes only into caller-provided storage; retains no pointers;
//   - never dereferences any game/module address — mapping and segment
//     addresses are integers.
#ifndef LMF_LIVE_MODULE_OS_OPS_HPP
#define LMF_LIVE_MODULE_OS_OPS_HPP

#include <cstddef>
#include <cstdint>

#include "sce_module_abi.hpp"

namespace lmf {

// Borrowed link-map observation (name storage is owned by the OS loader and
// valid only until the next ops call; the core copies immediately).
struct LinkMapEntry {
    const char *name;  // may be "" for the main object; never null
    void *base;        // rtld-reported base address of the object
};

// One kernel-reported VM mapping (converted from the pinned kinfo_vmentry by
// the SCE TU; the generic core never sees the kernel struct layout).
struct OsVmMapping {
    uint64_t start;
    uint64_t end;  // exclusive
    int32_t protection;  // opaque reported value
};

struct LiveModuleOsOps {
    void *context = nullptr;

    // Enumerate the dynamic loader's link map for this process
    // (SCE TU: this image's _DYNAMIC → DT_DEBUG → r_debug.r_map, then a
    // bounded walk of l_next; dlinfo is declared in the SDK's dlfcn.h but
    // exported by no pinned library, so it is NOT used). Copies at most
    // capacity entries into out; sets *count to the TOTAL number of entries
    // (may exceed capacity). Returns 0 on a completed walk, nonzero if the
    // walk failed.
    int (*link_map_snapshot)(void *context, LinkMapEntry *out, size_t capacity,
                             size_t *count);

    // Take a REAL loader reference on an already-loaded object
    // (SCE TU: dlopen(name, RTLD_NOLOAD) — never loads new bytes).
    // Returns null when the object is not currently loaded by rtld.
    void *(*loader_retain_noload)(void *context, const char *name);

    // Drop one loader reference (SCE TU: dlclose). Returns 0 on success.
    int (*loader_release)(void *context, void *loader_handle);

    // Authoritative Prospero module info (SCE TU: sceKernelGetModuleInfo).
    // The core zeroes info and sets info->size = profile.module_info_abi_size
    // before the call; the provider must echo that size. Pinned ABI only.
    int (*get_module_info)(void *context, sce::ModuleHandle handle,
                           sce::ModuleInfo *info);

    // Non-dereferencing mapping info (SCE TU: sysctl KERN_PROC_VMMAP for the
    // current pid, converted to OsVmMapping). Copies at most capacity;
    // *count is the TOTAL; return 0 on success.
    int (*query_vmentry)(void *context, OsVmMapping *out, size_t capacity,
                         size_t *count);

    // Observed executable path of THIS process (SCE TU: sysctl
    // KERN_PROC_PATHNAME). Writes a NUL-terminated path; 0 on success.
    int (*executable_path)(void *context, char *out, size_t capacity);

    // Process id (SCE TU: getpid from unistd.h). Returns pid or -1.
    int (*get_pid)(void *context);

    // OPTIONAL, DEFAULT NULL. sceKernelGetExecutableModuleHandle is exported
    // by the pinned libkernel.so but has NO pinned signature in this
    // workspace, so the SCE TU leaves this unbound. A firmware-profile owner
    // may bind a verified implementation after the target experiment in
    // REPORT.md establishes the true return convention. When null, capture()
    // proceeds in the documented degraded mode.
    int (*get_executable_module_handle)(void *context, sce::ModuleHandle *out);
};

}  // namespace lmf

#endif  // LMF_LIVE_MODULE_OS_OPS_HPP
