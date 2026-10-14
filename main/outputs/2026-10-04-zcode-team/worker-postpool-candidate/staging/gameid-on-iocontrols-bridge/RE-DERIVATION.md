# RE-DERIVATION — game-identity unit onto the CURRENT IO-controls bridge

worker-postpool-candidate · 2026-10-06 · PROPOSAL-ONLY staging. Mechanical
re-derivation only (patch apply + byte verification); no new unit code, no
new claims, no composition. Admission remains a controller decision with the
per-build gates.

## Why

worker-gameid-unit staged its 4-hunk bridge diff against baseline
`abeaa06a…`. The live bridge has moved twice since (edit-18 write-path
revert → `2f7108f3…`, edit-19 batched IO controls → `497348b8…` = the
current `launch10-build/final-units` pin inside candidate 12fcc41f). The
unit's own addendum verified clean apply only up to `ba305edd…`. The
post-pool candidate needs the gameid tags on the CURRENT bridge or the
undefined-delta/replace machinery would see a stale base.

## What was done (all under this worker dir)

1. Fresh copy of the current bridge verified:
   `497348b8d247d531912594ddaea85482c19131eff3be4ff155932c3d954091a6`.
2. Reproduction control: applied worker-gameid-unit's verified diff
   (`diffs/gameid.abeaa06a-to-012fb485.diff`, copied verbatim) to a fresh
   `abeaa06a…` copy → `012fb485dfe65a23b97a2ae1f22da11d6c1489b074a05c1d601471031c0669cd`
   — EXACTLY the unit's recorded staged sha. The apply procedure is sound.
3. Applied the SAME diff to the current `497348b8…` copy:
   `patch -p4` — exit 0, ZERO rejects, ZERO fuzz offsets.
   Result: `runtime_start_bridge_r14_diag_spanpub.gameid-497348b8.cpp`
   sha256 `41dd0c153b902391fd4448c86bf93db6eca1b6de3d88c777e4ab0663d4ec6e11`
   (2,175 lines vs base 1,696; the diff's own hunk arithmetic: +18/+462/+12/+11 added).
4. Byte-identity of every inserted region vs the verified 012fb485 form:
   - Hunk 1 include block (NID-routing law comment + cstdio/fcntl/time.h): IDENTICAL.
   - Hunk 2 gameid section (SHA-256 impl, pfs parser, bridge_gameid_emit,
     bridge_gameid_status_line, marker-bounded): IDENTICAL.
   - Hunk 3 call site: `bridge_gameid_emit();` at :2014 — sits after
     `supplier->capture();` (spanpub published) and BEFORE the r14
     lease-gap/coverage gate, exactly the admission note's placement law
     ("a refusing boot still leaves its identity evidence behind").
   - Hunk 4 call site: `bridge_gameid_status_line(mod_root);` at :2139 —
     on the start-success path, after state 1 'activated' is durably
     written by the ack sink, immediately before the `MRS_OK` return.

## Honest bounds

- The four inserted regions are byte-identical to the host-tested unit
  (worker-gameid-unit host tests 9/9 + 21/21 + diff round-trip cover these
  exact bytes), so the host evidence CARRIES to this staging. What does NOT
  carry automatically: the composition-level gates — undefined-import
  surface on the NEW combined TU, the 196-strong import audit, object
  measurement, determinism twin. Worker-gameid-unit §7 expected added UND
  = ∅ (fopen/fread/fclose, open/write/close, clock_gettime already in the
  frozen contract; O_* header constants) — that expectation must be
  RE-VERIFIED at compose time on 41dd0c15, never assumed.
- The b980cea7↔variant ini pairing, the analyzer_v2 consumption law and the
  `/app0` honesty design are the unit's own (§3-§5 of its admission note)
  and are unchanged by this re-derivation.
- Consumer follow-up still open (shared lineage, reported by the unit):
  analyzer_v2 `parse_status()` does not yet recognize the `gamedata\t` row.
