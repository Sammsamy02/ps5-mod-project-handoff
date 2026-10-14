# VALIDATION — worker-l3-profile (game-profile v1)

Worker: worker-l3-profile · 2026-10-05 · LOCAL ONLY, no console contact, no engine/source edits.
Artifact under test: `game-profile.townfall-ps5-ue56-v1.json` (generated, never hand-edited).

## 1. How this is validated

Three independent layers, all re-runnable:

1. **Generator cross-checks** (`tools/gen_profile.py`, results in `inputs-manifest.json`):
   the generator RECOMPUTES the image sha256 from the pinned 194,241,424-byte decrypted
   eboot, DERIVES every runtime address as `label + 0x400000` from a parsed label, and
   then cross-checks each emitted value against at least one independent source
   (SeedTable provenance strings, GEOMETRY.json seeds/label-tests, the static-survey
   profile, SeedValidation.hpp, the bridge TU, the live run diag jsonl, staged file
   sizes). The generator REFUSES to write the profile if any check fails — the shipped
   profile cannot exist in a failed-check state.
   **Result: 44/44 checks PASS** (V01…V20 groups; full verbatim list in
   `inputs-manifest.json#checks`).
2. **Schema conformance** (`tools/validate_profile.py`, self-contained 2020-12-subset
   validator): profile conforms to `schema/game-profile.v1.schema.json` (required
   sections, provenance enum, patterns, per-item required fields).
3. **Determinism**: two consecutive generations produced a byte-identical profile
   (sha256 `06f78bc4014ae749…` both times); `--check` mode re-runs all 44 checks with
   no writes.

Reproduce with:

```
python3 tools/gen_profile.py          # regenerate + validate
python3 tools/validate_profile.py     # schema conformance
python3 tools/gen_profile.py --check  # cross-checks only
```

## 2. What is at runtime X — every profiled seed slot answered

All six profiled seed slots (`addresses.seed_slots`). Arithmetic is `runtime = label +
0x400000` (uniform slide, V03); every row was cross-checked against at least one
independent second source (V04/V05/V06/V07 checks):

| slot | name | what is there | file/linked vaddr | runtime | class | provenance |
|---|---|---|---|---|---|---|
| 0 | g_uobject_array | GUObjectArray (FUObjectArray global) | 0x93ba110 | **0x97ba110** | data-readable (phdr7 BSS) | **in-game-observed** — anchor-validation layout A (71,959 objects, read+write, GC-order predicate re-validated); igf4 read 96 B at 0x97ba110 rc=0 + same-value writeback |
| 1 | fname_pool | FNamePool pool object (entry allocator) | 0x9365ec0 | **0x9765ec0** | data-readable (phdr7 BSS) | re-verified — 49/49 Ghidra xrefs == byte scan; init-flag guard stated at runtime 0x9765ea0 = pool − 0x20 (FNAME-POOL-RE §4.1) |
| 2 | fname_ctor_wchar | FName::FName(const wchar_t*, EFindName) | 0x23d590 | **0x63d590** | exec-only CALL-ONLY (phdr0 --X) | re-verified — 5-anchor route + prologue + 9 rel32 call sites + Ghidra decompile line-by-line identical |
| 3 | gmalloc | GMalloc interface pointer (FMalloc**) | 0x930add8 | **0x970add8** | data-readable (phdr7 BSS) | re-verified — single-store bootstrap (ONE store image-wide, 11 loads); rank-1 label lands exactly on GEOMETRY's observed runtime; DeleteFile candidate 0x9350950+0x400000 = 0x9750950 ≠ 0x970add8 (refuted) |
| 4 | static_construct_object_internal | StaticConstructObjectInternal | 0x490aa0 | **0x890aa0** | exec-only CALL-ONLY (phdr0 --X) | re-verified — dominant terminal near-call in 744/1918 windows + byte-identical expected-bytes at file offset 0x494aa0 of this same image |
| 5 | gmalloc_instance | FMallocBinned-family static instance (V1 target, NEVER bound) | 0x93089b8 | **0x97089b8** | data-readable (phdr7 BSS) | re-verified — stated runtime in FNAME-POOL-RE §1; gates slot-3 arming via V1 |

Support constants (never seeded; `addresses.support_constants`), same rule:

| name | file vaddr | runtime | role |
|---|---|---|---|
| gmalloc_vtable | 0x7ec26e0 | **0x82c26e0** | V2 validator constant (RELA-reconstructed; compared, never dereferenced); phdr3 RELRO, readable in-game |
| gmalloc_interface_ptr | 0x9350950 | **0x9750950** | GMalloc slot read by FMemory::Malloc 0xb7a20; wrapper chain installs here |
| ugameengine_tick_pinned | 0x2fac8c0 | **0x33ac8c0** | documentation-only (slot 10): runtime binds from the live vtable; seeding refuses. igf4 proved the CLASS at 0x33ac8c0 (covered_not_readable), not a bind |
| fname_heap_envelope_RETIRED | [0x8018c000,0x100118000) | [0x8058c000,0x100518000) | RETIRED from admission (v4): vintage-transient; re-derive from a fresh same-invocation mapscan |

The extent law they all live inside: runtime image extent **[0x400000, 0xc0602c8)**
(file extent 0xbc602c8 = max(p_vaddr+p_memsz)) — identical across SeedValidation.hpp,
the bridge TU, max(phdr memsz_end), and the in-game `lmf_extent_*` events on runs
1/3/4 (V08, V15).

## 3. Machine check inventory (44/44)

| group | count | what it proves |
|---|---|---|
| V01–V02 identity | 2 | recomputed eboot sha256 `3b6cfad4…` == SeedTable == GEOMETRY == capture.json == layout-profile pin; bytes 194,241,424 |
| V03 slide | 1 | slide 0x400000 agrees in GEOMETRY, SeedTable.kBaseObserved, bridge kCandidateBase |
| V04/V05 per-slot (×6) | 12 | label+slide arithmetic and agreement with each slot's independently stated runtime |
| V06 geometry seeds | 6 | GEOMETRY seeds (GUA/ctor/SCO/gmalloc/tick) match the slots; GMalloc rank-1 label test; rejected candidate refuted |
| V07 survey | 4 | static-survey labels (0x93ba110 / 0x23d590 / 0x490aa0 / 0x2fac8c0) equal the profiled labels |
| V08 extent | 1 | 0xbc602c8 (hpp==bridge==max phdr) and runtime end 0xc0602c8 == GEOMETRY == observed |
| V09 address classes | 2 | classify_seed_address boundaries == phdr table; hpp ranges parse |
| V10 pool structure | 3 | kPoolLayout offsets/bits/counts == RE verdict; RE + Ghidra tokens present (49/49 xrefs, 236 names, decompiles) |
| V11 placement | 1 | pool + instance in phdr7 BSS; vtable in phdr3 RELRO |
| V13/V14 runtime caps | 2 | kMaxPublishedSpans=16; capture 16384, kve 1160/64, page 16384 |
| V15 observed runs | 4 | run3+run4: lease 6/6, anchors 7, intervals 1, extent exact; spanpub saturated 16 of 123/127, envelopes 0x11003d0000 / 0x1100410000 (recomputed from low/high) |
| V16 mod tree | 2 | loadlist sizes == staged files (main.lua 21415, UEHelpers 9962, PS5Pad.ini 2958); final-package variant recorded |
| V17 API | 1 | 17 provided entries parsed, all mod-load-critical names present, 2 registered-but-unused |
| V18 anchors | 2 | bridge 6 window probes ≠ lmf_anchor_count 7 (= 7 image-intersecting spans, 6 readable == lease 6); staged lmf caps 16/16384 |
| V20 engine | 1 | 5.6 from survey; patch/revision honestly null |

## 4. Honest bounds

- Slots 1–5 carry `re-verified` (offline, two-channel) provenance — in-game arming
  (Phase F: pool predicates, GMalloc V1/V2, ctor behavioral probe, SCO bind) has NEVER
  run; launch-10 runs 1–4 refused earlier (pad/loadlist classes, E2/B7) with the
  geometry quartet reproduced. The profile records this in `unresolved` and
  `readiness.runtime_binding_ready=false`. Nothing here upgrades rung claims.
- Slot 0 is the only `in-game-observed` address (placement + readability + 96-byte
  same-value writeback — NOT structural header identity; igf4 structural predictions
  were REFUTED and are recorded as such).
- `lmf_anchor_count=7` and bridge `kDataWindowAnchors=6` are different counters
  (image-intersecting kernel spans vs window-anchor byte probes); both are profiled
  with their own meaning after the initial wrong equation was caught by check V18 and
  corrected — the shipped checks encode the correct semantics.
- The sdiag jsonl files are cumulative across the boot's sessions; per-session pids
  (355/370) come from status/oracle records, with the stale `pid_first` noted
  (classifier gap G2), and run1's 22761-entry vmwalk > 16384 is recorded as the
  unreconciled G7 question, not silently normalized.
- No console contact, no writes outside this worker directory; the generator reads the
  pinned sources and recomputes the image hash — it never trusts a recorded hash it
  can recompute locally.
