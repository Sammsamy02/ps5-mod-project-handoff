# REVIEW-VERDICT — cap #7 step 2 (runtime pak mount source stage)

Independent reviewer: agent JA, 2026-10-13. Mac-only: no console contact, no
target build, no game/mod file changes. Every claim cites file:line in the
stage (`caps-team-work/cap07-step2/`, below abbreviated) or the pinned local
sources. This review supersedes the interrupted review's provisional findings;
`HISTORICAL-REVIEW.md` (Team A baseline, BLOCK, fixes B1–B3/F1–F7) was used as
input, not as a verdict.

## VERDICT: CONDITIONAL PASS

The staged component is generic, data-only, fail-closed, and its engine-parity
claims check out against the pinned official source and saved machine
evidence. The interrupted review's three holds are resolved as follows: the
kMaxPins layout hold was REAL and is now FIXED in-stage (D1 below); the four
missing regression areas are now covered by 43 new assertions; the aggregate
ABI gap is assessed NOT a blocker (evidence-based, conditions below). One new
defect (D2, snapshot delegate under-verification) was found and FIXED. PASS
becomes unconditional for integration work once the conditions in section 5
are met by the integrator; nothing further is required from this source stage
except the two carry-overs marked [INTEGRATOR].

## 1. Source quality assessment

### 1.1 Core unit (PATCH/RuntimePakMount.cpp / .hpp) — sound

- Admission is total before any call: profile completeness
  (`RuntimePakMount.cpp:7-16` — all pin fields, exact ABI constants
  `slot_delegate_execute==0x60`, `slot_get_lower_level==0x68`,
  `pak_count_offset==0x18`, lowercase-hex sha, address extents vs the pinned
  image size), request shape (`:18-26`), game thread (`:61`), live base +
  image identity (`:63`), and all six code prefixes through the bridge
  (`:64`). No engine call precedes these.
- The call target is fully resolved from verified pins, not trusted input:
  delegate instance/size (`:68-69`), instance vtable == pinned
  (`:70`), Execute slot == pinned (`:70`), receiver == the census-found pak
  layer, member == pinned `handle_mount_pak`, adjustor == 0 (`:70`). This
  closes historical F1.
- Chain census (`:44-56`) descends only through layers whose vtable slot
  `+0x68` equals the pinned GetLowerLevel body (0xb3500) before reading
  `[layer+8]`, with overflow guards (`:50`) and cycle/depth bounds (`:45-47`).
  This closes historical F2; unresolvable wrappers refuse (fail-closed).
