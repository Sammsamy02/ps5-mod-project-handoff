// probe_abi.hpp
//
// ABI spellings for LIVEFACTS_PROBE. Every declaration below is classified:
//
// PINNED (fully pinned in this workspace):
//   - the 352-byte module-info struct: re-spelled identically to worker-1's
//     sce_module_abi.hpp, which privately spells the prosperodev MIT header
//     retained verbatim at
//     outputs/ps5-mod-bridge/native-module-metadata/upstream/libkernel.h
//     (commit 3cdd09c). LP64 static_asserts below prove the layout.
//   - sceKernelGetModuleInfo prototype (same provenance, line 159).
//
// UNPINNED (exported by the pinned SDK libkernel/libkernel_web but declared by
// NO SDK header; the signature itself is a probe subject, never trusted):
//   - sceKernelGetExecutableModuleHandle: declared here as returning int64 and
//     taking void, per the probe brief experiment E3. UNPINNED — MARKED.
//   - sceKernelGetModuleList: genuinely ambiguous spelling. The pinned
//     prosperodev header spells it (SceKernelModule*, size_t, size_t*) (3 args,
//     line 158); the probe brief supplies a 4-arg spelling
//     (flags, size, handles*, available*). E4 probes BOTH spellings and BOTH
//     size-unit readings (4*N bytes vs N elements) through a 6-argument
//     function pointer in which every register slot holds either 0 or a
//     pointer into owned zeroed scratch, so any register-level
//     reinterpretation either writes only inside our buffers or fails
//     in-kernel (graceful EFAULT-style) and is recorded.
#ifndef LIVEFACTS_PROBE_ABI_HPP
#define LIVEFACTS_PROBE_ABI_HPP

#include <stddef.h>
#include <stdint.h>

namespace livefacts {

using ModuleHandle = uint32_t;  // prosperodev: typedef uint32_t SceKernelModule;

// prosperodev upstream libkernel.h lines 45-51 (SceKernelModuleSegmentInfo).
struct ModuleSegment {
    void *address;
    uint32_t size;
    int32_t prot;  // opaque reported value; NOT interpreted as permissions
};
static_assert(sizeof(ModuleSegment) == 16, "segment ABI size");
static_assert(offsetof(ModuleSegment, size) == 8, "segment size offset");
static_assert(offsetof(ModuleSegment, prot) == 12, "segment prot offset");

// prosperodev upstream libkernel.h lines 53-60 (SceKernelModuleInfo).
// Provider contract (prior art native-module-metadata): caller zeroes the
// buffer and sets size before the call; the kernel must echo it.
struct ModuleInfo {
    size_t size;
    char name[256];
    ModuleSegment segmentInfo[4];
    uint32_t segmentCount;
    uint8_t fingerprint[20];
};
static_assert(sizeof(ModuleInfo) == 352, "module info ABI size");
static_assert(offsetof(ModuleInfo, name) == 8, "name offset");
static_assert(offsetof(ModuleInfo, segmentInfo) == 264, "segments offset");
static_assert(offsetof(ModuleInfo, segmentCount) == 328, "count offset");
static_assert(offsetof(ModuleInfo, fingerprint) == 332, "fingerprint offset");
static_assert(sizeof(void *) == 8 && sizeof(size_t) == 8, "LP64 ABI required");

}  // namespace livefacts

extern "C" {

// PINNED: prosperodev upstream libkernel.h line 159 spelling; export
// "T sceKernelGetModuleInfo" verified in the pinned SDK target libs.
int sceKernelGetModuleInfo(livefacts::ModuleHandle handle,
                           livefacts::ModuleInfo *info);

// UNPINNED declaration (E3 subject): no SDK header declares it; the pinned
// libs export "T sceKernelGetExecutableModuleHandle". The raw int64 return
// and errno are recorded verbatim; success is judged ONLY by whether the
// returned value validates under sceKernelGetModuleInfo with a 352-byte echo.
int64_t sceKernelGetExecutableModuleHandle(void);

// UNPINNED declaration (E4 subject): pinned prosperodev 3-arg spelling
// (upstream libkernel.h line 158). Export "T sceKernelGetModuleList" verified
// in the pinned SDK target libs. Called ONLY through ModuleList6 below.
int sceKernelGetModuleList(livefacts::ModuleHandle *array, size_t size,
                           size_t *available);

// E4 probe harness: six argument slots map to rdi/rsi/rdx/rcx/r8/r9 on
// x86-64 SysV. Every slot is passed either 0 or a pointer to owned zeroed
// scratch before the call, so neither the 3-arg nor the 4-arg reading of the
// real entry point can write outside our buffers.
typedef int (*ModuleList6)(void *, size_t, void *, void *, void *, void *);

}  // extern "C"

#endif  // LIVEFACTS_PROBE_ABI_HPP
