# Agent DA — l26 layer-2 verified-query fix (walk-finds-nothing, converged)

Date: 2026-10-08, late evening. THE fix per the converged diagnosis (CR N1
rank-1 + CW §5/§6.1 + CY TEMPLATE-C string-mode + CB Store-vs-shard-Find
background). IMPLEMENTED, compiled, harness-proven, rows synced. Only this
report, the staged candidate edit, its CHANGES rows/scope key, the
da-verify logs, and /tmp/l26-walk-harness-da/** written.

## What landed (one file edited)

`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp`
(`__PROSPERO__`-guarded; host bodies byte-identical to upstream; original
row identity unchanged: l22 diagnostics 9b335a51…):

1. **Helper block at :528-681** (inside the existing CP `#ifdef`, AFTER
   MakeFindXOfClassName — CP's :498-526 region is byte-untouched, so
   agent-CS's pinned `cp_makefindxof` extraction still resolves):
   - `FindXOfQueryMismatch(Name, Query)` — TOASTRING-VERIFY:
     `String::iequal(StringViewType{Name.ToString()}, Query)`. ToString is
     the port's existing machinery (the one GetFullName / init type-scan
     use), NOT the unbound CJ ToStringInternal@0x63FA00. On ToString
     exception → treated as mismatch (fallback runs, never trusts an
     unverified index).
   - `FindXOfStoredNameMatchesQuery(UStruct*, Query)` — the init-proven
     comparator shape: `String::iequal(StringViewType{Struct->GetNamePrivate().ToString()}, Query)`,
     per-object try/catch → false.
   - `FindFirstOfStringWalk(Query)` — mirror of the FindFirstOf(FName) walk
     body (direct + super-chain, same IsValidObjectForFindXOf placement),
     compare mode swapped to string.
   - `FindAllOfStringWalk(Query, Out, bApplyFindXOfFilter)` — mirrors the
     2-arg FindAllOf walk (filter=true: IsValidObjectForFindXOf gates both
     direct and super matches) or the cache-scan arm (filter=false: no
     FindXOf exclusion) — CR §4's arm semantics preserved exactly.
   - `FindFirstOfVerified / FindAllOfVerified` — construct via CP
     MakeFindXOfClassName, verify, then fast `FindFirstOf(FName)` /
     `FindAllOf(FName, Out)` on MATCH or the string walk on MISMATCH.
   - Marker: `Output::send<LogLevel::Error>(STR("[FFO] query-fallback
     [n:{:X}]\n"), query_index)` — deduped per constructed ComparisonIndex,
     128-site cap — the exact CP add-fallback dedup family.
2. **Entry rewiring** (all `__PROSPERO__` branches only): the five
   FindFirstOf string overloads and the five no-cache FindAllOf string
   overloads now call the Verified wrappers; FindAllOf(StringType, Out,
   bUseCache) verifies inline and passes
   `!bUseCache || ShouldUseHashTables()` as the filter flag (arm-selection
   semantics). Non-Prospero branches, the public FName overloads,
   FindObjects, the chunked walk, seeds, GUObjectArray, exclusions, the
   ctor adapter thunk, and member-name FNAME_Find are UNTOUCHED.

## Marker string (exact)

`[FFO] query-fallback [n:X]` (X = constructed ComparisonIndex, uppercase
hex, one line per index, 128 cap). Sibling: CP's `[FFO] add-fallback [n:X]`
unchanged. Console readout after this lands: fallback engaged ⇒ the N1
mismatch is directly visible; fallback silent + add-fallback silent ⇒ the
query index round-trips (N1 falsified in that pool state).

## Verification

1. **Dedicated walk harness `/tmp/l26-walk-harness-da/`** (CD pattern;
   CS's /tmp/l26-walk-harness/ template was still mid-flight at 22:12 so
   this fixture is separate and CS's tree was left untouched — CS's own
   run completed at 22:21, all their scenarios pass, and their extractor
   ran against my edited candidate confirming the CP region pin):
   35 verbatim sha-pinned regions (the code under test = live candidate
   :498-526 CP block and :546-680 DA block; the walk chain, FName ops,
   filters, flags, member-offset getters from the port tree), mock engine
   pool/ctor with the N1 knob (FNAME_Find of a non-init name returns a
   WRONG valid id; FNAME_Add correct = l25 proof), mock GUObjectArray
   (full 64K-slot chunk, console offsets). ASan+UBSan, 7/7 PASS:
   - DA1_GOOD: correct ctor → fast Equals path finds inst1/3 objects, CDO
     excluded, ZERO markers (fast path preserved, no false fallback).
   - DA2_N1_CONSOLE_SYMPTOM: N1 knob, PRE-fix call shape → nullptr / 0
     objects, no crash, no markers — tonight's console symptom reproduced.
   - DA2B_N1_FIX: same knob, FindFirstOfVerified → inst1 found (symptom
     INVERTED), marker byte-exact `[FFO] query-fallback [n:D]`, FindAllOf
     fallback finds 3 (derived via super-chain string compare), CDO still
     excluded.
   - DA3_ABSENT_NAME: Find miss → CP add-fallback (1 marker), Add-constructed
     name round-trips → NO query-fallback, correct nullptr.
   - DA4_DEDUP: three mismatched queries, same wrong index → ONE marker.
   - DA5_CACHE_ARM: filter=true excludes CDO (2-arg arm), filter=false
     includes it (cache arm).
   - DA6_BAD_ID: garbage out-of-pool id (0x7FFFFFFF) → ToString yields
     non-matching string, fallback still finds objects.
   Repro: `python3 /tmp/l26-walk-harness-da/extract_and_build.py`.
2. **Compiles (Z13 recipe, unit unreal-uobjectglobals argv from the l24
   receipt, source swapped, merged l26 header overlay):** host
   /usr/bin/clang++ -fsyntax-only exit 0 (3 known inherited warnings) AND
   full `-c` codegen exit 0 (1,012,888-byte object); REAL
   prospero-clang++ -fsyntax-only exit 0 with only the known inherited
   Atomic.hpp/-Wswitch + UnrealType warnings. Logs + overlay staged at
   `outputs/2026-10-08-l26-runtime/typescan-r4/results/da-verify/`
   (host-syntax/host-c/prospero-syntax-uobjectglobals.log).
3. **Suites:** l26-runtime tests/run_tests.py PASS (scan-refusal a-d +
   item-6, plain + asan-ubsan); l26-runtime profile/tests/run_tests.py
   PASS (preflight10/INI guard/digest/join15/prior88). NOTE:
   typescan-r4/tests/run.py currently FAILS in test_scan.py —
   PRE-EXISTING/concurrent: tests/actual_scan.cpp was modified 22:36 by
   another in-flight agent (references NativePropertyGuard::ReadClassChecked/
   ReadNextChecked refusal APIs their shim set doesn't declare yet); it
   never compiles my file (its only UObjectGlobals reference is a local
   mock namespace at :34/:145). Left untouched for its owner.
4. **CHANGES.json synced** (typescan-r4): candidate_sha256 for
   candidate/src/UObjectGlobals.cpp recomputed in BOTH changes[] and
   files[] rows — 12470d77b3a3ebcf… → **4abf743ba5f252d3…**
   (4abf743ba5f252d36ecb7a09a0b46eab703fc5d8d45a432bdc139091278351ee);
   original/copied_original rows unchanged (verified on disk); new scope
   key `scope_l26_layer2_verified_query` documents the change; full row
   integrity re-checked (every original/candidate sha matches disk).

## Notes for the rebuild owner

- build_runtime.py (TEMPLATE-C verification command 4 / l26-build-r2) was
  NOT run tonight: heavy twin build on the busy 16GB Mac, and concurrent
  agents are still landing edits (actual_scan.cpp 22:36; CS harness 22:21)
  — the l26_row_sync rule says in-flight edits drift candidate hashes
  until owners refresh them. My row is refreshed; others' rows must be
  re-checked at build time (build_runtime asserts every row against disk).
- Ready to rebuild: **YES** for this change (compiles clean on host+target
  front end, rows valid). No new imports/OS deps (Output::send/String::
  iequal/ToString all already consumed by this TU).
- Console acceptance signal once a build ships (authorized launch only):
  FindFirstOf("PlayerController"/"ItemDatabaseSubsystem") valid; expect
  ONE `[FFO] query-fallback [n:X]` per probed class if N1 is real in that
  pool state (that is the fix working); if `[FFO] add-fallback` fires for
  known classes instead, that is the GATE-1/pool-admission family, not N1.
  If NEITHER marker fires and probes still return nil, N1 is falsified →
  re-read CR §4 table (A2 wrapper layer next).

## Provenance

Inputs: agent-CR-object-reads.md (N1 rank-1, read table), agent-CW-
guobjectarray.md (§5/§6.1 fix shape), agent-CY-fix-templates.md +
fix-templates/candidate-C-name-compare/ (staged template, string-mode
variant), agent-CB-fname-machinery.md (Store 0x23c0b0 vs shard-Find
0x23c340 decode, Add-proven/Find-unproven). Consumption law verified in
build_runtime.py (rows key the l25-A-consumed original; candidate under
the l26 part; sha-pinned). CS walk harness left untouched; DA fixture
separate under /tmp/l26-walk-harness-da/.

— agent DA (ZCode), 2026-10-08.