- Order derivation (`choose_order`, `:27-43`) is faithful to the pinned
  engine source, which I re-read for this review:
  `official-source/.../IPlatformFilePak.cpp:5952-5974` — bonus applies only
  for names ending `_P.pak` (engine `EndsWith` default is case-insensitive;
  the core's lowercased tail at `:32` matches), version defaults to 1, is
  `n+1` only when the underscore-delimited part `IsNumeric()` and
  `Atoi>=1`. The core's model matches in every branch I could construct,
  including the production-shaped mixed part `mod_2K_P.pak` (engine
  `IsNumeric("2K")==false` → version 1 → bonus 100; core `:35-37` same), the
  empty part `a__P.pak`, `mod_0_P.pak`, and case variants. The core refuses
  `+ - .` spellings and numeric overflow (`:36-37`) that the engine would
  Atoi — a strictly-more-conservative divergence, documented, and covered by
  tests. `highest+1` is passed explicitly (never -1), so the engine's
  `PakOrder == INDEX_NONE` path (`IPlatformFilePak.cpp:6409-6412`) is never
  entered; the bonus is added by engine `Mount` (`:5953-5974`) on top of the
  passed order, which is exactly what `effective` models.
- Post-call epistemics are honest: after `call_made=true` (`:80`) every
  outcome other than full proof is `MountedUnverified` (`:81-88`); `Mounted`
  requires non-null return + exactly-one identity hit + exact order match +
  strict precedence over every other entry + count growth (`:88`). FString is
  freed on all exits via the owner (`:79`). The log line carries raw
  ret/before/after/order/verdict (`:89`).

### 1.2 Producer (PATCH/PakRequestProducer.hpp) — sound

All-or-nothing bounded census: normalized absolute root (`:10`), symlink
refusal for root and every path component (`:12`), 128-entry census cap
(`:16`), regular-file + size + name-length checks (`:20`), IoStore sidecar
pairing (`:21-24`), 16-container selection cap (`:25`), deterministic sort
(`:28`). Zero `.pak` selection returns empty and the adapter refuses
(`NativePakMountAdapter.inl:91`). No container is ever read or rewritten.

### 1.3 Native adapter (PATCH/NativePakMountAdapter.inl) — sound after D2 fix

- Every engine touch goes through admitted doors: `module_view_base`,
  `bridge_code_cache_context/_signature`, `PropertyReadBytes` bounded reads,
  `IsInGameThreadRaw` (`:13-24`). The chain head is produced only through the
  admitted P3/P4 pins (`:25-32`). FString via the real
  `LuaFNameBridge::Bridge(const char*, size_t)` + `to_fstring()` — I verified
  both exist in the current tree
  (`typescan-r4/include/LuaFNameBridge.hpp:407,533`), closing the historical
  F5 FString half for this component.
- Snapshot: aggregate-return thunk invoked with the captured SysV sret
  contract, output allocation freed through `(*GMalloc)->Free` on every exit
  including validation refusal and a thrown body (`:63-65`), then bounded row
  reads with identity/order projection only (`:69-72`); count/capacity/size
  guards at `:67-68`. No pak pointer is ever dereferenced.
- Trigger: once per process (`:80-81`), game-thread only (`:79`), derives the
  pakmods root solely from the existing `setup_mod_directory_path` contract
  (`:84-87`), missing directory = disabled, empty selection refuses
  (`:89-91`), stops at the first non-Mounted result (`:96`), and never claims
  vanilla after side effects (`:98-99`).

### 1.4 Integration patch (PATCH/current-source.patch, evidence/stage-recipe.json)

All five anchors verified against the live originals this review (all four
source SHA pins still match; `UnrealInitializer.cpp` insertion point at
original line ~3222 is inside `namespace RC::Unreal` (opened at original
`:3184`), so the `RC::Unreal::RunNativePakMountOnce` definition matches the
`NativePakMountEntry.hpp:3` declaration, and the unqualified call from
LuaMod.cpp resolves. The `.inl` lands inside SeedArming after
NativePropertyReadBackend.inl per its own header contract
(`NativePakMountAdapter.inl:1-2`). `pak_mount_pins.inc` includes `<cstdio>`
(`:3`), closing historical N3.

## 2. Test coverage assessment

Re-run by this reviewer (`tests/JA-REVIEW-GREEN.log`): gate 7 files/414 lines
PASS; 23 legacy + 11 + **43 new** regression + 13 adapter-fixture C++
assertions; 4 producer + 4 stager + 2 source-contract Python checks; runner
exit-nonzero re-proven on a seeded compiler failure. The 44→90 host
assertions are meaningful, not vacuous:

- The mock now fails closed on real defect classes: signature bytes AND
  address AND length (`mock_engine.hpp:88-92`, closing historical F6),
  instance/receiver/member/adjustor wiring (`:51`, `RuntimePakMount.cpp:70`),
  and synchronized-snapshot semantics for precedence (`:115-121`).
- The four coverage holes named by the interrupted review are now covered in
  `tests/regressions2.cpp`: signed boundaries (`:17-44`), engine-IsNumeric
  parity + numbered/overflow bonuses (`:46-77`), failed/degenerate post-call
  snapshots (`:78-125`), wrapper descent/refusal/cycle/unreadable-lower
  (`:127-176`). Mutation evidence: a scratch core copy with the slot-identity
  check and the parse-overflow guard removed fails 2/43 and exits 1
  (`tests/REGRESSIONS2-RED.log`). The parse-overflow mutation is masked by
  the outer effective-order overflow guard — defense in depth, not a hole.
- Stager inverse/anchor/drift properties (`tests/test_stage.py`) and the
  bounded binary replay remain separate, honest steps (APPLY.md:13).

Residual honesty note: these are host-fixture proofs of the pure core and the
adapter's fixture-visible behavior. They are not engine ABI, lock, allocator
or console evidence, and are not counted as such anywhere in the stage.

## 3. Defects found by this review

**D1 (real, FIXED) — kMaxPins layout hold: the stager updated only one of the
files that gate the shared layout.**
`CodeImageCache.hpp:28` embeds `Entry entries[kMaxPins]` in `Metadata`; the
patch grows kMaxPins 8→16 (`current-source.patch` CodeImageCache hunk). The
interrupted review was right that "the stager updates only the profile copy"
was insufficient — but the concrete gap is not another header copy to edit:
it is the PIN FILE. Consumer census (all verified this review):
1. `outputs/2026-10-08-l26-runtime/profile/units/CodeImageCache.hpp` — the
   promoted candidate every runtime TU resolves (prepended
   `profile/units` include from `profile/CHANGES.json`
   `include_directories`, plus the vfs alias). Was in the recipe already.
2. `outputs/2026-10-08-l26-runtime/profile/CHANGES.json` — pins that
   candidate's sha256 and is consumed by BOTH compilers:
   `build_variants.py:285-296` (runtime G2: `sha(candidate)==candidate_sha256`
   hard assert) and `l26_packaging_driver.py:88-134` (packaging: lineage
   overlay `assert sha(c['candidate'])==c['candidate_sha256']`, destination
   `lineage/CodeImageCache.hpp` per `:129`). Applying the kMaxPins=16
   candidate without refreshing these pins makes every build refuse (or, if
   someone sets tolerate_drift, silently mis-verify). WAS MISSING → now the
   recipe's fifth exact file (`evidence/stage-recipe.json`, derived sha
   97d1085564687609ab38cb7078cbb8110003af8bbc3c1534ed87a7fa3ce606cc), and
   `tools/stage_current.py:audit_code_image_cache` records the census in
   STAGE.json.
3. `outputs/2026-10-07-l19-local-profile-r4/units/CodeImageCache.hpp` — the
   immutable ORIGINAL revision the vfs alias shadows (pinned
   `original_sha256` b9006941… in CHANGES.json). Must NOT be edited (it is a
   different, older revision; hand-editing would create a third divergent
   copy). The stager now verifies it still matches its pin each run.
4. `lineage/CodeImageCache.hpp` inside package trees (98 CodeImageCache
   copies exist under `what/outputs/`; the live ones are the l26 package
   lineage roots, currently kMaxPins=8) — regenerated from (1)+(2) by the
   next package build; historical copies are stale and must not be reused
   [INTEGRATOR: do not link or republish pre-existing packages].
The fixed stager was executed: `derived/ja-final` (12 derived files,
5 exact sources/anchors/inverses, consumer audit OK,
`tests/STAGE-JA-FINAL.log`). The BASE `SOURCE-SELECTION.json` pins only
originals (l19/r5-thread, b9006941…/5737e263…), which stay unchanged — no
other pin file references the old candidate hashes (grep across
l26-runtime + l25-build-variants found only profile/CHANGES.json).

**D2 (real, FIXED) — snapshot delegate met a weaker identity standard than
the mount delegate.** The mount path requires receiver + pinned member +
zero adjustor (`RuntimePakMount.cpp:70`); the snapshot path checked only
receiver only (pre-fix `NativePakMountAdapter.inl:55`). A re-bound or
malformed snapshot delegate could pass identity and drive an indirect call
with the sret contract. Fixed: the snapshot now also requires a non-null
member target at `[inst+0x20]` and adjustor 0 at `[inst+0x28]`
(`NativePakMountAdapter.inl:54-58`), with the overflow guard raised to
`inst>UINT64_MAX-0x30`. The exact snapshot member address is not derivable
Mac-only (it is delegate-instance data, not code); it remains covered by the
image-sha gate + Execute pin + fail-closed row validation, and is listed as
a target-side confirmation in section 5. Fixture extended to 13 assertions
including adjustor≠0 and null-member refusals (`tests/native_adapter.cpp`).

**Confirmed clean (no defect found):** engine-parity of the bonus model
(verified against `IPlatformFilePak.cpp:5952-5974`), the `+0x18` count
offset (B2 fix verified in `EnginePins::valid`, `RuntimePakMount.cpp:8`),
runner failure propagation (B3 re-proven), no-detour/no-name law (gate + my
own read of every PATCH file), originals untouched (all recipe pins
re-verified this review), and the LuaMod/UnrealInitializer namespace
resolution.

## 4. The aggregate ABI question — NOT a blocker

The genuine `FMountedPakInfo` declaration is not retained locally and the
stage does not compile the engine type. Assessment: acceptable, because the
stage never depends on that declaration:
- The sret contract is machine-evidenced, not assumed:
  `evidence/snapshot-windows.json` disassembly of the Execute thunk
  (0x17831e0: `mov rsi,[rsi+0x18]; mov rbx,rdi; call 0x1783250; mov rax,rbx`)
  is a textbook hidden-output-pointer thunk, corroborating RDI=out16,
  receiver at instance+0x18, RAX=out. The adapter models exactly this
  (`NativePakMountAdapter.inl:61-66`), with `static_assert(sizeof(Result)==16)` at `:62`.
- The 16-byte row layout {u64 identity@0, u32 order@8} is evidenced by the
  captured body's row stores (`0x1783373`/`0x1783377` in
  `snapshot-windows.json`), replayed against the full saved image by the
  bounded verifier (`evidence/saved-window-pass.json`, 10 windows/8235
  selected bytes, full-image sha match).
