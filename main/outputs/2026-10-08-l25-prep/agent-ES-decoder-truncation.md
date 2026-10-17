# agent-ES: pool-decoder ToString truncation investigation + fixes (2026-10-09)

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (consumption law; sources
read-only except my assigned candidates/tests/results).
Console evidence: `/tmp/dec1.log` (2026-10-09 15:26 session).

## 1. Verdict up front

**The pool decoder (`FNamePoolDecoder::decode`, candidate/src/NameTypes.cpp
:173-517) does NOT truncate.** It is all-or-nothing by construction (either
the full `units` chars are pushed or the call refuses with `{ok=false}` and
`ToStringInternalWrapper` falls to the legacy route), and the mandated
fixture now proves it for the exact shapes the console seemed to contradict
(long names, capacity/cursor boundaries, units-vs-bytes canaries): 8/8 suite
runs pass in plain and ASan/UBSan modes.

**The truncated names in the console log are the KNOWN halved-widen render.**
Every one of the 16 refusal `object=` values — and the `reason=` value
itself ("slot read" rendered "slot") — is EXACTLY HALF its true length,
which is the documented signature of the PS5 SDK's broken short-wchar
`std::wstring_convert` widen (`ensure_str(<narrow>)`): the tree already
documents this codec in
candidate/src/UnrealInitializer.cpp:1025-1041 ("every wstring_convert call
consumes/produces every OTHER character ... 'outcome=verified-' for
'verified-and-bound'") with the established narrow-first workaround
(`send_narrow_diag`). The type-scan refusal print
(candidate/src/TypeChecker.cpp:962-969) was still passing
`ensure_str(reason_text)` + `ensure_str(object_name)` through that codec —
fixed (see 4b).

**The actual scan breaker is NOT ToString.** All 19 scan-object lookups
succeeded through the decoder-backed `GetFullName` (`StaticFindObject_
InternalSlow` compares full names with size-checked `String::iequal`; the
CDO/"Already constructed" lines and 16 found objects in dec1.log prove full,
correct names at lookup time). What refused is `ReadClassChecked` with
`reason=slot class=0x0` = `property_readable(field)`/`read_member` failing
in the guard's installed read backend (`PropertyReadBytes`: admission
coverage + `bridge_copyin_range_faulted` + memcpy,
include/NativePropertyReadBackend.inl) for EVERY scanned field — i.e. the
heap FProperty reads were refused before any class/name logic ran. That is
a guard/backend admission regression, separate from this assignment; the
evidence chain is in 3 below for the guard owner.

## 2. The exact cause of the "truncation" (units/bytes class, render-side)

Raw-bytes measurement of dec1.log lines 626-641 (UTF-16 runs decoded; odd
lengths show the dangling low byte of the cut unit, e.g. line 627's
`E\0n` = 9.5 units):

| # | rendered object          | units | true literal (scan call)                     | len | len/2 |
|---|--------------------------|-------|----------------------------------------------|-----|-------|
| 1 | `/Script/En`             | 10    | `/Script/Engine.Actor`                       | 20  | 10    |
| 2 | `/Script/En`+dangling n  | 9.5   | `/Script/Engine.Pawn`                        | 19  | 9.5   |
| 3 | `/Script/Engi`           | 12    | `/Script/Engine.NetDriver`                   | 24  | 12    |
| 4 | `/Script/Engine`         | 14    | `/Script/Engine.NetConnection`               | 28  | 14    |
| 5 | `/Script/Engine`(+e)     | 13.5  | `/Script/Engine.GameModeBase`                | 27  | 13.5  |
| 6 | `/Script/Landscape.`     | 18    | `/Script/Landscape.LandscapeComponent`       | 36  | 18    |
| 7 | `/Script/VariantManagerCon` | 25 | `/Script/VariantManagerContent.VariantObjectBinding` | 49 | 24.5+dangling |
| 8 | `/Script/Engine.I`       | 16    | `/Script/Engine.IntSerialization`            | 32  | 16    |
| 9 | `/Script/Engine.`(+?)    | 14.5  | `/Script/Engine.AudioComponent`              | 29  | 14.5  |
|10 | `/Script/MovieScene.Mov` | 22    | `/Script/MovieScene.MovieSceneSequencePlayer` | 44 | 22    |
|11 | `/Script/U`              | 9     | `/Script/UMG.Widget`                         | 18  | 9     |
|12 | `/Script/Engine.`        | 14.5  | `/Script/Engine.LevelStreaming`              | 29  | 14.5  |
|13 | `/Script/Engine.Pri`     | 17.5  | `/Script/Engine.PrimaryAssetTypeInfo`        | 35  | 17.5  |
|14 | `/Script/MovieSceneTrac` | 21.5  | `/Script/MovieSceneTracks.MovieSceneEventPtrs`| 43 | 21.5  |
|15 | `/Script/Engine.WorldPa` | 22    | `/Script/Engine.WorldPartitionRuntimeCellData`| 44 | 22    |
|16 | `/Script/CoreUOb`        | 14.5  | `/Script/CoreUObject.VerseEnum`              | 29  | 14.5  |

