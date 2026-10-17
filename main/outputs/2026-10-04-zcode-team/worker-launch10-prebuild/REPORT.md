# REPORT — worker-launch10-prebuild

2026-10-05 · The launch-10 **armed candidate is pre-built and verified**: a full
d1fix-lineage composition-6 one-pass (fresh compose + package, executed twice)
with exactly one change vs the launch-9 build — the initializer staging
`SeedTable.h` swapped to the 4/4 armed table (`SeedTable.launch10.h`). Launch 10
is now a **staging decision, not a build wait**. Host build only; no console
contact, no nested subagents; no in-game claim anywhere.

Read `CLAIM.json` first (delivered first, updated with measured verdicts); this
file is the closing report.

## 1. The one-line difference from the launch-9 build

**The table swap — nothing else.**

- `SeedTable.h` `2cab0c5213b10257950f8369030dc1cf2733222b3f6c7e349738a5d7a2d9a053`
  (launch-9: `kGMalloc{false, true, 0x0, "gmalloc", ""}`, `kRequiredSetComplete=false`)
  → `59effe6e4cd34eb0b1138532f1c147b32dc45ffb567e8f8a39eb2ee1d44b0090`
  (armed: `kGMalloc{true, true, 0x930add8, "gmalloc", "<derivation>"}`,
  `kRequiredSetComplete=true`). Two lines in the generated header; the gmalloc
  seed value is worker-gmalloc-derivation's candidate.
- `UnrealInitializer.cpp` is **byte-identical** (`c85c42f7aaab...`): the composed
  run receipts show 368/368 unit names equal and **367/368 source+object hashes
  identical**; only `unreal-unrealinitializer` differs, and only in its object
  (`fe639a859b2f...` → `983b18e24d32...`) — same source sha, new bytes solely
  from the constexpr table folding.
- Candidate build objects: **13 of 14 sha-identical** vs
  `worker-comp6-driver/prx-candidate-composition6-1`; only
  `build/combined-supplier.o` differs (`116f1d702434...` → `0a2a7c9f7cc4...`).
  The bridge object is **byte-identical** to launch 9 (`16331ebe299e514f...`).
- `supplier-contract.json` import list is identical (196 names, list-equal); its
  JSON byte-diff is path provenance (`evidence[].source` → each candidate's own
  combined object) plus the lineage `composed_runtime_sha256` field.

## 2. Verification results (all measured, evidence in this dir)

| check | result |
|---|---|
| Static packaging checks | **17/17 PASS** in each pass (`logs/run-a.console.log`, `logs/run-b.console.log`); gate P1: STATIC_CHECKS_PASSED, 20 checks, failing=none |
| Shape-A fold GONE | refusal string `Required-slot seed table incomplete`: **0 hits** in the armed initializer object (launch-9 Shape-A object: 1). Armed banner `[Seeds] Required-slot seed table armed` (UTF-16LE): present (launch-9: 0); `slot 3 gmalloc bound at` (UTF-16LE): present (launch-9: 0); `[Seeds] REFUTED`: present (launch-9: 0) |
| Seed constants present | all four `SeedTable::k*` slot constants exported as defined symbols; derivation text `FMallocBinned3 UTF-16 literals` + label `0x930add8` in .rodata (launch-9: 0) |
| vtable-walk/detour code present | `rc_seed` probe validators (`check_pointer_probe`, `check_pool_shape`, `entry_matches_ascii`, …) defined; ScanGame lambdas 14 vs 0; +370 defined externs vs Shape-A (2561 vs 2193); initializer object 1,925,952 B vs 1,676,944 B |
| Imports | **196 == 196**, `imports_equal_extended_contract` PASS; I1+ measured UND delta vs frozen r11 exactly `+[]/-[]` |
| Determinism | second full pass (fresh compose + package) **byte-identical**: bare-original `5884d88f...`, bare-sdk `b0353bf4...`, wrapped fself `d8cb4a76...`, combined-supplier.o `0a2a7c9f...`, composed runtime object `d6214abd88ad167f...` (fresh Phase A reproduced it bit-identically, twice) |
| Gate v2 manifest mode vs r11 | **GO (offline gate), 15/15** — W1 W2 W3 I1 D1 F1 S1 P1 + M0 M1 I1+ B1 B2 B3 D2 (`gate-receipt-launch10-armed-a-vs-b.json`, exit 0) |

## 3. B2 callee expectation (task item)

The armed path does **not** change the bridge. This candidate's
`build/runtime_start_bridge.o` is byte-identical to the launch-9 candidate's and
its external callee set (36) is set-equal to launch 9's. Relative to the FROZEN
r11 baseline the bridge lacks exactly 4 callees (`bad_array_new_length` C1/D1,
`__next_prime`, `memcpy`) — a delta **inherited verbatim from the launch-9
comp6 r12/r13 bridge revision** (launch-9's own bridge shows the identical
4-callee delta), not introduced by arming. The armed manifest declares this
r11-relative removal explicitly. The first gate attempt with
`bridge_unchanged:true` honestly FAILed B2 against the r11 baseline and is
preserved unchanged as `gate-receipt-launch10-armed-a-vs-b.json.b2-firstattempt.json`.

