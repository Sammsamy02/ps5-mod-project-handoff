# PROVIDER ROUTE VERDICT V2 — user-corrected

Agent JF, 2026-10-13. Mac-only correction + design pass over agent JC's
`PROVIDER-ROUTE-VERDICT.md` (V1, same date, retained unchanged on record at
`caps-team-work/cap01-03/PROVIDER-ROUTE-VERDICT.md` and `outputs/cap01-03/`).
No builds, no console or kernel contact, no client or exploit code. Every
claim about the existing sources cites the file and lines actually read; all
V1 line-level citations below were independently re-verified against the same
pinned extraction (`/tmp/kb-src-jc/kstuff-lite-1.11/`, archive
`what/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`, SHA256
`b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6` per
SOURCE-AUDIT.json).

## 0. The four corrections applied (user review, binding)

| # | V1 said | Corrected to | Why (source-level) |
|---|---|---|---|
| 1 | #1 "blocked… no public standing provider exists" | **NO VERIFIED APPLICABLE PROVIDER IN THE EXAMINED STACK** — not a claim that none exists anywhere; and the #1 acceptance requirement ("no game running") does not require a service inside the game PRX | Section 2 below |
| 2 | #3 "VIABLE now" via mprotect | **SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR UNVERIFIED** — the dbg.c precedent is breakpoint-specific, ignores mprotect's return value, and restores nothing | Section 3 below |
| 3 | `__sys_test_debug_rwmem` "legitimate research item" with privilege speculation | **UNEXPLORED — name only.** The name/export establishes nothing about signature, privileges, or kernel-vs-process memory | Section 5 below |
| 4 | kekcall insufficiency → extension "is the sanctioned next rung" | Kekcall insufficiency **establishes a technical gap only; it lifts no restriction.** Phase B rules still fully apply; authorization remains the user's | Section 4 below |

Corrected verdict summary (replaces the V1 table):

