# CAP03 DESIGN — Capability #3: in-process code patching

Status: **PROPOSAL ONLY.** Authored 2026-10-13 (agent JJ, Mac-only pass).
Not built, not compiled on the target toolchain, not loaded, not run on a
console. No console or kernel contact, no client or exploit code, no
modified kstuff. Console execution requires the explicit per-step user OK
and the 3-restart discipline (AGENTS.md PS5 rules; PS5-MODDING-MISSION.md
PHASE B).

Implemented verdict (cap01-03/PROVIDER-ROUTE-VERDICT-V2.md section 3):
**"SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR UNVERIFIED."** The kstuff
resident mprotect permission-check skip is real pinned source; every
property of actual target behavior is U1-U6 and unproven. This design
turns that verdict into a checkable protocol and a contained diagnostic —
it does not convert the candidate into something verified.

Artifacts in this directory:

| File | Role |
|---|---|
| `DESIGN.md` | this document |
| `src/CodePatch.hpp/.cpp` | the protocol implementation (`patch_code`, `unpatch_code`, `patch_code_ex`, `unpatch_code_ex`, `PatchScope` RAII) |
| `src/PatchDiagnostic.cpp` | standalone no-game probe; patches its OWN text; reports U1-U6 |
| `TEST-PLAN.md` | pre-console + 4 console rungs (diagnostic, restoration, 3-restart law, in-game) |
| `SAFETY.md` | failure-mode analysis (silent mprotect, partial write, restore failure, pause state, launch law) |

---

## 1. Engine facts the design relies on (pinned source, read 2026-10-13)

All citations re-verified today against the pinned extraction
`/tmp/kb-src-jc/kstuff-lite-1.11/` (kstuff-lite v1.11,
SHA256 `b4fcf169…47ddf6` per cap01-03/SOURCE-AUDIT.json) and the pinned
ps5-payload-sdk v0.43 copy at
`what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/`.

1. **The resident skip is unconditional and covers both sysents.**
   `handle_syscall` intercepts `IS(mprotect) || IS_PPR(mdbg_call)`
   unconditionally — not gated on `allow_kekcall` — and dispatches
   `handle_syscall_fix` (ps5-kstuff/uelf/main.c:87-93; `IS` macro at
   main.c:56 covers PPR and PS4-compat sysents). `SYS_mdbg_call` = 573
   (uelf/syscall_fixes.h:4); plain `mprotect` is syscall 74 in both
   tables (SDK `sys/syscall.h:82`).

2. **The skip is a debug-register trap, not a vm_map override.**
   `handle_syscall_fix` starts the syscall with debug registers armed at
   `mprotect_fix_start`, dr7 `0x405` (uelf/syscall_fixes.c:8-14, 66-68);
   the trap handler rewinds RIP `mprotect_fix_start -> mprotect_fix_end`
   (syscall_fixes.c:70-80). Net effect: kernel mprotect's
   permission-validation block is skipped. **It does not force the
   underlying vm_map operation to succeed** — that is U2.

3. **The weak precedent is weak.** `gdb_stub/dbg.c:393-396`
   (`mprotect_byte`) is `static void`, discards mprotect's return value,
   is reached only for single-byte 0xCC breakpoint installs
   (dbg.c:427-448, condition at 435, call at 437), and never restores
   the page — it stays RWX. `mprotect_rwx` (dbg.c:263-272) is the same
   unchecked pattern, self-image only, BLOB-gated.

4. **The better precedent captures the rc but has no callers.**
   prosper0gdb's `mprotect20` (prosper0gdb/r0gdb.c:962-975) returns
   mprotect's result, but implements its check-skip via prosper0gdb's own
   ptrace-trace mechanism (`fix_mprotect`, r0gdb.c:955-960), not kstuff's
   resident fix, and has zero in-tree call sites (decl at r0gdb.h:65-66).

