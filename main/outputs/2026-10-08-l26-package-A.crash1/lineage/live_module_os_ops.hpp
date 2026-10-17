// live_module_os_ops.hpp
//
// Host-injectable OS boundary for the live module facts supplier. The generic
// core (live_module_facts.cpp) sees ONLY this table; host tests inject fakes;
// live_module_os_ops_sce.cpp is the only TU binding pinned SDK symbols.
//
// IN-GAME ROUTE PROMOTION (checkpoint zcode_ingame_v1_results, 2026-10-04):
//   KERN_PROC_VMMAP is the PRIMARY and default identity/mapping source
//   (query_vmentry + scan_image_base). The exec-handle and DT_DEBUG routes
//   are DEMOTED to explicitly-disabled: the SCE TU refuses them with the
//   distinct codes in LiveLoaderObservedFacts.hpp citing the observed facts
//   (g1 raw 0; g2 DT_DEBUG absent/unfilled). Their code paths stay in the
//   tree, runtime-gated behind LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES —
//   never deleted.
//
// Contract for every callback (mirrors the trusted-provider rules of the
// project's native-module-metadata adapter):
//   - synchronous, bounded, returns 0 on success, nonzero provider code on
//     failure (the core records the code verbatim);
//   - writes only into caller-provided storage; retains no pointers;
//   - never dereferences any game/module address — mapping and segment
//     addresses are integers. SOLE EXCEPTION: scan_image_base dereferences
//     in-memory bytes, and only after span-validating each read against the
//     very mappings it was handed (see live_module_base_scan.hpp).
#ifndef LMF_LIVE_MODULE_OS_OPS_HPP
#define LMF_LIVE_MODULE_OS_OPS_HPP

#include <cstddef>
#include <cstdint>

#include "live_module_base_scan.hpp"
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
    // DEMOTED (observed in-game, g2): absent/unfilled DT_DEBUG in the game
    // process — the default SCE binding REFUSES with
    // LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame without
    // walking; the core then proceeds VMMAP-first. The real walk survives
    // behind LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES.
    int (*link_map_snapshot)(void *context, LinkMapEntry *out, size_t capacity,
                             size_t *count);

    // Take a REAL loader reference on an already-loaded object
    // (SCE TU: dlopen(name, RTLD_NOLOAD) — never loads new bytes).
    // Returns null when the object is not currently loaded by rtld.
    // Degraded dl retention (composition define LMF_DISABLE_DL_RETENTION)
    // returns null unconditionally; unchanged by the in-game update.
    void *(*loader_retain_noload)(void *context, const char *name);

    // Drop one loader reference (SCE TU: dlclose). Returns 0 on success.
    int (*loader_release)(void *context, void *loader_handle);

    // Authoritative Prospero module info (SCE TU: sceKernelGetModuleInfo).
    // The core zeroes info and sets info->size = profile.module_info_abi_size
    // before the call; the provider must echo that size. Pinned ABI only.
    // OBSERVED in-game (g4): on module-list handles this returns 0x7FFFFFFF
    // with empty names (domain/ABI mismatch, unresolved) — the supplier has
    // no working handle source in-game and stays degraded on this route.
    int (*get_module_info)(void *context, sce::ModuleHandle handle,
                           sce::ModuleInfo *info);

    // Non-dereferencing mapping info (SCE TU: sysctl KERN_PROC_VMMAP for the
    // current pid, converted to OsVmMapping). Copies at most capacity;
    // *count is the TOTAL; return 0 on success.
    // PRIMARY route in-game (g5: rc 0, 1252 entries).
    int (*query_vmentry)(void *context, OsVmMapping *out, size_t capacity,
                         size_t *count);

    // OPTIONAL, DEFAULT NULL. In-memory ELF image-base scan over the
    // kernel-reported mappings: the PRIMARY identity route whenever the
    // link-map walk does not yield an anchor (in-game: always, per g2).
    // Implementations MUST go through lmf_scan_image_bases semantics — no
    // dereference beyond span-validated reads. out is zeroed by the core
    // before the call; return 0 when the scan completed (even needs-base).
    int (*scan_image_base)(void *context, const OsVmMapping *mappings,
                           size_t count, VmScanResult *out);

    // Observed executable path of THIS process (SCE TU: sysctl
    // KERN_PROC_PATHNAME). Writes a NUL-terminated path; 0 on success.
    // Refuted in the payload environment; unobserved in-game — the adapter
    // still refuses an empty path rather than fabricating one.
    int (*executable_path)(void *context, char *out, size_t capacity);

    // Process id (SCE TU: getpid from unistd.h). Returns pid or -1.
    int (*get_pid)(void *context);

    // OPTIONAL, DEFAULT NULL. sceKernelGetExecutableModuleHandle: the SCE TU
    // now BINDS this slot to an explicitly-disabled implementation that
    // refuses with LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame
    // (observed in-game g1: raw 0) without calling the kernel; the real
    // unpinned int64(void) spelling survives behind
    // LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES. The core records the refusal
    // code in provider_code; capture() proceeds in the documented degraded
    // mode instead of inventing a handle.
    int (*get_executable_module_handle)(void *context, sce::ModuleHandle *out);
};

}  // namespace lmf

#endif  // LMF_LIVE_MODULE_OS_OPS_HPP
