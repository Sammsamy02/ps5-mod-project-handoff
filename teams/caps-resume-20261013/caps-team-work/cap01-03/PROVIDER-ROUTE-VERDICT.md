# PROVIDER ROUTE VERDICT — capabilities #1 (kernel reads) and #3 (code patching)

Agent JC, 2026-10-13. Mac-only source analysis. No builds, no console/kernel contact, no
exploit or bypass tooling. Every claim below cites the file and lines actually read.
Sources inspected (full reads unless noted):

- Pinned kstuff-lite v1.11 source archive
  `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`
  (archive SHA256 `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6`,
  matches SOURCE-AUDIT.json). Extracted read-only to `/tmp/kb-src-jc/kstuff-lite-1.11/`.
- ps5-payload-sdk v0.43 local pinned copy
  `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/`.
- Prior component findings: `HANDOFF.md`, `SOURCE-AUDIT.json`, `ROUTE-ADDENDUM.md/.json`,
  and `AI-HANDOFF-2026-10-13.md` section "Numbers 1 and 3".

Verdict summary up front:

| Route | #1 kernel reads | #3 code patching |
|---|---|---|
| kekcall REMOTE_SYSCALL (sub 5) | NOT sufficient for kernel-VA reads | Indirect fallback only (remote mprotect), unproven |
| Loader/pipe route (kernel_copyin/out) | Works only inside the exploit-spawned process; NOT callable from the PRX; not a standing service | Not usable at game time |
| Direct syscall from game process | No kernel-read syscall exists; in-process game-memory reads need no kernel mediation | VIABLE now: plain mprotect(RWX)+write+revert rides kstuff's existing resident mprotect fix |

Bottom line: under the standing constraints (no new kernel-resident code, no modified
kstuff, three-restart rule), **no general kernel-VA read provider exists in the pinned
public sources**. #3 does not need one: it is reachable with zero kernel changes through
kstuff's already-resident mprotect fix. The mission's own Phase B rule anticipates the #1
outcome ("only if [kekcall] does not [suffice], extend … EchoStretch/kstuff-lite v1.11" —
PS5-MODDING-MISSION.md, PHASE B section); that extension is the sanctioned next rung and
requires explicit user authorization plus the recovery/regression protocol — it is not
Mac-only work.

---

## 1. The kstuff finding — verified and sharpened

### 1.1 The exposed kekcall table (confirmed: no generic kernel-copy)

`ps5-kstuff/uelf/kekcall.c:19-108` (`handle_kekcall`) dispatches on `nr = args[RAX]>>32`
and implements exactly:

| nr | Operation | Evidence |
|---|---|---|
| 0 / other unhandled | falls through to `return ENOSYS` → caller's getppid proceeds as the real syscall (passthrough) | kekcall.c:102-107 with main.c:74-84 |
| 1 | READ debug registers: runs kernel `copyout` of the 48-byte dbgregs area to the client pointer | kekcall.c:21-48 |
| 2 | WRITE debug registers: kernel `copyin` of 48 bytes from client pointer, installed via trap 1 | kekcall.c:49-58, 110-139 |
| 3 | RDMSR: `rdmsr(args[RDI], &args[RAX])` — one MSR by index | kekcall.c:59-62 |
| 4 | reserved for WRMSR — NOT implemented (comment "//nr 4 reserved for wrmsr"), returns ENOSYS | kekcall.c:63 |
| 5 | REMOTE_SYSCALL: run `sysents[sysc_no].sy_call` against a chosen pid | kekcall.c:64-76, 140-231 |
| 6 | shared-area snapshot — compiled only under `KSTUFF_OBS`; otherwise `ENOSYS`. Default build has `KSTUFF_OBS ?= 0` | kekcall.c:77-90; ps5-kstuff/Makefile:5 |
| 7 | PPR plaintext request (`control_ppr_plaintext_request`) | kekcall.c:91-101 |
| 0xffffffff | CHECK/liveness: sets `args[RAX]=0`, returns 0 | kekcall.c:102-106 |