- The consumer is fail-closed by construction: rows are read with bounded
  `PropertyReadBytes`, validated (identity non-zero, count/capacity bounds,
  `NativePakMountAdapter.inl:67-72`), and a wrong row model cannot produce a
  false PASS because `Mounted` requires the engine-reported order of the
  returned identity to equal the locally computed effective order
  (`RuntimePakMount.cpp:87-88`). A wrong model degrades to
  `MountedUnverified`.
- Honest labeling is already in place (NOTES.md:7, SOURCE-RECEIPT limits).
Residual risk kept open, correctly, as a target-side condition (section 5):
the row stride/field order and the real delegate binding are confirmed only
at runtime; compilation of the genuine engine type remains unnecessary for
this design.

## 5. Integration prerequisites (exact conditions)

1. [INTEGRATOR] Compose `derived/ja-final` (or a fresh run of the fixed
   stager) with the other approved components; never overwrite cumulative
   files with full-file results. Apply all five transformed files together —
   including `profile/CHANGES.json` — or G2/packaging will (correctly) refuse.
2. [INTEGRATOR] Whole-selected-build gates (no-detour, fs-throw, nonempty
   counts), exact PRX build verification, and RuntimePakMount.cpp present in
   the actual runtime source selection.
3. [INTEGRATOR] Do not reuse any historical package/lineage artifacts built
   against kMaxPins=8; the lineage CodeImageCache must be regenerated by the
   packaging driver from the new candidate.
