# REPORT — worker-launch10-gmalloc-integration (2026-10-04/05)

Integrated the worker-gmalloc-derivation GMalloc validator (V1/V2) into the
launch-10 armed candidate's staging. No console contact, no nested
subagents, host build only (local prospero-clang++ + host c++ tests), no
engine execution, no frozen/wiring file modified. Proposal-only.

## 0. Verdict up front

**The armed launch-10 candidate does NOT run the GMalloc validator.** It
binds GMalloc (seeded 0x930add8 linked / 0x970add8 runtime) after only the
generic admission probes (address non-null/8-aligned; loaded qword non-null/
8-aligned). The V1/V2 data-side checks from GMALLOC-CANDIDATES.json
(V1: `[0x970add8] == 0x97089b8`; V2: `qword@0x97089b8 == 0x82c26e0`), whose
arming rule says "seed arms only if V1 AND V2 pass", are absent from the
staged source and from the compiled armed object.

**Delivered: the missing validator** as an anchored diff (+75/−1 source
lines, 4 files) applied to a copy of the armed staging under
`staging-l10-gmalloc-validator/` — the prebuild staging and the armed
candidate (fself d8cb4a76) are untouched. Compile gate with the EXACT
190-arg composition unit argv: exit 0, zero errors, zero new warnings.
Host pass/refute tests: ALL PASS. **State: armed + validator, staged and
integration-ready; the composed prx is NOT rebuilt and the validator has
not run in-game.**

## 1. Determination (task 2) — how it was proven

- Seed-consumption path: `SeedArming::consume_required_slot_seeds()`,
  Phase 2b (staged `UnrealInitializer.cpp:763-783`; armed staging content
  byte-identical, c85c42f7…). Sequence: `require_present` -> pointer probe
  on the address -> weak live probe (`read_u64` + `check_pointer_probe`) ->
  **bind** -> log. No equality against the derived instance/vtable anywhere;
  `SeedValidation.hpp` (rc_seed) has no such checks. The validator addresses
  appear in the staging only inside SeedTable.h provenance STRINGS.
- The other three validators run in the same function as: Phase 1 per-launch
  base check; Phase 3 FName pool chain (shape + retries inside the
  validation budget, then terminal entry ASCII matches); Phase 4 ctor bind +
  behavioral ByteProperty probe (`check_probe_id`); Phase 5 SCO bind.
  Failures are all `seed_refused` -> terminal `[Seeds] REFUTED` throw.
- Armed-object census (run-b `unreal-unrealinitializer.o`, 1,925,952 B):
  bind string present (1 hit); `slot 3 gmalloc V1 at`/`V2 at` and both
  derived-shape refusal strings: 0 hits. Matches the prebuild's own receipt:
  arming was a 2-line SeedTable.h swap, initializer content unchanged.
- Why it matters: a wrong-but-live aligned qword at the slot (heap churn,
  shifted allocator, absent bootstrap store) would bind and only surface at
  the PostInitialize consumer — the exact gap V1/V2 close.

## 2. The validator addition (task 3)

`gmalloc-validator.diff` vs `worker-launch10-prebuild/staging/
initializer-launch10/`:

1. `SeedTable.h` + `SeedTable.launch10.h` (kept byte-identical, as armed):
   `kGMallocInstanceLabel = 0x93089b8`, `kGMallocVtableLabel = 0x7ec26e0` —
   base-relative, same translation_rule as the slots; documented as
   validator support constants (never bound), regeneration must preserve.
2. `SeedValidation.hpp`: pure total checks `check_gmalloc_instance` (V1) and
   `check_gmalloc_vptr` (V2) in the rc_seed house style. Both reads stay in
   phdr7 BSS; the RELRO vtable is only COMPARED as a link-time constant —
   no mprotect/FC-1 gate needed at V1/V2 strength (V3's RELRO deref would
   need it; V3/V4 remain optional and unimplemented, documented).
3. `UnrealInitializer.cpp` Phase 2b: V1 on the already-loaded qword, then
   read the instance vptr and V2 it — both BEFORE
   `GMalloc = std::bit_cast<FMalloc**>(...)`, i.e. before Phase 3/4 and
   long before the PostInitialize consumer. Failure -> `seed_refused` with
   the offending addresses (terminal, nothing bound, no vtable walk, no
   detour — consistent with SHAPE-A all-or-nothing arming; stricter-earlier
   than the spec's "config.gmalloc=false -> PostInitialize throw" wording,
   same loud-no-misbind contract). Bind log now records
   `V1 instance + V2 vptr match`.

## 3. Gates and evidence (all in receipts/ unless noted)

- Host tests (`../host-tests/`): exact staged headers; `-std=c++23 -Wall
  -Wextra -Werror` exit 0; run exit 0 ALL PASS. Static asserts pin
  GMalloc 0x970ADD8 -> instance 0x97089B8 -> vtable 0x82C26E0 and the armed
  table; simulated Phase 2b read chain accepts a correct image and rejects
  6 wrong-image shapes (foreign heap pointer, null malloc, instance±8,
  corrupted/null vptr, swapped operands) plus pure near-miss cases.
- Compile gate: exact composition argv (190 args) recovered from the armed
  run-b `commands.json` (`run-b-initializer-argv.json`), only source/-o
  substituted (`compile-gate.argv.json`); local prospero-clang++ (LLVM
  21.1.8) via the SDK wrapper with LLVM_CONFIG set. **exit 0**, object
  1,928,968 B (+3,016 vs baseline), sha256 9abae93e…; stderr identical
  warning set to the armed baseline (6 pre-existing port/dep-header
  warnings; zero new). Note for reproducers: the SDK clang++ wrapper needs
  `LLVM_CONFIG` pointing at the LLVM-21.1.8 llvm-config.
- Object verification: new object contains `slot 3 gmalloc V1 at 0x`,
  `slot 3 gmalloc V2 at 0x`, both derived-shape refusal strings, and the
  new UTF-16 bind wording — all 0 hits in the armed baseline object.
- `hash-manifest.json`: sha256 of all 7 inputs (spec, armed staging trio,
  armed object, baseline stderr, argv source) and 15 outputs.

## 4. Honest limits

- The validator has NOT executed in-game; identity/ABI provenance stays
  "unverified" until an authorized launch runs the staged unit. No claim
  beyond compile/host-test evidence.
- The composed prx/fself is NOT rebuilt — adoption is one
  `--replace-runtime-unit` repoint in the existing composition flow (the
  gate here compiles that exact unit in isolation with the exact argv).
- `gen_seed_table.py` not extended; the two constants are documented
  in-header for the next regeneration.
- gmalloc remains a prototype-stage, data-side admission gate: V1/V2 make
  the bind shape-exact against the derived image, not a behavioral proof
  (allocation behavior is never exercised).

## 5. Files

- `CLAIM.json` (first), `GMALLOC-VALIDATION.md` (full verdict),
  `gmalloc-validator.diff`, `staging-l10-gmalloc-validator/` (4 files,
  integration-ready), `host-tests/` (test source + build/run receipts),
  `receipts/` (argv, gate stdout/stderr, gate object, run-b argv extract,
  hash manifest), `REPORT.md` (this file, last).
