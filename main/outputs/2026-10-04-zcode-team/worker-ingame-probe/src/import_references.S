// import_references.S — typed owned import references (lineage format).
//
// Mirrors native-full-runtime-aligned-entry-1/import_references.S: every
// strong import is declared .globl + @function and carried in the
// modbridge_import_table so the packager sees an exactly-typed reference per
// identity row. The weak dl family is declared .weak; unresolved weak
// references stay null and G3 records which (if any) resolved.
//
// Strong identities (12):
//   copied verbatim from the frozen 191-import contract:
//     __error, sceKernelOpen, sceKernelWrite, sceKernelClose, sysctl,
//     getpid, clock_gettime
//   derived rows (pattern of the frozen sceKernelOpen/sysctl rows:
//     module libkernel v257, soname libkernel.prx, library per row):
//     sceKernelGetModuleList, sceKernelGetModuleInfo (libkernel/libkernel),
//     sceKernelGetExecutableModuleHandle (libkernel/libkernel), dlopen,
//     dlclose (SDK stub target/lib/libkernel.so exports both friendly names)
// Weak (4): dlsym, __dlopen, __dlclose, __dlsym (writer drops unresolved
// weak symbols; G3 records the strong route)
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

.globl sceKernelGetModuleList
.type sceKernelGetModuleList,@function
.quad sceKernelGetModuleList

.globl sceKernelGetModuleInfo
.type sceKernelGetModuleInfo,@function
.quad sceKernelGetModuleInfo

.globl sceKernelGetExecutableModuleHandle
.type sceKernelGetExecutableModuleHandle,@function
.quad sceKernelGetExecutableModuleHandle

.globl dlopen
.type dlopen,@function
.quad dlopen

.globl dlclose
.type dlclose,@function
.quad dlclose

.weak dlsym
.type dlsym,@function
.quad dlsym

.weak __dlopen
.type __dlopen,@function
.quad __dlopen

.weak __dlclose
.type __dlclose,@function
.quad __dlclose

.weak __dlsym
.type __dlsym,@function
.quad __dlsym

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
