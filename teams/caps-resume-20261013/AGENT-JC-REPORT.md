# Agent JC report — cap01-03 provider-route verdict (Mac-only source analysis)

Date: 2026-10-13. Scope: capabilities #1 (kernel reads) and #3 (code patching) provider
routes. No builds, no console/kernel contact, no exploit or bypass tooling, no client
code written.

Deliverable: `caps-team-work/cap01-03/PROVIDER-ROUTE-VERDICT.md` (identical copy at
`outputs/cap01-03/PROVIDER-ROUTE-VERDICT.md`; SHA256
`3442ec570885093400afce83880abd4b5ad66e6e659cd5a4cac31b8e7dac2b2f` for both).

Sources actually read: pinned kstuff-lite v1.11 source zip (re-extracted read-only to
/tmp/kb-src-jc; archive hash unchanged, matches SOURCE-AUDIT.json) — kekcall.c/.h,
main.c, utils.h, syscall_fixes.c, traps.h, 8cc-kekcalls.c, debug-reader.c,
Makefile, prosper0gdb/r0gdb.c/.h + lib/r0gdb-bootstrap.h + lib/crt-elf-c.c,
gdb_stub/dbg.c + ring0.c, lib/syscalls.txt; ps5-payload-sdk v0.43 pinned copy —
ps5/kernel.h, samples (hello_kenv, arbitrary_syscall, ps, test_privileges), libkernel
.so symbol tables; AI-HANDOFF-2026-10-13.md; PS5-MODDING-MISSION.md capability/Phase B
sections.

Key findings (all line-cited in the verdict):

1. Kstuff kekcall table confirmed: subs 1 dbgregs-read, 2 dbgregs-write, 3 RDMSR,
   4 reserved/unimplemented, 5 REMOTE_SYSCALL, 6 snapshot (ENOSYS in default
   KSTUFF_OBS=0 build), 7 PPR-plaintext, 0xffffffff CHECK; unhandled nr = ENOSYS
   passthrough. No generic kernel-VA copy op exists; internal copy helpers are private.
2. REMOTE_SYSCALL (sub 5) cannot implement kernel-VA reads: it executes ordinary
   syscalls whose pointer args are user pointers of the current address space; no stock
   syscall reads arbitrary kernel VAs; process_vm_readv absent from the FreeBSD-11 PS5
   base; ptrace (exists, SYS_ptrace=26) is cross-process-userspace-only and needs
   credentials the PRX lacks (ps5-payload-sdk test_privileges elevates ucred
   authid+caps before ptrace — direct upstream evidence).
3. Correction: "REMOTE_SYSCALL proven in prosper0gdb" is FALSE per source — prosper0gdb
   contains zero sub-5/kekcall usage; its kernel reads use the IPV6_PKTINFO
   socket-corruption primitive (kwrite20, r0gdb.c:111-120) plus pipe-buffer redirection
   (copyout/copyin, r0gdb.c:173-192).
4. Loader route identified precisely: the exploit-time elfldr passes specter_args
   (pipe, rwpair master/victim sockets, kpipe_addr, kdata_base); kstuff's crt-elf-c.c
   builds r0gdb_bootstrap consumed by r0gdb_init. ps5-payload-sdk (GPL-3) declares
   kernel_copyin/kernel_copyout + full kernel_* family (kernel.h:59-143) but the pinned
   v0.43 copy ships NO implementation (no exports in any libkernel*.so; kenv is only an
   unresolved PLT import). The route is exploit-session-local, bound to the
   loader-spawned process, and NOT callable from the game PRX. Handoff's three
   preliminary observations all confirmed at line level.
5. #3 is viable with zero kernel changes: kstuff's resident syscall-fix intercepts
   mprotect globally and skips the kernel permission-validation block
   (main.c:87-93, syscall_fixes.c:63-80) → plain mprotect(RWX)+write+revert from the
   PRX, with in-tree precedent (gdb_stub/dbg.c:393-448). Preconditions: kstuff resident
   and unpaused (currently inferred, not measured), PPR sysentvec, clean-revert
   discipline.
6. Unexplored direct-syscall candidate for #1: Sony's __sys_test_debug_rwmem
   (lib/syscalls.txt:428; wrapper exported by SDK libkernel) — prototype and privilege
   gates unknown from pinned sources; probing is a console action needing authorization.

Verdict: #1 blocked under "no new kernel-resident code / no modified kstuff" — the
mission's own Phase B rule (extend EchoStretch/kstuff-lite only after kekcall shown
insufficient) is the sanctioned next rung and this document supplies the
insufficiency proof. #3 does not need #1: proceed via the mprotect fix route.

Written artifacts: the two verdict files above and this report. Nothing else modified;
/tmp extraction is disposable.
