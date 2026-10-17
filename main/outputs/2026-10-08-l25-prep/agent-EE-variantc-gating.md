# Agent EE — l26 integration: Variant C (engine AddUObjectDeleteListener) + Q/Z15 copyin gating

Date: 2026-10-08. Mandate: integrate the two Claude-review-3-approved l26
production items into the l26 tree (user authorized proceeding without further
review), per the review requirements: prologue-signature verification of both
engine targets at admission using cached disk signatures like the FName ctor /
SCO slots; FAIL CLOSED on mismatch; B skip path removed from production
(retained for the diagnostic variant); gating applied as a LABELLED change.
Lands AFTER the verified-query build's console verdict — nothing was built,
packaged, uploaded or run on console. Mac-local only. Report companion:
`outputs/2026-10-08-l26-runtime/NEXT-BUILD.md` (tree-state marker + next-build
procedure).

## 1. Integration status per item

### Item 1 — VARIANT C: LANDED (callee + admission + fail-closed chain)

- Rebase check first: the tree's
  `typescan-r4/candidate/src/UObjectArray.cpp` hashed `ea7221e8...` —
  BYTE-IDENTICAL to BC's rebase base, so no CP/DA-era rebase was needed
  (the tree movement was in other files). BC's
  `variant-c-patch-rebased.diff` applied with `/usr/bin/patch --fuzz=0`
  cleanly; intermediate sha `72d17fc8...` == BC's DECISION-MEMO §8 recorded
  applied-result sha (verified independently).
- EE additions on top (the review-3 fail-closed requirement):
  - NEW `typescan-r4/include/EngineListenerOpsProfile.hpp`
    (sha `39cbe0e83713c69295caed6353d0dc7f487d9ed7de23effa39ccc04a8ed0f673`):
    labels Add `0x4841A0` / Remove `0x484240` (runtime `0x8841A0`/`0x884240`
    at base 0x400000), GUObjectArray label `0x93BA110`, and the two 16-byte
    prologue signatures, pinned to image sha `3b6cfad4...` (the seed table's
    own image). EE INDEPENDENTLY RE-DERIVED the bytes from the pinned
    eboot before embedding: image exec PT_LOAD is off 0x4000 / vaddr 0, so
    file offset = label+0x4000; bytes at 0x4881A0 / 0x488240 are
    `55 48 89 e5 41 57 41 56 53 50 48 8d 5f 78 49 89` (Add) and
    `55 48 89 e5 41 57 41 56 41 54 53 48 8d 5f 78 49` (Remove) — matching
    AE's receipts and AA's corroboration. Constants only; no switch tokens.
  - `UnrealInitializer.cpp` (now
    `4b831a7a060dc7883ab51a52e78741d9d2536480c871f642ccd902539deebec0`):
    Phase 6 in `SeedArming::consume_required_slot_seeds`, under the C
    switch — for BOTH labels: `classify_seed_address` == CodeCallOnly, the
    shared admission-budget check, then the slot-4 pair
    `code_identity` + `cached_code_signature` against the cached 16-byte
    prefixes. ANY failure => `seed_refused` (terminal-for-launch throw,
    nothing bound): registration can never run, so the engine functions are
    NEVER called through unverified addresses. Success publishes the two
    VERIFIED runtime pointers via the new
    `UObjectArray::SetupEngineDeleteListenerOps` and logs
    `[Seeds][L26] engine delete-listener ops admitted: ...`.
  - `UObjectArray.cpp` (now
    `cbe50d15ad024932b297848360119a50b9357aadcec6cdd8b4576586a3dfb119`):
    the engine-call resolver was REWORKED to fail closed — it returns ONLY
    the admission-published pointers; unset => refusal code 8
    (`EngineListenerOpsUnverifiedByAdmission`), loud log, never calls;
    published != GUObjectArray-derived label address => refusal code 9
    (`EngineListenerOpsAddressMismatch`), loud log, never calls; codes 1/7
    semantics preserved. TU-local label constants replaced by the shared
    profile header (single source of truth). ALL l25 instrumentation kept
    (begin/done/pre/post/gmalloc snapshot markers, elapsed-ms engine-call
    reporting, mutex, `l25_dl_engine_call` 0/1 events).
  - `UObjectArray.hpp` (now
    `51e44f9a0c8d4bac0840ca5c528b47a5f18386377e31f8b562003b563fd0e4c9`):
    unguarded `SetupEngineDeleteListenerOps(void*, void*)` declaration
    (ODR-safe under any define mix — definition compiles in every build;
    no data members, no ABI change).
