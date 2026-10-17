# WEAKIFY-DECISION.md — worker-import-audit-weakify

Date 2026-10-04 · Question: for composition-5, weakify the 5 unproven import rows now
(defensive) or stay strong and let sdg2 decide?

## Decision

**STAY STRONG. Do not weakify composition-5 now.** Run the already-staged sdg2 probe
(196 imports = audit-proven exact composition-4 surface) and branch on its outcome.
Weakify remains a prepared one-command contingency
(`AUDIT-RESULTS.md` §5.3) — appropriate only if sdg2 is load-REFUSED.

## Evidence the decision rests on (all measured this session unless cited)

1. **composition-4 already survived the strong-bind gauntlet.** The packaged artifact
   (sha `de372302…`) carries 196 UND imports, every one STB_GLOBAL, zero weak
   (`AUDIT-RESULTS.md` §2.1), and it was accepted by ploader in-game with no abort
   (controller report, task context). A refused strong bind aborts the load before init
   and ploader surfaces FAIL with the raw kernel rc
   (`worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md` §1, §3). So the strongest
   available evidence already says all five "unproven" rows BIND on this console/game
   context.
2. **sdg2 turns that into rigorous proof at zero design cost.** sdg2's dynsym surface is
   identity+bind-set-identical to composition-4's (audit-proven this session,
   `AUDIT-RESULTS.md` §2.2; run1==run2 `0e5b36bc…`). Binding precedes module init, so
   "ploader OK + DIAG line at module_start" proves the load and all 196 binds completed.
   sdg2 is already staged for exactly this test.
3. **The identified root cause is upstream of imports.** module_start never calls the
   UE4SS startup chain; zero output follows from that even with perfect imports. Adding
   weakify now adds a toolchain step and permanent guard obligations without touching
   the actual failure.
4. **comp5-minimal already defuses 4 of the 5 rows structurally.** comp5 = frozen191 +
   sceKernelGetModuleInfo only, all strong (`AUDIT-RESULTS.md` §2.3); the dropped four
   cannot abort a load they are not in ("not imported" strictly dominates "weakly
   imported": no dynsym row, no slot, no guard debt). The single remaining unproven row
   (GetModuleInfo) is referenced address-taken only and its resolved pointer is LOGGED
   by comp5's module_start (`worker-composition-5-minimal/REPORT.md` §1), so even a
   hypothetical bind-to-0 would be visible in the log rather than silent.
5. **Weakify would degrade the evidentiary value of the next console run.** A weakified
   row that fails approval binds to 0 and the load proceeds — a "clean" ploader OK that
   no longer discriminates "all binds pass" from "one bind silently zeroed". Since the
   current open question is precisely whether the load path is clean (and where startup
   diverges), pre-weakifying spends the one experiment that could answer it.
6. **The Packager cannot emit STB_WEAK anyway** (`DynamicWriter.cs:779` hardcodes
   GLOBAL; weak-only references are dropped and statically zero-bound,
   `Linker.cs:249`, `DynamicWriter.cs:1473` — `AUDIT-RESULTS.md` §4). The only weakify
   path is the post-packaging byte rewrite, which is additional pipeline surface for a
   risk that evidence says is not live.

## Branch matrix after sdg2 runs

| sdg2 outcome | Meaning | Action |
|---|---|---|
| ploader OK + `DIAG: module_start reached…` appears | Load + all 196 strong binds proven on console | Weakify permanently unnecessary. Stay strong; fix the startup chain (module_start wiring) in the full runtime. |
| ploader OK but no DIAG | All imports bound (load completed); module_start itself did not run or died before the write | Imports exonerated — weakify irrelevant. Debug entry path (alignment/ABI/early-environment), triage T2/T5 territory. |
| ploader FAIL with kernel rc | At least one of the 5 strong binds refused (or a metadata gate) | Now weakify is warranted: apply the prepared rewrite to the failing surface with R1-R4 guards (one command, `AUDIT-RESULTS.md` §5.3), rewrap, restage with new distinct IDs. Discriminate the failing row cheaply first with comp5 (keeps only GetModuleInfo strong): comp5 loads ⇒ failure is among the 4 dropped; comp5 fails ⇒ GetModuleInfo (or pipeline) — then weakify exactly the implicated row(s). |
| Staging not attempted | No new evidence | Decision stands unchanged; nothing to redo. |

## What "weakify now" would have cost (for the record)

- 1 post-packaging byte-rewrite step + fself rewrap per artifact, permanent R1-R4 guard
  obligations on the import-table and GOT consumers (`AUDIT-RESULTS.md` §5.2),
  degraded bind-provenance in every subsequent log, and a misleading "loads fine" signal
  if an approval refusal silently zeroes a slot. None of this addresses the identified
  module_start root cause.

## Honest limits

- The ploader-OK for composition-4 is retained controller evidence; this worker had no
  console access and could not re-verify the raw rc. If that OK were ever retracted,
  branch 3 of the matrix (weakify contingency) becomes live immediately.
- "Weak binds to 0 and continues" is argued from the FreeBSD-rtld lineage and the
  Packager's own documented model (`optional_imports.h:5-9`, `Linker.cs:114-115`); it
  has never been measured on this console. The weakify contingency must therefore be
  validated by its own staged load, not assumed.
- No artifact was modified; all rewrites described are prepared commands under new IDs,
  to be applied only after a sdg2 FAIL branch and with the staging checklist gates.
