# Agent FX — parked P1/P2 patch sets (fault-ledger desaturation; variant-B / dead-R3 removal)

2026-10-10. READ-ONLY analysis; proposal patches only, nothing applied. No
console/network contact, no builds, no driver `--execute`, no live-tree,
driver, or CHANGES edits (verified post-session: every touched live file
still hashes to its pre-session value). Deliverables:

- `parked/P1-fault-ledger/01-fault-ledger-desaturation.patch` + `NOTES.md`
- `parked/P2-variant-b-removal/{01-build-variants-drop-b,02-tree-dead-code,03-packaging-driver}.patch` + `NOTES.md`

## P1 — fault-ledger desaturation (4 hunks, one patch)

Mechanism found and documented honestly: EO §0's candidate (record()
saturates on an invalid range; overlaps() returns true when saturated) is a
REAL latent false-positive class in `l13_sink::FaultLedger`
(CopyinSink.hpp:22-41, sha 2018c9bd identical across every l13→l26
lineage) — but the 15:36Z [Seeds][L27] refusal specifically was the
owner-gate fail-closed path in `bridge_copyin_range_faulted` (bridge line
1200), already root-caused by EM and fixed in-tree
(`typescan-r4/CHANGES.json` `l27_ei_stringfix`). P1 therefore hardens the
ledger itself, defense-in-depth, and does NOT touch the fail-closed line
(byte-verified unchanged).

Patch content per Claude spec: (a) record() rejects invalid ranges loudly
(RecordResult enum + rejected_invalid counter + one-shot
`copyin_fault_ledger_reject_invalid` event) and never saturates on them;
(b) overlaps() no longer consults `saturated` — capacity saturation is
queryable only via new `saturated_capacity()` accessor the admission gate
never calls (one-shot `copyin_fault_ledger_full` event when capacity hits);
(c) dual self-test: compile-time `static_assert(self_test_mask()==0xF)` in
the header (host clang++ -std=c++20 verified: syntax + runtime run PASS)
plus a one-shot runtime `[ST]` line `copyin_fault_ledger_st=15` /
`_st_pass=1` from `bridge_copyin_directory_prepare`, before any admission
activity. Real recorded faults still refuse; invalid QUERY ranges still
fail closed.

Wiring so the fix actually reaches the PRX: new candidate
`copyin-probe/candidate/CopyinSink.hpp` (sha 0b113b1c) + MANIFEST pin
update (bridge 94f81a17→a35c7f91, new base_files.copyinsink) + EQ-pattern
`lineage_overlays` destination `CopyinSink.hpp` in l26_packaging_driver
(hard error if the manifest entry is missing — never silently stage the
frozen saturating header). Console validation signature and the
distinguishing evidence for any future [Seeds][L27] refusal are in NOTES
§3.

## P2 — variant B dead-by-default + dead R3 marshal paths

Investigation results (task instruction: investigate first):

- Skip-token census: exactly 4 arms — UnrealInitializer.cpp ×3 (G3
  cache-block gate, class-delete guard, shutdown-listener guard) +
  LuaMod.cpp ×1 (Lua-mod delete-listener). UObjectArray has NO B arm (only
  production CTOKEN variant-C arms — untouched).
- Provably-dead R3 code REMOVED: (1) the ranked-natives carrier loop in
  `r3_marshal_calibration_consumer` — dead BY CONSTRUCTION (both ranked
  names are BlueprintImplementableEvents the FUNC_Native precheck refused
  on every world; the code's own comment admitted it) AND dead on console
  (GATE-1: StaticFindObject cannot resolve; readfix-session evidence);
  (2) its only-callee helpers `arm_funcptr_marshal_drain` +
  `r3_marshal_carrier_precheck`; (3) `R3MarshalQueryHarnessDispatch`
  (declared+defined, zero call sites — agent-H finding re-verified).
- DELIBERATELY KEPT, flagged for Claude: the Blueprint-override
  event-carrier family + `funcptr_marshal_hook` + `r3_marshal_lazy_arm`.
  Console-dead today (same GATE-1 base-lookup; VmtPump is the proven
  primary pump) but explicitly designated the FALLBACK pump by the
  Claude-approved EU FIX4 integration comment — removing it is a
  resilience-policy decision, not dead-code hygiene; the removal boundary
  is now clean for a one-hunk follow-up if Claude wants it gone.

Patch set: 01 makes B env-opt-in-dead (`L26_BUILD_B=1`, default off) in
build_variants.py (pair receipt/G9/G10/G4-token gates conditional; NB
env-append mechanism preserved); 02 removes the 4 skip arms + 3 dead R3
sites with `[L26][P2]` tombstones and refreshes both CHANGES.json rows
(changes[]+files[]) with an `fx_p2_row_sync` note; 03 mirrors B-dead in
l26_packaging_driver.py (`L26_PACK_B=1`; P10/P11 + pair receipts
conditional; adds `import os`, which also fixes EO's NB packaging patch's
implicit os.environ use). NOTES §3 is the full downstream census: pair
receipts no longer written on default runs, historical l26-B*/package-B*
dirs are frozen provenance never touched, EO NB patch rebase anchors,
rollback = reverse-order `patch -R`.

## Validation performed (all local, Mac-on-battery safe)

- Sequential apply of P1→P2/01→P2/02→P2/03 in a /tmp tree:
  `/usr/bin/patch -p1 --fuzz=0` clean, zero rejects, zero fuzz.
- Hash round-trip: all 7 patched files byte-match the constructed intent
  (shas in P2 NOTES §0); both drivers `py_compile` clean.
- Q's copyin-probe overlay verified to apply cleanly (`--dry-run`, rc=0)
  on the patched UnrealInitializer.cpp — G2q composition intact.
- Token census post-P2: 0 occurrences in any compiled source.
- Symbol census: zero remaining code references to the removed R3 symbols;
  preprocessor and brace balance preserved.
- Live tree verified untouched post-session (all shas pre-patch; no
  CopyinSink candidate in the live tree).

## Honest limits

Static analysis only — no target compile, no build, no packaging run, no
console session (also avoided `--plan`, whose stale-stamp cleanup would
have edited the live tree). Root must run `build_variants.py --plan` and
`l26_packaging_driver.py --plan` after applying, before any `--execute`.
P1 changes admission-adjacent behavior (needs a console session with the
ToString-bind context); P2 changes build topology (needs full rebuild +
packaging verify) — both carry NEVER-APPLY-WITHOUT flags in their NOTES.

— agent FX (ZCode), 2026-10-10.
