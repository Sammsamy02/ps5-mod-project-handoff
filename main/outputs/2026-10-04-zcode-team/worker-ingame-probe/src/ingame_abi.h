// ingame_abi.h
//
// ABI spellings for the MODBRIDGE IN-GAME FACTS PRX (worker-ingame-probe,
// team 2026-10-04). Adapted from worker-liveprobe/src/probe_abi.hpp (ZCode
// team, same provenance discipline) for a freestanding PRX context.
//
// PINNED (workspace-pinned declarations):
//   - 352-byte module-info struct + sceKernelGetModuleInfo prototype:
//     prosperodev MIT upstream libkernel.h retained verbatim at
//     outputs/ps5-mod-bridge/native-module-metadata/upstream/libkernel.h
//     (commit 3cdd09c), re-spelled by worker-1 sce_module_abi.hpp and
//     worker-liveprobe probe_abi.hpp. LP64 static asserts prove the layout.
//   - in-game file I/O trio sceKernelOpen/sceKernelWrite/sceKernelClose:
//     the only I/O route PROVEN inside the running Townfall process by the
//     native-aligned-entry lineage (.superpowers/native-dependency-spike/
//     native-full-runtime-aligned-entry-1/diagnostic.c, "three kernel
//     imports proven by the v2 probe"; NATIVE_FULL_CONSTRUCTORS_AND_
//     BUFFERED_LUA_CONFIRMED receipt).
//   - sysctl/getpid/clock_gettime identities: copied verbatim from the
//     frozen 191-import diagnostic identity contract of the same lineage.
//
// UNPINNED (probe subjects, never trusted; raw returns recorded verbatim):
//   - sceKernelGetExecutableModuleHandle: int64(void), no SDK header.
//   - sceKernelGetModuleList: prosperodev 3-arg spelling (array,size,avail)
//     RESOLVED by the controller livefacts run (4-arg = 0x8002003E EINVAL).
//   - dlopen/dlclose: STRONG derived libkernel identities (SDK stub
//     target/lib/libkernel.so exports both friendly names); load-failure risk
//     if the library classification is wrong is documented in STAGE-PLAN.
//   - dlsym/__dlopen/__dlclose/__dlsym: declared WEAK; the packager drops
//     unresolved weak symbols so these bind statically to null.
//
// RTLD constants: RTLD_NOLOAD|RTLD_NOW was 0x2002 in the pinned SDK dlfcn
// (controller livefacts receipt E6); re-spelled as constants with that
// observed value so no dlfcn.h include is needed in the freestanding module.
#ifndef INGAME_ABI_H
#define INGAME_ABI_H

#include <stddef.h>
#include <stdint.h>

#define IG_RTLD_NOW 0x2u
#define IG_RTLD_NOLOAD 0x2000u

typedef uint32_t IgModuleHandle;  // prosperodev: uint32_t SceKernelModule

// prosperodev upstream libkernel.h lines 45-51 (SceKernelModuleSegmentInfo).
typedef struct IgModuleSegment {
    void *address;
    uint32_t size;
    int32_t prot;  // opaque reported value; NOT interpreted
} IgModuleSegment;

// prosperodev upstream libkernel.h lines 53-60 (SceKernelModuleInfo), 352B.
typedef struct IgModuleInfo {
    size_t size;
    char name[256];
    IgModuleSegment segmentInfo[4];
    uint32_t segmentCount;
    uint8_t fingerprint[20];
} IgModuleInfo;

#if !defined(__cplusplus)
_Static_assert(sizeof(IgModuleSegment) == 16, "segment ABI size");
_Static_assert(sizeof(void *) == 8 && sizeof(size_t) == 8, "LP64 ABI required");
#if defined(__LP64__) && __LP64__
_Static_assert(sizeof(IgModuleInfo) == 352, "module info ABI size");
_Static_assert(offsetof(IgModuleInfo, name) == 8, "name offset");
_Static_assert(offsetof(IgModuleInfo, segmentInfo) == 264, "segments offset");
_Static_assert(offsetof(IgModuleInfo, segmentCount) == 328, "count offset");
_Static_assert(offsetof(IgModuleInfo, fingerprint) == 332, "fingerprint offset");
#endif
#endif  /* !__cplusplus */

// Strong imports (identity rows in the generated import contract):
int *__error(void);  // errno word; identity copied verbatim from frozen contract
int sceKernelOpen(const char *, int, uint16_t);
int64_t sceKernelWrite(int, const void *, size_t);
int sceKernelClose(int);
/* sysctl/getpid/clock_gettime: declared by the pinned SDK target headers
   (sys/sysctl.h, unistd.h, time.h) with the same identities the frozen
   contract carries; not redeclared here to avoid conflicts. */

int sceKernelGetModuleInfo(IgModuleHandle handle, IgModuleInfo *info);
int sceKernelGetModuleList(IgModuleHandle *array, size_t size, size_t *available);

// UNPINNED (G1 subject): judged only by 352-byte module-info echo validation.
int64_t sceKernelGetExecutableModuleHandle(void);

// dl family (G3 route): dlopen/dlclose strong; remainder weak-null-checked.
void *dlopen(const char *, int);
int dlclose(void *);
extern __attribute__((weak)) void *dlsym(void *, const char *);
extern __attribute__((weak)) void *__dlopen(const char *, int);
extern __attribute__((weak)) int __dlclose(void *);
extern __attribute__((weak)) void *__dlsym(void *, const char *);

#endif  // INGAME_ABI_H
