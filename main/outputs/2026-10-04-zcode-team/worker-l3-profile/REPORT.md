# REPORT — worker-l3-profile (L3 game-build profile)

Worker: worker-l3-profile · 2026-10-05 · LOCAL ONLY · no console contact, no edits outside this directory.

## 1. What was built

The **L3 layer** of the universal-translator stack — a machine-readable profile of
THIS game build that any L1 compat runtime or L2 engine adapter consumes:

```
L1 compat runtime      (UE4SS-PS5 composition: hooks, leases, capture, spanpub)
L2 engine adapter      (UnrealInitializer seed path: binds FName/GUObjectArray/GMalloc/SCO)
L3 GAME-PROFILE        (this artifact: the per-build facts both layers resolve against)
```

| deliverable | file | what it is |
|---|---|---|
| Schema | `schema/game-profile.v1.schema.json` | game-profile v1 contract: identity, engine+key structures, addresses (translation rule + seed slots), memory geometry (phdrs, observed VMMAP, owned-read policy), API surface, mod requirements, runtime facts, provenance enum (`in-game-observed` / `re-verified` / `host-derived`), sources, unresolved, readiness |
| Generator | `tools/gen_profile.py` | emits the profile from the verified sources; recomputes the 194 MB image sha256, derives every runtime address as `label + 0x400000`, parses the seed table / geometry / survey / validator / bridge / pool RE / Ghidra findings / loadlist / readiness API table / run diagnostics; **refuses to write if any of 44 cross-checks fails** |
| Profile | `game-profile.townfall-ps5-ue56-v1.json` | generated artifact (sha256-16 `06f78bc4014ae749`, deterministic) |
| Validator | `tools/validate_profile.py` | self-contained schema conformance check (offline host; no third-party deps) |
| Manifest | `inputs-manifest.json` | machine record: per-source hashes recomputed at generation + all 44 check results |
| Validation | `VALIDATION.md` | method, the "what is at runtime X" answer table for all 6 seed slots + support constants, check inventory, honest bounds |
| Claim | `CLAIM.json` | scope, hashes, inputs, honest bounds |

## 2. What the profile says (essentials)

- **Identity**: Silent Hill Townfall, PPSA33286, decrypted eboot 194,241,424 B,
  sha256 `3b6cfad4…22fc` (recomputed at generation; equals all five recorded pins),
  ELF64 x86-64 e_type 0xFE10, 14 phdrs, no sections, scrambled symbols.
- **Engine**: Unreal 5.6 (survey-resolved; patch/revision honestly null — the "5.6.1"
  RE-worker label is a recorded note, not a fact). Version slot 6 is settings-owned
  (`[EngineVersionOverride] 5.6`).
- **Address law**: `runtime = linked_file_vaddr + 0x400000`, validated in-game
  (window anchors, GUA live read, igf7 byte-matched reads) and reproduced by the
  launch-10 `lmf_extent` events; file-offset trap (+0x4000 for LOAD0/1) documented.
- **Six seed slots** (label → runtime): GUA 0x93ba110→0x97ba110 (in-game-observed);
  fname pool 0x9365ec0→0x9765ec0; FName ctor 0x23d590→0x63d590; GMalloc
  0x930add8→0x970add8; SCO 0x490aa0→0x890aa0; GMalloc instance 0x93089b8→0x97089b8
  (V1 target, never bound). Support constants: GMalloc vtable 0x7ec26e0→0x82c26e0,
  GMalloc interface slot 0x9350950→0x9750950, doc-only tick 0x2fac8c0→0x33ac8c0,
  RETIRED heap envelope.
- **FNamePool structure** (two RE channels, no disagreements): offsets
  CurrentBlock +0x8 / CBO +0xC / Blocks[] +0x10 / hash buckets +0x10040 /
  NoneId +0x14040 / BytePropertyId +0x14044; 16-bit block offsets, 8192 blocks,
  0x20000-byte blocks via `Malloc(0x20000, 2)`, u16 entry headers (wide bit0, probe
  hash 5:1, len 15:6), 236 default names. **Corrected CurrentBlock semantics**
  (0-based open-block INDEX, 0 valid and common) and the **entry-0 'None' anchor**
  are profiled exactly as the v4 validator encodes them.
- **Memory geometry**: full phdr table with igf3-observed kernel fragments, the
  owned-read policy earned by launch-9/igf4/igf6 (lease-covered reads only; exec-only
  pages call-only; LOAD8 unmapped-until-proven; loud span-table refusal; truncation
  honesty), address-class ranges byte-equal to `classify_seed_address`, and every
  retracted false claim preserved verbatim so no consumer re-derives them.
- **API surface**: 17 provided entries parsed from the file:line-evidenced readiness
  inventory (all used by the Item Spawner), 2 registered-but-unused.
- **Mod requirements**: the launch-10-verified loadlist (sizes byte-enforced — run2's
  code=-8 proves it), the pad→key map, the mod's 17 API usages, the UE4SS.log oracle
  line, and the v3 settings gates.
- **Runtime facts**: 13 capture/lease/spanpub constants (16384-entry capture,
  kve 1160/64, lease array 128, top-16 span publication, page 16384, caps 16/16384,
  `/app0` configured root, 6 window probes) plus observed runs 3 and 4 (lease 6/6,
  anchors 7, intervals 1, extent exact, spanpub 16 of 123/127 saturated, envelopes
  recomputed from low/high).

## 3. Validation summary

**44/44 generator cross-checks PASS; schema conformance VALID; regeneration is
byte-identical.** The checks caught one real modeling error during development —
equating the bridge's 6 window-anchor probes with the diag's `lmf_anchor_count=7`
(actually the image-intersecting kernel-span count, igf3 shape) — which was corrected
before anything shipped; the shipped checks encode the correct semantics. Full detail
in `VALIDATION.md`; verbatim check list in `inputs-manifest.json`.

## 4. Honest bounds

- The profile is an **assembly of verified facts** — it claims fidelity to its
  sources, never capability. `static_survey_ready` / `runtime_binding_ready` remain
  **false**, exactly as the upstream layout profile and launch-10 evidence record.
- Slots 1–5 are `re-verified` (offline, two-channel RE). In-game seed arming (Phase F)
  has never run; launch-10 runs 1–4 refused earlier (pad/loadlist classes) with the
  geometry quartet reproduced. No rung claim is created or upgraded here.
- Only slot 0 is `in-game-observed` — placement, readability and 96-byte same-value
  writeback; NOT structural identity (igf4 header predictions were refuted and are
  recorded as refuted).
- The mission's `worker-actual` input directory does not exist in this worktree; the
  runtime's capture/lease/spanpub facts were sourced from the actual final-units
  runtime sources (bridge TU + SeedValidation.hpp + staged lmf header) and the
  launch-10 evidence jsonl. Recorded in CLAIM.json; nothing invented to fill the gap.
- run1's 22761-entry vmwalk > the pinned 16384 capture capacity with a clean stop is
  carried as the unreconciled oracle gap G7, not normalized away.

## 5. How to consume / regenerate

```bash
python3 tools/gen_profile.py --check    # re-run the 44 cross-checks (no writes)
python3 tools/gen_profile.py            # regenerate profile + inputs manifest
python3 tools/validate_profile.py       # schema conformance
```

An L1/L2 consumer reads `addresses.seed_slots` (+ `support_constants`) for binding
targets, `memory_geometry.owned_read_policy` before touching memory,
`engine.key_structures` for adapter binding shapes, `api_surface` +
`mod_requirements` for mod-facing behavior, and must honor `provenance_class` per
fact: only `in-game-observed` facts have ever been exercised on the target.
