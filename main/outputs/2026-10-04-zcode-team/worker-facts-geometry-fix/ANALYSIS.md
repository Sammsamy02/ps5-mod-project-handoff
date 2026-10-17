# ANALYSIS — worker-facts-geometry-fix (launch-9 facts-adapter geometry refusal)

2026-10-05 · host-only analysis + bounded staged patch · no console contact, no
network, no canonical file edits. Deliverable order per convention: CLAIM.json
first, this analysis, staged patches, fixtures + receipts, REPORT.md.

## 0. Answer up front

**Root cause (both PRXs, composition-confirmed):** the conversion-side geometry
inputs are derived from the supplier snapshot's `anchor_mappings`, and the
generic core collects those as **the mappings CONTAINING the anchor base**
(`live_module_facts.cpp:449-458`). The launch-9 identity route is the r13
bridge's data-window anchor: `chosen_base = 0x400000`, `chosen_extent =
0xbc602c8` (VmScan, proven by the in-log `anchor_cov/anchor_match` block +
`lmf_vm_scan_cap=2`). The ONE VMMAP span containing 0x400000 is the **exec-only
LOAD0 text span `[0x400000,0x787c000) prot=4`** (trusted in-game record, pinned
in the r13 bridge comment; the IGW probe's far-page row at 0x7878000 prot=4
corroborates). The comp6 adapter's D2 Diff A protection filter
(`lmf_to_native_loader_facts.cpp:90` — `(protection & 1u) == 0` skipped, kept
OUT of leased readable ranges) then correctly drops that single span,
`derive_readable_ranges` finds nothing, and the adapter refuses at lines
228-232 with the exact launch-9 text. The bridge-side lease registration sees 6
readable spans (`bridge_lease_reg_spans=6/reg=6`) because it walks the FULL
fresh VMMAP against the extent — those readable spans were observed by the very
same capture but never reached `anchor_mappings`.

**The contradiction resolves exactly here:** bridge-side (full VMMAP vs extent)
= 6 readable spans; conversion-side (1 containing span, exec-only, filtered) =
0 intervals. Same attempt, same process, two different input sets.

**What it is NOT:**
- *Configured roots / attach ordering is NOT the bug.* The canonical adapter TU
  never references `configured_game_directory`/`configured_working_directory`
  (grep: zero hits in `comp6-integration/lmf_to_native_loader_facts.cpp`);
  conversion validates only the `allow_configured_roots_for_missing_executable_path`
  OPTION (`.hpp:139`), and `ValidateNativeProgramContext` consumes the attached
  fields later (Stage 2). Attaching before conversion is impossible by design —
  the facts object does not exist until conversion succeeds (Resolution B 2.5
  attaches to the RESULT). `bridge_cfg_gamedir/workdir` absent from the log is
  the downstream symptom of the geometry refusal, not a cause.
- *The extent source is NOT empty/zero.* `lmf_extent` math (adapter lines
  180-196) produced the valid `[0x400000, 0xc0602c8)` span; the span check at
  line 221 passed, which is why the failure is the INTERVAL refusal, not
  `InvalidModuleImageSpan`.
- *The D2 filter is NOT wrong.* Re-admitting the exec-only span would rebuild
  the launch-8 failure (conversion passes, lease walks exec-only pages that the
  registration deliberately excludes). The filter needs the readable spans,
  which is what the fix supplies.

## 1. The refusal site (task item 1, file:line)

Canonical adapter `outputs/2026-10-04-zcode-controller/comp6-integration/lmf_to_native_loader_facts.cpp`:

- `derive_readable_ranges` — lines 77-109. Inputs: `snapshot.anchor_mappings[0..anchor_mapping_count)` (≤ `kMaxAnchorMappings = 8`, `.superpowers/.../include/live_module_facts.hpp:88`), the resolved `extent`. Per-mapping rules: skip empty/inverted (`end <= start`, line 84); **skip non-readable (`(mapping.protection & 1u) == 0`, line 90 — the D2 Diff A filter)**; clamp `start = max(mapping.start, extent.base)` / `finish = min(mapping.end, extent.end)` (lines 91-92); skip non-intersecting (`finish <= start`, line 93); sort + coalesce overlaps AND adjacency (lines 97-107); **false when nothing remains (line 96)**.
- The throw/refusal — lines 227-232: `InvalidReadableMappings`, message
  `"no observed mapping interval intersects the module image extent with valid geometry"`
  — mirrored verbatim through the r4 entry's `write_ue4ss_log_line`
  (`runtime_module_entry_r4.c:184-222`, the `[t=44372082ms pid=273]` line in
  `UE4SS-log-final.txt:45`) and the runtime-status detail string
  (`runtime-status-final.txt`: `failed 1 runtime_start_failed no observed
  mapping interval ...`, session 273).
- Extent resolution — lines 174-225: `module_info` span → `vm_scan`
  `chosen_base + chosen_extent` → `observed_base` opt-in. The launch-9 route is
  the middle one (`LmfImageBaseProvenance::VmScanBase`).

## 2. The entry's standalone call path (task item 2)

The launch-9 entry PRX embeds the SAME chain as the composition — entry r4 +
r13-era bridge + comp6 adapter + unpatched supplier core (the r13-only
`bridge_lease_reg_spans` diagnostic appears in its log block; the core is the
spike `src/live_module_facts.cpp`, the comp6 driver's `--extra-unit
lmf-live-module-facts` source; the comp6 change manifest lists NO core change).

Standalone sequence at launch-9 (entry-log snapshot lines 18654-18707, and the
same terminal shape attributed to the composition chain per the controller's
ctor-count delta):

1. `bridge_register_readable_ranges()` (r13:584, BEFORE capture): fresh VMMAP
   (16384-entry bridge buffer; 10019 records walked clean) vs
   `[kCandidateBase, kCandidateBase+kImageExtentVaddr)` = `[0x400000,
   0xc0602c8)` → **6 readable spans, 6 registered** (page-quantized).
2. `supplier.capture()` (r13:629): `bridge_query_vmentry` (vmentry Ok, 10019
   records; core truncates to 1024); `executable_path` refused (source=0);
   link map refused (g2); exec handle refused g1 (`lmf_provider_code=0xfffff05f`
   = −4001 = `kRefusalExecHandleRawZeroIngame`); header scan finds no candidate
   (documented in-game reality: the eboot header span is X-only, and the V2
   scan enumerates readable spans only — `live_module_base_scan.cpp:178-194`)
   → the **data-window anchor fallback** verifies B,D,E,(C||C2) and returns
   `chosen_base=0x400000, chosen_extent=0xbc602c8` → `lmf_identity_source=2`.
3. The core sets `observed_base = chosen_base` (`live_module_facts.cpp:303-307`)
   and collects `anchor_mappings` = mappings CONTAINING 0x400000 → **exactly
   one span: `[0x400000,0x787c000) prot=4`**.
4. Conversion with the r13 options (`allow_vm_scan_base_as_module_image_base`,
   `allow_configured_roots_for_missing_executable_path` — r13:634-643): status
   gate passes (VmScan-identity `ModuleInfoNotObtained`), path gate passes
   (opt-in, path stays EMPTY), mapping-count gate passes (count=1), extent
   `[0x400000, 0xc0602c8)` passes, **interval gate fails: the only anchor span
   is filtered by protection → refusal (lines 228-232)**, `MRS_ERR_CONVERSION`
   (code=1). `bridge_cfg_gamedir/workdir` (r13:668-671) never reached.

## 3. Why the composition passed conversion earlier and both failed at launch-9 (task item 3)

Input-difference classes, exhaustive:

| input | bridge side (registers 6) | conversion side (derives 0) | difference? |
|---|---|---|---|
| mappings source | its own fresh `bridge_query_vmentry` walk, full 16384-entry buffer | the CORE's capture: same op, truncated to the core's 1024 and reduced to ≤8 CONTAINING-base spans | **YES — the load-bearing difference** |
| extent | `kCandidateBase + kImageExtentVaddr` constants | adapter-resolved `vm_scan.chosen_base/chosen_extent` — the SAME numbers via the window anchor | no (equal at launch-9) |
| module handle | n/a | none (`ModuleInfoNotObtained`, g1 refusal) | no effect on this check |
| configured roots | attached after conversion | never reached | downstream symptom only |
| protection filter | registration keeps readable only (R) | adapter keeps readable only (D2 Diff A) | identical predicate — but applied to DIFFERENT span sets |

The composition's earlier launches that PASSED conversion (launch-8's pre-D2
adapter) did so by keeping the exec-only span — which then failed the lease
read (`Owned read range includes unregistered bytes`, code=2). With the D2
filter present, the SAME core input produces the launch-9 interval refusal
(code=1). Both terminal states are two faces of the same geometry defect: the
conversion-side span set never contained the readable spans. The host fixture
(case "readable-anchor-contrast") also demonstrates the complementary class:
when the scan's chosen base lies in a READABLE containing span, the old rule
converts on both cores — the anchor's containing-mapping protection is the
discriminating input.

## 4. The fix (task item 4, bounded, staged — canonical files untouched)

Two staged copies under `staging/` (canonical sources byte-preserved; shas in
CLAIM.json):

### 4.1 `live_module_facts.geometryfix.cpp` (the fix)

Staged copy of `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp`
(sha 5eff2ad9…). ONE block changed — the anchor collection
(capture(), lines 443-459): when `identity_source == VmScan && vm_scan.chosen
&& chosen_extent != 0`, collect `anchor_mappings` as the mappings
**INTERSECTING `[chosen_base, chosen_base+chosen_extent)`** (overflow-checked
via the TU's existing `checked_add_u64`; on overflow the containing rule
applies unchanged). Every other route (LinkMap/name) keeps the byte-identical
containing rule. Still integers-only, still capped by `kMaxAnchorMappings=8`
(VMMAP order, lowest first). The adapter then derives exactly the readable
spans of the anchored image — at the launch-9 shape the same readable spans the
r13 registration counted — so the D2 filter keeps them instead of emptying the
set. Nothing is fabricated; the scan's own extent defines the geometry.

Why not adapter-side: the snapshot carries at most 8 containing-base spans and
NO full mapping list, so no downstream change can honestly recover the readable
spans. Why not bridge-side: the core, not the bridge, selects the anchor set.
Why not weaken the D2 filter: it encodes the lease's real R-per-page contract.

```diff
--- a/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp
+++ b/staging/live_module_facts.geometryfix.cpp
@@ -443,17 +443,50 @@
         if (facts_.observed_base != nullptr) {
             // Works for both routes: rtld-reported base or the span-validated
             // VmScan chosen base (the adapter plants images[MainExe].base
             // from the scan result via identity_source/vm_scan).
+            //
+            // worker-facts-geometry-fix (launch-9, 2026-10-05): on the VmScan
+            // route the conversion adapter derives readable_ranges by
+            // clamping THESE anchor mappings into
+            // [chosen_base, chosen_base+chosen_extent) and requires the
+            // kernel readable bit (comp6 D2 Diff A). The containing-base-only
+            // rule collected exactly ONE mapping — the span holding the ELF
+            // header — which on the launch-9 target is the EXEC-ONLY text
+            // span ([0x400000,0x787c000) prot=4, trusted in-game record).
+            // The filter then correctly dropped it and conversion refused
+            // with "no observed mapping interval intersects the module image
+            // extent with valid geometry" while the READABLE data spans of
+            // the SAME anchored image sat one entry away in the same
+            // capture. On the VmScan route the anchor mappings are therefore
+            // the mappings INTERSECTING the chosen extent — the honest
+            // anchor geometry the scan itself established; every other route
+            // keeps the exact containing rule. Integers only; never
+            // dereferenced; still capped by kMaxAnchorMappings (VMMAP order,
+            // lowest addresses first). On checked-overflow of the extent end
+            // (never observed) the containing rule applies unchanged.
             const uint64_t base =
                 reinterpret_cast<uintptr_t>(facts_.observed_base);
+            uint64_t scan_end = 0;
+            bool intersect_mode = false;
+            if (facts_.identity_source == IdentitySource::VmScan &&
+                facts_.vm_scan.chosen && facts_.vm_scan.chosen_extent != 0) {
+                intersect_mode =
+                    checked_add_u64(base, facts_.vm_scan.chosen_extent,
+                                    &scan_end);
+            }
             for (size_t m = 0; m < vm_copied; ++m) {
-                if (mappings[m].start <= base && base < mappings[m].end &&
-                    facts_.anchor_mapping_count < kMaxAnchorMappings) {
+                const bool hits = intersect_mode
+                    ? (mappings[m].start < scan_end &&
+                       base < mappings[m].end)
+                    : (mappings[m].start <= base && base < mappings[m].end);
+                if (hits &&
+                    facts_.anchor_mapping_count < kMaxAnchorMappings) {
                     VmMapping vm{};
                     vm.start = mappings[m].start;
                     vm.end = mappings[m].end;
                     vm.protection = mappings[m].protection;
                     facts_.anchor_mappings[facts_.anchor_mapping_count++] = vm;
                 }
             }
         }
```

(The full unified diff with the comment block verbatim is
`staging/live_module_facts.geometryfix.diff`.)

### 4.2 `runtime_start_bridge_r13.geometrydiag.cpp` (controller-directed diagnostics)

Staged copy of the canonical r13 bridge (conversion behavior byte-identical)
emitting the controller-required events through the EXISTING diagnostic channel
(no new sink, no new imports), on BOTH paths:

- conversion REFUSAL (alongside the existing `lmf_*` cap events):
  `lmf_extent_base_low/high`, `lmf_extent_end_low/high` (from the snapshot's
  `vm_scan.chosen_base` + `chosen_extent` — the inputs the predicate saw),
  `lmf_anchor_count` (the snapshot's `anchor_mapping_count`),
  `lmf_intervals_found = 0`.
- conversion SUCCESS (alongside `bridge_cfg_*`): same extent events from the
  adapter's `resolved_image_base/end`, `lmf_anchor_count`,
  `lmf_intervals_found = readable_ranges.size()`.

The per-refusal PREDICATE itself is already durable: every adapter refusal
carries a unique static `error_message` that the existing plumbing relays
verbatim into the r4 UE4SS mirror line and the runtime-status detail string —
the four new events name the INPUTS behind that predicate. 64-bit values follow
the entry's established `_low/_high` event convention. Compiled clean with the
pinned SDK (receipts/runtime_start_bridge_r13.geometrydiag.o).

## 5. Fixture proof (task item 5)

`host-test/test_geometry_fix_host.cpp` drives the REAL core + the REAL
unmodified canonical adapter with fake OS ops (the worker-supplier-ingame-update
host-test pattern). Shapes follow the controller's live-VMMAP semantics
(LOAD0 --X contains the base; LOAD1/LOAD3/LOAD7-fragment spans at observed
addresses with page gaps; readable spans NON-adjacent because the adapter
coalesces adjacency — `lmf_intervals_found` counts coalesced ranges, while
`bridge_lease_reg_spans` counts raw spans; equality holds when gaps exist, as
they do live).

- BEFORE (canonical core): **16/16 PASS** — reproduces `InvalidReadableMappings`
  + the verbatim launch-9 message on BOTH the observed shape and the
  bridge-parity six-readable-span shape; documents that the whole conversion
  input was the single exec-only span.
- AFTER (staged core): **26/26 PASS** — conversion succeeds on both shapes;
  intervals = 3 (observed) / 6 (parity) == the readable span counts;
  exec-only excluded, past-extent clamped, `game_executable_path` stays empty,
  configured-roots attach reachable, contrast + LinkMap regression guards pass.
- Receipts: `receipts/fixture-before.txt`, `receipts/fixture-after.txt`,
  `receipts/fixture-test-pass.txt`. Staged-copy cross-compiles with the pinned
  prospero SDK: rc=0 (`receipts/*.o`).

## 6. Residual risks / controller-verify list

1. **kMaxAnchorMappings=8 is tight.** The launch-9 shape needs 7-8 intersecting
   spans; a future layout with >8 silently drops the highest (pre-existing cap
   semantics, now first-come-by-address). If a launch log ever shows
   `lmf_anchor_count=8` with a truncated top interval, raise the cap in the
   same header.
2. **`lmf_intervals_found` vs `bridge_lease_reg_spans` can legitimately differ**
   when readable spans are page-adjacent (coalescing). Compare against the
   registration list with that in mind.
3. **Downstream effects on the composition's passing stage-1 are additive
   only**: the adapter's readable_ranges grow WITHIN the same resolved extent
   (all observed readable spans of the anchored image, D2-filtered). Stage 2's
   ReadLease now walks exactly the R spans the r13 registration page-quantizes —
   the intended D2-aligned input; the remaining page-boundary questions belong
   to worker-lease-coverage-fix.
4. **Both PRX builds must take the staged core**: the comp6 driver compiles
   `--extra-unit lmf-live-module-facts=<spike src>`; redirect that unit to
   `staging/live_module_facts.geometryfix.cpp` (and stage the diag bridge as
   the packaging lifecycle unit) for whichever PRXs rebuild. The ENTRY build's
   exact launch-9 recipe was not present in sources under the named dirs — the
   controller's packaging layer owns that wiring; the r13 overlay
   (worker-bridge-r13-polish/staging/lineage-overlay-2-r13) is the faithful
   replica for it.
