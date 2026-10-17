# REPORT — worker-comp6-post-build-reviewer

Date: 2026-10-04 · Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`
Deliverables: `CLAIM.json` (first), `POST-BUILD-VERDICT.md` (main), `REPORT.md` (this, last).
Supporting: `verify_comp6_build.py` + `verify_out.json` (re-runnable verifier + full dump),
`evidence/` (disassembly, relocation censuses, run-3 baselines).

## What was done

1. **WAIT-CHECK.** Polled `…/ue4ss-native-supplier-composition-6/run-1` at ~60–120 s
   intervals. During the wait, attempts 1–9 of the controller's one-pass progressed
   (logs `/tmp/comp6_onepass_run*.log`). Phase A completed 23:16 local writing run-1
   (receipt.json + composed object); Phase B packaging refused 23:20; run-1 was renamed
   to `run-1-f1791174131` (forensics) at 23:22; attempt 9 was still failing at
   verify-only (bridge source pin) at review close. Review proceeded against the
   preserved Phase A artifacts — receipt, composed object, per-unit objects — and the
   honest incomplete-gate state is part of the verdict.

2. **Independent verification (all re-derived from artifact bytes):**
   - (a) **Shape delta vs composition-4 run-3** — exactly the expected set:
     `retained_units_replaced == ["program"]`, fresh units 15 → 16 (+program only),
     bootstrap triple bit-identical, `undefined_delta` identical to run-3
     (`+[sceKernelGetModuleInfo]`, count 198). Composed-object UND set byte-identical
     to run-3 (198/198). DEFINED delta itemized: +1 (Validate-fork `filesystem::path`
     lambda) / −56 (initializer unreachable-tail template instantiations — Output::send
     arms, ScanResult/ScanError helpers, optional<bool>, libc++ `B8ne180100` internals),
     all covered by the declared initializer replacement; nothing else moved.
   - (b) **Shape-A initializer, objdump-level.** The shipped initializer object is
     bit-identical (sha `fe639a85…`, 1,676,944 bytes) to the reviewed compile-gate
     artifact. Composed `ScanGame` is exactly 0x4a4 bytes; its relocation target list
     contains only fmt/std/throw machinery — zero scan calls; whole-object ps_scan
     relocations = 1 and it is the `.eh_frame` FDE pointer (run-3 had 2; the ScanGame
     call site is gone). Zero DoScan lambda sections. The compiled `kAllSlots` table
     parses to gmalloc `present=0 addr=0x0` between three seeded slots. The refusal
     string `[Seeds] Required-slot seed table incomplete: present [{}] missing [{}]. …
     SHAPE A safe default, LAUNCH9-PREDICTION` is verbatim in the object, terminated by
     `__cxa_throw(runtime_error)`.
   - (c) **Configured-roots.** Both new Validate-fork error strings verbatim;
     `ValidateNativeProgramContext` + new lambda + both `MakeNativeProgramContext`
     overloads defined; the replaced program's banner (UTF-16,
     "unobserved on this platform; configured game directory: {}") present at 0x4680e.
     ODR gate true at the EDITED port sha (`0a068bb9…`); chain symbols 14/14 re-checked
     against the run-1 object post-compose. All three edited sources byte-equal between
     the controller `comp6-integration` copies and the canonical W paths.
   - (d) **Gate receipts for the real pass.** Read all that exist: run receipt
     (verified above), chain-symbols.json 14/14 true, odr-gate.json byte_identical true.
     The claimed GOs that could NOT be confirmed because the receipts do not exist yet:
     candidate PRX 17/17 (`prx-candidate-composition6-1` absent — packaging refused on
     `runtime_start_bridge.h` drift 4bb02413 vs pin ecada77e), ODR emitter
     `receipt-one-pass.json` (absent), gate v2 manifest verdict (absent; manifest not
     finalized — PENDING shas, no `resolution_b` section, entry r3/r4 self-contradiction).

3. **Deltas beyond the expected set, flagged loudly** (full list in
   POST-BUILD-VERDICT.md §1.5): no candidate PRX; bridge header/cpp moved after
   pinning; manifest entry-unit self-contradiction (r4 declared in packaging_units vs
   r3 pinned in P7/fold_ins); two D2 headers initially edited in place without rr
   coverage (admitted after fail-closed refusals, attempts preserved); one benign new
   adapter compile warning.

## Verdict

**NO-GO for staging.** The Phase A composition is genuine and exactly the declared
change set — including Shape A and configured roots — verified at byte, symbol, and
instruction level. The authorizing pass is incomplete (no candidate PRX, no emitter
receipt, no finalized manifest, no gate v2 verdict, controller still iterating), so
nothing may be staged. A fresh full one-pass after bridge-pin reconciliation must
reproduce §1 checks on its own run dir; this reviewer's verifier
(`verify_comp6_build.py`) is re-runnable against it.

## Tool budget

~45 tool calls (within the 30–60 bound). No console contact, no nested subagents, no
writes outside `outputs/2026-10-04-zcode-team/worker-comp6-post-build-reviewer/`.
