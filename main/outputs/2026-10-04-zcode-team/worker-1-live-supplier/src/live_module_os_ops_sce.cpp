// live_module_os_ops_sce.cpp
//
// The ONLY translation unit of this library that binds real OS symbols.
// Everything here is a pinned, verifiable API:
//
//   <dlfcn.h>            dlopen, dlclose, RTLD_NOLOAD
//                        (pinned SDK header target/include/dlfcn.h, FreeBSD)
//   <sys/elf64.h>        Elf64_Dyn (pinned SDK header)
//   <sys/elf_common.h>   DT_DEBUG (pinned SDK header)
//   <sys/link_elf.h>     struct link_map, struct r_debug
//                        (pinned SDK header target/include/sys/link_elf.h)
//   <sys/sysctl.h>       sysctl, CTL_KERN/KERN_PROC/KERN_PROC_PATHNAME/
//                        KERN_PROC_VMMAP (pinned SDK header, FreeBSD 11.4)
//   <sys/user.h>         struct kinfo_vmentry (kve_start/kve_end/kve_protection)
//   <unistd.h>           getpid
//   local declaration    sceKernelGetModuleInfo + 352-byte module-info ABI
//                        (prosperodev MIT header pinned verbatim at
//                        outputs/ps5-mod-bridge/native-module-metadata/upstream/
//                        libkernel.h; symbol exported by pinned
//                        target/lib/libkernel.so — see REPORT.md evidence)
//
// DISCOVERED FACT: dlinfo/RTLD_DI_LINKMAP is DECLARED in the SDK dlfcn.h but
// exported by NO library in the pinned SDK (verified: nm over every
// target/lib/*.so and *.a). It is therefore NOT used; the link-map walk uses
// the standard ELF mechanism instead: this image's own _DYNAMIC array
// (linker-provided), its DT_DEBUG entry (set by the runtime linker to point
// at its struct r_debug) and r_debug.r_map (structs pinned in sys/link_elf.h).
// Whether the PS5 rtld fills DT_DEBUG for a payload module is a TARGET
// EXPERIMENT (REPORT.md unresolved fact 1); a missing/zero DT_DEBUG fails the
// walk honestly with a distinct code.
//
// Deliberately NOT called (exported by libkernel.so but with NO pinned
// signature in this workspace; see REPORT.md unresolved facts + experiments):
//   sceKernelGetExecutableModuleHandle, sceKernelGetModuleInfoFromAddr,
//   sceKernelIsModuleLoaded, sceKernelGetProcessName,
//   sceKernelInternalMemoryGetModuleSegmentInfo, sceKernelGetModuleList,
//   sceKernelGetModuleInfo2/ForUnwind/Internal, get_module_info_list.
#include "live_module_os_ops.hpp"

#include <dlfcn.h>
#include <sys/types.h>  // must precede elf headers (u_int32_t etc.)
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <sys/link_elf.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>

// Linker-provided dynamic array of THIS image (standard ELF symbol; the
// final PRX/ELF defines it in its dynamic section). Not an invented ABI.
extern "C" Elf64_Dyn _DYNAMIC[];