Client-side encodings in-tree confirm the same table: `KEKCALL_GETPPID 0x000000027`,
`READ_DR 0x100000027`, `WRITE_DR 0x200000027` (ps5-kstuff/8cc-kekcalls.c:59-61) and
`KEKCALL_SHARED_AREA_SNAPSHOT 0x600000027`, `KEKCALL_CHECK 0xffffffff00000027`
(ps5-kstuff/debug-reader.c:9-10). `0x27 = 39 = SYS_getppid` (SDK
target/include/sys/syscall.h:47); the sub-op rides in the high 32 bits of RAX.

**Confirmed: there is no operation that copies an arbitrary client-specified kernel VA
to/from the client.** The only kernel-data movements are the fixed-function items above
(dbgregs, MSR, FSBASE special case, snapshot). The internal helpers the dispatcher
itself uses (`copy_from_kernel`, `copy_to_kernel`, `kpeek64_checked`, `kpoke64`,
`push/pop_stack_checked` — uelf/utils.h:28-44, 78-135) are private to the kstuff image
and are not reachable through any dispatched sub-operation. This verifies and does not
weaken HANDOFF.md's finding.

Client ABI (how any userspace process calls it): issue syscall 39 (getppid) with
RAX=(sub<<32)|39 and the six kekcall arguments in RDI,RSI,RDX,R10,R8,R9. In-tree plain-C
demonstration: debug-reader.c:23-46 (`kekcall()` inline-asm syscall wrapper). Entry
requirements: the call must land on the PPR sysent — `IS_PPR(getppid)` (uelf/main.c:64-65)
— i.e. the calling process must use the PPR (FreeBSD-style) sysentvec, not the
PS4-compat one (main.c:358-364 distinguishes `sysentvec_ps4`). The interception itself is
global once kstuff is resident, because `handle_syscall` hangs off the patched
syscall/int3 entry path (main.c:333-483).

### 1.2 Can REMOTE_SYSCALL (sub 5) implement kernel reads? — No, not kernel-VA reads

Mechanics, from source: the client passes `(pid, sysc_no, args_ptr)` in RDI/RSI/RDX
(kekcall.c:66-75 copies 48 bytes = 6 syscall args from the client's user pointer into a
kernel stack frame). The trap-2 handler (kekcall.c:140-231) then:

1. takes the caller's kernel thread pointer from the saved frame (tail[8]);
2. walks the process list from the caller's own proc to find `pid`
   (kekcall.c:158-184, using `kpeek64_checked` on `p_pid`/list links);
3. takes the target proc's **first thread** (`kpeek64_checked(proc+16, &regs[RDI])`,
   kekcall.c:185) and passes it as the syscall's `td` argument;
4. looks up `sysents[sysc_no].sy_call` (kekcall.c:189-205) and jumps to it with
   `handle_syscall(regs, 0)` (kekcall.c:209-224);
5. on completion, trap 3/4 copies the target thread's `td_retval` back into the caller's
   `td_retval` (kekcall.c:232-250), so the caller's getppid returns the remote syscall's
   result.

Why this cannot read kernel VAs: the executed function is an ordinary FreeBSD syscall
implementation. Every stock syscall interprets pointer arguments as **user** pointers of
the current thread's address space; there is no stock syscall that copies from an
arbitrary **kernel** VA to a user buffer (that would be a kernel-infoleak by design).
The only kernel-data read inside sub 5 is the hand-coded special case for
`sysarch(AMD64_GET_FSBASE)` (kekcall.c:190-218), which uses kstuff's private `kpeek64`
machinery — i.e. each such "read" had to be written inside kstuff. Since modifying or
extending kstuff is out of scope here, sub 5 yields no general kernel read.

Could a read be constructed from a process-memory syscall instead?

- `process_vm_readv` does not exist in this kernel base: no `SYS_process_vm_readv` in the
  SDK's target/include/sys/syscall.h (checked; the FreeBSD 11 table there has SYS_ptrace=26,
  SYS_mprotect=74, SYS_sysarch=165, SYS_kenv=390, SYS_getppid=39, no process_vm entry).