4. [INTEGRATOR] Target-time confirmations (cannot be proven Mac-only):
   genuine external-root reachability and `pakmods` visibility; delegate
   lifetime/no concurrent rebinding between resolution and call; engine
   allocator lifetime; snapshot row order/stride and the snapshot member
   address at `[inst+0x20]` (a one-time readback against the saved windows
   suffices); engine fatal paths are outside the C++ exception contract
   (LAUNCH-CARD.md:16 already says so).
5. [INTEGRATOR] Pristine game baseline per LAUNCH-CARD.md step 1 (repair +
   step-1 install state must be restored and hash-verified; no mod trio under
   game Paks), mod files unchanged in `pakmods`, one observed launch, rollback
   R11-equivalent recorded for the NEW build.
6. Precedence proof is a synchronized-snapshot statement only; later mounts
   can change it (LAUNCH-CARD.md:12). Shader checkerboard remains #8/#12
   (LAUNCH-CARD.md:14).

## 6. What this review changed (all inside cap07-step2/)

- `tools/stage_current.py` — five-file expectation + CodeImageCache consumer
  audit recorded in STAGE.json.
- `evidence/stage-recipe.json` — added exact profile/CHANGES.json row (both
  hunks, derived sha).
- `PATCH/NativePakMountAdapter.inl` — snapshot bound-member/adjustor identity
  (D2).
- `PATCH/current-source.patch`, `PATCH/APPLY.md`, `NOTES.md`, `REVIEW.md`,
  `SOURCE-RECEIPT.json` — synchronized with the above; derived tree
  `derived/ja-final` staged and verified.
- `tests/regressions2.cpp` (new, 43 assertions), `tests/native_adapter.cpp`
  (10→13), `tests/run.sh` (runs regressions2); new logs
  `tests/JA-REVIEW-GREEN.log`, `tests/STAGE-JA-FINAL.log`,
  `tests/REGRESSIONS2-RED.log`.

No original file, mod file, console action, target build or network access
was performed. `derived/current` and `derived/final` (older team trees) are
superseded by `derived/ja-final` for integration purposes.