namespace lmf {
namespace {

// prosperodev-pinned prototype, private spellings (sce_module_abi.hpp):
//   int sceKernelGetModuleInfo(SceKernelModule handle, SceKernelModuleInfo *info);
// Exported as a global text symbol by the pinned libkernel.so.
extern "C" int sceKernelGetModuleInfo(sce::ModuleHandle handle,
                                      sce::ModuleInfo *info);

constexpr size_t kMaxWalkSteps = 4096;  // corrupt-list guard, not a firmware fact

int lmf_sce_get_pid(void *) { return static_cast<int>(getpid()); }

int lmf_sce_executable_path(void *, char *out, size_t capacity) {
    if (out == nullptr || capacity == 0) return -1;
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t len = capacity;
    if (sysctl(mib, 4, out, &len, nullptr, 0) != 0) return -1;
    if (len == 0) return -1;
    out[capacity - 1] = '\0';  // sysctl output is NUL-terminated for strings
    if (out[0] == '\0') return -1;
    return 0;
}

int lmf_sce_link_map_snapshot(void *, LinkMapEntry *out, size_t capacity,
                              size_t *count) {
    if (count == nullptr) return -1;
    *count = 0;
    // Standard ELF route to the runtime linker's link-map list (dlinfo is
    // unavailable in this SDK — see file header). DT_DEBUG's d_un.d_ptr is
    // set by rtld to its struct r_debug; r_map is the list head.
    const Elf64_Dyn *debug_dyn = nullptr;
    for (const Elf64_Dyn *d = _DYNAMIC; d != nullptr; ++d) {
        if (d->d_tag == 0) break;  // DT_NULL terminates the array
        if (d->d_tag == DT_DEBUG) {
            debug_dyn = d;
            break;
        }
    }
    if (debug_dyn == nullptr) return -2;  // no DT_DEBUG entry at all
    auto *rd = reinterpret_cast<struct r_debug *>(debug_dyn->d_un.d_ptr);
    if (rd == nullptr) return -3;         // entry present but not filled by rtld
    struct link_map *lm = rd->r_map;
    if (lm == nullptr) return -4;
    size_t seen = 0;
    for (struct link_map *it = lm; it != nullptr; it = it->l_next, ++seen) {
        if (seen >= kMaxWalkSteps) break;
        if (out != nullptr && seen < capacity) {
            out[seen].name = it->l_name != nullptr ? it->l_name : "";
            out[seen].base = it->l_addr;
        }
    }
    *count = seen;
    return 0;
}

void *lmf_sce_loader_retain_noload(void *, const char *name) {
    if (name == nullptr || name[0] == '\0') return nullptr;
    return dlopen(name, RTLD_LAZY | RTLD_NOLOAD);  // never loads new bytes
}

int lmf_sce_loader_release(void *, void *loader_handle) {
    if (loader_handle == nullptr) return -1;
    return dlclose(loader_handle);
}

int lmf_sce_get_module_info(void *, sce::ModuleHandle handle,
                            sce::ModuleInfo *info) {
    if (info == nullptr) return -1;
    // Caller (generic core) has zeroed info and set info->size per profile.
    return sceKernelGetModuleInfo(handle, info);
}

int lmf_sce_query_vmentry(void *, OsVmMapping *out, size_t capacity,
                          size_t *count) {
    if (count == nullptr) return -1;
    *count = 0;
    const pid_t pid = getpid();
    if (pid < 0) return -2;
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, pid};
    // KERN_PROC_VMMAP needs a buffer large enough for every entry or it
    // fails with ENOMEM; each kinfo_vmentry is ~1 KiB, so 512 entries live
    // in .bss, not on the stack. NOT REENTRANT: the supplier contract is
    // single-threaded synchronous captures.
    static kinfo_vmentry buffer[512];
    size_t len = sizeof(buffer);
    if (sysctl(mib, 4, buffer, &len, nullptr, 0) != 0) return -3;
    if (len == 0 || len % sizeof(kinfo_vmentry) != 0) return -4;
    const size_t entries = len / sizeof(kinfo_vmentry);
    const size_t copied = entries < capacity ? entries : capacity;
    if (out != nullptr) {
        for (size_t i = 0; i < copied; ++i) {
            out[i].start = buffer[i].kve_start;
            out[i].end = buffer[i].kve_end;
            out[i].protection = buffer[i].kve_protection;
        }
    }
    *count = entries;  // total; truncation is recorded by the core
    return 0;
}

}  // namespace

// Builds the ops table with every pinned slot bound and the unpinned
// sceKernelGetExecutableModuleHandle slot intentionally left null.
LiveModuleOsOps make_sce_live_module_os_ops(void *context) noexcept {
    LiveModuleOsOps ops{};
    ops.context = context;
    ops.get_pid = lmf_sce_get_pid;
    ops.executable_path = lmf_sce_executable_path;
    ops.link_map_snapshot = lmf_sce_link_map_snapshot;
    ops.loader_retain_noload = lmf_sce_loader_retain_noload;
    ops.loader_release = lmf_sce_loader_release;
    ops.get_module_info = lmf_sce_get_module_info;
    ops.query_vmentry = lmf_sce_query_vmentry;
    ops.get_executable_module_handle = nullptr;  // no pinned signature (REPORT)
    return ops;
}

}  // namespace lmf
