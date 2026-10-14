# Agent BB — consolidated l24->l25 CHANGESET SUMMARY (Claude packet)

Date: 2026-10-08. Deliverable:
`outputs/2026-10-08-l25-prep/CHANGESET-SUMMARY.md` (this file is the work
report). Trees were treated READ-ONLY; the only writes are this file and the
summary.

## Method (what was actually verified, not just read)

1. Enumerated every row of typescan-r4/CHANGES.json (20), profile/CHANGES.json
   (4), thread/CHANGES.json (0, placeholder) and the copyin-probe overlay (2
   patch targets: composed UnrealInitializer.cpp second hop wired by
   build_variants.py `compose_copyin_probe`; runtime_start_bridge.cpp wired by
   l25_packaging_driver.py outer_bridge substitution — both confirmed in code).
2. Re-hashed every changed candidate in the l25 trees (8/8 typescan-r4 + 4/4
   profile match CHANGES.json) and every original pin (all 24 match: port root
   x5, l24 typescan-r3 x2, l23 thread x2, r5 x1, l23 profile x4, bridge x1) —
   the "originals = l24-consumed paths" one-hop law holds and no upstream tree
   was modified.
3. Computed per-file diff magnitudes (l24-consumed original -> l25 candidate):
   TypeChecker 153, UnrealInitializer 74, UE4SSProgram 14, LuaMod 107,
   UObjectArray.hpp 28, UObjectArray.cpp 433, ClassListener 114, FMalloc
   FunctionBody 94, facts json 79, RuntimeBuildProfile 4,
   CompiledNativeLayoutDeclarations 6, JoinedLayoutPreflight 8, Q-composed +96,
   Q-bridge +72; 12 rows byte-identical.
4. Verified the copyin-probe wiring state: Q's recorded base c4364dd7 was stale
   (typescan-r4 candidate is cac95e48 after O/AO), so the build uses the
   patch-rebased composed file 12c99461... (hash-verified in
   staging-copyin-probe/ and in PLAN.json), not Q's ready candidate 125a0471.
5. Cross-checked counts against 2026-10-08-l25-build-variants/plans/PLAN.json
   (16:26): changes=24, added_headers=4, selected_units=374,
   affected_units=162, retained=212, unit_row_units=12, header consumer census,
   row classifications (no NOT-CONSUMED rows).

## Row counts per theme (26 enumerated entries = 24 CHANGES rows + 2 Q targets)

- Theme 1 crash root-cause fixes (profile offsets, FMalloc vtable map,
  scan-crash containment): 7 rows — 4 profile (056cf45a/7a289aba/900bc7ac/
  32c9856c -> 95d22dbe/09a14712/bf9419dd/1646a99e; earlier-batch item 5 + AS/AA
  engine-binary correction), FMalloc FunctionBody 95488869 -> 2f20d64d (AU
  +8-slot shift), UnrealVirtual506.cpp carried row (AU compile-forcing
  mechanism), TypeChecker.cpp dcebc6e6 -> c05b06a7 (earlier-batch items 1-3 +
  N fixes F1-F3).
- Theme 2 listener ABI + registration hardening: 3 rows — UObjectArray.hpp
  b01bf4ef -> 69a2391a (O/V slot-4 GetAllocatedSize), UObjectArray.cpp
  171560a9 -> ea7221e8 (O hardening + Z19 E1 + AT code-6 guard + AD/AU GMalloc
  logging hunks), ClassListener.cpp a579b272 -> a8e9b6f4 (O dispatch-proof
  markers).
- Theme 3 variant machinery (B gates/switches): no dedicated row — carried by
  UnrealInitializer.cpp (O #if + AO G3 cache-block suppression) and LuaMod.cpp
  (AO G1 FLuaObjectDeleteListener gate) hunks, filed once under their dominant
  themes with full credit; plus the build_variants.py A/B driver (not a row).
- Theme 4 instrumentation: 3 rows + 2 Q targets — UE4SSProgram.cpp
  030407d7 -> 352da81b (P phase tags), LuaMod.cpp fc9589db -> cef8d02f (P
  marshal/EIGT tags + AO G1/G2 hunks), UnrealInitializer.cpp 78c16fb7 ->
  cac95e48 (earlier-batch PublishVerifiedPoolAddress + O markers + AO gate;
  filed under theme 3/1 with cross-refs), Q composed overlay cac95e48 ->
  12c99461 (+96 liveness lines), Q bridge 89e4b643 -> f510c014 (+72).
- Theme 5 hygiene (carried pins, CHANGES discipline, E1/E3): 12 rows — 9
  container rows + CustomProperty.cpp + ScriptHookThunks.cpp (all
  byte-identical carried pins), thread/CHANGES.json placeholder rewrite; E1
  fixed inside UObjectArray.cpp (Z19), E3 enforced by driver gates G12/G12b.

Content deltas vs l24: 12 CHANGES rows + 2 Q targets; 12 rows zero-delta pins.

## Unit-count reconciliation

12 units with direct unit-source rows (unreal-flat-typechecker,
unreal-unrealinitializer, unreal-uobjectarray, unreal-flat-classlistener,
luamod, program, native-script-hook-thunks, lua-custom-property, and the three
property units + unrealvirtual506) + 150 header-only consumers = 162 affected
units recompiled per variant per pass (374 selected, 212 retained).
UObjectArray.hpp alone has 162 consumers = the entire affected set (the slot-4
ABI repair forces full-fleet recompile; mixed-header ODR hazard otherwise), so
every other replaced header's consumers (125 per HeaderWrapper, 1 per
SrcWrapper, 2/1/1 for the profile units) are subsets. The plan's "12 affected
units + 162 consumer recompiles" is therefore exactly explained by the table;
gates G2s/G2t/G12b prove no row no-oped and no consumer was left retained.

## Not-changed confirmations

Port root, l23/l24/r5 originals, engine binaries (image 3b6cfad5 unchanged),
and the Nexus mod main.lua all untouched (verified by hash / absence of any
.lua in l25 parts); copyin GATING not wired (Z15 l26 proposal only);
UObjectDeleteListenersCritical 0x78 verified but deliberately unpublished.

## Caveats

Build status at snapshot: PLAN green (16:26); variant-A execute in progress
from 16:27 (pass1 objects); l25-B, twin/pair receipts, packaging and all
console proof pending — the summary makes no built-runtime or tested-runtime
claim. LuaMod/UE4SSProgram originals are the l23 thread sources because that is
what the l24 composition pinned (thread/CHANGES.json scope), not an l24
candidate. Report totals: 26 entries, 14 content deltas, 12 carried rows.
