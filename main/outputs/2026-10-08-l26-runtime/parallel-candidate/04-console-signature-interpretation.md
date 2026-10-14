# NB (no-bind) control — console signature interpretation table

agent-EO, 2026-10-09. Companion to 01-03 patches. For the authorized NB
launch ONLY (one launch, user-driven, in-world, L3+R3 once, clean exit;
zzdiag v8 1488 B auto-prober resident; klog + UE4SS.log harvest to NEW
files). Baseline being compared against: tonight's 10b11e437f session
(2026-10-09 15:36:21Z eboot start → 15:37:43 UE4SS init + Phase 4c refusal
+ "Stopping mod ... for uninstall" with ZERO post-L15 init logging →
15:39:21Z eboot gone).

## Fixed expectations (in EVERY healthy NB outcome)

| marker | expectation | meaning |
|---|---|---|
| `[Seeds][L27] ...` (any) | ABSENT | Phase 4c compiled out — the control is live |
| `[FN] tostring-bind ...` (any) | ABSENT | same |
| `[Seeds] slot 2 FName ctor bound at 0x63d590 ...` | PRESENT | slot-2 unaffected |
| `[Seeds][L26] engine delete-listener ops admitted` / `seed_refused` | one of them PRESENT | variant C Phase 6 runs (A-shaped); loud refusal = separate variant-C problem, capture addresses, do not retry |
| `[L25][DeleteListener] ...` registration markers | PRESENT (unless C refused) | registration layer unchanged |

## The discriminator: how far init gets

| # | observation | interpretation | next action |
|---|---|---|---|
| A | Init completes: `[l25]` phase ladder, `[L22]`/`[l22] Lua mod started`, registration markers, L3+R3 → `pad_l3r3_edge` → `pad_l3r3_f7_dispatched` | **Phase 4c's mere presence (even in refused state) caused tonight's silent init abort.** The admission/string layer is exactly EM's fix target; runtime otherwise healthy | EM's fixed-bind build is the cure; expect its refusal path (if any) to be non-fatal once EM lands |
| B | Init aborts identically: last lines `[Seeds][L15] ... outcome=verified-...` then `Stopping mod ... for uninstall`, nothing after | **Phase 4c EXONERATED for the abort.** The abort is in the OTHER new code this build carried: variant C Phase 6 admission (EE), Z15 gated bridge early-release, or the composition itself | Next control: variant-C-off build (CTOKEN undefined BUT hand-rolled path is known-broken — use the l26-B skip-listener diagnostic instead, one launch), and re-read the 10b11e437f EXECUTION receipt vs 5ea2c845 deltas |
| C | Init aborts EARLIER (before slot 2 / L15 lines) | not the Phase 4c question at all — tree/binary drift between 10b11e437f and NB | stop; reconcile build inputs (G2/G2q pins, STAGED-SOURCE-PINS) before any further launch |
| D | Crash (CE-108255-1 / klog SIGSEGV) instead of clean uninstall path | different failure class | harvest immediately, REBOOT, no relaunch until reconciled |

## String/admission layer signatures (in outcome A, in-world)

| marker | expectation with bind absent | meaning |
|---|---|---|
| `[FFO] query-fallback [n:X num:Y ts:'...']` | PRESENT, one per probed class; **ts tail EMPTY (`ts:''`)** | DA's verify runs, port ToString (legacy Kismet route) returns empty → round-trip mismatch → fallback engages; the empty tail directly names the Kismet route as the liar, in-binary, same family as tonight's refused build |
| `[FFO] add-fallback [n:X]` | ABSENT for known classes | Find construction still hits (N1-free at construction); if it FIRES for PlayerController/GameEngine → GATE-1/pool-admission family, different fix |
| zzdiag v8 `T<n> db=?` / FindFirstOf/FindAllOf tokens | `db=n` / all-invalid | EXPECTED — the bind is the decisive FindXOf repair; NB must not fix this |
| `GetEngine`/`F1r/F2r/A1r` | invalid | same |
| U1 (UEHelpers require) | works (`y`) | unchanged vs every prior session |

## EC StaticFindObject arm signatures (bind-INDEPENDENT — what NB additionally proves)

| observation | interpretation |
|---|---|
| S-probes (`StaticFindObject("/Script/Engine.PlayerController")` etc.) return VALID; U2 CacheDefaultObject works | **EC's fix resolves end-to-end without the bind** (splitter + FNAME_Find parts + index-walk + class gate). StaticFindObject path SOLVED; the ToString bind then only matters for FindXOf/marshaling |
| S-probe returns invalid (n) quietly, no refusal text | walk clean-miss: either N1 is real for these names (constructed Find index ≠ stored index — cross-check: did `[FFO] query-fallback` fire this session? same-index-family question) or the object genuinely absent |
| S-probe pcall ERROR `[StaticFindObject] long-name part 'X' not in FName pool (FNAME_Find miss); refusing to scan` | splitter worked; the named PART is not interned (dot-loss on the bridge survives in the part, or pool-admission gap at part level). The part name is the new evidence |
| S-probe pcall ERROR `GetPackageNameFromLongName: Name wasn't long.` | EC's fix is NOT in the shipped binary (composition/rows drift) — STOP, reconcile, never relaunch that PRX |

## What NB can never prove

- It cannot fix FindXOf/db (that is the bind's job — EM's build).
- It cannot distinguish "Kismet route empty" from "Kismet route absent" at
  the DA-verify site beyond the `ts:''` shape (harness EI2 already proved
  the empty case; NB is the in-binary corroboration only).
- If outcome B fires, NB alone cannot name WHICH non-4c change aborted
  init (variant C vs Z15 bridge vs composition) — it only confines the
  question, cheaply and loudly.