- `ptrace` (SYS_ptrace=26; mapped `ptrace ptrace` in kstuff lib/syscalls.txt:24) can read
  **another** process's userspace via PT_READ_D / PT_IO (`proc_rwmem`, SDK
  target/include/sys/ptrace.h:197). Two blockers: (a) `p_candebug` credentials — the
  ps5-payload-sdk's own `test_privileges` sample has to *raise its own ucred authid
  (0x4800000000010003) and caps before calling SYS_ptrace and restore them after*
  (samples/test_privileges/main.c:49-91), which is direct upstream evidence that plain
  credentials are insufficient; (b) with sub 5's forged `td` the candebug outcome is
  plausible but **unproven** — nothing in either tree exercises sub5+ptrace. For our
  actual geometry this is moot anyway: the PRX already runs inside the game process
  (ploader `inject` per PS5-MODDING-MISSION.md "THE DELIVERY STACK"), so reading game
  memory needs no cross-process syscall at all.

**Correction to the received context:** the claim "REMOTE_SYSCALL (sub 5) proven in
prosper0gdb" is not supported by the pinned source. A full-tree grep finds zero kekcall /
sub-5 client usage in prosper0gdb or gdb_stub; prosper0gdb's only getppid use is tracing
the syscall CFI table to find an int3 address (prosper0gdb/r0gdb.c:1569-1578). Its actual
kernel reads use completely different primitives (section 2). Sub 5 exists kernel-side
(kekcall.c:64-76/140-231) and its client ABI is demonstrated in-tree only by
debug-reader.c's generic wrapper — there is no in-tree demonstration of a sub-5 remote
syscall actually succeeding on console.

---

## 2. The loader route — precisely identified, and why it does not fit our use case

The handoff lead ("loader-supplied pipe/socket context and kernel_copyin/kernel_copyout
APIs") resolves to **two distinct public components**, both now pinned down at source
level.

### 2a. Which loader?

**The kernel-exploit ELF loader ("elfldr") that spawns the payload process.** In the
kstuff archive, the payload C runtime `lib/crt-elf-c.c:5-42` receives a
`struct specter_args { void* dlsym; int* pipe; int* rwpair; uint64_t kpipe_addr;
uint64_t kdata_base; int* retval; }` from the loader, builds a
`struct r0gdb_bootstrap { magic "R0GDBPIP"; int rwpipe[2]; uint64_t kpipe_addr; }`
(lib/r0gdb-bootstrap.h:6-13) from the pipe fds and kernel pipe address, and passes it as
the 4th `_start` argument, alongside the master/victim socket pair (`args->rwpair`)
( lib/crt-elf-c.c:32-41). prosper0gdb consumes it in `r0gdb_init` (r0gdb.c:1696-1711):
matching magic + kpipe_addr sets `inherited_pipe = 1`. The loader is referred to in-tree
as "elfldr" (r0gdb.c:1718-1725, offsets.c:31), and the kstuff README highlights exactly
this feature: "Reuse of the loader's existing kernel pipe by prosper0gdb"
(README.md, Highlights).

Separately, **ps5-payload-sdk (John Törnblom, GPL-3)** declares the API family in
`target/include/ps5/kernel.h:59-143` (license notice kernel.h:1-15): `kernel_copyin`,
`kernel_copyout`, `kernel_get/setlong…char`, `kernel_get_proc(_thread/_ucred/…)`,
`kernel_proc_copyin/copyout`, `kernel_get/set_vmem_protection`, `kernel_mprotect`,
`kernel_dynlib_*`, ucred/QA-flag accessors, plus exported absolute kernel pointers
(KERNEL_ADDRESS_ALLPROC etc., kernel.h:25-57). The project's own PRX harness documents
the matching payload-CRT feature set: "`__kernel_init` + kernel pipe primitives" and
"`kernel_copyin` + `kernel_copyout` + `kernel_write`"
(outputs/2026-10-07-l16-full-build/prx-candidate-l16-1/harness/tools/SharpProspero.Link/PayloadProfile.cs:29-36).
This is the "independently licensed public homebrew loader/SDK source" the handoff meant.

### 2b. What are kernel_copyin / kernel_copyout?

FreeBSD-semantics cross-boundary copies, with the *kernel* address as a plain integer:
`int32_t kernel_copyin(const void *udaddr, intptr_t kaddr, size_t len)` — write `len`
bytes from the caller's user buffer **into kernel VA kaddr**;
`int32_t kernel_copyout(intptr_t kaddr, void *udaddr, size_t len)` — read `len` bytes
**from kernel VA kaddr** into the caller's user buffer (kernel.h:61-62). I.e. copyout is
the read primitive #1 wants, and copyin is a kernel-write primitive.

How the equivalent is actually implemented in the pinned kstuff tree (prosper0gdb):

- `kwrite20(ptr, a, b, c)` — arbitrary 20-byte kernel write via the classic overlapping
  IPv6-socket pktopts trick: `setsockopt(master_fd, IPV6_PKTINFO, {ptr})` aims the
  destination, then `setsockopt(victim_fd, IPV6_PKTINFO, {a,b,c})` writes the 20-byte
  in6_pktinfo there (r0gdb.c:111-120).
- `copyout(dst, src, count)` — bulk kernel→user read: kwrite20 rewrites the loader
  pipe's `pipe_buffer` fields at `rpipe` and `rpipe+15` (size/offset pattern
  `0x4000000040000000`, then the target address encoded as `(src<<8)|0x40`), then a plain
  userspace `read(the_pipe[0], dst, count)` pulls kernel memory out (r0gdb.c:173-183).
