# REPORT.md — worker-multi-mod, M2 multi-mod lifecycle

worker-multi-mod, 2026-10-04. Task: design the N-mod lifecycle (install/toggle/
uninstall state machine + conflict classification) — M2 of
`worker-product-roadmap/PRODUCT-ROADMAP.md` — and prototype it as a
host-testable C module.

## What was done

1. **Read the basis.** `worker-7-installer/src/modinst.c` (single-mod
   transaction slice: states downloaded/prepared/installed, TSV records,
   journal stage/swap + recovery, priority-then-id load order via the vendored
   composer, requirement/framework blockers) and the roadmap M2 section
   (N-mod install/enable/disable/update/uninstall, four-way classification,
   per-mod records transitions, mod list UI, ≥3-mod exit criterion).

2. **Designed the lifecycle** — `DESIGN.md`, answering the five task items:
   - a. State machine: per-mod stored states
     `{downloaded, prepared, installing, installed_enabled,
     installed_disabled, failed}` (`running` stays derived-only, as modinst
     already treats it); the transaction unit remains the SET (one journal,
     one root swap); full transition table with refusal reasons; dependency
     rules for "A depends on B" including disable/uninstall blocking by
     *enabled* transitive dependents and force-cascade for disabled ones.
   - b. Conflict classification: Supported / Needs conversion / Unsupported /
     Failed with concrete detection rules (framework maturity from the caps
     table, family verdict, shared-destination-path policy `last-wins` vs
     `refuse`); Failed is evidence-only (runtime ack or recovery), never
     statically predicted — `mm_class_merge` makes a runtime Failed dominate.
   - c. Load order: Kahn topological sort (dependency before dependent) with
     (priority, id) tie-break among ready nodes; cycle → honest refusal.
     Determinism proven across all 6 request permutations. Integration path:
     `mfx_compose_ordered` or refuse-on-divergence until it lands.
   - d. Records: TSV `mod` line extended append-only to 10 columns
     (version, priority, layout, reason, deps) — old parser reads new lines,
     new parser reads old lines, proven by a legacy fixture test; game row
     (generation + loadlist sha) stays the set-level truth; single-writer
     per title; recovery reconciles from the live load list + per-mod dirs.
   - e. UI states: per-row derived state + legal-action bits from the same
     guards the ops enforce (Not Installed / Ready / Updating / Enabled /
     Disabled / Failed + offered buttons), actionable per-mod reason strings.

3. **Prototyped the state machine** — `multi_mod_lifecycle.h` /
   `multi_mod_lifecycle.c`: dependency-free C99, pure (no FS, no engine),
   ~750 lines. Ops: declare, prepare, plan_install (pure)/install_commit,
   enable, disable, uninstall(force), mark_failed/clear_failure, recover,
   order_set, classify_mod/pair/against_set, ver_cmp, store_save/load,
   ui_state_of/ui_actions_of.

4. **Host-tested** — `test_multi_mod.c`, 184 checks, **0 failures**
   (`cc -std=c99 -O2 -Wall -Wextra`, macOS arm64, exit 0). Scenarios per the
   task: 3-mod install with a refused conflict (shared container path,
   actionable reason naming the path), dependency chain a←b←c (partial
   installs refused naming the missing dep; shuffled orders → identical load
   order), selective uninstall (middle-of-chain refused; leaf uninstalls;
   force cascades disabled dependents to prepared with reasons),
   enable/disable toggle with generation bumps and idempotency, recovery
   against live-root truth, failure hooks, four-way classification, version
   gate (0.9.0 < 1.0.0 refused citing both), capacity gate, cycle refusal,
   records round-trip + legacy parse, UI surfacing, plan purity.

## Defect found and fixed during testing

First run hung: `mm_plan_install` cast the 2D `char target[64][128]` array to
`const char *const *` (two call sites), so `set_contains`/dep checks read the
array's bytes as pointers — UB, caught by sampling the stuck process (stack:
`check_deps_against_set → strcmp`). Fixed by maintaining a lockstep pointer
array (`targetp`); an unused helper was removed (warning). Final build is
warning-clean under `-Wall -Wextra`.

## Deliverables (all under outputs/2026-10-04-zcode-team/worker-multi-mod/)

- `CLAIM.json` — task claim (first written)
- `DESIGN.md` — the a–e design (state machine, classification, order,
  records, UI) with honesty notes
- `INTEGRATION.md` — concrete mapping onto modinst.c (records columns,
  three call sites inside cmd_install, toggle/uninstall endpoints,
  per-mod-dir vs merged layout, failure surfacing, test plan, named gaps)
- `multi_mod_lifecycle.h` / `multi_mod_lifecycle.c` — the prototype
- `test_multi_mod.c` — the host test suite
- `REPORT.md` — this file

## Honest scope

Design + prototype only, per task bounds. No modinst.c/vendored file was
modified (they are read-only inputs here); no console run; nothing M2 is
claimed complete. Open for implementing workers: `mfx_compose_ordered`
vendored change (composition-gate lineage), per-mod byte budgets, real
conflict-corpus families beyond the one exercised case, and the ≥3-mod
console E2E that is M2's actual exit criterion.
