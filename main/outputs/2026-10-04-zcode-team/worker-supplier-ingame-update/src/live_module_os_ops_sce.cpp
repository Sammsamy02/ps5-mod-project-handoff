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
//   <string.h>           memcpy (scan raw-read callback only, span-validated)
//   local declaration    sceKernelGetModuleInfo + 352-byte module-info ABI
//                        (prosperodev MIT header pinned verbatim at
//                        outputs/ps5-mod-bridge/native-module-metadata/upstream/
//                        libkernel.h; symbol exported by pinned
//                        target/lib/libkernel.so — see REPORT.md evidence)
//
// IN-GAME UPDATE (checkpoint zcode_ingame_v1_results, records
// outputs/2026-10-04-zcode-controller/console/ingame-v1-facts-2026-10-04.jsonl,
// build igf-f4a91d8123b0, inside the running game process, 2026-10-04):
//   * KERN_PROC_VMMAP fully works (g5: rc 0, 1252 entries, kve 1160B) —
//     promoted to the PRIMARY mapping source, and — through the vendored
//     V2 base scan below — the PRIMARY identity source.
//   * sceKernelGetExecutableModuleHandle returned RAW 0 in-game (g1) —
//     DEMOTED to explicitly-disabled: this TU now binds the ops slot to a
//     refusal that returns LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame
//     without calling the kernel. The real call (unpinned int64(void)
//     spelling, exactly the in-game probe's) survives behind
//     LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES for a NEW firmware experiment.
//   * DT_DEBUG is absent/unfilled in the game process (g2: value 0, r_map 0)
//     — DEMOTED the same way: the default link_map_snapshot refuses with
//     LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame; the real walk
//     survives behind the same gate. Runtime-gated, NOT deleted.
//   * Module-list handles + sceKernelGetModuleInfo returned 0x7FFFFFFF with
//     empty names in-game (g4: domain/ABI mismatch, UNRESOLVED) — this TU
//     still does not use the module-list route.
//   * dl retention degradation (LMF_DISABLE_DL_RETENTION) stays EXACTLY
//     as-is: observed g3, no path source / handle null in-game.
//
// DISCOVERED FACT: dlinfo/RTLD_DI_LINKMAP is DECLARED in the SDK dlfcn.h but
// exported by NO library in the pinned SDK (verified: nm over every
// target/lib/*.so and *.a). It is therefore NOT used; the gated experimental
// walk uses the standard ELF mechanism instead: this image's own _DYNAMIC
// array (linker-provided), its DT_DEBUG entry and r_debug.r_map.
//
// Deliberately NOT called (exported by libkernel.so but with NO pinned
// signature in this workspace; see REPORT.md unresolved facts + experiments):
//   sceKernelGetModuleInfoFromAddr, sceKernelIsModuleLoaded,
//   sceKernelGetProcessName, sceKernelInternalMemoryGetModuleSegmentInfo,
//   sceKernelGetModuleList, sceKernelGetModuleInfo2/ForUnwind/Internal,
//   get_module_info_list. (sceKernelGetExecutableModuleHandle joins this set
//   in the default build; its gated call uses the probe's spelling.)
#include "live_module_os_ops.hpp"

#include "LiveLoaderObservedFacts.hpp"

#ifdef LMF_DISABLE_DL_RETENTION
/* Composition build: dl retention compile-time disabled. Observed 2026-10-04:
 * dlopen(self, RTLD_NOLOAD) returned NULL in the SDK payload environment
 * (livefacts-probe E6), and the dl entry points resolve only to the bundled
 * crt1 rtld (__dlopen/__dlclose) which is not exported to game-module links.
 * Retention therefore runs in the documented degraded mode; IN-GAME g3
 * confirmed the same state (path_source none, handle null). Unchanged. */
#else
#include <dlfcn.h>
#endif
#include <sys/types.h>  // must precede elf headers (u_int32_t etc.)
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <sys/link_elf.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>

#include <cstring>