16/16 exact halves. Same line: `reason=s\0l\0o\0t\0` = "slot read" (9) halved
to 4.5. A decoder truncation cannot produce this table (it would have to
know each literal); a per-string halving widen does, mechanically. Not a
buffer (record `object_name` is char[64], max name 49), not a race (the
values are deterministic), not decode() (all-or-nothing; ByteProperty probe
verified; all lookups matched full names).

Note the truncated names are clean PREFIXES of the correct names — which
initially read as "decode returns partial output". The half-length law is
what disambiguates: a prefix from `units` miscount would track the header
len field, not the literal length of the caller's string.

## 3. Evidence chain for the scan failure (reason=slot class=0x0)

- dec1.log 15:26:46: `[FN] pool-decoder verified (ByteProperty)`;
  `Already constructed: ["/Script/Engine", ...]` (full names);
  Actor/GameMode/GameViewportClient CDOs FOUND (full-name comparisons
  through decoder-backed GetFullName); `[Seeds][L25] copyin cleanup
  completed after Phase 6` (the EP sequencing fix IS in this build);
  `used-vq=1 used-copyin=0`.
- dec1.log 15:26:47-48: every scanned object's FIRST field read refuses as
  SlotRead (class=0x0, klass never read), 16 objects, all core types
  missing, `core-ready=false`. `NativePropertyGuard::Install` ran
  (UnrealInitializer.cpp:1931), so `read()` reached `PropertyReadBytes` and
  the backend refused the heap addresses (admission coverage / fault-ledger
  verdict) — before `known_property_class`, before any ToString. The
  previous (pre-decoder) build's scan succeeded through the same guard, so
  something in the l26 admission/backend sequencing changed the verdict for
  heap FProperty spans. This is the component that actually broke the scan
  and needs its own assignment (guard owner); my assignment's fixes do not
  touch it, and my fixture proves the decoder is not a co-conspirator.

## 4. Fixes applied (my assigned surface)

### a) candidate/src/NameTypes.cpp — `FName::GetPlainNameString` (real units-vs-bytes bug on the ToString surface)

Upstream line: `memcpy(OutName, &String[0], String.size() * sizeof(File::
StringType::size_type))` — multiplies the TCHAR COUNT by `sizeof(size_t)`
== 8: a 4x over-read of the u16 buffer (UB for every name) and an overflow
of the 1024-TCHAR (2048-byte) OutName for any name >= 257 units (a 500-unit
name writes 4000 bytes; 1023 units writes 8184). Fixed to
`min(size, NAME_SIZE-1) units * sizeof(TCHAR)`, always NUL-terminated,
return = copied units. No in-tree caller exists (verified by grep across
candidate + port sources), so the tightened return contract disturbs
nothing. This was the one genuine units/bytes defect in the ToString
surface; the ER-region manifest pins that the fix is OUTSIDE the verbatim
decoder region (`assert 'GetPlainNameString' not in er_inc`).

### b) candidate/src/TypeChecker.cpp — refusal print narrow-first

