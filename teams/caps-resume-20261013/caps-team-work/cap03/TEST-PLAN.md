# CAP03 TEST PLAN — Capability #3 code patching

Status: PROPOSAL ONLY (2026-10-13). No console step below is authorized
by this document; each rung requires the user's explicit OK for that step
(PS5-MODDING-MISSION.md lines 288-291), one test per boot, three logged
console passes across three separate reboots for anything claimed as
verified (lines 280-282), and a written pass/fail + revert path per
session (lines 283-285). Run order is mandatory: no rung starts before
the previous rung has a written pass/fail.

Preconditions for ANY console rung:

- G0. cap03 code review signed off (this pass + one independent reviewer);
  syntax checks recorded (done 2026-10-13, see "Pre-console" below).
- G1. kstuff loaded and FTP up after boot; `pldmgr` usable
  (`/loadpayload` between launches in one boot; ploader stays LAST in
  autoload.txt and exits per launch cycle — restart it via pldmgr, never
  touch autoload).
- G2. klog capture available for evidence; every rung's full
  `[CAP3]` log saved under a dated evidence dir before anything else
  runs; raw logs never rewritten.
- G3. Executables uploaded to the console are verified on readback
  (magic + full sha256, decrypt OFF) before any launch — AGENTS.md PS5
  rule 4. (pldmgr payload ELFs are not SELF-faked game executables, but
  the readback-verify rule is applied to every upload regardless.)
- G4. No rapid-fire launches: after ANY failed launch, clean-exit or
  reboot before further tests (AGENTS.md PS5 rule 5).
- G5. Abort criteria, all rungs: exit code 2 (DIRTY), any
  RESTORE-*-FAILED line, any panic, any ambiguous step. On abort: stop,
  save raw log, reboot, write fail report; never re-run the same
  ambiguous write hoping it succeeds.

---

## Phase 0 — Pre-console (Mac only)

| # | Check | Pass criterion | Status |
|---|---|---|---|
| P0.1 | Syntax check each TU | zero errors | DONE 2026-10-13: CodePatch.cpp OK, PatchDiagnostic.cpp OK (`clang++ -target x86_64-sie-ps5 … -fsyntax-only -Wall -Wextra`, SDK v0.43 headers; the SDK's own `prospero-clang` wrapper could not exec on this Mac — its `bin/clang` resolves `LLVM_BINDIR/llvm-clang`, absent — so the wrapper's exact flag set was replayed through Apple clang 17. Recorded honestly; rerun with real prospero-clang before any build.) |
| P0.2 | Design review | verdict section 3b steps 1-8 + 7a-c all present and rc-checked; REVERT-ONLY on every post-OPEN failure; restoration proof present | DONE (DESIGN.md section 3) |
| P0.3 | SAFETY.md review | every failure mode has a detection AND a containment answer | see SAFETY.md |
| P0.4 | Rollback artifact written down | boot-from-scratch recovery: nothing in cap03 persists across reboot (no loader changes, no autoload changes, no SELF changes); evidence = raw log only | this document = the written rollback path |
| P0.5 | Build definition (NOT executed) | future build = one static PIE payload: `prospero-clang++ -target x86_64-sie-ps5 CodePatch.cpp PatchDiagnostic.cpp -o cap03-diag.elf` + real toolchain syntax pass first; distinct filename `cap03-diag.elf` (never overwrite any existing payload) | pending user OK |

## Console test 1 — diagnostic mode (no game running)

Purpose: first real measurement of U1(payload)/U2(self-text)/U3(t0)/U6.
Satisfies the "separate probe" model: no game PRX involved.

1. Fresh boot; FTP up; klog on. No game launched at any point.
2. Upload `cap03-diag.elf` (decrypt-OFF readback verify, G3).
3. `pldmgr /loadpayload cap03-diag.elf` — ONCE. No replay of ambiguous runs.
4. Capture full stdout + klog to evidence dir.
5. Pass: exit 0; S0 control PASS; S2 shows own-text R|X (or an explicit
   already-writable INCONCLUSIVE note); S3-S7 PASS; U-report printed;
   no panic; FTP still up after exit.
6. Fail handling per G5; a FAIL or INCONCLUSIVE is a RESULT (recorded),
   not a reason to re-run in the same boot.
7. Evidence: raw log + U1-U6 lines.

## Console test 2 — restoration proof cycle

Purpose: U5 beyond the diagnostic's internal cycle: patch → read →
unpatch → read, externally observed, twice, same boot (loadpayload is
repeatable in one boot via pldmgr; each load is a separate probe run).

1. Same as test 1 steps 1-4.
2. Run A: patch cycle; capture log; independently verify via a second
   query payload OR accept the diagnostic's own S7 double-check if no
   second payload is authorized.
3. Run B: same again (fresh process). Confirms restoration stuck across
   process instances (coarse U5 stickiness: memory is per-process, so
   the real sticky test is rung 4's re-read over game uptime).
4. Pass: both runs exit 0; protections identical in both logs; no DIRTY.
5. Abort handling per G5.

## Console test 3 — 3-restart law

Purpose: the AGENTS.md/mission discipline: a capability claim needs
three logged passes across three separate clean reboots, one test per
boot, several reboots without a panic.

1. For boot i in 1..3: clean reboot → FTP up → load diagnostic once →
   capture → write per-boot PASS/FAIL line + revert path statement.
2. Pass: 3/3 exits 0 with consistent U-lines; zero panics; FTP up after
   each run.
3. Any panic at ANY point: full stop; postmortem before anything else
   (kernel-adjacent work rule).
4. Output: a single dated verdict file referencing all three raw logs.
   Only after this file may cap03 be called "console-verified for the
   payload-process case". It still says nothing about the game case.

## Console test 4 — game-running patch (the actual use case)

Purpose: U1 (game-PRX engagement), U2 (SELF game text), U4 (executing
cores), U5/U6 in the real environment. This is the in-game rung the
verdict calls "explicitly in-game"; acceptance wording: "harmless
in-game patch: demonstrate the patch and a clean revert".

Hard gates BEFORE any implementation detail is chosen:

1. Explicit user OK for this rung specifically.
2. Runs only AFTER Townfall Item Spawner spawns (Phase B gate order) if
   this capability is treated as Phase B work; otherwise a scoped,
   user-approved exception must be written down first.
3. Target selection: a quiesced, harmless function in game memory with a
   written revert path (original bytes + original protection + restore
   procedure) BEFORE the boot; compare-before-write from a live query;
   no patch anywhere near the mod hook chain or loader-resumed code.
4. Execution barrier: patch only while the span is provably not
   executing (suspend the owning thread(s) via the existing debug
   primitives, or choose a cold function); U4 makes every other choice
   unbounded.
5. 3-restart law applies; one patch attempt per boot; exit criteria
   identical to rung 1; klog + before/after screenshots of the game
   unaffected as evidence.
6. Same exit code semantics (0/1/2-DIRTY). On DIRTY: game exits via its
   own menu, klog archived, no retry, write-up before anything else.

---

## Evidence and reporting contract

- Every rung writes: raw `[CAP3]` log, exit code, U-lines, per-boot
  verdict line, revert-path statement. Raw logs are never edited.
- The phrase "console-verified" may be used for cap03 only after rung 3
  writes its verdict file; "game-verified" only after rung 4. Anything
  less is reported exactly as the verdict states today: source-backed
  candidate, target behavior unverified.
