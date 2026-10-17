// import_references.S — typed owned import references (lineage format).
// VENDORED from worker-fc1-validator-probe/src/import_references.S (fc1pool1,
// itself vendored from worker-record-sink-addon from the frozen 191-import
// contract lineage), LABELED CHANGE for worker-carrier-probe-build (cc1,
// team 2026-10-04):
//
//   TEN rows, each copied VERBATIM from the SAME frozen 191-row contract
//   (native-full-runtime-aligned-entry-1/import-contract.json; verbatim-
//   verified by build_carrier_probe.py before packaging):
//     the igf 7 (__error, sceKernelOpen, sceKernelWrite, sceKernelClose,
//     sysctl, getpid, clock_gettime)
//     PLUS nanosleep — the bounded 4 s poll sleeps of the census cadence
//     (fc1pool1 precedent)
//     PLUS pthread_create, pthread_detach — the qdrv1/observer-lineage
//     precedent (worker-record-sink-addon carries exactly these rows plus
//     pthread_self; worker-aligned-entry-observer runs exactly one worker
//     thread). cc1 runs ONE detached census thread (30 polls x 4 s);
//     pthread_self is DROPPED relative to qdrv1 because cc1 publishes no
//     thread-identity fold (data-side only, no game_thread_id_key).
//
// None is a derived identity (the v1 load-rejection correlate). No
// module-query, dl*, mprotect, mutex, or raw-syscall row exists here; the
// FORBIDDEN list is unchanged from qdrv1/fc1pool1.
// The probe reads engine data ONLY through the igf6-proven span-gated
// direct-dereference class (mapscan + ig_map_cover first), so no
// memory-permission import is needed or allowed.
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

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