5. **Liveness probe ABI is source-demonstrated.** kekcall rides syscall
   39 (getppid) with the sub-op in RAX's high bits
   (ps5-kstuff/debug-reader.c:23-46); CHECK sub-op
   `0xffffffff00000027` returns 0 when alive
   (debug-reader.c:15; uelf/kekcall.c:102-106); gated on
   `allow_kekcall && IS_PPR(getppid)` (main.c:65) — the caller must land
   on the hooked PPR sysent. That landing is exactly U1 from any given
   process context.

6. **Query APIs exist and are exported.** The pinned libkernel.so exports
   `sceKernelQueryMemoryProtection`, `sceKernelVirtualQuery`,
   `sceKernelVirtualQueryAll`, `mprotect` (nm -D verified); the name map
   lists `query_memory_protection sceKernelQueryMemoryProtection`,
   `virtual_query sceKernelVirtualQuery` (kstuff lib/syscalls.txt:356,
   381). Neither is declared by the v0.43 SDK headers — this design
   declares them locally with public PS4-family signatures, flagged U3.
   `mdbg_call` is NOT exported by name; its raw number 573 is
   tree-verified but its signature is not — this design does NOT use it
   (plain `mprotect` 74 is the intercepted entry we want to exercise).

---

## 2. Preconditions (all logged before any patch; refusal on any unknown)

Same as verdict section 3b, now mapped to code:

- **P1 — liveness.** `kekcall_check()` (CodePatch.cpp, exact
  debug-reader.c:23-46 ABI) must return 0. Default for `patch_code()`
  via `kFlagRequireLiveness`. A failed probe aborts and is never
  retried blindly.
- **P2 — ownership + protection recorded in-process.**
  `query_span_protection()` per page of the aligned span before anything
  is modified, corroborated by `query_span_protection_vq()` (non-fatal,
  logged).
- **P3 — original bytes captured.** Compare-before-write everywhere:
  pre-OPEN capture into `PatchBackup`; unpatch refuses to write unless
  current bytes equal the recorded replacement AND the caller's
  `original` matches the module's capture.
- **P4 — rollback artifact prepared before the first test.** The
  `PatchBackup` record (original bytes + original per-page protection +
  restore procedure) exists in memory before OPEN; TEST-PLAN.md writes
  the on-disk rollback procedure down before any console rung.

## 3. The protocol (every return value checked)

Implemented in `patch_code_ex` / `unpatch_code_ex`. Each step emits one
contiguous log line (step id, rc, errno, protections, capped hex bytes —
LOG CONTRACT below). U-items are annotated where they bite.

| Step | Action | Failure → | U-item |
|---|---|---|---|
| 0 ARG | null/length/flag validation (`1..64` bytes, `kMaxPatchBytes`) | `kBadArgs/kTooLarge/kBadFlags` (nothing touched) | — |
| 1 ALIGN | span page-aligned (PS5 16 KiB via `machine/param.h` `PAGE_SHIFT 14` under `__SCE__`) | internal | backing granularity unverified — see U3 note |
| 2 PRE (P2,P3) | per-page `sceKernelQueryMemoryProtection`; corroboration via `sceKernelVirtualQuery` with bracket sanity; log original bytes (hex, capped 16) | `kQueryFailed`; mixed protections across span pages → `kMixedProtection` (single restore value required) | **U3** — whether query APIs faithfully report state incl. our later change; PS5 struct layout of VirtualQuery info unverified (a wrong layout shows as bracket failure, never passes garbage) |
| 2b LIVENESS (P1) | `kekcall_check()` when `kFlagRequireLiveness` | `kLivenessFailed` — abort, no retry | **U1/U6** — engagement + pause state from the calling process |
| 3 OPEN | `mprotect(page_base, span_len, R\|W\|X)` — **rc CHECKED** (the deliberate dbg.c deviation) | `kMprotectFailed` (nothing changed yet; errno logged) | **U2** — skip covers the permission check, not vm_map success; SELF/file-backed text may still refuse or COW |
| 4 VERIFY-OPEN | re-query every page; require W\|R reported; VirtualQuery disagreement on writability counts as failure | `kVerifyOpenFailed` → **REVERT-ONLY** (never write on an unverified open) | **U3** |
| 5 WRITE | `memcpy` + seq-cst fence + byte-compare readback against intended content; already-equal target refused as `kAlreadyApplied` (no ambiguous double-write) | `kReadbackMismatch` → **REVERT-ONLY** | **U4** — caller must hold other threads off the span until this readback passed (logged BARRIER step; not enforceable in-process) |
| 6 PUBLISH | fill/validate `PatchBackup`; publish module slot for `unpatch_code()` | — | **U5** precondition: the record restoration needs |
| 7 (unpatch) OPEN/VERIFY/WRITE | same rc-checked path, writing the ORIGINAL bytes | as above → REVERT-ONLY | U2/U3 again |
| 8 RESTORE (7b) | per-page `mprotect(page, PAGE_SIZE, orig_prot[i])` — **rc CHECKED**; retry budget: NONE for rc failures (terminal) | `kRestoreMprotectFailed` — STATE DIRTY, loud | **U5** |
| 9 RESTORE-PROOF (7c) | re-query every page; REQUIRE equality with recorded original; **one retry, mismatch only**; query failure is terminal | `kRestoreVerifyFailed` / `kRestoreQueryFailed` — STATE DIRTY, loud, refuse further patches this boot | **U5** — this is the proof dbg.c never supplies |