- `copyin(dst, src, count)` — the reverse using `write(the_pipe[1], src, count)`
  (r0gdb.c:185-192).
- `kread8` without an inherited pipe uses getsockopt(IPV6_PKTINFO) on the corrupted
  socket to fetch 8 kernel bytes (r0gdb.c:84-109).

This confirms all three of the handoff's preliminary observations at line level: the
loader *constructs context* (crt-elf-c.c builds the bootstrap rather than forwarding raw
descriptors), nominal reads *manipulate pipe metadata* (r0gdb.c:175-178), and the
wrappers *do not expose reliable partial-I/O counts* (`read()`'s return is passed through
with no retry/partial accounting, r0gdb.c:182).

### 2c. Callable from another userspace process? — No

Two independent blockers, one per component:

1. **ps5-payload-sdk v0.43 ships no implementation.** No `kernel_*` function is exported
   by any library in the pinned copy: dynamic-symbol dumps of libkernel.so,
   libkernel_sys.so, libkernel_web.so show no kernel_copy*/get*/proc* exports (only
   `kenv` appears, and it is an *unresolved PLT import* in libkernel_sys.so — symbol at
   0x14054, section index 5, i.e. expected to be provided by the loading environment);
   no static archive in target/lib contains such members. The provider side lives in the
   upstream loader project and is simply not present in this archive. A declaration
   without a provider is not a callable route (consistent with ROUTE-ADDENDUM.md's
   refusal to treat declarations as services).
2. **The r0gdb/pipe implementation is bound to the exploit-spawned process.** The
   bootstrap exists only as startup arguments (fds + kernel addresses) of the one process
   the exploit loader spawned (crt-elf-c.c:5-42; r0gdb.c:1696-1711). Those fds and the
   kernel-write socket pair cannot migrate into the game process afterwards: the game is
   launched by the OS, not forked from the loader, and no SCM_RIGHTS/fd-passing channel
   exists between them. prosper0gdb's whole design (walking its own proc's file table to
   find `rpipe`, r0gdb.c:158-171) assumes it *is* the process that inherited the
   descriptors.

### 2d. What context/privileges does it need?

Exploit-session state, not ordinary process credentials: the kernel exploit's
arbitrary-write artifacts (the corrupted/overlapping master/victim socket pair), the
loader's kernel pipe whose kernel address is known, and the kernel data-base anchor
(specter_args: `rwpair`, `kpipe_addr`, `kdata_base`). These exist only during/after the
exploit chain run, in that session's payload process. The gdb_stub ring0 path is the
same story by other means — it needs `kexec` (privileged kernel function execution from
userspace) to install itself (gdb_stub/ring0.c:198-226), which is an exploit primitive,
not a service.

