# Review prompt for all ZCode capability work (2026-10-12 through 2026-10-13)

## LOCATION

All work is in TWO places:

1. **Main tree fixes** (STAGED, not applied):
   `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/`
   (lookup-speed, GROUP B, shim-fix, lua-alloc, no-detour gate patches in `round5-staging/`)

2. **Capability work** (isolated worktree, nothing applied):
   `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/`

## READ FIRST

- `HANDOFF-20261012.md` (main tree state)
- `CLAUDE-ACCEPTANCE-PASS1.md` (self-test results + fix plan)
- `CHATGPT-HANDOFF-7-10.md` (#7/#10 already given to ChatGPT)

---

## WORK STREAMS TO REVIEW

### A. Runtime fixes (round5-staging/ in the main tree area)

**A1. Lookup speed** (`round5-staging/lookup-speed/`): 6 patches targeting
<10ms per lookup (from 78ms). Root cause: 6 IsA<> calls per object
in the FindXOf walk. Fix: class-verdict memo + name→items cache.
Review: is the per-walk memo correct? Is the cache invalidation
sound (delete-listener only, no create-listener)?

**A2. GROUP B** (`round5-staging/group-b/`): ForEachUObject stack-leak fix
(upstream inherited bug: 1 Lua slot leaked per object call),
IterateGameDirectories PS5 fallback, UEnum diagnostics.
Review: is the stack-leak diagnosis correct? Is the PS5 fallback safe?

**A3. Shim fix** (`round5-staging/shim-fix/`): deploy-time pre-create + lazy
write + PC semantics. Root cause of crash: stale Lua stack index
(NOT filesystem). Review: is the fix complete?

**A4. Lua allocator** (`round5-staging/lua-alloc/`): counting allocator +
64MB arena (env-gated). Root cause: shared libc heap exhaustion
during engine save/map-load squeeze. Review: is the arena safe?

**A5. No-detour gate** (`harvest/no_detour_gate.py`): build-time static gate
preventing code-detour attempts on PS5. 19/19 self-tests.
Review: does it catch the right patterns? Any false positives?

### B. #1 Kernel reads (caps-team-work/cap01-03/ + byepervisor/)

- `PROVIDER-ROUTE-VERDICT-V2.md`: "no verified applicable provider in
  the examined stack." Review: is this assessment complete and honest?
- `byepervisor/ASSESSMENT.md`: Byepervisor Reborn = HOLD (too fresh,
  open issues). Review: is the HOLD justified? What would change it?
- **Confirm: ZERO kernel reads have been performed or claimed.**

### C. #3 Code patching (caps-team-work/cap03/)

- `DESIGN.md`: 9-step rc-checked mprotect protocol with U1-U6 unverified.
- `src/CodePatch.cpp/.hpp`: full implementation (PatchScope RAII, backup,
  restoration proof). Review: is the protocol correct and complete?
- `src/PatchDiagnostic.cpp`: standalone no-game self-test.
- `TEST-PLAN.md` + `SAFETY.md`. Review: are the gates meaningful?
- **Confirm: status is "source-backed candidate; target behavior unverified."**

### D. #13 DLL mods (caps-team-work/cap13/)

Four sub-deliverables:

**D1. PE analyzer** (`TOOL/pe_analyzer.py`, 31/31 tests): PE format-aware
discovery. Review: are the tests meaningful? Edge cases?

**D2. Native DLL loader** (`DESIGN/native-dll-loader.md` + `native-loader/`):
PE mapper, Win API shim, ABI bridge, DllMain invoker. Key verdicts:
static TLS = permanent refusal; SEH/EH = not feasible for MVP;
~60-80% of envelope mods conditional on J1 probe (can anonymous
memory be mmap PROT_EXEC?). Review: are these verdicts correct?

**D3. Managed DLL route** (`DESIGN/managed-dll-route.md` + `managed-loader/`):
SDK-only v1, BepInEx structurally impossible in Unreal,
resident-owner bridge. Review: is the structural impossibility claim correct?

**D4. Mod selection** (`REPORT/mod-selection.md`): evidence gap (no real
Townfall DLL mod exists). Review: is the fixture approach honest?

---

## CRITICAL QUESTIONS FOR THE REVIEWER

1. The **J1 probe** (can game-process anonymous memory be mmap PROT_EXEC)
   gates the entire native DLL route. Should this probe run BEFORE
   any further native loader investment?

2. The **lookup-speed fix** changes the hot path for every mod. What
   regression testing is needed before applying it?

3. The **ForEachUObject stack-leak** is an UPSTREAM bug. Should we report
   it upstream or just fix it in our port?

4. Given that **no DLL has been executed**, what is the realistic timeline
   for the first DLL mod running on PS5?

5. The **#3 code patching** implementation depends on kstuff's mprotect
   interception working from the game PRX. What is the safest way to
   verify this (the U1 diagnostic) without risking the console?

---

## OUTPUT FORMAT

For each work stream: **PASS / CONDITIONAL PASS / FAIL** with file:line
citations. A single overall verdict with the **top 5 actions needed
before any console integration**. Explicitly call out any claim that
overstates completion.

## COMPLETION STATUS (for the reviewer's awareness — verify these claims)

| Work | Claimed status | Console evidence |
|---|---|---|
| Lookup speed | 6 patches staged | Zero (not applied) |
| GROUP B | 3 patches staged | Zero (not applied) |
| Shim fix | 1 patch staged | Zero (not applied) |
| Lua allocator | 3 patches staged | Zero (not applied) |
| No-detour gate | Tool complete | Self-test only |
| #1 kernel reads | Research verdict | Zero (nothing implemented) |
| #3 code patching | Source complete | Zero (never compiled for PS5) |
| #13 PE analyzer | Tool complete | 31/31 host tests |
| #13 native loader | Design + source | Zero (never compiled) |
| #13 managed route | Design + source | Zero (no PS5 Mono port) |
| #13 mod selection | Evidence gap documented | Zero (no real DLL exists) |

**Nothing in this review has been executed on PS5. Every claim of
"complete" means "Mac-side preparation complete," not "working on console."**