| Route | #1 kernel reads | #3 code patching |
|---|---|---|
| kekcall REMOTE_SYSCALL (sub 5) | Not sufficient for kernel-VA reads (technical finding stands, kekcall.c:21-107) | Indirect fallback hypothesis only; unproven, do not stack |
| Loader/pipe route (kernel_copyin/out) | Real but exploit-session-local; no provider in the pinned SDK copy; not a standing service | Not usable at game time |
| Direct syscall from game process | No kernel-read syscall exists in the examined stack; `__sys_test_debug_rwmem` unexplored (name only) | SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR UNVERIFIED — resident mprotect fix is real in source; every target-side property is untested |
| **Separate no-game probe** (new row; correct frame for #1 acceptance) | The natural shape of #1 acceptance: standalone payload, no game PRX involved — but it still needs a kernel-read primitive, and none is verified in the examined stack | Not applicable (#3 acceptance is explicitly in-game) |

---

## 1. What stands from V1 (verified, unchanged)

The following V1 findings were re-verified line-by-line and remain the factual
base. Full detail is in V1 sections 1-2; condensed here with the binding
citations.

1. **kekcall exposes no generic kernel-copy op.** `handle_kekcall`
   (ps5-kstuff/uelf/kekcall.c:19-107) dispatches exactly: sub 1 READ debug
   registers via kernel `copyout` (kekcall.c:21-48); sub 2 WRITE debug
   registers via `copyin` (49-58); sub 3 RDMSR (59-62); sub 4 reserved,
   unimplemented (63); sub 5 REMOTE_SYSCALL (64-76, 140-231); sub 6
   shared-area snapshot, `ENOSYS` in the default `KSTUFF_OBS ?= 0` build
   (77-90; ps5-kstuff/Makefile:5); sub 7 PPR plaintext request (91-101);
   sub 0xffffffff CHECK liveness, returns 0 (102-106); anything else
   `ENOSYS` (107). The private helpers (`copy_from_kernel`,
   `copy_to_kernel`, `kpeek64_checked`, `kpoke64`, `push_stack_checked`;
   uelf/utils.h:28-29, 78, 91, 96) are not reachable through any sub-op.
2. **Sub 5 cannot construct a kernel-VA read.** It executes a stock syscall
   implementation against a target pid (kekcall.c:158-231); stock syscalls
   interpret pointers as user pointers; no stock syscall copies from an
   arbitrary kernel VA; `process_vm_readv` is absent from the SDK syscall
   table; ptrace needs credentials the PRX does not hold (upstream
   test_privileges raises ucred first, ps5-payload-sdk
   samples/test_privileges/main.c:49-91). The V1 correction also stands: sub
   5 is NOT "proven in prosper0gdb" — zero client call sites exist in-tree;
   prosper0gdb's getppid use is int3-table tracing only
   (prosper0gdb/r0gdb.c:1569-1578).
3. **The loader/pipe route is real but session- and process-bound.** The
   exploit loader ("elfldr") passes `specter_args` (pipe fds, kpipe_addr,
   kdata_base) to the one payload process it spawns (lib/crt-elf-c.c:5-42;
   prosper0gdb/r0gdb.c:1696-1711). prosper0gdb's kernel R/W rides corrupted
   IPv6 socket pktoptions (`kwrite20`, r0gdb.c:111-120) to rewrite the
   loader pipe's kernel `pipe_buffer` (`copyout`/`copyin`, r0gdb.c:173-192).
   ps5-payload-sdk v0.43 declares `kernel_copyin`/`kernel_copyout`
   (target/include/ps5/kernel.h:61-62) but ships no implementation in the
   pinned copy — a declaration without a provider is not a callable route.
4. **kstuff's resident mprotect fix is genuinely in the pinned, unmodified
   build.** `handle_syscall` intercepts `IS(mprotect) || IS_PPR(mdbg_call)`
   unconditionally — not gated by `allow_kekcall` — and covers both the PPR
   and PS4-compat sysents (`IS` macro, uelf/main.c:56; interception at
   main.c:87-93). It arms debug registers (dr7 0x405) so that when kernel
   mprotect reaches its validation block, a trap at `mprotect_fix_start`
   rewinds RIP to `mprotect_fix_end` (uelf/syscall_fixes.c:8-11, 63-80; trap
   routing main.c:319-327). That is a resident permission-check **skip** for
   mprotect — source fact, distinct from any claim about target behavior
   (section 3).
5. **Client ABI for kekcall is source-demonstrated** (syscall 39 with
   sub-op in RAX high bits; inline-asm wrapper at
   ps5-kstuff/debug-reader.c:23-46; constants at 8cc-kekcalls.c:59-61 and
   debug-reader.c:9-10), gated on `allow_kekcall && IS_PPR(getppid)`
   (main.c:65) — i.e. the caller must land on the PPR sysent (PS4-compat
   sysentvec distinguished at main.c:361-364).

---

## 2. #1 acceptance requirement analysis — "no game running"

### 2a. The original requirement

PS5-MODDING-MISSION.md, PHASE B test order (lines 329-330):

> "1. **#1 — read-only kernel reads:** no game running; several reboots
> without a panic, with the required three logged console passes."

Capability definition: #1 = "Kernel safe reads" (HANDOFF-GPT-ASTRA.md,
CAPABILITIES 1-8 table). Acceptance elements: (i) read-only kernel reads,
(ii) demonstrated with **no game running**, (iii) several reboots without a
panic, (iv) three logged console passes (mission lines 280-282), one test
per boot.

### 2b. Does this require a service inside the game PRX? — No

The acceptance text never mentions the game PRX, and "no game running" points
the opposite way: the game PRX only executes when a game IS running (ploader
injects at game launch; mission "THE DELIVERY STACK"). V1 evaluated every
route against a "callable from the PRX / standing service" criterion that V1
itself introduced (V1 sections 2e, 3a). That criterion is stricter than the
acceptance requirement and, for #1, misdirected. The PRX-callability analysis
remains useful for #3 (explicitly in-game) and any future in-game kernel-read
needs — but it must not silently redefine #1's acceptance.

### 2c. What a separate authorized probe looks like

A standalone payload ELF in the same class as `ploader.elf` /
`debug-reader`, loaded via pldmgr `/loadpayload` with **no game launched**
(dispatch mechanism per mission line 147), that:

1. runs in the payload-daemon process context;
2. performs bounded, read-only kernel-VA reads against targets with
   independently known expected values (HANDOFF.md acceptance rule:
   "genuine provider reads with independent known answers and exact byte
   counts");
3. logs byte-exact results and byte counts to its own NEW log file;
4. exits cleanly without panicking;
5. repeats across three separate reboots — three logged passes, one test
   per boot.

No game launch is involved, so the one-user-launch-per-test discipline that
binds in-game tests does not even arise; loading the probe each boot still
requires the user's explicit OK, and the "no panic across several reboots"
element is part of the pass criteria.

### 2d. How this changes the #1 assessment

The right question for #1 acceptance is: **"is there a callable kernel-read
primitive from any no-game process context we are allowed to use?"** — not
"from the game PRX." Re-assessed under that question:

- **kekcall**: still insufficient in the examined stack (section 1.1-1.2).
  A standalone probe could call the kekcall ABI (debug-reader.c:23-46), but
  no sub-op returns arbitrary kernel VAs. Whether the payload-daemon process
  even lands on the hooked PPR getppid path is itself an unverified
  console property (main.c:65, 361-364).
- **The probe running prosper0gdb's own primitives**: in a fresh exploit
  session, with no game running, prosper0gdb demonstrably reads kernel
  memory (r0gdb.c:111-192) — this is exactly the "loader route" and it does
  not touch any game. But it requires running the jailbreak chain and a
  kernel-writing primitive (kwrite20 corrupts kernel socket state) — new
  privileged execution, gated on explicit user authorization, and a
  session-local property of the exploit chain rather than a repeatable
  resident capability. Recorded as an authorized-probe *option*, not a
  verified provider; it changes nothing about the examined-stack verdict.
- **A probe using Byepervisor**: Byepervisor is NOT part of the examined
  stack. Its only in-tree presence is two source-comment URLs
  (ps5-kstuff/uelf/npdrm.h:1, npdrm.c:1, pointing at PS5Dev/Byepervisor
  fpkg sources). Adopting it means loading a different, new kernel-resident
  framework — squarely Phase-B-gated new kernel code with its own
  firmware-offset risk profile (the exact risk class the kstuffSharp
  quarantine exists for). It does not change the examined-stack verdict; it
  adds a candidate family that would need its own authorized evaluation.
- **`__sys_test_debug_rwmem`**: unexplored, name only (section 5).

**Corrected #1 verdict: NO VERIFIED APPLICABLE PROVIDER IN THE EXAMINED
STACK.** This is a scoped statement about the pinned kstuff-lite v1.11
archive + ps5-payload-sdk v0.43 copy (plus the components they reference in
 comments); it is not a proof that no provider exists anywhere, and the
solution space for #1 explicitly includes non-PRX shapes. The cleanest
sanctioned path remains the Phase B kstuff-lite extension — a bounded,
read-only kekcall sub-op — consumed by a standalone no-game probe; note this
does not make it authorized (section 4). Also note the mission's own
gate-order prerequisite still stands: Phase B begins "only AFTER the
Townfall Item Spawner successfully spawns an item" (mission lines 273-275).

---

## 3. #3 corrected verdict — SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR UNVERIFIED

### 3a. What the dbg.c precedent actually is (and is not)

V1 cited `gdb_stub/dbg.c` as "in-tree precedent" for the userspace recipe.
Re-examined, the precedent is much weaker than "viable":

1. **Breakpoint-specific, not a general code-patching mechanism.** The only
   call site of `mprotect_byte` is the fallback branch of `write_mem`,
   taken only when `errno == EFAULT && sz == 1 && buf[0] == 0xcc` — i.e.,
   only for single-byte 0xCC **software-breakpoint** installs
   (dbg.c:427-448, condition at 435, call at 437). No in-tree code uses it
   for an arbitrary patch.
2. **Ignores mprotect's return value.** `mprotect_byte`
   (dbg.c:393-396) is `static void` and discards the return; if the fixed
   mprotect failed (EACCES/EINVAL/ENOMEM), the caller proceeds to the
   retrying `pk_read` regardless and infers success only from the byte
   read-back (dbg.c:438-439). The same unchecked pattern repeats at
   `mprotect_rwx` (dbg.c:265-272) and gdb_stub/strace.c:164.
3. **No protection-restoration proof — in fact, no restoration at all.**
   After `mprotect_byte`, the page is simply left RWX. A full-tree search
   of gdb_stub finds no second mprotect restoring prior permissions
   anywhere on this path. The comment "mprotect with disabled permission
   checks" (prosper0gdb/r0gdb.h:65-66, for `mprotect20`) documents intent,
   not restoration.
4. **Contrast inside the same tree:** prosper0gdb's `mprotect20`
   (r0gdb.c:962-975) DOES capture and return mprotect's result — but it
   implements the check-skip via prosper0gdb's own ptrace-trace mechanism
   (`fix_mprotect`, r0gdb.c:955-960; `set_trace`), not kstuff's resident
   fix, and it has **zero in-tree callers** (declaration r0gdb.h:66;
   no call sites found). So the tree contains no demonstrated, general,
   return-checked, restoring mprotect patch sequence at all.

Therefore: source precedent supports only that (a) a fixed/unchecked
mprotect-to-RWX-then-write sequence was plausible enough for its author to
ship in a breakpoint fallback, and (b) kstuff's resident permission-check
skip exists in the pinned build (section 1.4). Everything about how a real
patch target behaves is unverified.

### 3b. Design — PRX-side in-process code patch via the resident mprotect fix

PROPOSAL ONLY. Not built, not compiled, not tested. No console action is
authorized by this section. Phase B #3 acceptance wording: "harmless
in-game patch: demonstrate the patch and a clean revert" (PS5-MODDING-
MISSION.md, PHASE B test order, lines 331-332).

