# CAP03 SAFETY ANALYSIS — Capability #3 code patching

Status: PROPOSAL ONLY (2026-10-13). Analysis of the implementation in
`src/CodePatch.cpp` / `src/PatchDiagnostic.cpp` against the protocol in
`DESIGN.md`. Scope: Mac-only document; no console contact is authorized.

Context that bounds every scenario below: this capability is
kernel-adjacent (kstuff is kernel-resident), but the code itself runs
strictly in process context (plain `mprotect` syscall 74, query APIs,
one raw getppid-shaped syscall). Nothing here writes kernel memory. The
worst direct outcome of a bug is damage to the calling process's own
address space, plus whatever a destabilized game process does to the
running session. A panic remains possible through unexplored kernel
paths (U2/U3) — that is why the 3-restart law and per-step user OK bind.

---

## 1. What happens if mprotect fails silently?

Two distinct "silent" cases:

a) **mprotect returns non-zero.** NOT silent for us: OPEN (step 3),
   RESTORE (step 8) and every REVERT protect call check rc, log rc+errno
   (`[CAP3][PATCH] STEP=OPEN … rc=… errno=…`), and abort/refuse. This is
   the exact dbg.c gap (dbg.c:393-396 discards rc) that the corrected
   verdict calls out; it is closed in code.

b) **mprotect returns 0 but the change did not happen** (kernel lies, or
   the fix rewinds the validation block but the vm_map op is refused
   downstream — the U2 surface). Detection layers:
   - VERIFY-OPEN (step 4) re-queries and refuses to write unless the
     query reports W|R; a VirtualQuery disagreement also fails the open.
   - If the query ALSO lies (U3), the write itself faults: `memcpy` to a
     non-writable page raises SIGSEGV/SIGBUS in OUR process. That is a
     process crash, not a silent half-patch — the diagnostic dies
     loudly, the game (in rung 4) dies loudly. We accept this residual:
     there is no in-process way to distinguish a lying rc + lying query
     without executing the write, and this analysis refuses to hide it
     as "safe".
   Containment: the diagnostic targets only its own text (payload
   process death is the blast radius). Rung 4 accepts that a lying-kernel
   scenario can kill the game session — the reboot is the recovery, and
   the 3-restart law is the evidence discipline.

## 2. What happens if the write partially succeeds?

- Same-thread `memcpy` to a verified-writable page either completes or
  faults; a mid-span fault leaves a partial byte image only in the
  process-death case (scenario 1b) — never observable as a "working
  half-patch" because the process is gone.
- Ambiguity AFTER a completed write is handled by the byte-compare
  readback (step 5): mismatch → REVERT-ONLY (rewrite originals while the
  page is still open, restore protection, re-query proof) → return
  `kReadbackMismatch`. The library never reports success on a partial
  write, and never leaves the page open.
- The `kAlreadyApplied` refusal prevents a double-write of an
  already-patched span (ambiguous state by definition).
- Cross-core partial visibility (store visible to one core executing the
  span mid-patch) is U4: unbounded, therefore the design REQUIRES a
  quiesced span (execution barrier, DESIGN.md step 6) and the diagnostic
  is single-threaded. Without that guarantee this capability must not be
  used, whatever else passes.

## 3. What happens if restoration fails?

- Restore mprotect rc != 0 → `kRestoreMprotectFailed`, terminal (no
  retry — rc failures get no retry budget), loud
  `RESTORE-*-FAILED … STATE DIRTY` lines.
- Final re-query mismatch → exactly one retry (verdict 7c), then
  `kRestoreVerifyFailed` + loud `STATE DIRTY`.
- Re-query itself fails → `kRestoreQueryFailed`, terminal. Ambiguous
  state is never retried blindly.
- DIRTY semantics for the caller: exit code 2 from the diagnostic; the
  patch bytes and/or RWX protection REMAIN. Consequences:
  - bytes-only drift: the patched function behaves as the patch defines;
    in the diagnostic that is a self-contained probe function — no
    system impact; in rung 4, a "harmless" target must be chosen so even
    a stuck patch is benign (that choice is a rung-4 gate, not a code
    property).
  - RWX left open: an exposure/permissiveness residue in that process
    only. It dies with the process. No cross-boot persistence exists:
    page protections and patch bytes are memory state; nothing in cap03
    touches disk, autoload, SELF, or the loader.