- Instrumentation audit: B-skip path retained verbatim for the diagnostic
  variant; switch matrix documented in CHANGES.json + NEXT-BUILD.md:

  | build | SKIP token | ENGINE_ADD token | path |
  |---|---|---|---|
  | production (l26-A) | undef | DEFINED | engine-call via admission-verified pointers |
  | diagnostic (l26-B) | DEFINED | any (B wins) | registration compiled out (UNSAFE diagnostic) |
  | hand-rolled (not built) | undef | undef | known-broken vs current profile; do not build pre-AA-fix |

### Item 2 — Q/Z15 GATING: LANDED as a LABELLED change

- `gating-patch.diff` applied to the CONSUMED bridge candidate
  (`copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp`, base
  f510c014) with `/usr/bin/patch --fuzz=0`, clean: result byte-identical to
  Z15's proof candidate `50caba3b...` (verified). Then the ONLY delta vs
  50caba3b: a 3-line `[Seeds][L26]` marker comment. Final sha
  `94f81a17d6904b9edfe8b06e8e4284268fbd4c738f96ac4475931225e719274c`,
  pinned in `copyin-probe/MANIFEST.json` (`l26_gating_note` + the new
  diagnostic tag entry) and given its own CHANGES entry in
  `gating-proposal/CHANGES.json` with the full composition chain
  (89e4b643 -> +Q liveness f510c014 -> +Z15 gating 50caba3b -> +marker
  94f81a17). Consumption: `l26_packaging_driver.py` wires exactly this file
  as `outer_bridge` (its pin assertions re-validated by --plan, 9/9 ok).
- Z15's own compile-proof harness re-run: "ALL CHECKS PASSED: gating
  property holds, release idempotent".

### Item 3/4 — drivers, CHANGES, NEXT-BUILD

- `build_variants.py` (now `549b54dd...`): variant A = production default
  C-ON (`-DMODBRIDGE_L25_ENGINE_ADD_LISTENER`, NO skip token); variant B
  diagnostic keeps the skip; `token_targets` parametrized per token (B:
  luamod+unreal-unrealinitializer; C: unreal-unrealinitializer+
  unreal-uobjectarray; conservative superset because the new profile header
  is a shared added header); pair G10 allowed-set = union; new G4-ctoken
  gate refuses to execute if the C switch is missing from the tree.
- `l26_packaging_driver.py` (now `68146c3f...`): variant-A note updated to
  the variant-C production text.
- `typescan-r4/CHANGES.json` (now `b9cd04ca...`): the three refreshed
  candidate hashes (UObjectArray.cpp ea7221e8->cbe50d15, hpp
  69a2391a->51e44f9a, UnrealInitializer.cpp cac95e48->4b831a7a), the new
  added header + hash, and a full `scope_l26_variant_c_engine_listener`
  section (details, switch matrix, refusal-code table 1-9, evidence).
- `NEXT-BUILD.md` written at the l26-runtime root: baseline 5ea2c845,
  everything the next build adds, proofs, and the exact next-build
  procedure (incl. the parallel-agent precondition, below).

## 2. Compile matrix (all rc=0, 0 errors)

- HOST `/usr/bin/clang++ -fsyntax-only` (BC's exact recipe, l26 vfs
  overlay routing headers at the port paths like the real build):
  UObjectArray.cpp — control (ea7221e8), C-off, C-on, B+C; ClassListener.cpp
  C-on (hpp consumer). 2 warnings each = the known inherited Atomic.hpp
  -Wswitch pair. `-H` proof: `Unreal/UObjectArray.hpp` + `NameTypes.hpp`
  resolve to the l26 candidates; `EngineListenerOpsProfile.hpp` consumed.
