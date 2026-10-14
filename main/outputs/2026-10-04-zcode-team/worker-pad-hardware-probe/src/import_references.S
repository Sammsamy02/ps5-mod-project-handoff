// import_references.S — typed owned import references (lineage format) for
// the standalone pad probe PRX (worker-pad-hardware-probe, team 2026-10-04).
// VENDORED from worker-ingame-probe-v3/src/import_references.S (lineage:
// worker-ingame-probe v2 isolation variant).
//
// Row provenance (the generated contract assembled by build_pad_probe.py
// must carry EXACTLY these 14 identities; this object is linked FIRST in
// the ld -r combine so the typed FUNC references survive merging):
//
// Strong identities (7) — VERBATIM frozen-contract rows, the console-proven
// v2/v3 set (loaded and logged on console):
//   __error, sceKernelOpen, sceKernelWrite, sceKernelClose, sysctl,
//   getpid, clock_gettime
//
// Strong identities (7) — NEW pad/service rows, NOT console-load-proven in
// a ploader plugin (this launch is the experiment). Identities are copied
// from Townfall's OWN saved executable import declarations
// (ue4ss-public-import-declarations-1/declared-input.json,
// townfall-declared-imports.json; sceUserServiceGetLoginUserIdList from the
// retained SDK stub catalog) — i.e. the exact identities the game process
// itself imports:
//   scePadInit, scePadOpen, scePadReadState, scePadClose (libScePad),
//   sceUserServiceInitialize, sceUserServiceGetLoginUserIdList,
//   sceUserServiceTerminate (libSceUserService)
//
// FORBIDDEN on purpose (v2 isolation, kept): sceKernelGetModuleList,
// sceKernelGetModuleInfo, sceKernelGetExecutableModuleHandle, dlopen,
// dlclose, dlsym, __dlopen, __dlclose, __dlsym, _DYNAMIC, __DYNAMIC.
.section .data.rel.ro.modbridge_imports,"aw",@progbits
.p2align 3
.globl modbridge_import_table
.hidden modbridge_import_table
.type modbridge_import_table,@object
modbridge_import_table:

.globl __error
.type __error,@function
.quad __error

.globl sceKernelOpen
.type sceKernelOpen,@function
.quad sceKernelOpen

.globl sceKernelWrite
.type sceKernelWrite,@function
.quad sceKernelWrite

.globl sceKernelClose
.type sceKernelClose,@function
.quad sceKernelClose

.globl sysctl
.type sysctl,@function
.quad sysctl

.globl getpid
.type getpid,@function
.quad getpid

.globl clock_gettime
.type clock_gettime,@function
.quad clock_gettime

.globl scePadInit
.type scePadInit,@function
.quad scePadInit

.globl scePadOpen
.type scePadOpen,@function
.quad scePadOpen

.globl scePadReadState
.type scePadReadState,@function
.quad scePadReadState

.globl scePadClose
.type scePadClose,@function
.quad scePadClose

.globl sceUserServiceInitialize
.type sceUserServiceInitialize,@function
.quad sceUserServiceInitialize

.globl sceUserServiceGetLoginUserIdList
.type sceUserServiceGetLoginUserIdList,@function
.quad sceUserServiceGetLoginUserIdList

.globl sceUserServiceTerminate
.type sceUserServiceTerminate,@function
.quad sceUserServiceTerminate

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
