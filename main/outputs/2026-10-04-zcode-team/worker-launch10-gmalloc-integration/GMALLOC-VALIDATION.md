# GMALLOC-VALIDATION.md — launch-10 candidate vs the GMalloc validator (V1/V2)

Worker: worker-launch10-gmalloc-integration · 2026-10-04/05 · proposal-only,
no console contact, no nested subagents, host build only, no engine execution.

## 0. Verdict

**The armed launch-10 candidate (`prx-candidate-composition6-l10-a`, fself
d8cb4a76) does NOT run the worker-gmalloc-derivation V1/V2 data-side
validators. It arms the gmalloc seed and binds GMalloc on the generic
probes alone (non-null + 8-aligned, address and loaded pointer).**

This delivery: the missing validator, authored as a small anchored diff
against the armed staging, compiled clean under the composition's exact
190-arg unit argv (exit 0, zero new warnings), host-tested pass/refute
(ALL PASS). **State after this delivery: armed + validator (staged,
integration-ready — NOT yet composed into a prx; the armed candidate itself
is untouched).**

## 1. What the spec demands (worker-gmalloc-derivation/GMALLOC-CANDIDATES.json)

Rank-1 candidate `3_gmalloc` = the FMalloc* GLOBAL at linked 0x930add8
(runtime 0x970add8). The instance is a separate global at linked 0x93089b8
(runtime 0x97089b8, factory 0xb3f0 constructs it in place); the vtable is
linked 0x7ec26e0 (runtime 0x82c26e0, phdr3 RELRO). Validator:

- **V1**: `qword@0x970add8 == 0x97089b8` — [GMalloc] must equal the derived
  instance address exactly (pointer-to-known-BSS, not a heap pointer).
- **V2**: `qword@0x97089b8 == 0x82c26e0` — the instance vptr must equal the
  derived vtable address.
- **Arming rule**: "seed arms only if V1 AND V2 pass; any failure leaves
  config.gmalloc=false -> today's loud PostInitialize throw."
- V3 (RELRO dtor-pair read) and V4 (negative-control re-read) are explicitly
  optional; NOT included here (V3 would additionally need the FC-1
  mprotect-first discipline for a RELRO deref — neither V1 nor V2
  dereferences RELRO: the vtable is only ever COMPARED as a link-time
  constant, so both reads stay in phdr7 BSS).

## 2. What the armed candidate actually does (determination, task 2)

Seed-consumption entry point: `SeedArming::consume_required_slot_seeds()`
(merged staged `UnrealInitializer.cpp`, Phase 2b, source lines 763-783;
armed staging is byte-identical content c85c42f7…). Slot 3 sequence:

1. `require_present(kGMalloc)` (arming itself is the all-four-slots Shape-A
   gate at ScanGame; kRequiredSetComplete=true in the armed table).
2. `check_pointer_probe(gmalloc_runtime)` — the ADDRESS is non-null/aligned.
3. Weak live probe: `read_u64` at the slot; `check_pointer_probe` —
   `[GMalloc]` is *some* non-null 8-aligned pointer.
4. `GMalloc = std::bit_cast<FMalloc**>(gmalloc_runtime)` — **BIND**, then
   `[Seeds] slot 3 gmalloc bound at …` log.

No equality against 0x97089b8, no vptr read, no 0x82c26e0 comparison exists
anywhere in the unit (`SeedValidation.hpp` has no such check; grep for
93089b8/7ec26e0/97089b8/82c26e0 across the staging hits only the SeedTable.h
derivation PROVENANCE STRINGS).

Object-level corroboration (armed initializer object,
run-b/unreal-unrealinitializer.o, 1,925,952 B):
- `'slot 3 gmalloc bound at'` (UTF-16LE): present — the bind is compiled in.
- `slot 3 gmalloc V1 at` / `slot 3 gmalloc V2 at` (narrow): 0 hits.
- `…derived FMallocBinned3 instance address` / `…derived vtable address`
  refusal strings: 0 hits.

Consistent with the prebuild's own receipts: the arm was a 2-line generated
header swap (SeedTable.h 2cab0c52… -> 59effe6e…; CLAIM.json "the_swap"),
initializer content unchanged c85c42f7…. **Conclusion: armed, validator
absent — the seed binds on probe-only evidence.** A wrong-but-live aligned
pointer at 0x970add8 (heap churn, ASLR-shifted allocator, stale bootstrap)
would bind and only fail later at the PostInitialize consumer — the exact
gap the spec's V1/V2 arming rule closes.

