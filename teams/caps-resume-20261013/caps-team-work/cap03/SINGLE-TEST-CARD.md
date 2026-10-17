# CAP03 SINGLE-TEST CARD — the one console test that proves or disproves #3

Status: PROPOSAL ONLY (2026-10-13, agent KC). Plain-language summary of
`DESIGN.md` (protocol), `TEST-PLAN.md` (rungs/gates), `SAFETY.md` (failure
modes). Nothing here authorizes a console action.

## The ONE test

Build `cap03-diag.elf` (from `src/CodePatch.cpp` + `src/PatchDiagnostic.cpp`,
prospero toolchain, distinct filename). Upload it, verify the upload by
reading it back with decryption OFF and comparing the full SHA256 (PS5 rule
4), then on a fresh boot with FTP up and klog capturing, and **NO game
running**, run exactly once:

```
pldmgr /loadpayload cap03-diag.elf
```

Save the complete output log. That single run is the test.

## What the test does, in plain words

The probe performs surgery on ITSELF, on one tiny function that does nothing
but return the number 0x1337. First it makes that function's memory
writable (asking the kernel through the special mprotect path that kstuff
patches in). Then it rewrites the function so it returns 0xC0FFEE instead,
and calls it to prove the new number comes out. Then it puts the original
instructions back, restores the memory's original write-protection, calls
the function again to prove the old number 0x1337 comes back, and
re-queries the memory protection to prove the page is exactly as it found
it. It never touches the game, the console's settings, or anything on disk.
If any step misbehaves, it puts things back and reports loudly.

## What PASS looks like

- The probe exits with code **0** and prints `SUMMARY: failures=0 dirty=0 -> PASS`.
- Every step line S0-S7 says PASS: the plain mprotect control works (S0),
  the liveness check result is recorded (S1), the patch succeeded (S3),
  the patched function returned 0xC0FFEE (S4), the restore succeeded (S5),
  the function returned 0x1337 again (S6), and the independent re-query
  shows the page protection identical to before (S7).
- No crash, no panic, and FTP still answers after the probe exits.

## What FAIL looks like

- **Exit 1 (FAIL):** one or more steps failed but everything was put back.
  Most informative case: `OPEN ... rc≠0` (the kstuff mprotect skip is NOT
  engaged for this process — that alone is a real, publishable result).
- **Exit 2 (DIRTY):** the restore failed — the probe's page may still be
  patched or left writable. Treated as indeterminate state.
- **Worst case:** a crash/panic (residual risk per SAFETY.md section 1b:
  if the kernel both lies about mprotect and the query lies too, the write
  itself faults — the probe dies loudly; blast radius is the probe's own
  process only).

## What to do after each outcome

- **PASS:** record the raw log in a dated evidence dir; proceed to
  TEST-PLAN console test 2 (repeat patch/restore cycle twice in the same
  boot), then test 3 (the three-separate-reboots law). Only after three
  clean boots may cap03 be called "console-verified for the payload
  process". It still says nothing about game memory.
- **FAIL (exit 1):** it is a RESULT, not a retry prompt. Save the log,
  write the fail report, and do NOT re-run in the same boot (TEST-PLAN G5:
  never repeat an ambiguous run hoping it succeeds). A new attempt needs a
  fresh boot and, if the failure changed anything about the approach, a
  revised design with new IDs.
- **DIRTY (exit 2):** stop. Save the raw log, reboot the console. Nothing
  else may be attempted that boot. State recovers by construction because
  nothing in cap03 persists across reboot (no loader, autoload, or SELF
  changes — SAFETY.md section 3).
- **Panic:** full stop, postmortem before anything else.

## Restoration and error handling — every step (from DESIGN.md section 3)

| Step | If it fails | Restoration action |
|---|---|---|
| 0-2 ARG/ALIGN/PRE-FLIGHT (query + record original bytes & protection) | abort; nothing was modified | none needed |
| 2b LIVENESS (kekcall CHECK) | abort, never retried blindly | none needed |
| 3 OPEN (mprotect RWX, return value CHECKED) | abort; page untouched | none needed |
| 4 VERIFY-OPEN (re-query; write only if kernel reports writable) | REVERT-ONLY: rewrite original bytes (page still open), restore per-page original protection rc-checked, re-query proof | yes |
| 5 WRITE (write + byte-compare readback) | mismatch -> REVERT-ONLY as above | yes |
| 8 RESTORE-PROT (re-protect each page, rc CHECKED, NO retry on rc failure) | terminal: loud `STATE DIRTY` log, exit 2, refuse further patches this boot | reboot |
| 9 RESTORE-PROOF (re-query must EQUAL recorded original; exactly ONE retry, mismatch only; query failure terminal) | `kRestoreVerifyFailed`/`kRestoreQueryFailed` -> STATE DIRTY, exit 2 | reboot |
| any kstuff pause window (U6) | shows up as a normal rc-checked OPEN failure | abort; nothing touched |

Compare-before-write everywhere: the patch refuses if the target already
equals the replacement (`ALREADY_APPLIED`), and the unpatch refuses unless
the bytes match the recorded patch (`NOT_PATCHED_STATE`) — no ambiguous
double-writes, ever.

## Phase B prerequisite — READ THIS BEFORE ANY CONSOLE ACTION

**This test cannot run without the user's explicit approval.** Nothing
changes on the console without the user's OK for that exact step
(PS5-MODDING-MISSION.md lines 288-291). As Phase B work it additionally
waits for the gate order: Phase B begins **only AFTER the Townfall Item
Spawner successfully spawns an item** (mission lines 273-275). The upload
must be readback-verified with decryption OFF before launch (PS5 rule 4),
the payload is loaded at most ONCE per boot, and any failed launch means
clean-exit or reboot before further tests (PS5 rule 5). Precondition P0.1
from TEST-PLAN.md also still applies: the syntax check done on 2026-10-13
used Apple clang replaying the SDK wrapper's flags — rerun with the real
prospero-clang before building.
