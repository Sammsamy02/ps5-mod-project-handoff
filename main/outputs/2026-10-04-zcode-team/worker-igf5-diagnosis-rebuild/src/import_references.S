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

.globl modbridge_import_table_end
.hidden modbridge_import_table_end
modbridge_import_table_end:
.size modbridge_import_table, modbridge_import_table_end - modbridge_import_table
