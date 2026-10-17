# D2-DESIGN — text_base vs contains/permits_code on an exec-only-text target

Worker: worker-d2-textbase-design, 2026-10-04. PROPOSALS ONLY — no frozen/wiring
file was modified; every diff below is a proposal for the controller. Static
source reading; every claim cites `file:line`. All paths relative to the
workspace root `/WORKSPACE/.codex/worktrees/translation-core/what`
(abbreviated `W:`).

## 0. Verdict up front

**Recommended: option (c) — honor the ABI's own metadata-absence convention for
`text_base` (two one-line header diffs), plus one required satellite diff in the
adapter so `readable_ranges` stop claiming the exec-only text span.** This is
the only resolution that is simultaneously honest (registers/claims nothing the
hardware does not provide), minimal (no OwnedMemory/ABI/Rust change, all diffs
inside freshly compiled units), and sufficient to make hazard rows 2.9/2.11
pass as designed. Option (a) is rejected as a hardware lie that also crashes on
first scan; option (b) is sound only as a future ABI-V2 program and is not
worth its blast radius today (no consumer of `view.text_base` exists anywhere
in the tree).

An important honest correction to the task framing is documented in section 2:
with the currently staged units, r8 still dies at row **2.6**, not 2.9 — the
adapter's `readable_ranges` still include the exec-only text span, which the
r8 pre-registration deliberately does not register. The satellite diff (A) is
therefore part of the D2 fix, not optional.

---

## 1. The gate chain, exactly as staged (composition-5 r8 lineage)

Build identity of the two gated headers: `W:outputs/2026-10-04-zcode-team/
worker-composition-5-rebuild/staging/lineage-overlay-1/{PsMappedImageCaller.hpp,
OwnedMappedImageCaller.hpp}` are byte-identical (verified by diff) to
`W:.superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include/`
and `.../ue4ss-native-ps-scan-provider-1/include/` respectively — matching the
`.d`-proven include resolution (HAZARD-TABLE.md:11-14).

### 1.1 Construction order (who runs first)

`MakeNativeProgramContext` (tick-file-owner-1 `src/NativeLoaderContext.cpp:19`)
builds `OwnedMappedScanSource(snapshot, facts.mapped, ...)`. Its ctor
(`src/OwnedMappedScanSource.cpp:10`) constructs `OwnedMappedImageCaller` with
`facts_.text_base` — the adapter's 0 — verbatim. Inside
`OwnedMappedImageCaller` (provider variant `include/OwnedMappedImageCaller.hpp`):

- `:55-56` member declaration order `ranges_; lease_; caller_;` — the
  **ReadLease (row 2.6) runs before any PsMappedImageCaller check**;
- `ranges()` `:14-20` copies `ReadableRanges().at(MainExe)` verbatim;
- `segments()` `:21-31` maps each lease span's `PLH` protection to wire flags:
  `PS_MAPPED_READ` iff `span.protection & PLH::R` (`:25`);
- ctor gate `:57`: `if(!permits_code(text_base)) throw std::invalid_argument(
  "Mapped text base must be registered readable executable bytes")` — row 2.11;
- `permits_code` `:69` = `permits(address,1,PS_MAPPED_READ|PS_MAPPED_EXECUTE)`;
  `permits` `:32-43` walks view segments and requires `(permissions &
  required)==required` per covering segment (`:39`).

The read-lease-1 variant (`ue4ss-native-read-lease-1/include/
OwnedMappedImageCaller.hpp:50`) carries the identical gate and flag mapping
(`:25-28`, `:62`); it differs only in holding `context_` instead of `source_`.

### 1.2 The two text_base sub-gates

**Sub-gate 2.9 — `contains` (PsMappedImageCaller.hpp).**
`contains` `:23-34`: `{0,0}` returns **true** (`:24` — the ABI absence
spelling); `{0,1}` returns **false** (`:25` — `!extent.address` fails the
guard). The ctor `:68` tests `contains({text_base,1})` — a hard-coded size 1 —
so the adapter's `text_base = 0` becomes `{0,1}`, fails, and `:70` throws
`"Mapped metadata or spans exceed supplied readable bytes"`. Before that, the
ctor has already required every segment to carry `PS_MAPPED_READ` (`:50`) and
exact lease-span/snapshot-range coverage (`:55-67`).

