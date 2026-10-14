// sce_module_abi.hpp
//
// Private spellings of the only Prospero module-loader ABI that is pinned in
// this workspace. This file invents nothing: every declaration below is either
// (a) copied from the MIT prosperodev public header retained verbatim at
//     outputs/ps5-mod-bridge/native-module-metadata/upstream/libkernel.h
//     (commit 3cdd09cc86ba90ef371bd0a834e07cc3883c49e7, 2022-10-30), or
// (b) a POSIX/FreeBSD declaration from the pinned SDK's own headers
//     (target/include/dlfcn.h, target/include/sys/link_elf.h).
//
// The pinned ps5-payload-sdk v0.43 libkernel.so exports many more module
// symbols (sceKernelGetExecutableModuleHandle, ...FromAddr, ...IsModuleLoaded,
// ...GetProcessName, sceKernelInternalMemoryGetModuleSegmentInfo,
// get_module_info_list, sceKernelGetModuleList) but the SDK ships NO header
// declaring their signatures. Those are intentionally NOT declared here and
// are never called by this library; see REPORT.md "Unresolved facts".
#ifndef LMF_SCE_MODULE_ABI_HPP
#define LMF_SCE_MODULE_ABI_HPP

#include <stddef.h>
#include <stdint.h>

namespace lmf::sce {

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
// The provider contract (prior art native-module-metadata) is that the caller
// zeroes the buffer and sets size before the call; the kernel must echo it.
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

}  // namespace lmf::sce

#endif  // LMF_SCE_MODULE_ABI_HPP
