# REPORT — worker-l11-plan (r2, 2026-10-05)

Mission: produce THE single ordered LAUNCH11-PLAN.md from the COLLAPSED
decision tree — the marshal is the only pump path, both L11 luamod fixes are
DELIVERED, the compose chain is proven, and the in-flight pad-defer folds into
the same lineage. LOCAL ONLY: read-only over cited worker dirs plus one /tmp
dry-run patch stack; no builds, no console, no composition, no packaging.
Deliverable order honored: CLAIM.json → LAUNCH11-PLAN.md → this REPORT.md last.

## 1. What was verified (each mission fact, with where)

1. **Marshal-only pump path** — worker-l11-loadmap-guard/REPORT.md §4 +
   CLAIM.json E2/E4/honest_bounds: live game text is never registry-backed →
   the PLH detour layer cannot arm against it in ANY kernel world; the
   "W-works initializer owns the pump" oracle is unreachable; the FuncPtr
   marshal is the only armed pump path. The plan therefore has NO
   W_WORKS/W_FAILS pump fork; the probe window is kept as a parallel evidence
   item (IGW/D-1 verdicts, walkguard fold admissibility), not a gate.
2. **Enqueue-ladder DELIVERED** — worker-l11-enqueue-ladder/{REPORT,CLAIM,DIFF}.md
   + host-test/PASS-receipt.txt: staging `f50976ae1e2626b0…`, diff
   `3b19068a194ec82d…`, fixed object `8593884ea825d523…`, control object
   `854a476bdd4f9985…`, host fixed 23/23 / control 14/14, throw site
   LuaMod.cpp:4807.
3. **LoadMap r2 defer DELIVERED** — worker-l11-loadmap-guard/{REPORT,CLAIM,DIFF}.md
   + host-test/receipt.json: soft refusal as composed (no crash — the feared
   class does not exist there); r2 gates the eager arms on
   `R3MarshalPumpPending`; r2 = log hygiene + explicit loss declaration;
   staging `9a7a37d6c8377c02…`, deliverable diff `d2ad2818db4b7338…`, r2
   object `3eeba02fd92efd99…`; the mod's `RegisterLoadMapPreHook` reset is
   never invoked (stale cross-map state; menu-rebuild-on-reopen mitigation).
4. **Consumer linkage + variant-B admission** — worker-l11-marshal-integration/
   RUNBOOK.md (provenance chain, controller steps, tiered oracle, rollback) +
   worker-luamod-l11-dryrun/VERDICT.md (GO for admission+compose; packaging
   unproven; `633a8c82…` comparator INVALID off variant-B). Comparator CHAIN
   verified: `854a476bdd4f9985` byte-matched 4x → `8593884ea825d523` (enqueue)
   / `3eeba02fd92efd99` (loadmap r2); both fix diffs name the SAME base
   `04c9d91664dd82c7` and their hunks are disjoint (5004–5143 vs
   7321–7377 + 8100–8157).
5. **pad-defer IN FLIGHT** — no worker-pad-defer directory or artifact exists
   anywhere in the worktree (name + content search, 2026-10-05). The pad path
   it defers is located via worker-pad-chord-audit/PAD-DISPATCH-PATH.md Hop
   2/3 (eager activation `ue4ss_backend.cpp:21-26` / `PS5PadNative.cpp:20`
   BEFORE program init; `Handler::init` throw `UE4SSProgram.cpp:731-734`) —
   hence "prerequisite for ANY runtime start surviving to mods". The plan
   holds a BLOCKING admission slot for it in the backend/bootstrap lineage,
   never the luamod stack.

## 2. New mechanical fact this worker produced (M1)

Dry-run patch stack in /tmp (patch -p1 only):

- base `04c9d916…` + enqueue diff → `f50976ae1e2626b0…` BYTE-IDENTICAL (diff
  faithful);
- base `04c9d916…` + loadmap r2 diff → `9a7a37d6c8377c02…` BYTE-IDENTICAL
  (diff faithful);
- canonical stack (enqueue → loadmap-r2) applies CLEANLY: combined source
  sha256 `dc68760253ec141f2eec4696050b8a2dd9f01b7f2894829d5f543516fb44f7f4`;
- reverse stack (loadmap-r2 → enqueue) → IDENTICAL combined sha (order-
  independent, as the disjoint hunks implied); canonical order fixed anyway.

The combined SOURCE sha is host evidence for the controller's re-derivation
check; the combined OBJECT comparator still must come from the frozen-argv
gate (A2 in the plan) — it is a NEW sha, and neither single-fix object is
valid for it. All temp dirs were removed; no repo tree was mutated.

## 3. What the plan decides

- ONE ordered chain: A0 sha gates → A1 combined staging → A2 frozen-argv gate
  (control must reproduce `854a476b…` a 5th time; combined comparator NEW) →
  A3 pad-defer BLOCKING gate → B tagged driver copy (ONE edit, NEW id) →
  C regen receipts + verify-only + two serialized full passes with packaging →
  D structural acceptance (incl. the three consumer UND symbols RESOLVED —
  armed-fold pairing law) → E manifest + gate v2 15/15.
- ini: ploader.ini `1216aab6…` and settings v3 `d8faa4ec…` UNCHANGED (the
  candidate replaces the composition-slot bytes; the flow never writes the
  ini; EngineTick default stays — the fixed ladder consumes it and
  marshal-first overrides when armed).
- window: ONE fresh window, game CLOSED, probe window = parallel evidence
  only; GRACE = new window, never re-rating.
- oracle: Tier 0 boot/arm lines + ONE r2 deferral Warning (keypress-
  independent), Tier 1 pump line (keypress-independent; absence = F-1
  evidence), Tier 2 drain via the fixed ladder (ONE OPTIONS press, zero
  `EngineTick callback registration failed`), Tier 3 optional explicit-method
  probe; the W-works `marshal not armed` line is declared UNREACHABLE — its
  appearance is an anomaly to reconcile.
- rollback: per-fix stack re-derivation (with the one unmeasured reverse pair
  named), pad-defer independent revert, deployed-module file swap under the
  readback law, in-session = rebuild not toggle.

## 4. Honesty bounds

- Everything is host-side verified or a spelled-out controller/console step;
  no in-game behavior is proven anywhere; the oracle lines are the only
  launch-11 acceptance evidence and none exist.
- pad-defer has no sha to pin; the compose is BLOCKED on its delivery or an
  explicit recorded waiver — stated, not silently assumed.
- The five WithDelay-family bindings remain unpatched (EL's declared scope);
  F-1 stays OPEN; walk survival in the R3 world stays unproven (this chain
  links the shared prebuilt asmhelper; walkguard absorption is the separate
  probe-window-gated matrix decision).
- This r2 SUPERSEDES the prior same-day attempt in this dir (three-branch
  tree, fixers as pending, W_WORKS-only fold-1 gate); that attempt's tighter-
  than-pinned fold reading is corrected in the plan (WST-5 admits fold-1 on
  seq5 `rx_result.mprotect_rc==0`, i.e. W_WORKS or W_FAILS_R_WORKS). The
  prior attempt's probe-window reads (Option P `d6770f44…` pending pin,
  Option A `fd9c7393…`, xwp `igd1-e030a09eb4d1`, H-1 hazard) are reused with
  attribution to their owning workers.
- No quality score, completion, kernel-behavior, or in-game claim is made or
  inherited; no failed/reserved-job replay; serialized builds and console
  windows belong to the controller/user.