## 3. The delivered validator (diff, task 3)

`gmalloc-validator.diff` (anchored, 4 files, +75/−1 source lines; 113 lines
with headers) — applied to a copy of the armed staging in
`staging-l10-gmalloc-validator/`; the prebuild staging is NOT modified.

1. `SeedTable.h` + `SeedTable.launch10.h` (kept byte-identical, same as the
   armed pair): two base-relative constants next to kHeapLo/kHeapHi —
   `kGMallocInstanceLabel = 0x93089b8`, `kGMallocVtableLabel = 0x7ec26e0`
   (runtime = base + label, same translation_rule as the slots; documented
   as validator support, NOT seeds; regeneration must preserve them).
2. `SeedValidation.hpp`: two pure, total checks in the rc_seed house style
   (plain values, never dereference) — `check_gmalloc_instance(loaded,
   expected_instance)` (V1) and `check_gmalloc_vptr(vptr, expected_vtable)`
   (V2), with the arming rule and the no-RELRO-deref note in-code.
3. `UnrealInitializer.cpp` Phase 2b: between the weak live probe and the
   bind — read `[GMalloc]` (already loaded as malloc_ptr) → V1 against
   `base + kGMallocInstanceLabel`; on V1 pass read the instance vptr → V2
   against `base + kGMallocVtableLabel`; any failure is
   `seed_refused("slot 3 gmalloc V1/V2 at 0x…: …")` — the existing terminal
   `[Seeds] REFUTED` throw: nothing bound, no Phase 3/4, no vtable walk, no
   detour. Bind log extended to `(data; V1 instance + V2 vptr match;
   identity and ABI unverified)`.

Failure-mode note (honest deltas vs the spec sentence): the spec's
"failure leaves config.gmalloc=false -> today's loud PostInitialize throw"
predates the merged SHAPE-A unit. In the armed architecture there is no scan
fallback for slot 3 (ps_scan is skipped when armed), and arming is
all-or-nothing, so the strictest faithful translation of the rule is the
existing `seed_refused` path: terminal loud refusal BEFORE anything binds —
earlier and stricter than a PostInitialize throw, same "loud, no misbind"
contract. V1/V2 run before the bind and before the FName pool chain
(Phase 3), so a wrong image refuses in microseconds, not after pool waits.

## 4. Gates (host build, both green)

- **Host tests** (`host-tests/host_gmalloc_validator_tests.cpp`, the exact
  staged headers): `c++ -std=c++23 -Wall -Wextra -Werror` exit 0; run exit 0
  ALL PASS — static_asserts pin GMalloc 0x970ADD8 -> instance 0x97089B8 ->
  vtable 0x82C26E0; correct-image bind accepted; 6 refusal shapes rejected
  (foreign heap pointer, null malloc, instance±8, corrupted vptr, null vptr,
  swapped operands) plus pure V1/V2 near-miss cases.
- **Compile gate**: the EXACT 190-arg composition unit argv recovered from
  the armed run-b `commands.json` (only the source/-o paths substituted),
  local prospero-clang++ (LLVM 21.1.8), **exit 0, zero errors, zero new
  warnings** (same 6 pre-existing port/dep-header warnings as the baseline
  run-b stderr: Atomic.hpp, UnrealType.hpp, asmjit.h). Object 1,928,968 B
  (baseline 1,925,952, +3,016), sha256 9abae93e….
- **Object verification**: new object contains the V1/V2 refusal strings,
  the new UTF-16 bind wording, and `check_gmalloc_instance`; the armed
  baseline object contains none of them; the old bind prefix is retained.

## 5. What this does NOT claim

- The validator has NOT run in-game (V1/V2 are armed-path checks; they
  execute only when the staged unit launches against the live image).
  Identity/ABI provenance stays "unverified" until then.
- The composed prx is NOT rebuilt: launch-10's armed candidate
  (fself d8cb4a76) is untouched; adopting the validator means re-running the
  composition with `--replace-runtime-unit` pointed at
  `staging-l10-gmalloc-validator/UnrealInitializer.cpp` (same argv as the
  prebuild; the gate here proves that unit compiles clean in isolation).
- V3/V4 optional checks not implemented; SeedTable regeneration tooling
  (gen_seed_table.py) not extended (constants documented in the header).