REVERT-ONLY (used after OPEN from any mid-protocol failure): rewrite
original bytes + readback (page still open) → per-page restore mprotect
(rc checked) → re-query proof. The primary failure is returned
unchanged; revert problems get dedicated loud `STATE DIRTY` lines so a
half-applied patch can never masquerade as success.

LOG CONTRACT (verdict 3b step 8): `[CAP3][PATCH|UNPATCH|REVERT|QUERY|
LIVENESS|HEX|DIAG]` prefixed lines, sequence-numbered; PASS only if
every recorded rc matches expectation; partial runs are recorded as
partial, never as success.

## 4. Public API (`src/CodePatch.hpp`)

```cpp
PatchResult patch_code(void* target, const void* replacement, size_t length);
PatchResult unpatch_code(void* target, const void* original, size_t length);
PatchResult patch_code_ex(void* target, const void* replacement, size_t length,
                          PatchBackup* backup_out, unsigned flags);
PatchResult unpatch_code_ex(PatchBackup* backup, unsigned flags);
class PatchScope { /* RAII: patch on construct, revert on destruct */ };
PatchResult query_span_protection(const void* addr, SpanProtection* out);
PatchResult query_span_protection_vq(const void* addr, SpanProtection* out);
int kekcall_check(void);
```

Notes:

- `patch_code()` defaults to `kFlagRequireLiveness` (the verdict's P1
  discipline) and records the module-slot backup consumed by
  `unpatch_code()`. `unpatch_code()` REFUSES (`kBackupInvalid`) if no
  successful `patch_code()` precedes it in-process — it never guesses
  the original protection — and cross-checks the caller's `original`
  bytes against its own pre-OPEN capture.
- `PatchScope` reverts on destruction; an explicit `revert()` failure is
  NOT blind-retried by the destructor (the verdict-7c retry budget is
  spent); it logs `[CAP3][SCOPE-LEAK] … STATE DIRTY`.
- `kFlagSkipLiveness` exists ONLY for the diagnostic's own-text
  mechanics test (below). It must never be used against game memory.

## 5. The diagnostic mode (`src/PatchDiagnostic.cpp`)

Shape of the "separate no-game probe" (verdict section 2c): standalone
payload ELF, `pldmgr /loadpayload`, **no game launched**; own log lines;
clean exit; one test per boot, three boots for the law.

- Target: `cap3_diag_probe_fn` — hand-written asm in the probe's OWN
  text (`mov $0x1337,%eax; 8×nop; ret`), patched with
  `mov $0xC0FFEE,%eax; ret` (6 bytes). Single-threaded, span quiescent
  by construction (U4 containment). No game memory is touched, ever.