## 4. Deliverables (this dir)

- `CLAIM.json` (first) · this `REPORT.md` (last)
- **Armed candidate**: `prx-candidate-composition6-l10-a/` (deliverable; `-b/`
  is the byte-identical determinism reproduction)
  - bare-original `5884d88f6cd6b990de6d8fee9b21033134c4507cfba6ab9a185cb166be51e57a` (30,807,368 B)
  - bare-sdk `b0353bf4cccd062f6ee7dce7b3b758299442355347d19b78a1485d8f89a5c07f`
  - wrapped fself `d8cb4a7637b2ac581270fa05b2d9237dbbea3843f775983b4d96186f573f2c83` (30,818,247 B)
  - combined-supplier.o `0a2a7c9f7cc4e0aeb1f181b595540b281b4eacbf693b26036e7fd37b93a54cc8`
  - composed runtime object `d6214abd88ad167fe9e44cd2d938f4b04b77adacd43f2fe616121b755dee7d83` (117,831,360 B; launch-9: `4dd4f309...`, 117,702,184 B → +129,176 B)
  - initializer unit object `983b18e24d32058ff5093d7f493598eb9551e29f923acba3c12eaf67769191df`
- **Receipts**: `receipts/composition6-replace-receipt.json`
  (`5e3371cccaef8ee3...`, 99 files — launch-9 lineage file-for-file with
  identical hashes on shared paths + explicit `--file` admissions of the 3
  armed-staging files + the 4 Resolution-B headers),
  `receipts/composition6-extra-receipt.json` (`0528d3d60085abfb...`, files map
  byte-equal to launch-9's)
- **Armed manifest**: `composition-change-manifest-launch10-armed.json`
  (`68bc30eed3ccc44c...`, manifest_id `composition6-launch10-armed-prebuild-1`,
  baseline `composition5-r11`)
- **Gate receipts**: `gate-receipt-launch10-armed-a-vs-b.json` (GO) +
  `.b2-firstattempt.json` (the honest B2 first attempt, preserved)
- **Driver**: `build_runtime_composition6_l10pre.py`
  (`007ac4df97fa4269...`) — d1fix copy + L10PB E1/E2 only (diff vs
  `27d85da3...` verified to contain nothing else)
- **Armed staging copy**: `staging/initializer-launch10/` (SeedTable.h =
  armed `59effe6e...`; SeedValidation.hpp/UnrealInitializer.cpp byte-identical
  to the launch-9 staging)
- Runs/overlays: `ue4ss-native-supplier-composition-6-l10/run-a|run-b`,
  `staging/lineage-overlay-2-l10-a|b`; logs `logs/run-a|run-b.console.log`;
  shared driver evidence snapshots `evidence/` (from run-b; run-a gate lines in
  its console log — same composed object, 14/14 chain symbols, 99/6 receipt
  coverage)

## 5. Provenance + scope

- Inputs: launch-9 program source `worker-comp6-driver/staging/comp6-program/`
  (`75abf2ab...`, unchanged), entry r4 (`c97055f4...`) + bridge r13
  (`10385e22...`) driver defaults = the launch-9 packaging units; armed table
  from `worker-initializer-integration/staging/SeedTable.launch10.h`
  (generated by `gen_seed_table.py`; gmalloc 0x930add8 per
  `worker-gmalloc-derivation/GMALLOC-CANDIDATES.json`;
  `worker-seed-table-derivation/SEED-TABLE.json.launch10`).
- Untouched: `worker-initializer-integration/staging` (both seed tables),
  `worker-comp6-driver/*` (candidates + receipts), `worker-d1-determinism-diagnosis/*`,
  all frozen spike trees, the real transport ledger (mirror only, in this dir).
- Writes confined to `worker-launch10-prebuild/` + its script-owned
  run/overlay/candidate dirs inside it.

## 6. Honesty bounds

- No claim that arming makes launch 10 succeed. The armed path re-opens
  FLAG-2 (the first vtable-walk read of a prot=4 page — the igf6 crash class)
  and the tick-detour write as launch-10 verdicts (LAUNCH10-MATRIX rows R1/R2);
  the PLH per-page-transition layer (U-SWAP-4) is a separate, unbuilt design.
  `[Seeds] REFUTED` is the new early terminal if a seeded slot fails its
  in-object validation. Per LAUNCH10-MATRIX §4, the controller reads launch-9
  evidence boxes 1–6 and applies consistency rules BEFORE staging this
  candidate — this pre-build does not substitute for that lookup.
- All evidence here is packaging-level: byte identities, symbol/string censuses,
  gate receipts. No kernel behavior, seed validity, or gameplay quality is
  claimed. Gate S1 (0.12% vs the comp-4-era size baseline) and D1 (double-build
  byte identity) are not quality statements.