**Preconditions (all logged before any patch; refusal on any unknown):**

- P1 — kstuff resident and not paused at test time. Current state is
  "inferred, not directly measured" (mission log). Preferred probe: kekcall
  CHECK sub-op `0xffffffff00000027` (kekcall.c:102-106; client constant
  debug-reader.c:10) via the debug-reader.c:23-46 ABI. If the probe cannot
  be issued or returns anything other than 0, ABORT. (Whether CHECK is
  reachable from the game process is itself U1 below — a failed probe must
  abort, never be retried blindly.)
- P2 — target span ownership and protection recorded in-process via
  sceKernelVirtualQuery / sceKernelQueryMemoryProtection (mission l12
  upgrade C.1 names these as the PRIMARY coverage proof, mission lines
  255-257). Record mapping, extent, protection for the page-aligned span
  BEFORE anything else.
- P3 — original bytes captured (compare-before-write, HANDOFF.md rule).
- P4 — rollback artifact prepared and written down before the first test:
  original bytes + original protection + restore procedure. Refuse to patch
  if rollback cannot be guaranteed (Phase B rule: recovery path before
  first test).

**Protocol (every return value checked; any unexpected result → abort):**

1. PRE: log P2 query result + first 16 bytes of the span (hex, capped).
2. LIVENESS: kekcall CHECK (P1). rc != 0 or unreachable → ABORT.
3. OPEN: `mprotect(page_base, page_len, PROT_READ|PROT_WRITE|PROT_EXEC)`.
   **Check the return value** (the dbg.c precedent does not — this deviation
   is deliberate). rc != 0 → log errno, ABORT (nothing was changed).
4. VERIFY-OPEN: re-query the span (P2 API). Log observed protection. If the
   span is not reported writable → go to REVERT-ONLY (U3 covers query
   fidelity; never write on an unverified open).
5. WRITE: write the new bytes; then RE-READ the span and byte-compare
   against the intended content. Any mismatch → REVERT-ONLY.
6. EXECUTION BARRIER: do not let any other thread execute the patched span
   until step 5's readback has passed. If other threads may be executing
   the span, hold them off (the acceptance rung is a "harmless" patch —
   pick a target that is quiesced) and serialize before resuming. No
   in-tree precedent exists for this on PS5 (U4); treat as a design
   requirement, not a verified property.
7. REVERT (demonstrates the "clean revert" half of acceptance):
   a. rewrite the ORIGINAL bytes using the same OPEN/VERIFY/WRITE sequence;
   b. `mprotect(page_base, page_len, original_prot)` — **check the return
      value**;
   c. RE-QUERY the span and REQUIRE the reported protection to equal the
      recorded original — this is the restoration proof the dbg.c precedent
      never supplies. Mismatch → one retry, then log RESTORE-FAILED, keep
      the failure state visible, refuse all further patches this boot.
8. LOG CONTRACT: one line per step (step id, rc, errno, capped hex bytes,
   before/after protections). PASS only if every recorded rc matches
   expectation; partial results are recorded as partial, never as success
   (HANDOFF.md acceptance rules).

**Refusal conditions (abort list):** CHECK probe fails or unreachable;
OPEN rc != 0; VERIFY-OPEN still non-writable; readback mismatch; REVERT
mprotect rc != 0; final protection query != recorded original; any
partial/ambiguous step. Mission discipline throughout: stop on ambiguity,
never repeat an ambiguous write hoping it succeeds.

### 3c. Unverified register (each needs its own authorized console test)

- **U1 — Interception engagement from the game PRX.** The fix covers both
  sysents (main.c:56, 87-93), but whether the game process's mprotect lands
  on the hooked entry at test time — kstuff resident, not paused (ShadowMount
  pause semantics), correct sysentvec — is a console property, not a source
  property.
- **U2 — Does mprotect SUCCEED on SELF-mapped game text even with checks
  skipped?** The fix skips the permission-validation block
  (syscall_fixes.c:68-80); it does not force the underlying vm_map operation
  to succeed. File-backed/read-only text mappings may still refuse or COW.
  Entirely unverified for the game-PRX case.