- Recovery path (P4, written down here as required): clean exit/reboot
  restores pre-test state by construction; evidence is the saved raw
  log. Further patches in the same boot are refused after DIRTY.

## 4. Interaction with kstuff's pause state (U6)

- ShadowMount pauses kstuff ~15-25 s after a game launch and resumes on
  clean game exit (AGENTS.md PS5 rule 5; mission log). During a pause
  window the mprotect interception may not be resident → OPEN fails with
  EACCES/EPERM. The design treats that as a NORMAL, rc-checked failure
  (`kMprotectFailed`), not as something to work around.
- The diagnostic runs with NO game launched, so the ShadowMount pause
  cycle does not apply; liveness CHECK is still recorded to measure U1
  from the payload context.
- Rung 4 (in-game) must NOT be attempted inside the post-launch pause
  window; the liveness CHECK gating (`kFlagRequireLiveness` default) is
  the code-level gate, and the rung plan adds a timing gate (write the
  pause window down; patch only after the game is fully up and the
  window has passed, or on a cleanly resumed title).
- Hard rule respected by design: failed launches leave kstuff paused
  until a clean exit — cap03 never launches anything, never kills
  processes, and aborts on the first ambiguous step so it cannot be the
  cause of a rapid-fire launch sequence.

## 5. Interaction with the no-rapid-launch rule

- The no-rapid-launch rule exists because failed launches wedge the
  kstuff/auth state for ALL games until a clean exit. cap03's exposure
  is bounded by construction: one payload load per rung step, one boot
  per test, payload exit codes 0/1/2, and an explicit "no replay of
  ambiguous runs" gate (TEST-PLAN G5). The only scenario where cap03
  contributes to a failed launch is a console crash during rung 4 —
  which is already a full-stop event under G5 (postmortem before
  anything else).
- The 3-restart law is implemented as rung 3, one test per boot, with
  panic-at-any-point = full stop.

## 6. Additional exposures (not asked, but owed)

- **Whole-page effect:** mprotect acts on pages, not bytes: during the
  patch window the entire 16 KiB span page (and any co-spanned page) is
  RWX, and neighbors of the target live on the same page. The window is
  closed by RESTORE+proof on every path; still, rung 4 target selection
  must treat the WHOLE PAGE as exposed, i.e. no secrets/other-critical
  code co-resident assumptions without checking the query span.
- **COW/shared text:** making file-backed SELF text writable may
  copy-on-write the page into the process (U2) — expected behavior, but
  if instead it mutated shared storage, other processes would see the
  patch. Unverified; the restoration proof is also the detection
  (post-restore re-query), and rung 1-3 use only the payload's own text,
  which cannot be shared with the game.
- **Query API trust (U3):** every protocol decision that depends on a
  query is cross-checked (qmp primary, vq corroborated with a bracket
  sanity check; disagreement = failure). A query API that lies in BOTH
  channels consistently is undetectable in-process and is recorded as an
  open unverified item rather than assumed away.
- **No global state:** cap03 keeps one module-slot backup, installs no
  handlers, no threads, no signals, no atexit work beyond RAII; the
  PatchScope destructor never throws and never exceeds the protocol's
  retry budget.

## 7. Summary matrix

| Failure | Detected by | Containment | Residual risk |
|---|---|---|---|
| mprotect rc != 0 | rc check (OPEN/RESTORE/REVERT) | abort / REVERT-ONLY / loud log | none beyond wasted boot |
| mprotect rc 0, no effect | VERIFY-OPEN re-query | refuse write, revert, U2 evidence | write-fault crash if query also lies (U3) |
| partial/ambiguous write | byte-compare readback | REVERT-ONLY, kReadbackMismatch | U4 if span not quiesced (design forbids) |
| restore mprotect fails | rc check | terminal, STATE DIRTY, refuse further | RWX + bytes remain until process death |
| restore verify mismatch | re-query proof | one retry (7c), then STATE DIRTY | same as above |
| kstuff paused (U6) | liveness CHECK / OPEN rc | abort, no retry, normal failure | none |
| console panic | G5 | full stop + postmortem | the reason per-step user OK exists |
