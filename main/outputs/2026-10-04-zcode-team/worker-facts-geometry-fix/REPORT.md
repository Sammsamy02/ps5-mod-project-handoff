# REPORT — worker-facts-geometry-fix

2026-10-05 · host-only delivery · no console contact, no network, no nested
subagents, no canonical file edits. All writes under
`outputs/2026-10-04-zcode-team/worker-facts-geometry-fix/`.

## Verdict

Root cause found, bounded staged patch + diagnostics + fixture proof delivered.
**Not shipped to any device; nothing on target has changed.** The controller
owns staging, rebuild, gate, and any future launch.

## Root cause (one paragraph)

The launch-9 refusal `runtime_start_failed code=1 detail=no observed mapping
interval intersects the module image extent with valid geometry` is produced by
the canonical adapter (`comp6-integration/lmf_to_native_loader_facts.cpp:96,
227-232`) when `derive_readable_ranges` (lines 77-109) finds no readable
interval after clamping the snapshot's `anchor_mappings` into the resolved
image extent. On the launch-9 VmScan route (data-window anchor: base 0x400000,
extent 0xbc602c8 — proven by the anchor_cov/anchor_match block and
`lmf_vm_scan_cap=2` in the log delta), the generic core's anchor rule
(`live_module_facts.cpp:449-458`: mappings CONTAINING the base, cap 8) collected
exactly ONE span — the exec-only LOAD0 text span `[0x400000,0x787c000) prot=4`
— which the D2 protection filter (adapter line 90) correctly excluded. The six
readable spans the bridge-side registration counted (`bridge_lease_reg_spans=6`)
were observed by the same capture but never entered `anchor_mappings`. That is
the whole bridge-6/conversion-0 contradiction. Configured roots and their
attach ordering are NOT involved (the adapter TU never reads the configured_*
fields; attach-after-conversion is the designed Resolution B order).

## Fix (two staged copies; canonical sources byte-preserved)

1. `staging/live_module_facts.geometryfix.cpp` (from the canonical spike core,
   sha 5eff2ad9…): ONE block — on `identity_source == VmScan && vm_scan.chosen
   && chosen_extent != 0`, collect anchor_mappings as the mappings INTERSECTING
   the chosen extent (overflow-checked; cap 8 unchanged); all other routes
   byte-identical. Cross-compiles clean with the pinned prospero SDK (-Werror).
   Unified diff: `staging/live_module_facts.geometryfix.diff`.
2. `staging/runtime_start_bridge_r13.geometrydiag.cpp` (from the canonical r13
   bridge, conversion behavior byte-identical): emits `lmf_extent_base_low/high`,
   `lmf_extent_end_low/high`, `lmf_anchor_count`, `lmf_intervals_found` on BOTH
   the refusal and success paths through the existing diagnostic channel — the
   controller-directed visibility (observed mappings count, extent base/end,
   per-refusal predicate via the already-durable unique refusal messages). No
   new sink, no new imports. Compiles clean with the pinned SDK.
   Unified diff: `staging/runtime_start_bridge_r13.geometrydiag.diff`.

## Fixture proof (host)

`host-test/test_geometry_fix_host.cpp` — REAL core + REAL unmodified canonical
adapter, fake OS ops, launch-9 shapes built from the controller's live-VMMAP
semantics (LOAD0 --X contains the base; LOAD1/LOAD3/LOAD7-fragment at observed
addresses with page gaps; synthetic spans labeled):

- BEFORE (canonical core): **16/16 PASS** — the exact `InvalidReadableMappings`
  refusal with the verbatim launch-9 text reproduced on the observed shape AND
  on a bridge-parity shape carrying SIX readable intersecting spans; the
  conversion input documented as the single exec-only span.
- AFTER (staged core): **26/26 PASS** — conversion succeeds on both shapes;
  intervals 3/6 == readable span counts (bridge parity); exec-only span stays
  excluded (D2 intact); past-extent clamped; `game_executable_path` stays
  EMPTY; configured-roots attach reachable; readable-anchor contrast converts
  on both cores (documents the discriminating input class); LinkMap-route
  containing rule unchanged.

Receipts: `receipts/fixture-before.txt`, `receipts/fixture-after.txt`,
`receipts/fixture-test-pass.txt` (shas of binaries' sources and outputs).

## Honest limits (controller must verify)

1. **Conversion-boundary proof only.** The fixture stops exactly where launch-9
   died. `MakeNativeProgramContext` / ReadLease / PS-scan / UE4SS init are NOT
   exercised; a green conversion does not claim any downstream pass.
2. **Both PRX rebuilds must consume the staged core.** The comp6 driver compiles
   `--extra-unit lmf-live-module-facts=<spike src>/live_module_facts.cpp` —
   redirect that unit to the staged copy; stage the diag bridge as the
   packaging lifecycle unit. The ENTRY PRX's exact launch-9 build recipe was
   not locatable in the dispatched source dirs (the overlay
   `worker-bridge-r13-polish/staging/lineage-overlay-2-r13` is a faithful
   replica; the packaging layer owns the real wiring). If only ONE PRX takes
   the patch, the other keeps refusing with code=1.
3. **PRX attribution caveat.** Per-attempt attribution of the two diagnostic
   blocks (composition 235-ctor vs entry 232-ctor) rests on the controller's
   delta analysis; the root cause is attribution-independent because both
   blocks fail at the same site with the same inputs shape.
4. **8-slot anchor cap is tight.** Launch-9's shape needs 7-8 intersecting
   spans. A layout with >8 intersecting spans silently drops the highest
   (pre-existing cap semantics). Watch the new `lmf_anchor_count`: pinned at 8
   with a truncated top interval ⇒ raise the cap in the same header.
5. **`lmf_intervals_found` counts coalesced ranges** (the adapter merges
   adjacency); `bridge_lease_reg_spans` counts raw spans. They agree only when
   readable spans are non-adjacent (true on the live layout).
6. **Downstream effect on stage 1-2 is intentionally additive**: readable_ranges
   grow within the SAME resolved extent, becoming exactly the D2-aligned R-span
   set the r13 registration page-quantizes. Residual page-boundary lease
   questions belong to worker-lease-coverage-fix, which should treat this as
   its input contract.
7. **No quality or completion claim of any kind**: this is a root-cause fix for
   one refusal site, host-proven at that site only.

## Deliverables index

- `CLAIM.json` — scope, provenance, non-causes, limits (written first).
- `ANALYSIS.md` — refusal site file:line, standalone trace, input-difference
  table, fix rationale + diffs, fixture design, residual risks.
- `staging/live_module_facts.geometryfix.cpp` + `.diff`
- `staging/runtime_start_bridge_r13.geometrydiag.cpp` + `.diff`
- `host-test/test_geometry_fix_host.cpp` (+ `host-test/` objects/binaries kept
  for reproduction; rebuild commands embedded in `receipts/fixture-test-pass.txt`)
- `receipts/` — fixture-before.txt (16/16), fixture-after.txt (26/26),
  fixture-test-pass.txt, two pinned-SDK object cross-checks + compile stderr
  captures.
- `REPORT.md` — this file.