### 2e. Viable for reading game-process memory from the PRX? — No

The PRX lives inside the game process (ploader injects at game launch —
PS5-MODDING-MISSION.md "THE DELIVERY STACK"; the runtime already reads pool spans
in-process, GATE-1 section). The loader route's primitives belong to a *different*
process that exits between launch cycles (ploader "exits after each game launch cycle").
For #1 as defined (read-only kernel reads with no game running, as a repeatable logged
capability), the loader route could read kernel memory only inside a fresh exploit
session in that session's payload process — prosper0gdb proves that works — but that is
a session-local property of the jailbreak chain, not a standing callable provider, and
operationalizing it for our runtime would mean loading a new kernel-writing payload,
i.e. new privileged execution, outside the current constraints.

---

## 3. Alternatives assessment

### 3a. REMOTE_SYSCALL via kekcall (sub 5)

- Availability: pinned in the resident kstuff-lite build; callable from any PPR-sysentvec
  process via the debug-reader.c:23-46 ABI — including, in principle, the game PRX.
- #1 kernel reads: **insufficient** (section 1.2). Fixed-function reads that do exist:
  sub 3 RDMSR (kekcall.c:59-62), sub 1 dbgregs (kekcall.c:21-48); sub 6 snapshot returns
  ENOSYS in the default `KSTUFF_OBS=0` build (kekcall.c:77-90, Makefile:5).
- Cross-process userspace R/W via sub5+ptrace(PT_READ_D/PT_IO): mechanically plausible
  (forged `td`, retval plumbing kekcall.c:185-250) but candebug behavior is unproven and
  upstream evidence says plain creds fail (test_privileges elevates ucred first,
  samples/test_privileges/main.c:49-91). Moot for our in-process geometry.
- #3 code patching via sub5+mprotect(pid, …): since `sys_mprotect` operates on
  `td->td_proc`'s vm_map and sub 5 passes the target proc's first thread as `td`
  (kekcall.c:185-224), this is a candidate *cross-process* page-permission primitive.
  Unproven; would need one controlled console test under the standard rules. Not needed
  for the in-process case (see 3c).

### 3b. The loader route

Real, source-demonstrated kernel R/W — but session-local, wrong process, and its
adoption would add privileged execution. Not usable for either capability as we need
them (standing service, callable from the PRX). See section 2.

### 3c. Direct syscall from the game process (no kernel mediation)

- #1 game-process memory: the PRX is in-process; plain loads/stores already work for
  mapped spans (GATE-1 facts 1, mission). Kernel-mediated readouts with exact byte
  accounting are a separate engineering problem (the copyin-oracle short-write issue is
  file-write-path behavior, not a missing kernel provider).
- #1 kernel VA reads: no direct syscall exists. The one unexplored candidate is Sony's
  own `test_debug_rwmem` — `__sys_test_debug_rwmem` is a real PS5 syscall with a libc
  wrapper exported by all three SDK libkernel flavors (lib/syscalls.txt:428;
  objdump -T of libkernel*.so). No prototype is declared anywhere in the pinned SDK copy,
  and its privilege gates (debugger/dev-mode) are unknown from these sources. It is a
  legitimate research item but cannot be assessed Mac-only beyond this, and probing it is
  a console action requiring authorization.