The refusal print passed `ensure_str(reason_text(r.reason))` and
`ensure_str(r.object_name)` as wide args — the exact halved-widen victims.
Now formats the line NARROW first (fmt::format; reason/object are narrow
already; addresses numeric), widens ONCE through the locale-free
`RC::Unicode::DecodeUtf8` (Helpers/Utf16Utf8.hpp include added), and falls
back to the previous wide line on any failure — the tree's own
`send_narrow_diag` doctrine (agent-EM 2026-10-09). Next console run will
render `reason=slot read object=/Script/Engine.Actor ...` in full, killing
the "truncation" illusion at the source.

### c) tests/actual_er_pooldecoder.cpp — new `truncation` suite (+ run.py wired)

See 5.

## 5. Fixture proof (long names, boundaries, OOB exactness)

`tests/actual_er_pooldecoder.cpp` `truncation` suite — the VERBATIM
production decoder region (marker-extracted, sha-pinned,
results/er-pooldecoder-verify/er-region-manifest.json) against a
byte-layout mmap pool with a 0x20000-capacity closed block and a live
cursor. Host sandbox denies mprotect/MAP_FIXED, so the boundary cases are
behavioral proofs; the read-before-refute ordering is already proven by the
happy suite's PROT_NONE block-pointer cases.

- Long names, production routing (`try_tostring`): 20/50/100/500/1023
  units, ANSI AND wide (wide injects U+00E9/U+4E2D so byte/unit mixups
  cannot pass equality), plus 500-wide + `_4242` number suffix — every one
  decodes with EXACT size and content (anti-truncation AND anti-halving
  canary). Successful decodes fire ZERO `[FN] pool-decoder OOB` markers and
  zero anomaly notices.
- Closed-block capacity boundary: 500-unit WIDE (needed 1002) and 100-unit
  ANSI (needed 102) entries ending EXACTLY at kBlockCapacityBytes decode
  completely; an entry needing 2 bytes past capacity refuses oob; a forged
  one-unit-longer wide header at an exact fit refuses oob — this is the
  units-vs-bytes canary (10 wide units = 20 bytes, not 10).
- Open-block live-cursor boundary: entry ending EXACTLY at
  CurrentByteOffset decodes; id exactly AT the cursor refuses (no header
  room); payload crossing the cursor refuses.
- OOB-marker exactness: the four genuinely-OOB ids each fire exactly ONE
  marker through production routing; a repeat fires none (per-id dedupe).
- Console-observed widths 18/27/29/35/36/43/44/49 decode at FULL length —
  pinning that the halving observed on console is render-side forever.

Results: `results/er-pooldecoder-verify/HOST-TESTS.json` — happy/refuse/
race/truncation x plain/asan-ubsan = 8/8 SUITE-PASS.

## 6. Compile + harness receipts

- Host: ER suites compile+run (clang++, plain + ASan/UBSan) 8/8 pass;
  DA harness rerun (results/er-pooldecoder-verify/da-rerun/) 10/10
  scenarios pass — DA1-DA6, ER1/ER2/ER3 — against the post-fix candidate
  (regions re-extracted, sha-pinned).
- Prospero: `results/ES-TARGET-COMPILE.json` — unreal-nametypes and
  unreal-flat-typechecker compile exit 0 with the real prospero-clang++ argv
  (composition receipt units) + a VFS overlay carrying this tree's replaced
  headers (results/headers-es.vfs.json, 28 roots; a bare -I replay
  double-includes port vs candidate NameTypes.hpp through quoted includes —
  the overlay is the honest equivalence of the parent build's vfs).
- CHANGES.json: candidate_sha256 resynced for candidate/src/NameTypes.cpp
  (e1ace331ccb3.. -> 21ab06e6623e..) and candidate/src/TypeChecker.cpp
  (c05b06a7db9a -> c59e3024cda4) in both `files` and `changes` rows (4
  rows).

## 7. Honest limits

- No console contact, no upload, no launch: my fixes are host+prospero
  compile-verified and host-fixture-verified only; the next console run
  must confirm (a) refusal lines render full-length, (b) the guard read
  backend's SlotRead behavior (section 3) — which is NOT fixed here and is
  the component that actually broke the scan on console.
- The scan's `reason=slot` forensics are evidence-chain only; I did not
  modify the guard, backend, or admission sequencing (out of assigned
  scope).
