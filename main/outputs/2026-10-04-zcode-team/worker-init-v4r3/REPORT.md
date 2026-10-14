# REPORT.md — worker-init-v4r3 (2026-10-05)

Mode: HOST ONLY (merge + compile gate + host suite). No console contact, no
composition/packaging runs, no nested subagents, nothing executed in-game. All
writes confined to `outputs/2026-10-04-zcode-team/worker-init-v4r3/`. Deliverable
order honored: CLAIM.json first, staging/diffs/receipts, MERGE.md/DIFF.md/FOLD.md,
this REPORT last. Tool calls: ~25 (within budget).

## 1. Verdict

**The lineage collision is resolved: ONE initializer now carries BOTH lineages.**
`staging/UnrealInitializer-v4r3.cpp` (== `staging/UnrealInitializer.cpp`, sha
`22f06286…`, 2375 lines) = the launch-10-final v4 initializer (`4a2b9400`, the
final-units RE-corrected pool predicate world) + the R3 armed fold (DIFF-2 rev B
`68d0c006` + the in-file `UE4SS_R3_MARSHAL_MODE 0->1` flip, exactly the `8b0c7e99`
lineage's initializer delta). The merge applied CLEANLY (no fuzz, no manual hunk
placement, no semantic conflict — nothing STOPPED). All four gates PASS:

| gate | result |
|---|---|
| Series byte-exactness | O `c85c42f7` + revB = `72247480` (== B's `.r3fallback0`), + flip = `8b0c7e99` == B; both `cmp`-exact |
| Compile gate (190-entry argv, LLVM_CONFIG per mission) | control: final-units v4 → `a437861a…` EXACT (== v4 worker's recorded object); merged: `fe5d26d9…` (1,830,904 B), 2 byte-identical passes; warning multiset (6) == control == v4 baseline |
| Object census | v4: 9 plain + 2 UTF-16 strings PRESENT, 4 retired ABSENT; R3: `[R3] refused:` + deferral line PRESENT, 3 `R3Marshal*` bridges DEFINED (exact mangled names), 0 `R3Marshal*` UND; polarity: Phase-2 FName-verify string compiled out in merged only; armed banner in both |
| Host predicate suite | 69/69 exit 0 vs the merged staging (adapted AL1 call site, one token, documented) AND 69/69 with the unmodified suite vs the v4 worker's own staging (baseline repro); receipts byte-identical |

**Comparator (recorded):** initializer-unit object `fe5d26d9aa1d2abfab6db0eac84ff28e3a5da9afdf1afaa5a449b716409c133c`
— standard 190-entry single-unit argv scope ONLY (see FOLD.md §Edit 2.4 for the
honest scope note vs composition-argv objects).

## 2. How the merge composes (the one-paragraph law)

The rev-B fold compiles out the FName-ctor PLH detour verification and BOTH
EngineTick registrations (R3 arm; pump deferred to the marshal drain); the v4
predicate governs pool ADMISSION in the seed path
(`SeedArming::consume_required_slot_seeds`), which R3 mode does not compile out —
so a valid pool still ADMITS under the engine CurrentBlock law /
span-membership-either-class / entry-0 None anchor, the l11 merged luamod's two
bridge UNDs resolve against this object's DEFINED `R3Marshal*` symbols (plan-D
pairing law pre-satisfied 3/3), and armed-seed / Shape-A-refused / R3-mode launches
all coexist exactly as rev B's comment claims (`bSeedBoundFNameCtor` untouched by
v4). Overlap map: MERGE.md §3 — textually disjoint (A: O-lines 614–852; revB hunks
at 158/223/1312/1782/2041); measured zero-fuzz application onto A.

## 3. Honest deviations / notes