- **U3 — Query fidelity.** Whether sceKernelVirtualQuery /
  sceKernelQueryMemoryProtection faithfully reflects a protection change
  made through the fixed mprotect path (needed for VERIFY-OPEN and the
  restoration proof).
- **U4 — Patched-byte visibility to executing cores.** x86-64 is
  architecturally coherent for the patching core, but cross-modifying code
  (another core executing the span mid-patch) requires the cross-modifying
  protocol; no in-tree PS5 code demonstrates either. Design requires the
  step-6 barrier; its sufficiency is unverified.
- **U5 — Does restoration stick?** No source evidence exists anywhere in
  the tree that a page re-protected to its original permissions after an
  RWX patch remains correct over time or across subsequent operations.
- **U6 — kstuff pause/resume state at test time** (folds into U1; listed
  separately because it is operational, not structural).

**Corrected #3 verdict: SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR
UNVERIFIED.** The candidate rests on real, pinned source (the resident
mprotect permission-check skip, section 1.4); every property of the actual target
behavior — U1 through U6 — is unproven, and the only in-tree userspace
precedent (dbg.c) is breakpoint-specific, return-unchecked, and
non-restoring.

---

## 4. Kekcall insufficiency — a gap finding, not an authorization

V1's bottom line described the kstuff-lite extension as "the sanctioned next
rung," which reads as if demonstrating kekcall insufficiency itself advances
the extension. Corrected: the insufficiency finding (section 1.1-1.2) is a
**technical gap statement only**. Per the mission's Phase B rule
(PS5-MODDING-MISSION.md:309-311: "First determine whether kstuff-lite's
existing kekcall interface suffices for kernel reads. Only if it does not,
extend…"), the gap is the *precondition* for considering an extension — it
does not lift any restriction, and all Phase B rules continue to apply in
full:

- Phase B has not started: "Begin only AFTER the Townfall Item Spawner
  successfully spawns an item" (mission lines 273-275).
- Every modified-kstuff rule binds: never overwrite `kstuff-lite_v1.11.elf`
  (distinct filename, mission line 315); recovery path proven and written
  down before the first test, FTP must still come up (mission lines 317-320);
  read-only additions first, writes only after reads are console-proven
  (lines 321-322); Astro Bot / Stellar Blade / Townfall regression launches
  after any kstuff change (lines 323-325).
- Session rules: three logged console passes across three separate reboots
  (lines 280-282), one capability per session with a written pass/fail and
  revert path (lines 283-285), one console owner, one test per boot.
- Nothing changes on the console without the user's explicit OK for that
  step (lines 288-291).

This document supplies only the "kekcall shown insufficient" half of the
Phase B precondition — for the examined stack — and authorizes nothing.

---

## 5. `__sys_test_debug_rwmem` — UNEXPLORED (name only)

Recorded as an unexplored item. Mac-only verifiable facts:

- The syscall-name mapping table lists `test_debug_rwmem` →
  `__sys_test_debug_rwmem` (kstuff-lite lib/syscalls.txt:428).
- A symbol `__sys_test_debug_rwmem` is exported by every pinned SDK stub
  flavor: libkernel.so, libkernel_sys.so, libkernel_web.so, and
  libc_stub_weak.so (objdump -T of the pinned ps5-payload-sdk v0.43 copy).
- No prototype for it exists anywhere in the pinned SDK headers.

The name and export establish **nothing** about: its signature, argument
order, or length limits; its privilege gates (debugger-attached? dev-mode?
none?); whether "rwmem" accesses kernel memory, the calling process's
memory, or another process's memory; or its error semantics. Any of those
readings is consistent with the name. It is therefore NOT counted as a #1
candidate beyond "unexplored name," and probing it is a console action
requiring authorization. Signature/gate discovery from public sources
remains a legitimate Mac-only research item if the user directs it.

---

## 6. Record hygiene

- Mac-only. No builds, no console or kernel contact, no uploads, no client,
  engine or exploit code, no modified kstuff. Artifacts written by this
  pass: this V2 verdict (outputs/cap01-03/ and caps-team-work/cap01-03/) and
  the agent report at the worktree root. Source was read from JC's existing
  read-only extraction; nothing was re-extracted or modified.
- V1 remains on record unchanged (PROVIDER-ROUTE-VERDICT.md); its route
  analysis (sections 1-2) is retained and re-verified, its conclusions are
  superseded by this V2 where they conflict.
- Licensing unchanged from V1: ps5-payload-sdk files carry GPL-3 notices
  (kernel.h:1-15); the kstuff-lite archive has no license member
  (SOURCE-AUDIT.json `license_status: NOT_FOUND_IN_ARCHIVE`); no source was
  copied.
