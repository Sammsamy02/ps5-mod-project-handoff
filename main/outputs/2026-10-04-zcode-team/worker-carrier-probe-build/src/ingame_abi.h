// ingame_abi.h — v2 ISOLATION VARIANT (worker-ingame-probe-v2, team
// 2026-10-04). Vendored from worker-ingame-probe/src/ingame_abi.h with every
// unpinned/derived import declaration REMOVED (G1/G3/G4 dropped entirely).
//
// PINNED (workspace-pinned declarations; the ONLY imports of this module):
//   - in-game file I/O trio sceKernelOpen/sceKernelWrite/sceKernelClose:
//     the only I/O route PROVEN inside the running Townfall process by the
//     native-aligned-entry lineage (.superpowers/native-dependency-spike/
//     native-full-runtime-aligned-entry-1/diagnostic.c; receipt
//     NATIVE_FULL_CONSTRUCTORS_AND_BUFFERED_LUA_CONFIRMED, log fresh again on
//     the same launch that rejected v1).
//   - __error/sysctl/getpid/clock_gettime: identity rows copied VERBATIM from
//     the frozen 191-import diagnostic identity contract of the same lineage
//     (all 7 rows console-proven by the aligned-entry module's load).
//
// REMOVED relative to v1 (never referenced anywhere in v2 sources):
//   IgModuleSegment/IgModuleInfo (352-byte module info), sceKernelGetModuleInfo,
//   sceKernelGetModuleList, sceKernelGetExecutableModuleHandle, dlopen/dlclose
//   strong rows, dlsym/__dlopen/__dlclose/__dlsym weak rows, RTLD constants.
// Base discovery is import-free: sysctl KERN_PROC_VMMAP spans + in-memory
// ELF header/program-header reads validated against those spans (V2).
#ifndef INGAME_ABI_H
#define INGAME_ABI_H

#include <stddef.h>
#include <stdint.h>

// Strong imports (the complete identity set; rows in the generated import
// contract are byte-identical to the frozen 191-import contract):
int *__error(void);  // errno word; identity copied verbatim from frozen contract
int sceKernelOpen(const char *, int, uint16_t);
int64_t sceKernelWrite(int, const void *, size_t);
int sceKernelClose(int);
/* sysctl/getpid/clock_gettime: declared by the pinned SDK target headers
   (sys/sysctl.h, unistd.h, time.h) with the same identities the frozen
   contract carries; not redeclared here to avoid conflicts. */

#endif  // INGAME_ABI_H
