# ANALYSIS — lease coverage, the launch-8 throw, and the r14 fix

Worker: worker-lease-coverage-fix · 2026-10-05 · local sources only
Deliverable: `runtime_start_bridge_r14.cpp` (this dir) — a minimal diff on r13; original files untouched.

---

## 1. Throw-site mechanics (the real code, file:line)

The composition PRX's owned-memory service is
`.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/src/OwnedMemory.cpp`
(sha `d824528d…c3edbf`, byte-identical to the composition-6 run-1 receipt's
`native-owned-memory` unit — CLAIM C1). The observed detail text comes from
exactly one line:

```cpp
// OwnedMemory.cpp:120-126 (ReadLease::Impl ctor)
while (current < end) {
    const auto* e = covering(state(), current, 1);
    if (!e) throw std::invalid_argument("Owned read range includes unregistered bytes"); // :122
    ...
    if (!(protection & R)) throw std::invalid_argument("Owned read range is unreadable"); // :126
    current = clipped_end;   // advances to min(range end, entry end)
}
```

Mechanics that matter:

- `ReadLease` **never registers** anything (OwnedMemory.hpp:64-70); the ranges
  handed to it must already be covered by `ScopedRegion`/`OwnedMapping`
  registrations, and `covering()` (OwnedMemory.cpp:45-49) requires a SINGLE
  entry to contain the current byte. Coverage must therefore be **contiguous
  across entry boundaries** from each range's start to its end — any gap,
  however small, throws at :122.
- `ScopedRegion` registers `[base, base+size)` with ONE protection flag and
  **throws `std::invalid_argument("Owned regions must not overlap")` on any
  overlap** (OwnedMemory.cpp:53-54, ctor :207-211). Outward page-quantization
  of adjacent records can only produce touching (half-open) regions — which
  PLH accepts — but true overlaps throw.
- `ReadLease` also refuses unsorted/overlapping request ranges (:110-118) and
  empty ranges (:108).

## 2. The caller and the EXACT lease ranges (not the whole extent)

There is exactly **one** `ReadLease` construction in the launch path
(CALL chain, all file:line in CLAIM C3):

```
modbridge_runtime_start_with_activation           (bridge, Stage 2)
└─ RC::Platform::MakeNativeProgramContext         NativeLoaderContext.cpp:7-23
   └─ make_shared<OwnedMappedScanSource>(...)     NativeLoaderContext.cpp:21
      └─ OwnedMappedScanSource ctor               OwnedMappedScanSource.cpp:10
         └─ OwnedMappedImageCaller ctor           OwnedMappedImageCaller.hpp:52-56
            ranges_ = source->ReadableRanges().at(ScanTarget::MainExe)   :14-20
            lease_(ranges_)                                              :55
```

`ReadableRanges().at(MainExe)` is **not** the whole image extent. It is built
by the conversion from the supplier's FROZEN capture:

1. `capture()` makes **one** `query_vmentry` call and never re-queries
   (live_module_facts.cpp:200-211, "from the FIRST-pass VMMAP capture; never
   re-queried" :421-422). The supplier buffer is `kMaxVmMappings=1024`
   (live_module_facts.hpp:85).
2. `anchor_mappings` keeps **only the records containing `observed_base`**
   (the scan's chosen base — the bridge wrapper plants `kCandidateBase`
   0x400000), capped at `kMaxAnchorMappings = 8`
   (live_module_facts.cpp:443-458, hpp:88).
3. `derive_readable_ranges` (lmf_to_native_loader_facts.cpp:77-109) keeps only
   records with `kve_protection & 1` (comp6 Diff A filter, :85-90 — "Exec-only
   text stays out"), clamps to `[extent.base, extent.end)`, sorts, and
   adjacency-merges (:100-107). Those become `images[MainExe].readable_ranges`
   (:252-260) and then the lease ranges.

So on this target the derivable lease set is, in practice, **the single
base-containing record** — the LOAD0 text record — filtered by its live
`kve_protection`. That is the whole game.

## 3. Root cause of the launch-8 `code=2` throw (and why r11 couldn't fix it)

The r10/r13 bridge registers **before** `supplier->capture()`, from its OWN
fresh sysctl and its OWN 16384-entry buffer
(runtime_start_bridge_r13.cpp:483-523, called at :584 — "T1"). The lease
ranges derive from the capture's snapshot ("T2") per §2. **Two different
observations of a live, churning VMMAP** (the launch-9 log alone shows three
different record counts across attempts: 0x5917, 0x2712, 0x2723) with
**different filters**:

| | registration (T1, r13:502) | lease ranges (T2, lmf:90) |
|---|---|---|
| records considered | all readable∩extent | only records **containing base 0x400000** |
| readable test | `prot & 1` | `prot & 1` |
| exec-only text | **excluded** (deliberate, r13:477-478) | excluded |

The only record containing base 0x400000 is the LOAD0 text record
([0x400000,0x787c000), observed prot=4 — CLAIM C5). Therefore:

- text exec-only at **T2** → derivation empty → the conversion refuses with
  `no observed mapping interval intersects the module image extent with valid
  geometry` (lmf:230) → **code=1 MRS_ERR_CONVERSION**. This is the shipped
  r13's launch-9 terminal state (its registration PASSED: `spans=6/reg=6`,
  entry-log :18663-18664; the refusal trail :18702-18707) — CLAIM C6.
- text readable at **T2** (prot flips to 5, or the record is replaced by a
  readable one) but exec-only at **T1** → conversion succeeds, the lease walks
  `[0x400000, 0x787c000)`, `covering(0x400000,1)` finds nothing (the lowest
  registered span is ≥ the first readable record), and **OwnedMemory.cpp:122
  throws at the lease's very first byte** → caught by the Stage-2 catch →
  `code=2 MRS_ERR_CONTEXT` with exactly the observed detail
  (runtime_start_bridge.h:38, r13:713-717). Attribution per the controller:
  this was launch-8's r10 binary (pid 234, session-234 window); r13 is
  dormant for this path (LAUNCH9-RESULT.md:15-16).

**Why r11 didn't help:** r11 fixed the FINAL PARTIAL PAGE case — the extent
end (0xc0602c8 under the shipped constant) is not page-aligned, so exact-end
registration left the last page unregistered; outward quantization
(r13:514-515) covers that. The launch-8 throw happened at the lease's FIRST
byte, in a range the registration had never seen under either quantization —
a two-snapshot race, not an alignment bug. No quantization rule can close a
drift between the registering snapshot and the leasing snapshot.

Residual classes, ranked with the coordinator's corrections applied:

1. **(a) exec-only/base-record drift between T1 and T2 — CONFIRMED ROOT CAUSE**
   of the launch-8 throw (§3; reproduced in fixtures `drift` + `capgate`).
2. **(b) unmapped/inter-fragment gaps inside the extent** — real (LOAD8 is a
   genuine `---` tail; fragments are separated), but the conversion's filter
   (§2) means leases never cross them BY CONSTRUCTION — unless a consumer
   requests a whole-extent read (none does today; see §4). Gap-crossing reads
   must fail closed — r14 enforces and reports this (fixture `gap`).
3. **(c) the 128-span cap — eliminated for launch-9** (spans=6, reg=6,
   entry-log :18663-18664) and structurally unable to bite an anchor-derived
   range in a real ascending snapshot (the base record is the lowest address
   and registers first). Kept as hardening: r14 makes saturation observable
   (`bridge_lease_cap`) and refuses loudly through the L3 gate if it ever
   bites coverage (fixtures `cap`, `capgate`).
4. **(d) kve_protection bit semantics** — same bit (0x1) is used by the
   bridge registration, the window-anchor coverage test, and the adapter
   filter (r13:168-169, lmf:85-90), so a prot-semantics mismatch cannot split
   the two sides; a kernel-side CHANGE of the bits between snapshots is
   exactly cause (1), which r14 eliminates by construction.
5. **(e) other findings**:
   - `bridge_lease_span_containing` in an earlier r14 draft had the classic
     `e - addr` unsigned-underflow false-match; fixed with the `addr < e`
     guard (the same class PLH's `extent()`/`covering()` guards,
     OwnedMemory.cpp:36-48). Caught by the fixtures, not by review.
   - The bridge's own `window_anchor_verify` byte-compares were direct reads
     guarded only by a fresh-VMMAP check — now owned-gated (r14 L4, CLAIM C10).
   - Pre-existing, OUT of r14 scope, needs the controller: the staged
     UnrealInitializer seed path reads game memory via **direct volatile
     reads** (`read_u32/read_u64`, UnrealInitializer.cpp:678-688, used at
     :700-706, :715-722, :775-776, :798) entirely outside the PLH discipline,
     and the code-slot seeds (FName ctor 0x63d590, static_construct 0x890aa0)
     live in exec-only LOAD0 pages that are deliberately never registered —
     any verification READ of those bytes is a live instance of this throw
     class (CLAIM C8).

## 4. Consumer audit (coordinator requirement 2: per-span end-to-end)

The downstream consumer does **not** issue whole-extent reads:

- `OwnedMappedImageCaller::segments(lease)` turns `lease.spans()` into
  `PsMappedSegment[]` — one segment PER lease span (OwnedMappedImageCaller.hpp:21-31).
- All address admission goes through `permits()/permits_code()/permits_data()`
  which walk those segments (:32-43, :71-75). Addresses outside any span
  (e.g. the seed at 0x63d590) are refused by the consumer's own containment
  check — fail-closed, silently.
- `ReadLease::Impl` itself clips emitted spans to covered entries
  (OwnedMemory.cpp:127-135), so after r14 every emitted span ⊆ registered
  readable bytes. Per-span leasing + per-span consumption is therefore
  end-to-end once the registry provably covers the derived ranges.
- The bridge's own gated reads (window anchors) now also route through the
  registry (L4), and the bounded-refusal reporter (`bridge_report_lease_refusal`)
  is the pattern the seed consumer must adopt when the controller routes its
  reads through a lease-equivalent check (REPORT.md follow-ups).

## 5. The r14 fix (design follows the PLH code, read first)

Constraint honored: **no exec/gap page is ever registered**; reads that would
cross an unreadable or unmapped region fail closed and name the gap.

- **L1 — one snapshot.** Registration moved into the `scan_image_base`
  wrapper — the first op `capture()` drives — and registers from the very
  mappings array capture passed in (`bridge_register_from_frozen_snapshot`).
  The pre-capture fresh sysctl is REMOVED. The registered set and the
  conversion's lease ranges now derive from the SAME frozen bytes → the race
  class is gone by construction (no second sysctl exists to drift).
- **L2 — true protection.** Readable records register `R` (|`X` when the kve
  bits carry exec). `W` is deliberately never registered (the service must not
  authorize writes into game memory; `ReadLease` consults `R` only,
  OwnedMemory.cpp:126). Exec-only / no-protection records stay unregistered.
- **L3 — coverage gate.** After the scan result is known (either route), the
  gate recomputes EXACTLY the ranges the conversion will lease (records
  containing the winning chosen base, readable, clamped — byte-identical
  filter to lmf:81-107 over anchor-derived records) and demands each be fully
  covered by the registry. A gap → `bridge_lease_gap` + bounded
  requested-range/nearest-span events + `MRS_ERR_CONVERSION` with detail
  `lease coverage gap: requested [0x… +0x…) outside registered readable spans;
  nearest registered [0x…..0x…)` — BEFORE any lease exists. An empty derivable
  set is NOT a gate failure: the conversion's own honest geometry refusal
  (code=1) stands.
  *Sufficiency proof sketch:* the conversion's merge only unions
  overlapping/adjacent derivable ranges; each unmerged range is checked
  covered by a single span; overlapping covered spans form a contiguous
  union; `ReadLease::Impl` walks across entry boundaries as long as coverage
  is contiguous — hence every merged lease range is walkable.
- **L4 — anchor reads gated.** `window_anchor_verify` now requires the frozen
  registry (not just the fresh VMMAP) before its byte compares, refusing with
  bounded events (`bridge_anchor_refuse` + nearest span) on drift.
- **L5 — idempotence + safe growth.** Skip-if-covered prevents the PLH
  "Owned regions must not overlap" throw on re-registration; touching heads
  are clipped against the last span (never overlapping); out-of-order records
  are skipped with `bridge_lease_order` and left to the L3 gate (never risk
  the PLH throw). The 128 cap returns false → `bridge_lease_cap` → the L3
  gate turns it into a loud pre-lease refusal when it bites coverage.
- Kept tags: `bridge_lease_reg_spans` / `bridge_lease_reg` (same meaning,
  frozen-snapshot basis). No new imports (hand-rolled hex formatter; only
  pre-linked symbols).

## 6. DIFF (unified, r13 → r14)

Full file: `r13_to_r14.diff` (next to this document). Summary:

```
 runtime_start_bridge_r14.cpp
 @@ header        + r14 provenance block (L1-L5, launch-8/9 attribution)
 @@ :108-113      + forward decl of bridge_anchor_owned_covered (L4)
 @@ window_anchor_verify
                  + owned-registry gate before the byte compare (L4)
                    (+ bridge_anchor_refuse event on refusal)
 @@ registration block (was bridge_register_readable_ranges, -56 lines)
                  + LeaseSpan registry mirror (g_lease_spans[128])
                  + lease_span_plh_prot (true prot, never W)
                  + bridge_lease_span_containing (addr < e underflow guard)
                  + bridge_report_lease_refusal (bounded lo/hi/len + nearest)
                  + bridge_register_span (quantize r11-kept, skip-if-covered,
                    clip-against-last, order guard, 128 cap -> false)
                  + bridge_register_from_frozen_snapshot (same tags, frozen basis)
                  + bridge_verify_lease_coverage (L3 predicate)
                  + bridge_anchor_owned_covered (L4)
                  + g_lease_gap_reported/detail + lease_gap_append_hex
                  + bridge_gate_after_scan (L3 refusal recording + detail)
 @@ start function
                  - bridge_register_readable_ranges() pre-capture call
                  + wrapper: register from frozen snapshot at entry (L1)
                  + wrapper: bridge_gate_after_scan on BOTH success routes (L3)
                  + post-capture: gap -> copy_detail + MRS_ERR_CONVERSION
 everything else: byte-identical to r13 (P2-1 adoption, ladder, kve path
 capture, configured roots, stop path, MRS codes).
```

## 7. Fixtures and proof (fixtures/, PASS.json)

One test TU compiles the **production** r13 (OLD) and r14 (NEW) bridges plus
the **real** OwnedMemory service (`-DPLH_OWNED_MEMORY_HOST_TEST`); one process
per scenario isolates the process-lifetime registry. Shapes use the TRUE phdr
table (LOAD0 --X, LOAD1 R-- with unaligned end 0x82ba918, LOAD3 RW-, the
observed LOAD7 fragment [0x970c000,0x9a34000), LOAD8 --- tail to the
non-aligned extent end 0xc0602c8) and the armed seed address 0x63d590.

| scenario | OLD (r13) | NEW (r14) |
|---|---|---|
| happy (stable RX text + true phdr) | lease over derived ranges passes; r11 partial-page kept; NOREAD tail throws | identical + registry excludes tail, no gap |
| drift (launch-8 shape: text 4@T1 → 5@T2) | **production lease throws exactly "Owned read range includes unregistered bytes"** — REPRODUCED | lease PASSES (frozen-snapshot registration) |
| exectext (text exec-only at capture) | derivation empty (honest geometry refusal upstream); text unregistered | same + gate stays silent on an empty derivable set |
| cap (>128 fragments, ascending) | silent saturation reg=128/spans=132; derivable lease still passes | cap VISIBLE (bridge_lease_cap), registry ==128, lease identical |
| capgate (synthetic: cap bites before the anchor range) | **lease throws the launch-8 text again** | **MRS_ERR_CONVERSION + detail naming the gap BEFORE any lease** |
| gap (whole-extent consumer read attempt) | n/a (no gate) | refused; first uncovered byte + nearest registered span reported; registry unchanged |
| seedgate | n/a | seed 0x63d590 (exec-only LOAD0) refused with bounded report; readable LOAD1 address admitted |
| idempotent | n/a (would throw "Owned regions must not overlap") | second registration is a no-op |

Result: **70 checks, 0 failures, runner exit 0** (fixtures/run_all.log,
fixtures/PASS.json — includes shas of both production sources).