- Sequence S0-S7: control mprotect on an anon page (proves the plain
  mprotect path works — separates "mprotect broken" from "text write
  refused"); kekcall CHECK (recorded, non-fatal here — see below);
  pre-flight own-text query (detects the already-writable-text case that
  makes U2 INCONCLUSIVE: payload text may arrive RWX from the loader, in
  which case a successful write proves mechanics, not the skip); patch
  via the library; behavioral check (0xC0FFEE); unpatch via the library;
  behavioral restore (0x1337); independent re-query proof.
- Exit codes: 0 = PASS, 1 = FAIL, 2 = DIRTY (restore failed; treat
  state as indeterminate; nothing else may be attempted this boot).
- Documented deviation: the diagnostic passes `kFlagSkipLiveness` so the
  mechanics test yields measurements even when CHECK fails. Justification:
  the abort-on-CHECK-fail rule protects GAME memory; patching the
  probe's own text in the payload process with full revert is the
  contained case, and aborting would suppress exactly the U1/U2 evidence
  the rung exists to gather. The in-game rung keeps hard liveness.

## 6. U1-U6 register (what each console rung can and cannot verify)

| Item | Question | Measured by diagnostic | Still unverified after it |
|---|---|---|---|
| U1 | Does the game process's mprotect land on the hooked entry (correct sysent, kstuff resident, not paused)? | payload-context CHECK only | in-game engagement (TEST-PLAN rung 4) |
| U2 | Does mprotect SUCCEED on SELF text even with checks skipped? | own-payload text (INCONCLUSIVE if loader mapped it RWX) | game eboot/PRX SELF text behavior, COW |
| U3 | Do the query APIs faithfully reflect a change made through the fixed path? | VERIFY-OPEN + restore re-query + vq bracket sanity, t0 | fidelity across time/other processes |
| U4 | Do other cores see patched bytes mid-execution? | NOT TESTED (quiescent span) | cross-modifying protocol sufficiency |
| U5 | Does restoration actually restore — and stick? | t0 proof (bytes + behavior + protection) | stickiness over time / across operations (rungs 2-3) |
| U6 | kstuff pause state at test time | CHECK covers liveness; no game → ShadowMount pause N/A | in-game pause-window behavior |

## 7. What this design does NOT claim

- Not "viable now", not ">8 quality", not acceptance of capability #3.
- No claim that the interception engages from the game PRX, that SELF
  text can be made writable, that queries are faithful, that other cores
  see patches, that restoration sticks, or that pause state is benign.
- No console action is authorized by this document. TEST-PLAN.md rungs
  each require explicit user OK; rung 4 additionally requires the
  3-restart law and a written revert path per PS5-MODDING-MISSION.md.

## 8. Reference list (all read 2026-10-13)

- cap01-03/PROVIDER-ROUTE-VERDICT-V2.md sections 1.4, 3a, 3b, 3c
- /tmp/kb-src-jc/kstuff-lite-1.11/ps5-kstuff/uelf/main.c:56, 65, 87-93
- /tmp/kb-src-jc/kstuff-lite-1.11/ps5-kstuff/uelf/syscall_fixes.c:8-14, 66-80
- /tmp/kb-src-jc/kstuff-lite-1.11/ps5-kstuff/uelf/syscall_fixes.h:4
- /tmp/kb-src-jc/kstuff-lite-1.11/ps5-kstuff/uelf/kekcall.c:102-106
- /tmp/kb-src-jc/kstuff-lite-1.11/ps5-kstuff/debug-reader.c:9-15, 23-46, 632
- /tmp/kb-src-jc/kstuff-lite-1.11/gdb_stub/dbg.c:263-272, 393-396, 427-448
- /tmp/kb-src-jc/kstuff-lite-1.11/prosper0gdb/r0gdb.c:955-975; r0gdb.h:65-66
- /tmp/kb-src-jc/kstuff-lite-1.11/lib/syscalls.txt:356, 381-382, 483
- ps5-payload-sdk v0.43: include/sys/mman.h:52-55, 263; include/sys/syscall.h:82;
  include/machine/param.h (__SCE__ PAGE_SHIFT 14); include/errno.h:62-63;
  lib/libkernel.so exported symbols (nm -D verified 2026-10-13)
- AGENTS.md PS5 rules (SELF/fake-sign, no rapid launch, 3-restart law);
  PS5-MODDING-MISSION.md PHASE B
