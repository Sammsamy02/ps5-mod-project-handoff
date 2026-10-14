// import_references.S — typed owned import references (lineage format).
// VENDORED from worker-record-sink-addon/src/import_references.S (qdrv1,
// itself stripped from worker-ingame-probe-v2 from the frozen 191-import
// contract lineage), LABELED CHANGE for worker-fc1-validator-probe (fc1pool1,
// team 2026-10-04):
//
//   EIGHT rows, each copied VERBATIM from the SAME frozen 191-row contract
//   (native-full-runtime-aligned-entry-1/import-contract.json; verbatim-
//   verified by build_fc1pool_probe.py before packaging):
//     the igf 7 (__error, sceKernelOpen, sceKernelWrite, sceKernelClose,
//     sysctl, getpid, clock_gettime)
//     PLUS nanosleep — the single FLAGGED ADDITION beyond the igf 7, used
//     only for the bounded 1 s grace sleeps of the pool-initialization wait;
//     the qdrv1/observer pthread-lineage precedent covers verbatim frozen-row
//     additions, and this row is dropped from the qdrv1 set together with the
//     three pthread rows because THIS probe is a one-shot synchronous
//     module_start validator with NO thread.
//
// None is a derived identity (the v1 load-rejection correlate). No
// module-query, dl*, mprotect, or raw-syscall row exists here; the
// FORBIDDEN list is unchanged from qdrv1.
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

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
