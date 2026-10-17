// import_references.S — typed owned import references (lineage format).
// VENDORED from worker-ingame-probe/src/import_references.S and STRIPPED for
// the v2 isolation probe (worker-ingame-probe-v2, team 2026-10-04).
//
// v1 (build igf-f4a91d8123b0) was silently load-rejected on console while the
// sibling aligned-entry module loaded in the same launch (checkpoint
// zcode_worker_ingame_probe.second_launch_diagnosis). PRIME-SUSPECT HYPOTHESIS
// (unproven until v2 evidence): the 5 DERIVED import identities were not
// accepted by the real console loader. v2 therefore carries ONLY the 7
// identities copied VERBATIM from the frozen 191-import contract — the exact
// provenance family the console-proven lineage loaded with. Every derived row
// (sceKernelGetModuleList, sceKernelGetModuleInfo,
// sceKernelGetExecutableModuleHandle, dlopen, dlclose) and every weak dl row
// (dlsym, __dlopen, __dlclose, __dlsym) is REMOVED; base discovery now comes
// from the sysctl VMMAP scan + in-memory ELF parse (V2 experiment), which
// needs no module-query or dl imports at all.
//
// Strong identities (7), all verbatim frozen-contract rows:
//   __error, sceKernelOpen, sceKernelWrite, sceKernelClose, sysctl,
//   getpid, clock_gettime
//
// LABELED CHANGE (worker-record-sink-addon, qdrv1 build, team 2026-10-04):
// FOUR additional rows appended, each copied VERBATIM from the SAME frozen
// 191-row contract (native-full-runtime-aligned-entry-1/import-contract.json;
// verbatim-verified by build_qdrv_probe.py before packaging):
//   nanosleep, pthread_create, pthread_detach, pthread_self
// None is a derived identity (the v1 load-rejection correlate). pthread_create
// is the observer-lineage precedent (worker-aligned-entry-observer runs exactly
// one worker thread; source/generated/import_names.h:99-102 carries the same
// rows). These rows exist ONLY so the driver thread can exist; no module-query,
// dl*, or mprotect row is added, and the FORBIDDEN list is unchanged.
//   pthread_self: the driver folds its OWN thread identity into the harness's
//   game_thread_id_key and admits exactly that one thread (see qh_driver.cpp).
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

.globl nanosleep
.type nanosleep,@function
.quad nanosleep

.globl pthread_create
.type pthread_create,@function
.quad pthread_create

.globl pthread_detach
.type pthread_detach,@function
.quad pthread_detach

.globl pthread_self
.type pthread_self,@function
.quad pthread_self

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