- **Suite AL1 adaptation (one token).** The unmodified v4 suite cannot build
  against `SeedValidation.hpp f09ec9b3` because the CONTROLLER's own final-units v4
  fold deleted the retired `publish_nonimage_readable_spans` alias that AL1 calls.
  The adapted copy changes exactly that one call site to the primary
  `publish_readable_spans` (mirroring the controller's bridge rename); AL1 is
  degenerate-by-design in the alias-free world. Both suite variants measured 69/69
  with byte-identical receipts — nothing was silently skipped.
- **Mission input note.** Mission said A's armed SeedTable is `59effe6e`; the
  shipped launch-10-final table is `d183e0c0` = `59effe6e` + GMalloc V1/V2
  constants, which A's validators REQUIRE (`59effe6e` alone does not compile with
  A). The merged staging ships the A-world headers (`f09ec9b3` / `d183e0c0`) as a
  REQUIRED invariant (MERGE.md §5); B's older staging headers would not compile
  against the merged source.
- **Basename discipline.** The staging carries the mission-named
  `UnrealInitializer-v4r3.cpp` AND a byte-identical `UnrealInitializer.cpp`
  because the l11 runner's regen_receipts hardcodes the composition basename;
  always verify equal hashes before use (FOLD.md §0).
- **Comparator scope.** `fe5d26d9` is NOT a composition-argv comparator; the
  composition-time initializer object will differ (4 prepended `-I`), per the
  l10r3 precedent. The composition-time initializer contract is the runner's
  `consumer_und_gate`, which this census pre-verifies.
- Inherited non-claims stand: no in-game/marshal-drain/calibration claim, no
  launch verdict change, `[Seeds] REFUTED` remains a possible early terminal, no
  FName detour verification under R3 mode (FC-1 future replacement, F-1 open).

## 4. Deliverable inventory (this dir)

| file | what |
|---|---|
| `CLAIM.json` | written first; verdicts V4R3-1..6 + non-claims + input pins |
| `MERGE.md` | lineages, extracted series, overlap map, resolution, composition argument |
| `DIFF.md` | staging file map + the applied series + suite adaptation note |
| `FOLD.md` | l11 editions re-point instruction (driver L11-E2, EDITIONS admissions/compose/units/comparators, runner note) |
| `REPORT.md` | this file |
| `staging/UnrealInitializer-v4r3.cpp` | `22f06286…` the merged initializer (deliverable) |
| `staging/UnrealInitializer.cpp` | byte-identical composition basename copy |
| `staging/SeedValidation.hpp` | `f09ec9b3…` A-world pin (required invariant) |
| `staging/SeedTable.h` | `d183e0c0…` A-world armed+GMalloc table (required invariant) |
| `diffs/r3-initializer-r3-marshal-mode.revB.diff` | `68d0c006…` verbatim series step 1 |
| `diffs/r3-initializer-arming-flip.diff` | `006e18f3…` series step 2 (round-trip clean) |
| `diffs/A-to-v4r3.unified.diff` | full A→v4r3 unified delta (157 lines) |
| `host-test/test_pool_bounds_host.v4r3.cpp` | `78e20890…` suite copy (one-token AL1 adaptation) |
| `receipts/derivation.json` | machine-readable 3-way record (every input sha) |
| `receipts/control-v4-final-units.{argv.json,stdout,stderr,record.json}` + `.o/.d` | control compile (a437861a EXACT) |
| `receipts/v4r3-merged-pass1.*` / `v4r3-merged-pass2-determinism.*` + `.o/.d` | merged compile (fe5d26d9, deterministic ×2) |
| `receipts/object-census.json` | v4 + R3 string/symbol census, both objects |
| `receipts/warning-multiset.json` | 6 warnings, multiset == control == v4 baseline |
| `receipts/fixture-pool-bounds-v4r3.txt` | suite vs merged staging: 69 passed, 0 failed |
| `receipts/fixture-pool-bounds-v4-baseline-repro.txt` | unmodified suite baseline repro: 69/69 (byte-identical receipt) |
| `receipts/suite-v4r3-build.stderr` / `suite-v4baseline-build.stderr` | 0 bytes (clean builds) |
| `receipts/suite-v4r3.stdout` / `suite-v4baseline.stdout` | suite run captures |
| `work/` | scratch (lineage copies, recon reconstructions, suite binaries) |

Nothing outside this worker dir was written; every other worker/controller file
was read-only (copy-from only). No frozen script, no staging, no EDITIONS file was
modified — FOLD.md is the instruction, execution belongs to the controller/loop.