// Linker-provided dynamic array of THIS image (standard ELF symbol; the
// final PRX/ELF defines it in its dynamic section). Not an invented ABI.
// Referenced only by the gated experimental DT_DEBUG walk.
extern "C" Elf64_Dyn _DYNAMIC[];

namespace lmf {
namespace {

// prosperodev-pinned prototype, private spellings (sce_module_abi.hpp):
//   int sceKernelGetModuleInfo(SceKernelModule handle, SceKernelModuleInfo *info);
// Exported as a global text symbol by the pinned libkernel.so.
extern "C" int sceKernelGetModuleInfo(sce::ModuleHandle handle,
                                      sce::ModuleInfo *info);

// VMMAP scratch (single-threaded synchronous captures, unchanged contract).
// Sized from the OBSERVED in-game run (g5: 1252 entries, 1160B each) with
// headroom; overflow is still reported honestly by the core's truncation
// flag. .bss, not stack.
constexpr size_t kVmMapBufferEntries =
    LiveLoaderObservedFacts::kVmMapBufferEntries;  // 1408
kinfo_vmentry g_vmmap_buffer[kVmMapBufferEntries];

// Base-scan scratch (same single-threaded contract as the vmmap buffer).
VmScanCandidate g_scan_candidates[kVmScanCandidateCapacity];      // 64
VmScanSpan g_scan_spans[LiveLoaderObservedFacts::kVmMappingSnapshotCapacity];  // 1024

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

// --------------------------------------------------------------------------
// DT_DEBUG link-map walk — DEMOTED, runtime-gated (observed in-game g2).
// --------------------------------------------------------------------------
#ifdef LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES
constexpr size_t kMaxWalkSteps = 4096;  // corrupt-list guard, not a firmware fact

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
#else
int lmf_sce_link_map_snapshot(void *, LinkMapEntry *out, size_t capacity,
                              size_t *count) {
    // EXPLICITLY DISABLED (runtime-gated, not deleted). OBSERVED IN-GAME
    // (checkpoint zcode_ingame_v1_results, seq 2 g2_dtdebug, build
    // igf-f4a91d8123b0): DT_DEBUG absent/unfilled in the game process —
    // dt_debug_present=0, value 0x0, r_map 0x0, entries 0. The route is dead
    // on this target; refusing with the distinct recorded code instead of
    // walking lets the core proceed VMMAP-first and lets callers cite the
    // exact observed fact. Re-enable only behind
    // LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES for a NEW firmware experiment.
    (void)out;
    (void)capacity;
    if (count == nullptr) return -1;
    *count = 0;
    return LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;  // -4002
}
#endif

#ifdef LMF_DISABLE_DL_RETENTION
void *lmf_sce_loader_retain_noload(void *, const char *name) {
    (void)name;  /* degraded: no dl handle bridge in this composition */
    return nullptr;
}

int lmf_sce_loader_release(void *, void *loader_handle) {
    (void)loader_handle;  /* degraded: nothing was retained via dl */
    return -1;
}
#else
void *lmf_sce_loader_retain_noload(void *, const char *name) {
    if (name == nullptr || name[0] == '\0') return nullptr;
    return dlopen(name, RTLD_LAZY | RTLD_NOLOAD);  // never loads new bytes
}

int lmf_sce_loader_release(void *, void *loader_handle) {
    if (loader_handle == nullptr) return -1;
    return dlclose(loader_handle);
}
#endif

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
    // fails with ENOMEM; each kinfo_vmentry is ~1 KiB (observed exactly
    // 1160B in-game, g5), so the entries live in .bss (raised from 512 to
    // the observed-facts constant 1408 to cover the in-game 1252), not on
    // the stack. NOT REENTRANT: the supplier contract is single-threaded
    // synchronous captures.
    size_t len = sizeof(g_vmmap_buffer);
    if (sysctl(mib, 4, g_vmmap_buffer, &len, nullptr, 0) != 0) return -3;
    if (len == 0 || len % sizeof(kinfo_vmentry) != 0) return -4;
    const size_t entries = len / sizeof(kinfo_vmentry);
    const size_t copied = entries < capacity ? entries : capacity;
    if (out != nullptr) {
        for (size_t i = 0; i < copied; ++i) {
            out[i].start = g_vmmap_buffer[i].kve_start;
            out[i].end = g_vmmap_buffer[i].kve_end;
            out[i].protection = g_vmmap_buffer[i].kve_protection;
        }
    }
    *count = entries;  // total; truncation is recorded by the core
    return 0;
}

// --------------------------------------------------------------------------
// Executable module handle — DEMOTED, runtime-gated (observed in-game g1).
// --------------------------------------------------------------------------
#ifdef LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES
// Unpinned spelling, exactly the in-game probe's declaration
// (worker-ingame-probe/src/ingame_abi.h line 87: int64_t (void); judged only
// by the caller's module-info echo validation).
extern "C" int64_t sceKernelGetExecutableModuleHandle(void);

int lmf_sce_get_executable_module_handle(void *, sce::ModuleHandle *out) {
    if (out == nullptr) return -1;
    const int64_t raw = sceKernelGetExecutableModuleHandle();
    // Raw 0 or out-of-32-bit-range: refuse with the observed-fact code.
    if (raw <= 0 || raw > 0xffffffffLL)
        return LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;
    *out = static_cast<sce::ModuleHandle>(raw);
    return 0;
}
#else
int lmf_sce_get_executable_module_handle(void *, sce::ModuleHandle *out) {
    // EXPLICITLY DISABLED (runtime-gated, not deleted). OBSERVED IN-GAME
    // (checkpoint zcode_ingame_v1_results, seq 1 g1_exec_handle, build
    // igf-f4a91d8123b0): sceKernelGetExecutableModuleHandle returned RAW 0
    // (errno 12) inside the running game — the route is dead on this target,
    // so the kernel is not even called and the distinct recorded code is
    // returned for citation by callers. Re-enable only behind
    // LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES for a NEW firmware experiment.
    (void)out;
    return LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;  // -4001
}
#endif

// --------------------------------------------------------------------------
// In-memory ELF base scan (PRIMARY identity route in-game). Vendored V2
// logic; every dereference is span-validated inside lmf_scan_image_bases
// against exactly the mappings handed in by the core.
// --------------------------------------------------------------------------
size_t lmf_sce_scan_raw_read(void *, uint64_t addr, void *dst, size_t len) {
    // Reached ONLY after [addr, addr+len) was proven to lie inside one
    // readable reported span (lmf_scan_image_bases contract).
    (void)std::memcpy(dst, reinterpret_cast<const void *>(
                               static_cast<uintptr_t>(addr)), len);
    return len;
}

int lmf_sce_scan_image_base(void *, const OsVmMapping *mappings, size_t count,
                            VmScanResult *out) {
    if (out == nullptr) return -1;
    if (mappings == nullptr) return -1;
    if (count > LiveLoaderObservedFacts::kVmMappingSnapshotCapacity)
        count = LiveLoaderObservedFacts::kVmMappingSnapshotCapacity;  // bound, honest
    for (size_t i = 0; i < count; ++i) {
        g_scan_spans[i].start = mappings[i].start;
        g_scan_spans[i].end = mappings[i].end;
        g_scan_spans[i].protection = static_cast<uint32_t>(mappings[i].protection);
    }
    *out = lmf_scan_image_bases(g_scan_spans, count, lmf_sce_scan_raw_read,
                                nullptr, g_scan_candidates,
                                kVmScanCandidateCapacity);
    return 0;
}

}  // namespace

// Builds the ops table with every pinned slot bound. In-game promotion:
// query_vmentry + scan_image_base are the primary routes; link_map_snapshot
// and get_executable_module_handle are bound to the explicitly-disabled
// refusals citing the observed facts (g2/g1) unless
// LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES re-enables the real code paths.
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
    ops.scan_image_base = lmf_sce_scan_image_base;  // primary identity (in-game)
    ops.get_executable_module_handle =
        lmf_sce_get_executable_module_handle;  // explicitly-disabled (g1)
    return ops;
}

}  // namespace lmf