**Sub-gate 2.11 — `permits_code` (OwnedMappedImageCaller.hpp:57).**
Reached only when `contains({text_base,1})` passed, i.e. a segment covering
`text_base` exists; it then demands that segment carry **R|X** (`:69` + `:39`).

### 1.3 Registration reality (the PLH owned-memory service)

`ue4ss-native-read-lease-1/src/OwnedMemory.cpp`:

- lease walk `:106-138`: every byte of every range must be covered by a
  registered entry (`:121-122`, throws `"Owned read range includes unregistered
  bytes"` = row 2.6) and every covered page must have **R** (`:125-126`, throws
  `"Owned read range is unreadable"`);
- registration paths: `ScopedRegion` `:207-211` (uniform protection, validated
  `:41-44` — any subset of RWX including `R|X` is accepted), `OwnedMapping`
  `:212-227` (R|W only), `AllocatorBridge::record_mapping` `:159-169`;
- the service header states the intent contract:
  `include/polyhook2/OwnedMemory.hpp:25-29` — "Caller asserts these are
  genuinely owned, accessible bytes… **This is not an API for registering
  discovered process/game mappings.**"

The r8 bridge (`worker-module-start-wiring/runtime_start_bridge_r8.cpp`) is the
loader-side registrant, per the factory contract (`tick-file-owner-1/include/
NativeLoaderContext.hpp:4-6`: "A legitimate in-process loader supplies …
already-existing owned-service registrations. This factory never registers
pointers"):

- `bridge_register_readable_ranges()` r8:398-421 registers, per **R-only**
  vmentry mapping (`:411` skips `(protection & kVmProtRead)==0`), the clamp of
  the mapping to the anchored extent `[0x400000, 0x400000+0xbc602c8)`
  (`:407-408,412-413`), as `ScopedRegion(..., PLH::R)` (`:415-417`), BEFORE
  conversion runs (`:476-479`);
- r8:392-393 states the design decision verbatim: *"Exec-only text is NOT
  registered readable — the D2/text_base question stays open upstream."*

### 1.4 The adapter's facts.mapped assembly

`ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` (the
compiled adapter; composition5 driver pins exactly this path —
`worker-composition-5-rebuild/build_runtime_composition5.py:155`; the
`worker-supplier-ingame-update` copy is byte-identical, verified by diff):

- `derive_readable_ranges` `:74-103` clamps **all** observed anchor mappings
  into the extent (`:82-89`) — **it never inspects `mapping.protection`**;
- `facts.mapped.text_base = 0` with the design comment `:226-227`: *"text_base
  NEVER supplied: unresolved by design (see header + REPORT.md)"*; the header
  (`lineage-overlay-1/lmf_to_native_loader_facts.hpp:46-50`) repeats it and
  adds: *"the adapter does not relay a text base without a target-verified
  joined fact (unchanged gate)"*;
- `image.readable_ranges` planted from the clamped/merged list `:239-247`.

The composed supplier (`worker-composition-prep/staging/.superpowers/
native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp`,
507-line in-game version, = `worker-supplier-ingame-update/src/` copy) records
anchor mappings at `:443-459`: every vmentry mapping **containing
`observed_base`** (`:449-451`) with its true protection recorded (`:455`) but
**no protection filter**. On the r8 VmScan route `observed_base` = chosen_base
= 0x400000 (overlay `live_module_facts.hpp` comment, `:40` in the diff:
"rtld-reported base OR VmScan chosen base"), so the single recorded anchor
mapping is the exec-only text span itself.

### 1.5 Target geometry (trusted records)

- Game text/header span `[0x400000, 0x787c000)` is **exec-only** (kve_protection
  = 4): r8:113-119 ("The eboot header page is EXEC-ONLY on this target …
  trusted in-game records"); the in-memory header scan can never anchor
  (composition-5 launch: NoImageAnchor/needs-base, r8:114-119) — i.e. even the
  supplier's phdr parse cannot read there.
- igf6 record (`worker-checkpoint-reconcile/CURRENT-STATE.md:9`): a code-anchor
  byte-read was **blocked**; *"Mechanism NOT localized (user-amended); …
  leading hypothesis exec-only page enforcement. User directive: no broad
  investigation, no repeated text-read attempts."* So "reads fault" is the
  trusted working truth and may not be re-probed.
- All verified anchor windows (B/D/E/C/C2, r8:133-140) sit at absolute
  addresses ≥ 0x7c26782, i.e. **above** 0x787c000, inside readable spans —
  r8 coverage requires `protection & kVmProtRead` (`:146-147,311-315`). The
  anchor therefore never proved text readability.
- Consequence for ranges: within the extent, the first **readable** bytes start
  at 0x787c000.

---

## 2. Honest finding: the currently staged r8 still fires row 2.6 first

Chaining sections 1.3-1.5 with the staged sources:

1. bridge registers R∩extent spans only → registered bytes start at 0x787c000
   (r8:411-417);
2. supplier records the exec-only anchor mapping (facts cpp:449-457, no filter);
3. adapter derives `readable_ranges = [{0x400000, 0x787c000})` (lmf cpp:82-89,
   no filter);
4. `OwnedMappedImageCaller` leases those ranges (hpp:55-56 provider / :48-49
   read-lease variant) → `covering(state(),0x400000,1)` finds no entry →
   `std::invalid_argument("Owned read range includes unregistered bytes")`
   (OwnedMemory.cpp:121-122) — **row 2.6**, not 2.9.

The task's stated geometry ("readable ranges start at 0x787c000", 2.6
addressed, 2.9 open) describes the state **after** the ranges are aligned with
the registration. Alignment cannot live in the supplier: filtering
`anchor_mappings` to R-only there would leave `anchor_mapping_count == 0`
(the only mapping containing 0x400000 is X-only) and trip the conversion
refusal "snapshot has no observed mappings…" (lmf cpp:155-158; HAZARD row 1.9).
It belongs in the adapter's `derive_readable_ranges`, which runs **after**
that gate (lmf cpp:215). This is satellite diff **A** below.

Corollary honesty fix: HAZARD-TABLE row 2.7's remedy hint — "registration must
assert R even for exec-only pages (exec pages read fine in-game — anchor
proved it)" (HAZARD-TABLE.md:100) — is contradicted by r8:113-114, by the igf6
blocked-read record (CURRENT-STATE.md:9), and by the anchor windows themselves
(all ≥ 0x7c26782, section 1.5). It should be treated as stale.

---

## 3. Resolutions enumerated and ranked

### Option (a) — supply `text_base = 0x400000` (exec span base), everything else unchanged

To pass 2.9, `contains({0x400000,1})` needs a **segment** covering 0x400000;
segments exist only as lease spans (OwnedMappedImageCaller.hpp:21-31,56), and a
lease span for text requires the text to be (i) inside `readable_ranges` and
(ii) **registered R** (OwnedMemory.cpp:121-126). To then pass 2.11 the same
segment needs R|X (hpp:57,69,39). That means registering live game text with a
false readable flag and handing the scanner a "readable" borrow of bytes that
the kernel reports prot=4 and igf6 showed unreadable:

- violates the registration contract itself ("genuinely owned, accessible
  bytes… not an API for registering discovered process/game mappings",
  OwnedMemory.hpp:25-29);
- creates a standing false permission record that `permits_data` (hpp:70)
  would happily certify to any later caller;
- the very first pattern-read through that span dereferences exec-only pages —
  exactly the read class igf6 found blocked, and the user directive forbids
  re-probing (CURRENT-STATE.md:9);
- if one stops short of registration and merely sets `text_base=0x400000`,
  the build still throws at 2.9 (no segment covers 0x400000 — readable ranges
  start at 0x787c000).

**Verdict: REJECT — dishonest and, if completed, a crash path.** It would pass
both gates service-side only by lying; that is precisely what the gates exist
to prevent (PsScanAbi.h:91-93: spans are "synchronous borrows"; "No numeric
runtime address permits constructing an unsupplied raw byte slice").

### Option (b) — register the text span as a loader-asserted EXEC-only span and extend the chain to admit it

The honest version of "make text_base real": a new registration/lease category
so `[0x400000,0x787c000)` can be registered with its true `X` and named as
`text_base` without claiming readability. Minimal diff surface, unit by unit:

1. **Adapter** (`lmf_to_native_loader_facts.cpp`): derive a second list,
   exec-only spans (`protection&1==0 && protection&4`), clamp+merge like
   `:82-101`; join `text_base` from a target-verified fact — the only honest
   source on this target is an **offline parse of the retained eboot.bin file**
   (the pinned image, r8:116-118 sha 3b6cfad4…) whose `.text` `p_vaddr` joins
   with the anchor-validated base 0x400000; in-memory phdr parsing is dead
   (r8:113-119). New conversion option/field + `LmfConversionResult` provenance.
2. **ReadLease / OwnedMemory.cpp** (read-lease-1 `src/OwnedMemory.cpp`): the
   lease throws on non-R pages (`:125-126`) — needs a parallel exec-span walk
   (or a second lease class) emitting X-only spans, plus a decision on how
   X-registrations enter the service without abusing `ScopedRegion`'s contract
   (OwnedMemory.hpp:25-29).
3. **OwnedMappedImageCaller.hpp** (both variants): `segments()` `:21-31` gains
   the exec spans; ctor passes the merged list.
4. **PsMappedImageCaller.hpp**: `:50` mandates `PS_MAPPED_READ` on **every**
   segment — must learn an EXEC-only segment kind; `:55-67` exact-coverage walk
   must cover readable ∪ exec extents; `contains` `:23-34` semantics carry
   over; `permits_code` then passes on X-registered text.
5. **ABI**: `PsMappedSegment.permissions` would carry EXEC-without-READ for the
   first time, while PsScanAbi.h:91 promises "Supplied readable spans are
   synchronous borrows" — the Rust/C++ resolver may read any supplied span.
   Handing it unreadable bytes invites the igf6 fault class inside the scanner.
   Soundness requires a V2 view (non-borrowable exec metadata) and a resolver
   audit; `PS_MAPPED_IMAGE_ABI_V1` is pinned (PsScanAbi.h:71).
6. **Bridge**: register text X (ScopedRegion accepts `PLH::X`,
   OwnedMemory.cpp:41-44,207-211) + supply the file-derived text_base fact.
7. **Scripts**: `native-owned-memory` is NOT in the composition's affected-unit
   inventory — `build_runtime_composition4_fixed.py:57-61` asserts the affected
   set exactly equals `{'unreal-unrealinitializer',
   'native-ownedmappedscansource','native-psscanbinding',
   'native-ownedmappedcandidateselection','native-loadercontext',
   'native-programfactory'}` and `:79` restricts `--replace-runtime-unit` to
   those — so item 2 cannot land at all without widening the pinned inventory
   (a fixed-script change) and regenerating `--replace-receipt` attestations.

**Verdict: DEFER.** Honest in intent, largest blast radius (5 units + ABI +
scripts), crosses a pinned ABI, and — decisive — **nothing consumes
`view().text_base`**: a tree-wide search finds no reader outside the ctor gates
and the facts field itself (grep over tick-file-owner-1 include tree,
mapped-collector-1 / mapped-candidate-selection-1 / collector-properties
includes, and the vendored patternsleuth Rust crate
`ue4ss-patternsleuth-ps5-input-1/upstream/` — zero hits for `text_base`). The
mapped scan's candidate collection reads segment bytes (data spans), not
text_base. So option (b) buys metadata nobody reads, at ABI-soundness risk,
while the scan itself can never read text on this target. Revisit only if a
future consumer needs in-view text metadata.

### Option (c) — honor the ABI's own absence convention (RECOMMENDED)

The ABI already defines absence for metadata: PsScanAbi.h:92 — *"Metadata is
either `{0,0}` or wholly contained in supplied readable spans."*
`PsMappedImageCaller::contains` implements exactly that (`{0,0}` → true,
hpp:24). The gap is purely that the scalar `text_base` parameter is wrapped as
`{text_base,1}` (hpp:68), making absence unrepresentable — every other metadata
field arrives as `PsMappedExtent` and is passed `{0,0}` by the adapter (lmf
cpp:233-235). The adapter's contract ("text_base NEVER supplied",
cpp:226-227 + hpp:46-50) is thus already the absence spelling; the C++ wrapper
contradicts its own ABI by refusing it.

**Diff B — `PsMappedImageCaller.hpp:68`** (abi unit; proposal):

```cpp
        if(index!=segments_.size() || !contains({text_base,1}) ||
```
→
```cpp
        // PsScanAbi.h: metadata is {0,0} (absent) or wholly contained in the
        // supplied readable spans. text_base==0 is the ABI's only absence
        // spelling for the scalar text_base field ({0,1} is not a valid
        // extent); containment is required only for a SUPPLIED text base.
        if(index!=segments_.size() ||
            (text_base!=0 && !contains({text_base,1})) ||
```
(the `!contains(eh_frame) || !contains(eh_frame_hdr) ||
!contains(pe_exception_directory)` tail of `:68-69` is unchanged).

**Diff C — `OwnedMappedImageCaller.hpp:57`** (provider variant, the composed
one) **and `:50`** (read-lease variant); proposal, message text unchanged:

```cpp
        if(!permits_code(text_base)) throw std::invalid_argument("Mapped text base must be registered readable executable bytes");
```
→
```cpp
        // Absent text_base (0) carries no permission requirement (PsScanAbi.h
        // metadata rule); a SUPPLIED base must still be registered R|X.
        if(text_base!=0 && !permits_code(text_base)) throw std::invalid_argument("Mapped text base must be registered readable executable bytes");
```

**Diff A — satellite, required for 2.6 to actually pass**
(`ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp`, inside
`derive_readable_ranges`, after the `:84` degeneracy check, before the clamp):

```cpp
        // readable_ranges are LEASED (ReadLease requires R per page,
        // OwnedMemory.cpp:126) and SCANNED; only mappings the kernel reports
        // readable (kve_protection & 1 — the same bit the bridge's PLH
        // pre-registration and the window-anchor coverage test use) may
        // enter. Exec-only text stays out: it is neither registered nor
        // scanned on this target (r8 bridge:392-393; igf6 record).
        if ((mapping.protection & 1u) == 0) continue;
```

Consumer audit supporting B/C: with `text_base = 0` flowing into `view_`
(hpp:74), no code reads it — see the option-(b) grep result (section above);
`OwnedMappedScanSource::collect_with_evidence` fails soft regardless
(`OwnedMappedScanSource.cpp:28` catch-all → unavailable collection), and
`scanner_source` is documented optional with no validation requirement
(native-coherent-runtime-1 `.../Platform/NativeProgramContext.hpp:64-66,83-101`).

Residual honesty note for (c): a `text_base = 0` view becomes constructible
for the first time. The Rust resolver crate contains no `text_base` consumer
(grep: zero hits in `ue4ss-patternsleuth-ps5-input-1/upstream/`), so there is
no reader to misbehave; the controller should still record this audit in the
receipt that attests the header replacement.

### Ranking (honesty first, then blast radius)

| Rank | Option | Honesty | Blast radius | Effect on the gates |
|---|---|---|---|---|
| 1 | **(c)** absence-honored text_base + satellite A | No false records; matches PsScanAbi.h:92 and the adapter's own declared contract (cpp:226-227) | 3 small diffs, all in freshly compiled units (section 4); no ABI/OwnedMemory/Rust/script change | 2.6 passes (A), 2.7/2.8 unchanged-pass, 2.9/2.11 pass as absent (B/C) → Stage 3 reachable |
| 2 | (b) exec-span registration category (ABI V2 program) | Honest end-state; soundness requires resolver-side work | 5+ units, pinned ABI, affected-inventory script change (composition4_fixed.py:57-61,79) | Same pass, plus real text metadata nobody yet consumes |
| 3 | (a) text_base=0x400000 with R-registered text | Hardware lie; contradicts OwnedMemory.hpp:25-29; violates the no-text-read directive | Looks smallest, is largest in consequence | Passes gates then faults on first scan read; unregistered variant still dies at 2.9 |

---

## 4. Composition integration path for option (c)

All three diffs live in code the r8 rebuild already recompiles:

- **Adapter (A)**: `lmf_to_native_loader_facts.cpp` is an `--extra-unit`
  (`lmf-loader-adapter=…/ue4ss-native-tick-file-owner-1/src/
  lmf_to_native_loader_facts.cpp`, build_runtime_composition5.py:155). Extra
  units are freshly compiled every run; the controller updates the source hash
  in the extra-receipt attestation (`--extra-receipt
  outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/receipts/
  composition4-extra-receipt.json`, build_runtime_composition5.py:168).
- **Headers (B, C)**: `PsMappedImageCaller.hpp` and `OwnedMappedImageCaller.hpp`
  are header-only and reachable only via
  `NativeLoaderContext.hpp → OwnedMappedScanSource.hpp →
  OwnedSerializedElfSource.hpp → OwnedMappedImageCaller.hpp →
  PsMappedImageCaller.hpp` (verified include chain in the tick-file-owner-1
  include tree). Both top-level headers are trigger headers of the affected
  inventory (`build_runtime_composition4_fixed.py:57`), so every TU that
  instantiates the changed constructors is in the freshly-compiled affected set
  (`:64`); the exact-set assertion (`:61`) keeps failing loudly if that ever
  changes. The replacements must be reflected in the `--replace-receipt`
  attestation covering "every spike project file the replacement touches"
  (`:41`), and the overlay copies under
  `worker-composition-5-rebuild/staging/lineage-overlay-1/` must be refreshed
  (they are currently byte-identical to the spike originals — verified by diff
  — so the overlay step is a straight re-copy).

Host re-verification expectation (no target claims): with B/C applied, the S1
host suite's probe A (`worker-s1-adapter/host_tests/test_lmf_adapter_host.cpp:
458-478`, R-registered pages at `:430-437`) **completes construction** instead
of throwing "Mapped metadata or spans exceed supplied readable bytes"
(REPORT.md:149); probe C (`:501-519`, supplied in-span base, R-only pages)
**must still throw** "Mapped text base must be registered readable executable
bytes" (REPORT.md:151) — that is the supplied-base honesty path, intentionally
preserved by the `text_base!=0` guards. Probe B (`:481-499`, D1/ELF64
image_base rule) is untouched.

Stage-3 honesty note (out of D2 scope, recorded so nobody expects a free win):
with (c) the mapped scan constructs but its requested **code** slots cannot
match patterns inside exec-only text (segments cover readable data spans only).
`ScanGame` retries until `SecondsToScanBeforeGivingUp` and then throws "PS scan
timed out" (`worker-query-s2-arm/staging/UnrealInitializer.cpp:610-631`), which
surfaces as hazard row 3.13 (async, ack `failed` in S only). Making the
post-D2 scan terminate honestly on this target (slot policy, witness route,
timeout) is the next deviation to design — it is not solvable by any
text_base representation, because the bytes are unreadable.

---

## 5. Launch-7 runtime diagnostics: which sub-gate fired

Channel shapes (HAZARD-TABLE.md:26-33,106-107): a Stage-2 throw yields
`MRS_ERR_CONTEXT` (2); D `runtime_start_failed`=2 (entry r3), UE4SS.log line
`modbridge module_start failed: runtime_start_failed code=2 detail=<what()>`
(entry r3:166-192), and runtime-status.txt
`state\t1\tfailed\t2\truntime_start_failed <what()>` after the
`modforge-runtime-status\t1\nsession\t<pid>\t<ms>` head. Each sub-gate's
`what()` string is unique, so the detail string alone identifies the gate:

| detail string (exact) | Sub-gate | Meaning if seen on launch-7 |
|---|---|---|
| `Owned read lease requires ranges` (OwnedMemory.cpp:108) | 2.6 (degenerate) | adapter produced zero ranges — satellite A over-applied or extent regression |
| `Owned read ranges overlap or have invalid bounds` (:118) | 2.6 (geometry) | range list unsorted/overlapping — merge logic regressed |
| `Owned read range includes unregistered bytes` (:122) | 2.6 (the predicted one) | ranges ⊄ registrations: satellite A missing/stale, or the map shifted between the bridge's registration query (r8:402) and the capture query (same op, r8:296) |
| `Owned read range is unreadable` (:126) | 2.6/2.7 boundary | a registered page lacks R — registration bug, never expected with r8:411-417 |
| `Invalid supplied mapped span` (PsMappedImageCaller.hpp:51) | 2.7 | a lease span lost its R flag or `bytes != runtime_address` — the tripwire that would also catch an unsound X-only segment attempt (option b without ABI work) |
| `Mapped spans must exactly cover supplied snapshot ranges` / `Mapped span extends beyond supplied range` (:62/:64) | 2.8 | lease/snapshot partition mismatch — registration and derivation used different vmentry snapshots |
| `Mapped metadata or spans exceed supplied readable bytes` (:70) | **2.9** | `contains({text_base,1})` failed. With diffs B/C **absent**: pre-fix binary, or a supplied nonzero text_base that no readable segment covers (an option-a attempt). This string for text_base=0 is the D2 signature (S1 probe A, REPORT.md:149) |
| `Mapped metadata does not match format` (:73) | 2.10 | ELF64 image_base nonzero — D1 regression (probe B, REPORT.md:150) |
| `Mapped text base must be registered readable executable bytes` (OwnedMappedImageCaller.hpp:57) | **2.11** | supplied text_base contained but its segment is not R\|X — expected **only** when someone supplies a real base (probe C semantics, REPORT.md:151); must NOT appear for text_base=0 after diff C |

Cross-checks on the D channel that disambiguate 2.6-cause vs 2.9-cause without
a log: `bridge_lease_reg` (r8:420) = number of registered regions; after
satellite A the adapter range count equals it (both are R∩extent over the same
op). `bridge_vmentry_entries` / `bridge_vm_walk_stop` (r8:251-252) and the
Stage-1 `lmf_*` caps (r8:543-547) rule out upstream capture causes.
Optional (proposal only, bridge is a frozen wiring file): one
`diagnostic_event("bridge_ctx_gate", <n>)` emitted from a catch-site table in
the bridge's `MRS_ERR_CONTEXT` handler, n indexed into the string table above —
useful if the log channel is ever unavailable, but the L/S detail already
carries the same information, so it is not required for launch-7.

Post-fix expected launch-7 shape (honest prediction, not a claim): Stage 2
silently passes (no D/L/S failure), D shows `mfa_activated_low/high` (entry
r3:270-273) then Stage-3 events; the first honest residual risk is the Stage-3
scan timeout (section 4), visible as D `runtime_start_ok` + S-only
`state\t<n>\tfailed\t…` per HAZARD rows 3.12-3.13 (HAZARD-TABLE.md:126-127).

---

## 6. Explicit non-claims

- No console contact, no target execution, no text-read probes (user directive,
  CURRENT-STATE.md:9). Everything above is static source reading of staged
  units plus the existing igf6/igf7/S1 records.
- The "exec-only text" premise rests on trusted in-game records (r8:113-114)
  and the igf6 blocked read whose mechanism the user amended to NOT-localized —
  the design treats text as unreadable either way and never depends on
  re-proving it.
- Option (c) makes Stage 2 passable; it does not claim Stage 3 scan success,
  engine candidates, or runtime activation. The hazard-table's 2.7 remedy hint
  is flagged stale (section 2) and should be corrected by its owner.
