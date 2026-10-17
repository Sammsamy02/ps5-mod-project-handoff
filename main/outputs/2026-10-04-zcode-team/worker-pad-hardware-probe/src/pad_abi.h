// pad_abi.h — import declarations of the standalone pad probe PRX
// (worker-pad-hardware-probe, team 2026-10-04). Vendored structure from
// worker-ingame-probe-v3/src/ingame_abi.h (lineage:
// native-full-runtime-aligned-entry-1/ingame_abi.h).
//
// PINNED import identity set (14 strong rows, ZERO weak; the generated
// contract is assembled by build_pad_probe.py and every row is verified):
//   - __error, sceKernelOpen, sceKernelWrite, sceKernelClose, sysctl,
//     getpid, clock_gettime: the 7 rows copied BYTE-IDENTICAL from the
//     frozen 191-import contract of the console-proven aligned-entry
//     lineage (exactly the set the loaded-and-logged v2/v3 modules carry).
//   - scePadInit/scePadOpen/scePadReadState/scePadClose (libScePad) and
//     sceUserServiceInitialize/sceUserServiceTerminate/sceUserServiceGetLoginUserIdList
//     (libSceUserService): NEW rows proven from Townfall's OWN saved
//     executable import declarations (sceUserServiceGetLoginUserIdList:
//     retained SDK stub catalog). These are the exact identities the game
//     process itself imports; loader acceptance for a ploader plugin is
//     NOT proven and is what this probe's launch tests.
//
// The typed references live in import_references.S (lineage format: typed
// FUNC .quad rows, object linked FIRST in the ld -r combine).
#ifndef PAD_PROBE_ABI_H
#define PAD_PROBE_ABI_H

#include <stddef.h>
#include <stdint.h>

#include "pad_sdl_ps5.h"  // PS5_PadData (vendored pinned SDL header)

// Verbatim frozen-contract identities (same declarations as the v3 lineage):
int *__error(void);
int sceKernelOpen(const char *, int, uint16_t);
int64_t sceKernelWrite(int, const void *, size_t);
int sceKernelClose(int);
/* sysctl/getpid/clock_gettime: declared by the pinned SDK target headers
   (sys/sysctl.h, unistd.h, time.h) with the same identities the frozen
   contract carries; not redeclared here to avoid conflicts. */

// Pad/service identities (NEW rows; signatures exactly as the retained
// provider's PadBackend uses them — PS5PadSource.hpp:15-23 — and as the
// retained PS5PadNative.cpp declares sceUserServiceTerminate):
int scePadInit(void);
int scePadOpen(int user_id, int type, int index, void *param);
int scePadReadState(int handle, PS5_PadData *data);
int scePadClose(int handle);
int sceUserServiceInitialize(void *);
int sceUserServiceGetLoginUserIdList(int userId[4]);
int sceUserServiceTerminate(void);

#endif  // PAD_PROBE_ABI_H
