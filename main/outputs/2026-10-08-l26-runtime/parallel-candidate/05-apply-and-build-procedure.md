# NB control — apply + build procedure (for the parent, after EM lands)

agent-EO, 2026-10-09. READ-ONLY analysis produced patches 01-03; nothing
applied (EM is live-editing UnrealInitializer.cpp). Serialized steps:

1. WAIT for agent-EM's report + final UnrealInitializer.cpp state (tree
   stable, EM's CHANGES rows refreshed per EJ's STEP 0 G2 mirror).
2. Apply 01-no-tostring-bind-guard.patch (`/usr/bin/patch --fuzz=0`,
   context-anchored at the Phase 4c boundaries — the slot-2 "bound at"
   message above and the Phase 5 header comment below). If EM moved the
   boundary context, hand-place `#if defined(MODBRIDGE_L27_NO_TOSTRING_BIND)`
   immediately BEFORE the `// ---- Phase 4c` comment and `#endif` immediately
   AFTER the Phase 4c closing brace — the guarded region is EI's whole
   Phase 4c block including EM's fix inside it.
3. Refresh typescan-r4/CHANGES.json files[] AND changes[] rows for
   candidate/src/UnrealInitializer.cpp (both arrays; EJ §1 discipline).
4. Apply 02-build-variants-nb-env.patch and 03-packaging-nb-env.patch.
5. Plan gates (from the `what/` cwd, exactly like bind_launch STEP 0b):
     build_variants.py --plan          # G2/G2q must be green post-refresh
     L26_BUILD_NB=1 build_variants.py --plan
       # G4-nbtoken-present must list the composed UnrealInitializer.cpp
     l26_packaging_driver.py --plan
     L26_PACK_NB=1 l26_packaging_driver.py --plan
6. Build when a slot frees (directive 4: build the candidate while the
   EM-fixed build's launch test runs — NB's semantics do not depend on
   EM's fix content, only on tree stability):
     archive l26-A/l26-B/staging-copyin-probe/l26-package-A/B per
     bind_launch STEP 1, then:
     L26_BUILD_NB=1 build_variants.py --execute
     L26_PACK_NB=1 l26_packaging_driver.py --prepare --execute
   Gates that must be green: 20/20 static per pass, twins identical
   (G8 NB), imports 207 unchanged, G11 NB≠A, G12 deltas ==
   ['unreal-unrealinitializer'] EXACTLY, NB PRX signed-SELF magic
   4f153d1d, NB PRX != 10b11e437f and != 5ea2c845.
7. Upload/launch ONLY with explicit authorization (prep_modded_launch
   --variant A --prx-path <NB prx>; decrypt-off readback; one launch;
   user-driven; harvest to new files; interpret per 04).
8. Rollback/resident discipline: NB is diagnostic-only. After the NB
   launch cycle ends, restore the production PRX (EM-fixed A build) via
   the normal prep before any further modded play.

Build cost: one twin-pair compile+link (~18 min Mac-local, 16GB-serialized
with other heavy jobs per the standing rule). Zero console contact until
step 7.
