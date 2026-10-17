// import_references.S — typed owned import references (lineage format).
// VENDORED from worker-v3-startup-diag/src/import_references.S (itself
// stripped for the v2 isolation probe). EXACTLY ONE delta for
// worker-minimal-path-test, labeled below.
//
// Strong identities (8), ALL verbatim frozen-191-contract rows:
//   __error, sceKernelOpen, sceKernelWrite, sceKernelClose, sysctl,
//   getpid, clock_gettime            <- the 7 console-proven igd1 rows
//   read                             <- ADDED for this mission ONLY
//
// DELTA vs igd1 (loud, single): POSIX-name `read` (libScePosix family,
// ModuleName libkernel — same class as getpid/clock_gettime) copied
// byte-identical from
//   .superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json
// The aligned-entry module that carried all 191 verbatim rows LOADED on
// console, but this lineage has never INVOKED `read` before — first use,
// exactly one call site (16-byte loadlist read), failure recorded, no
// dereference of the result beyond the recorded hex. Every derived row
// (sceKernelGetModuleList, sceKernelGetModuleInfo,
// sceKernelGetExecutableModuleHandle, dlopen, dlclose) and every weak dl
// row (dlsym, __dlopen, __dlclose, __dlsym) remains ABSENT, as in v2+.
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

.globl read
.type read,@function
.quad read

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
