# worker-axes-detector — REPORT.md (2026-10-04, zcode team)

Delivers the in-probe **build-configuration** and **stats** detectors so the
runtime's axes (`results.build_configuration` 0..3, `results.stats` 0..2,
consumed in `UnrealInitializer.cpp` ps_scan first pass and by
`SetResolvedNativeLayoutAxes` on `__PROSPERO__`) resolve from **game
evidence**, not settings.

## 1. What the runtime detector actually reads (verified in source)

The axes are NOT single flags. The genuine classifiers are the patternsleuth
resolvers `BuildConfiguration` / `Stats` in:

`/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-patternsleuth-1/upstream/patternsleuth/src/resolvers/unreal/engine_version.rs`

* They fingerprint the image for **244 UTF-16LE marker strings** (CVar names,
  log/error strings: `"r.Shadow.DistanceFieldPenumbraSize"`,
  `"Cycle counters (flat)"`, `"Plugin commandlet disabled in shipping mode."`,
  ...). A marker is "present" when the substring occurs anywhere in image
  memory (raw byte stream, both 2-byte phases, no alignment assumption; the
  Rust `utf16_pattern` has no NUL terminator).
* Each marker carries a **gate** — the UE preprocessor condition that includes
  it: `NotShipping` (#if !UE_BUILD_SHIPPING, 16 markers), `DevOnly` (16),
  `TestOrShipping` (3), `ShippingOnly` (10), `WithStats` (#if STATS, 12) —
  plus an engine-version window; 187 ungated markers carry only windows.
* Decision = **cost minimization** over candidate builds
  (30 versions 4.7–5.8 + unknown) × {Development, Test, Shipping} ×
  {Stats Off, On}: present-but-not-covered costs **CONTRADICTION=5**,
  covered-but-missing costs **OMISSION=1**. Cheapest candidate wins per axis
  iff it beats every candidate differing on that axis by **MIN_MARGIN=4**,
  and at least **MIN_MARKERS_PRESENT=10** markers were found at all
  (`fingerprint()` bails below that). Otherwise the axis stays **0 = not
  detected** — the runtime never guesses.
* Enum mapping (binding-port `src/lib.rs`): Shipping=1, Development=2,
  Test=3; Stats Off=1, On=2.

Consequence for the probe: there is no single byte to read. The honest
in-probe equivalent is the same marker fingerprint + cost model, which is
exactly what was built — and it reads only image bytes the probe already
proves readable (VMMAP spans, base 0x400000).

## 2. Deliverables (all under this directory; nothing else written)

| File | Purpose |
|---|---|
| `src/axes_table.h` | GENERATED marker/version tables, transcribed mechanically from the Rust source by `tools/gen_axes_table.py` (244 markers, 30 versions, 6764 code units; decode-back self-check inside the generator) |
| `src/axes_detector.h` | API + runtime axis conventions + cost constants |
| `src/axes_detector.c` | Freestanding C detector: chunked scan (64 KiB chunks + straddle carry, refusal-halving over span edges) with first-code-unit bucket index; pure cost-model entries `ig_axes_costs_of` / `ig_axes_resolve_costs` / `ig_axes_from_presence`; `ig_axes_scan` = scan + resolve. No libc, no imports, no allocation |
| `src/axes_probe_glue.c/.h` | Drop-in probe experiment `ig_axes_experiment(sink, seq, build_id, base, extent)` → one bounded `exp="axes"` JSONL record. Compile-verified against the real probe headers (`factstore.h`, `record_sink.h`); reads via `ig_copy_checked` only |
| `host/test_axes_detector.c` | Host tests T1–T4 (below) |
| `host/run_host_tests.sh` | Reproducible pipeline: regenerate table → unit tests → real-image run → Python cross-check |
| `tools/gen_axes_table.py`, `tools/ref_axes.py` | Table generator; independent Python reference (different scan strategy: per-segment `bytes.find`) |

## 3. Host-test evidence

```
T1 self-test: 186 candidate builds, each resolves own config+stats+version   PASS
T2 straddle:  marker split across 64 KiB chunk boundary found, 8 jittered
              read-window trials; a single marker stays not-detected        PASS
T3 degenerate: all-absent presence and fully-refused scan stay 0/0 with
              refusal evidence (never guessed)                              PASS
T4 real image: decrypted Townfall eboot (see below)                          PASS
```

T1 is the port of the upstream `fingerprint_table_is_well_formed` property
(costs → resolve, bypassing the scan-time marker-count gate, exactly like the
upstream test), plus a check that the scan-path gate never flips a verdict.

Two bugs were caught and fixed during development, both by these tests:
1. `TestOrShipping` gate inverted (allowed Development instead of
   non-Development) — caught by T1.
2. Generator emitted byte offsets into a code-unit pool (2× corruption after
   the first marker) — caught by T2's marker lookup. The generator now has a
   decode-back assertion so this class of error cannot ship again.

## 4. REAL game evidence — retained Townfall image

Input: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(ELF64 x86-64, 5 PT_LOADs, mapped extent 0xbc602c8 = 188.4 MiB; runtime
address = 0x400000 + p_vaddr, consistent with worker-vaddr-semantics'
LOAD0 mapping).

C detector (0.2 s) and independent Python reference (7.8 s) **agree exactly**:

```json
{"build_configuration": 1, "stats": 1, "version": "5.6",
 "present_count": 54, "extent_bytes": 197526216,
 "costs": {"config": [43, 103], "stats": [43, 55], "version": [43, 63]}}
```

**Verdict: Townfall eboot = SHIPPING (build_configuration=1, margin 60),
STATS OFF (stats=1, margin 12), engine 5.6 (margin 20, evidence only — the
runtime detects version separately; do not feed this into `Version::Major/
Minor` without its own acceptance).**

Human-auditable breakdown of the 54 present markers:

* 9/10 `ShippingOnly` strings present (e.g. `log.flushInterval.Shipping` at
  file vaddr 0x772xxxx range; runtime ≈ 0x400000+vaddr). The one absent
  (`"Plugin commandlet disabled in shipping mode."`) is a stale string not in
  this UE — an OMISSION(1), correctly outvoted.
* `"TransientUserTexture"` (TestOrShipping, 5.5+) present → not Development.
* All 16 `NotShipping` and all 16 `DevOnly` markers ABSENT → consistent with
  shipping, contradicting development.
* All 12 `WithStats` markers ABSENT → STATS off, detected independently of
  configuration as the upstream resolver requires.
* Version windows: 44 ungated windowed markers present; the cost model picks
  5.6 (runner-up margin 20). Present-but-out-of-window markers for 5.5
  include 5.6-only strings (`r.Substrate.BlendableGBuffer`,
  `net.QueuedBatchTimeoutSeconds`, `r.MegaLights.DownsampleFactor`,
  `r.LensFlareBlurComputeShader`), while 5.6 leaves only older-window
  stragglers — 5.6 is the cheapest explanation.

Acceptance-honesty note: this is a HOST measurement of the retained image.
It establishes what the in-probe detector will report for this image; it is
not itself a console run. The console-side acceptance is the igf5 probe
record (below).

## 5. Integration notes for igf5/igf6 (probe-v2 lineage)

1. Copy `src/axes_detector.{c,h}`, `src/axes_table.h`,
   `src/axes_probe_glue.{c,h}` into the probe's `src/` (or add `-I` to this
   directory) and extend `build_ingame.py`:
   `C_SOURCES = [..., "axes_detector.c", "axes_probe_glue.c"]`. The detector
   adds no imports (pure computation over memory the probe already reads) and
   ~80 KiB static data (tables + one 64.5 KiB combined buffer + 8 KiB index).
   Build-id change is expected (source hash input changes).
2. Call after the V2 base scan chose the main image, once
   `g_facts.chosen_index >= 0`:
   ```c
   ig_axes_experiment(&sink, seq++, build_id,
                      g_facts.chosen_base,
                      g_facts.chosen_extent);  /* runtime bytes */
   ```
   For Townfall: `chosen_base = 0x400000` (proven) and
   `chosen_extent = 0xbc602c8` (= max(p_vaddr+p_memsz), the `IgCand.extent`
   convention). Unreadable holes are impossible in this extent for the real
   mapping, but refusals are counted in-record either way.
3. Record schema (one line, well under the 2 KiB cap): `exp="axes"` with
   `build_configuration` (0..3 runtime convention — feed straight into
   `results.build_configuration`), `stats` (0..2 → `results.stats`),
   `version_known/major/minor` (evidence only), `markers_present/total`,
   `bytes_scanned`, `chunks_read`, `read_refused`, per-axis
   `*_resolved`/`*_cost_best`/`*_cost_runner`, `image_base`, `image_extent`.
4. Runtime semantics on PS5 (`__PROSPERO__`): with `results.stats != 0` the
   initializer now sets `Version::DebugBuild` from detection instead of the
   settings fallback, and `SetResolvedNativeLayoutAxes` gets a genuine
   `native_stats` value — the axes stop being "unknown ⇒ layout facts
   blocked". With `DebugBuild=Detect` + `Stats=Detect` settings and this
   detector feeding results, Townfall resolves NonShipping=false,
   DebugBuild=false.
5. In-game runtime budget: the scan is a single linear pass (~188 MiB, 3015
   chunk reads host-side). If the probe's quiesce window is too tight, the
   call can be bounded by scanning first the read-only LOADs (LOAD0+LOAD1 +
   LOAD8 contain all observed markers) — the axes still resolved from 54
   markers spread across those; but ship the full-extent version unless a
   measured need appears.

## 6. Reproduce

```sh
outputs/2026-10-04-zcode-team/worker-axes-detector/host/run_host_tests.sh
```

## 7. Claimed vs not claimed

CLAIMED: faithful port of the genuine classifiers (verified against upstream
source, self-test across all 186 candidates, C ≡ Python on the real image);
host-measured axes for the retained Townfall eboot (shipping / stats-off /
5.6); drop-in probe glue that compiles against the real probe headers.

NOT CLAIMED: no console/in-game execution of the detector (that is igf5's
record to make); no UE4SS runtime recomposition; version "5.6" is fingerprint
evidence, not the runtime's engine-version acceptance; nothing here validates
any game layout beyond the two axes.
