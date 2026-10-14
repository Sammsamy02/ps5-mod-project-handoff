# MANIFEST — postpool-combined-luamod (worker-postpool-candidate)

2026-10-06 · PS5 UE4SS runtime port · PROPOSAL-ONLY STAGING.
Prepared by worker-postpool-candidate. Byte-verified copies only; no source
was authored here, no shared lineage edited, no composition run, no console
contact, no game claims.

## 0. What this set is

The LuaMod-side source set of the POST-POOL candidate: the runtime sources
that change (or deliberately do not change) relative to the current pool-
validation candidate `12fcc41f` (prx-candidate-l11-2), with the G-AP1
input-tag unit ON TOP of the c92c92ff LuaMod base — NOT the merged
facf993a, which is blocked by the R3Marshal cross-TU import issue
(worker-r3-import-fix diagnosing; EDITIONS edit-20 reason: the merge's
RC::Unreal::R3MarshalPumpPending/PublishGameThreadIdentity references trip
three packaging gates).

"Combined" is at CANDIDATE level, not file level: the input-tag unit does
not touch LuaMod.cpp (its admission note §6: "Luamod is untouched by this
unit: NO luamod comparator regen is triggered"). It changes two SEPARATE
pinned TUs that ride in the same composition. LuaMod.cpp in this set is
therefore a byte-identical carry of the admitted base — the deliberate
NOT-facf993a decision recorded in the file's sha.

## 1. Files (sha256 verified at staging time)

| file | sha256 | provenance |
|---|---|---|
| `LuaMod.cpp` | `c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0` | byte-identical carry of `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` — the EDITIONS `compose.luamod_merged` / `comparators.luamod_source` pin (8342 lines). UNCHANGED from the 12fcc41f candidate. |
| `Handler.cpp` | `16fcfe49436270760492cc56034e013fca5a66543ebcd449bf8e013cda00c3af` | byte-identical carry of `worker-input-tag/staging/input-handler-input-tag/Handler.cpp` (baseline = ADMITTED pad_coexist `f7539c60…`, +52 lines, 2 hunks: per-press `pad_key_edge` tag, 60/boot cap + one `pad_key_edge_silent=60`). |
| `PS5PadSource.cpp` | `d2f14de14d6a4697b2709fbe7c7298dc795b9b90553f981fd0be986892575865` | byte-identical carry of `worker-input-tag/staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp` (baseline = ADMITTED pad_coexist `24692ec7…`, +18 lines, 2 hunks: once-per-boot `pad_first_state_read=<buttons-raw>`). |
| `PS5PadSource.hpp` | `2d63c094bf8d3718c53e14087bbf304dcabd8d8ebcc4674513d8f7bce27bda33` | byte-identical CARRY of the staged-provider header (same bytes as the admitted coexist staging header) so the staged TU's quoted include resolves self-contained. NOT a change; no pin moves for it (per worker-input-tag §1). |

Diffs (copied under `../../diffs/`, verified round-trip by worker-input-tag
via `stage_units.py`):
- `PS5PadSource.cpp.24692ec7-to-d2f14de1.diff`
- `Handler.cpp.f7539c60-to-16fcfe49.diff`

## 2. How the input-tag unit integrates (from its ADMISSION-NOTE)

- Both TUs are currently pinned under `EDITIONS.admissions.pad_coexist.files`
  (Handler `f7539c60…`, PS5PadSource `24692ec7…`); adoption moves exactly
  those two rows (16fcfe49 / d2f14de1) and adds a `pad_input_tag` admission
  record in the same shape. No header pins move (PS5PadSource.hpp
  byte-identical; Handler.hpp untouched — all new state is file-local or
  function-local, ABI-consistent).
- `diagnostic_event` needs NO new import: it is DEFINED in-link by the
  composed `diagnostic.c` (units row f5e96c41…, already pinned); the staged
  TUs add only namespace-scope `extern "C"` declarations of that same
  symbol — the identical pattern the admitted coexist PS5PadSource.cpp uses
  today. EXPECTED added undefined imports = ∅ (admission note §5); the
  fixed edition's undefined-delta gate must see exactly that.
- Unchanged and not restaged: `PS5PadNative.cpp` (501d77af…),
  `NativePadInput.cpp` (03c9f6d0…), `diagnostic.c` (f5e96c41…).
- Comparators: no PS5PadSource/Handler object comparator is pinned;
  `input-handler.o` and `native-pad-PS5PadSource.o` (and the composed
  object) are re-measured fresh on both passes and the determinism twin
  must agree. NO luamod comparator regen is triggered by this unit.
- Host evidence: 96/96 PASS (`worker-input-tag/host-test/`, ASan+UBSan
  clean) — tag predicates, 60-press bounding, six distinct keybind codes,
  G-AP3 allow=false silence, input-dead control. Console sink semantics are
  NOT exercised by that unit (honest bound carried).

## 3. What this set does NOT include (deliberate)

- The merged `facf993a` LuaMod (r3-ladder + gate5): blocked by the R3Marshal
  import issue. When worker-r3-import-fix lands, the candidate's LuaMod
  becomes facf993a(+fix) via the worker-next-candidate cascade S1/S7
  (fresh prebuild object measure REQUIRED — never reuse 8343578b/761b610a).
  Until then c92c92ff is the operative LuaMod; consequence carried honestly:
  in any future walk-permitted world, c92c92ff still has the GATE-4
  EngineTick-first ladder (first ExecuteInGameThread throws
  "EngineTick callback registration failed") — the marshal-first re-order
  rides in facf993a.
- The initializer, bridge, program, walkguard, pad-native and diagnostic
  units: unchanged from the 12fcc41f working set (see
  `../../manifest/MAX-EVIDENCE-MANIFEST.json` for the full candidate list).