- PROSPERO real toolchain (`prospero-clang++`, env
  LLVM_CONFIG=native-probe/LLVM-21.1.8-macOS-ARM64): UObjectArray.cpp
  C-off / C-on / B+C; UnrealInitializer.cpp C-on / C-off (full TU, Phase 6
  in both states). All warnings inherited (Atomic.hpp, UnrealType.hpp
  enum-enum, asmjit deprecation) — none from this change. This is the first
  real-target compile of the variant-C family (BC's rebase was host-only).

## 3. Fixtures (host, plain + ASan/UBSan, all exit 0)

Harness `/tmp/ee-variantc-fixture` (Z12 doctrine; the production region
compiled byte-verbatim — extract of the landed UObjectArray.cpp lines
446..1029, sha `2156166568c8dcb0ee7cb0fa1953e9c09394f613ced926822a3d765a557c241c`):

- NEW engine-Add-path fixture (C-on), 22/22 checks x2 modes: a mock module
  image mmap'd at a page-aligned base with arm64 thunks at the REAL
  production labels and GUObjectArray at base+0x93BA110 (the shipping-image
  layout). Proves: admission publication event; engine Add/Remove called
  exactly once each with args (this == &GUObjectArray, listener);
  `l25_dl_engine_call` 0/1 + resolved/cross-check/returned logs; ZERO
  hand-rolled array writes; FAIL-CLOSED code 8 (unset pointers — mock NEVER
  called), code 9 (published+8 skew — mock NEVER called), code 1 (null
  GUObjectArray). ASan/UBSan clean.
- EXISTING listener harness regression (C-off), 16/16 checks x2 modes:
  fresh-alloc registration, duplicate refusal 4 (with growth NOT required —
  documents the AT guard order: growth screens run BEFORE Contains, so a
  duplicate at full capacity refuses 5/6 first, by design), insane header
  3, GMalloc-unbound growth 5, implausible-Data growth 6 (aligned but
  above the 2^47 canonical bound — the discriminator between the sane gate
  and the heap-plausibility screen), exception containment 2, Remove
  was_present 0/1, post-Append ANOMALY on a simulated concurrent writer,
  and two-thread Add serialization through the file-local mutex.
- Gating: Z15 compile-proof harness re-run PASS (see above); the applied
  gating delta vs 50caba3b is 3 comment lines only (diff-verified).

## 4. Driver validation

- `build_variants.py --plan` (run from the repo ROOT cwd — relative BASE
  pins require it): 64 gates. Only false gates, both explained:
  1. `G2-part-change-pins` on `UObjectGlobals.cpp` — a PARALLEL agent's
     in-flight edit (declared `4abf743b...`, actual `83e20b38...`, mtime
     22:11). NOT an EE file; its owning agent must refresh that CHANGES row
     before the next `--execute` (hard gate), exactly as the log warns.
  2. `G2q-copyin-probe-pin-unrealinitializer.base` — by design: Q's base
     pin is an expectation, not an invariant; the driver took the
     `patch-rebased-onto-revised-base` path and Q's overlay applied cleanly
     onto my revised UnrealInitializer (composed sha `23864f58...`,
     definition-before-use order gates pass, BOTH switch tokens present in
     the composed file).
  Key gates all true: G1 base pins (19044, no drift), G2r rows consumed,
  G2s, G2t listener units affected, G12b header consumers recompiled,
  G4-token + G4-ctoken present, G5 define units (B: luamod+
  unreal-unrealinitializer; C: unreal-unrealinitializer+unreal-uobjectarray).
- `l26_packaging_driver.py --plan`: 9/9 gates ok (frozen sources, l26
  profile candidates, gated-bridge pin flow).

## 5. Tree delta vs 5ea2c845 (summary; details in NEXT-BUILD.md)

Runtime source: `UObjectArray.cpp` ea7221e8 -> cbe50d15 (variant C +
fail-closed resolver), `UObjectArray.hpp` 69a2391a -> 51e44f9a (Setup
declaration), `UnrealInitializer.cpp` cac95e48 -> 4b831a7a (Phase-6
admission), NEW `EngineListenerOpsProfile.hpp` 39cbe0e8, gated bridge
f510c014 -> 94f81a17, CHANGES/MANIFEST/driver updates. Runtime behavior
delta: production registration goes through the engine's own locked,
engine-allocator functions gated on cached-prologue admission, and the
copyin owner gate closes at admission completion. Expected on-console
signatures and the refusal-code reading (8/9 = seed/pin problem, capture
addresses, never retry) are in NEXT-BUILD.md.

## 6. NOT done / boundaries

- No build_variants --execute, no packaging --prepare/--execute, no PRX, no
  upload, no console contact (lands after the console verdict per mandate).
- No promotion of the two labels into SEED-TABLE.json slots (DECISION-MEMO
  §6.6 remains a separate architect-level follow-up; the profile-header
  approach keeps the seed table format untouched).
- The AA profile-offset fix (create 0x58 / delete 0x68 / crit 0x78) is NOT
  landed by EE — the hand-rolled #else path stays not-buildable until that
  owner lands it; variant C moots it for registration but not for direct
  array reads (unchanged position from AE/BC memos).
- The UObjectGlobals.cpp CHANGES-row refresh belongs to that file's owner.