- #3 code patching: **viable now with zero kernel changes.** kstuff's resident
  syscall-fix interception hooks mprotect (and mdbg_call) globally — uelf/main.c:87-93
  (`IS(mprotect) || IS_PPR(mdbg_call)` → `handle_syscall_fix`) — and arms debug
  registers so that when kernel mprotect reaches its validation block, the trap handler
  skips it: on trap at `mprotect_fix_start`, `regs[RIP] = mprotect_fix_end`
  (uelf/syscall_fixes.c:8-11, 63-80). That is precisely a resident, permission-check
  bypass for mprotect, already in the pinned unmodified build. The userspace recipe with
  in-tree precedent: page-align, `mprotect(page, PROT_READ|PROT_WRITE|PROT_EXEC)`,
  write, verify, restore protection (gdb_stub/dbg.c:393-396 `mprotect_byte` and
  dbg.c:427-448 write/retry path; r0gdb.h:66 documents the semantics as "mprotect with
  disabled permission checks"). Preconditions/risks: kstuff must actually be resident and
  not paused at test time (mission log: current state "inferred, not directly
  measured"); the calling process must be PPR-sysentvec for the interception to engage
  the same way it does for kekcall (main.c:64-65); and the mission's clean-revert,
  one-launch and three-restart disciplines apply. This route adds no kernel-resident
  code and modifies nothing.

---

## 4. Verdict and exact next steps

### #1 — read-only kernel reads

**Verdict: blocked under current constraints; no public standing provider exists in the
pinned sources.**

- kekcall (verified): no generic kernel-copy op; only MSR (sub 3), dbgregs (sub 1),
  FSBASE special case, and a snapshot sub that is ENOSYS in the default build.
- Loader route (identified): real kernel reads exist (r0gdb pipe/socket primitives,
  ps5-payload-sdk kernel_copyout API) but only inside the exploit-spawned process; no
  implementation ships in the pinned SDK copy; not callable from the PRX; not a standing
  service.
- Sub 5 cannot construct a kernel-VA read (no stock syscall does that; process_vm_readv
  absent; ptrace needs credentials the PRX does not have and upstream elevates via
  kernel RPC it lacks).

Next steps, in order:

1. (Mac-only, now) Freeze this verdict and the corrected sub-5/prosper0gdb record into
   the component handoff so no future worker repeats the "proven in prosper0gdb" claim.
2. (Mac-only, now) Specify the acceptance contract for the anticipated kstuff-lite
   extension — a new bounded read sub-op in `handle_kekcall` (distinct sub number,
   explicit length cap, EFAULT/partial semantics, CHECK-gated), per the mission's own
   Phase B rule that extension is the sanctioned rung once kekcall is shown
   insufficient. This document supplies the "shown insufficient" half.
3. (User-gated, not Mac-only) If and when the user authorizes Phase B kernel work: build
   the extended kstuff-lite under its rules — new filename (never overwrite
   `kstuff-lite_v1.11.elf`), written recovery path before first test, read-only op
   first, Astro Bot/Stellar Blade/Townfall regression launches, three clean passes
   across three separate restarts, one console owner, one test per boot.
4. (Optional research, needs authorization) `test_debug_rwmem` signature/gate discovery
   from public sources or a controlled console probe — the only remaining direct-syscall
   candidate found.

### #3 — code patching

**Verdict: viable without any new kernel-resident code, via the already-resident kstuff
mprotect fix.**

1. (Mac-only, now) Write the PRX-side patch recipe with revert: page-aligned
   mprotect(RWX) → compare-before-write → write → readback verify → mprotect back to the
   original protection → log every step; refuse on any partial/unexpected result.
2. (Console, user-gated) Precondition proof before the first live test: kstuff resident
   and not paused, target page ownership and original protection recorded, rollback
   artifact prepared — then the mission's "harmless in-game patch + clean revert" rung
   under one user-launched test.
3. Fallback only if the fixed mprotect still fails (e.g. EACCES on SELF-mapped text):
   sub 5 remote mprotect against the game pid (section 3a) as a *hypothesis* requiring
   its own single controlled test; do not stack unproven primitives.
4. Not routes for #3: loader pipe writes (wrong process), ring0/kexec installation
   (privileged execution, out of scope).

### Record hygiene

- This investigation made no console contact, no builds, no uploads, no kernel work, and
  adds no client code. The only artifacts written are this verdict (component dir and
  outputs/cap01-03/), the worktree-root agent report, and a read-only source extraction
  under /tmp.
- Licensing note for any future integration: ps5-payload-sdk files carry GPL-3 notices
  (kernel.h:1-15); the kstuff-lite archive contains no license member
  (SOURCE-AUDIT.json `license_status: NOT_FOUND_IN_ARCHIVE`), so any kstuff-derived code
  integration still needs upstream license verification before copying source.
