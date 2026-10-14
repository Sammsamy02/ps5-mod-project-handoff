# Variant C decision memo — call the engine's own AddUObjectDeleteListener

Agent AE, 2026-10-08. READ-ONLY analysis + review-ready patch draft. Nothing
outside `variant-c-proposal/` was modified; typescan-r4, CHANGES.json and build
scripts untouched.

- Patch draft: `variant-c-patch.diff` (this directory; cut against 4f4afddf)
- **Rebased patch (current): `variant-c-patch-rebased.diff` (this directory; cut
  against ea7221e8 — see section 8)**
- Binary receipts: `engine-listener-binary-receipts.txt` (this directory)
- Image analyzed: `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`,
  sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
  (same pinned image as the seed table; verified before analysis).

## 1. Feasibility verdict: YES — engine Add/Remove ARE present, addresses found

Claude's method ("find its address from the image the same way the other
bindings were found") works, with one caveat about which "way" applies:

- LoadMap / InitGameState / ProcessEvent are found by VTABLE WALKS
  (`OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` in UnrealInitializer.cpp:2181/2316 —
  find the CDO via reflection, read its vtable slot). That way does NOT apply:
  AddUObjectDeleteListener is a non-virtual member function.
- The applicable way is the SEED route (FName ctor, StaticConstructObject:
  offline derivation against the sha-pinned image + runtime label = module base
  + file-image offset). That is what this memo delivers.

How they were found (full receipts in `engine-listener-binary-receipts.txt`):

1. GUObjectArray file-image label 0x93BA110 (seed slot 0) is BSS; instead of
   data, code references were harvested: a full exec-segment scan for
   RIP-relative instructions targeting 0x93BA110 found 1,565 xrefs; 148 are
   `lea rdi, [GUObjectArray]` followed within a few instructions by a direct
   `call/jmp rel32` — i.e. `this = &GUObjectArray` member invocations.
2. Histogramming those call targets surfaced a tight cluster in the CoreUObject
   unit (0x480000-0x485000). Three functions access `this+0x78` as a lock;
   their bodies identified them beyond doubt:
   - **0x4841A0 = `FUObjectArray::AddUObjectDeleteListener`** — locks, appends
     arg2 to the TArray at this+0x68, grows through the engine allocator chain,
     unlocks. No Contains/dedup (upstream 5.6 shape `FScopeLock; Add(Listener)`).
   - **0x484240 = `FUObjectArray::RemoveUObjectDeleteListener`** — locks, linear
     Contains scan, `memmove` swap-with-last, Num--, optional shrink, unlocks.
   - **0x4843A0 = shutdown-notify** — under the same lock, iterates the 0x68
     array calling vtable slot 3 (`[vptr+0x18]` = OnUObjectArrayShutdown), then
     the 0x58 array likewise (create listeners).
   - the enclosing function of **0x484062** = the delete-listener
     `NotifyUObjectDeleted` dispatch (GC/object-destruction): iterates the 0x68
     array under the 0x78 lock calling vtable slot 2 (`[vptr+0x10]`).
3. Presence is LIVE, not vestigial: **14 engine call sites** invoke Add with
   `lea rdi,[GUObjectArray]; (lea/mov) rsi,<listener>; call 0x4841A0` —
   boot-time registrations by engine subsystems (one registers a STATIC global
   listener object), and 69+ sites call Remove (subsystem dtors). This is why
   the family survived dead-stripping, unlike the Z6-proven-stripped
   diagnostics family (`LogHashMemoryOverheadStatistics`).

**Addresses (this image only):**

| Function | file-image | runtime (base 0x400000) |
|---|---|---|
| `FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener*)` | 0x4841A0 | 0x8841A0 |
| `FUObjectArray::RemoveUObjectDeleteListener(FUObjectDeleteListener*)` | 0x484240 | 0x884240 |

Prologues (documented for any future byte-gate):
`Add: 55 48 89 E5 41 57 41 56 53 50 48 8D 5F 78 49 89`
`Remove: 55 48 89 E5 41 57 41 56 41 54 53 48 8D 5F 78 49`

## 2. CRITICAL by-product: the profile's listener offsets are WRONG (the AA answer)

The engine's own code fixes the FUObjectArray layout for this image:

| Member | Profile (live-facts JSON) | Engine binary (proven) |
|---|---|---|
| UObjectCreateListeners | 104 (0x68) | **0x58** {Data 0x58, Num 0x60, Max 0x64} |
| UObjectDeleteListeners | 120 (0x78) | **0x68** {Data 0x68, Num 0x70, Max 0x74} |
| UObjectDeleteListenersCritical | 0x88 (assumed 40B pthread_mutex) | **0x78** = {mutex@0x78, owner@0x80, recursion@0x88}, 24 bytes |
| PrimarySerialNumber | 176 (0xB0) | **0x90** (implied) |
| UEP_TotalSize | 184 (0xB8) | **~0x98** (implied) |

Proof: Add/Remove/Shutdown/Notify-dispatch all lock at this+0x78 BEFORE
touching the arrays at 0x58/0x68; declaration order in the vendored 5.6.1
header (create, delete, critical-section) then forces create=0x58, delete=0x68.
The profile assumed PS5 pthread_mutex_t = 40 bytes; this title's engine was
built with a 24-byte recursive pthread mutex (owner/count fast path inlined at
~7,100 engine sites). Four independent engine functions agree; there is no
consistent alternative layout.

Consequences (for agent AA / profile owner):
- **The port's hand-rolled registration has been treating the LOCK as the
  TArray**: "Data" = mutex word at 0x78, "Num" = low-32(owner) at 0x80,
  "Max" = high-32(owner) at 0x84. Its Contains/Append walked/stomped the live
  mutex+owner+count, and the first growth allocation was the Z11-crash GMalloc
  call. This subsumes and sharpens the mechanism-B lead: it was not merely an
  unlocked write to the right array — it was a write to the WRONG OBJECT.
- Variant C moots this offset for registration, but the profile MUST still be
  fixed (create 0x58, delete 0x68, crit 0x78, serial 0x90, total 0x98) because
  agent O's instrumentation headers and any future direct array reads consume
  the same offsets. Fix the profile FIRST; do not build variant A (#else path)
  against the current profile.
- If agent-AA's report contradicts this, the binary evidence here (four
  functions, receipts file) should be reconciled against AA's method before
  promotion; note AA's report was not present when this memo was written.

## 3. Signature, calling convention, and how we call it

- Declaration (vendored UObjectArray.h:1133/1140, impl non-vendored but now
  recovered from bytes): `void FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener*)`.
- PS5 x86-64 SysV member convention, verified at the 14 engine call sites:
  **rdi = this = &GUObjectArray (seed slot 0, runtime 0x97BA110), rsi =
  listener, returns void, no callee-saved surprises beyond rbx/r12-r15/rbp.**
- Port invocation: plain function pointer
  `using EngineListenerOp = void(*)(void* thisPtr, void* listener);` — no
  member-pointer machinery needed on this ABI.
- Address derivation in the patch follows the seed doctrine (no naked text
  reads — the igf6 crash class): `module_base = (uintptr_t)GUObjectArray -
  0x93BA110` (slot 0 is session-validated by SeedArming, including the
  whole-image code-cache identity check), then `fn = module_base + label`.
  Alignment/refusal checks + loud logs on every doubt; refusal is clean
  (init continues, `l25_dl_refused` code 7).

## 4. Lock + engine allocator — proven from bytes (Claude's two requirements)

- **Locked**: Add/Remove acquire the critical section at this+0x78 (PLT
  lock-primitive + `pthread_self` owner store + recursion count) before any
  array access, release at exit; the GC Notify-dispatch and shutdown-notify
  take the same lock. The mutex is RECURSIVE (owner/count fast path), so our
  `OnUObjectArrayShutdown` self-Remove re-entering the engine's Remove inside
  the engine's locked dispatch nest is legal.
- **Engine allocator**: growth path is `0x298A0 (quantize; elem|align<<8 =
  0x808) -> 0x62FC0 (ResizeAllocation) -> 0xBEF20 = engine FMemory::Realloc ->
  [engine GMalloc global at file-image 0x9340950] -> virtual [vtable+0x40]`.
  The engine's OWN GMalloc binding is used end-to-end; our port's GMalloc
  variable/repoint is never consulted on this path. Shrink (Remove) uses the
  parallel 0x27140 helper into the same chain.
- Bonus finding for the allocator-crash thread (Z11's sharpened suspect): the
  engine's true GMalloc pointer variable lives at file-image 0x9340950
  (runtime 0x9740950), and Realloc is vtable slot +0x40. If the port's GMalloc
  binding disagrees with that slot, the l24 in-allocator +0x10 fatal dispatch
  is exactly what a wrong-object virtual call would produce. Variant C removes
  the listener path's dependence on the port's GMalloc entirely.

## 5. Fallback if the functions had been stripped

Not needed — both are present with live engine callers. For the record, the
fallback stays Z11's recommendation: defer registration out of the init/save-load
window (queue on the R3 marshal's game-thread action list once the pump is
armed; registration then happens on the game thread post-pump, where the
engine's own subsystems register). That fallback also remains the right design
if a FUTURE title strips the family (no engine callers -> possible gc-sections
removal; re-derive per image, per the seed doctrine).

## 6. Risks and open review points

1. **Calling engine code from our init thread**: safe by the engine's own
   contract — the engine's Add/Remove are called from arbitrary engine threads
   under the recursive lock; growth runs on the engine allocator inside the
   lock. The residual risk is not the registration but what registration
   ENABLES: the engine will now dispatch slot 2 (`NotifyUObjectDeleted`) into
   our listeners on the game thread during GC/save-load. That requires the
   l25 slot-4 vtable fix (landed, Z8-verified) and correct handler behavior
   (unchanged since l24). This is the intended UE4SS design on other platforms.
2. **Semantic deltas vs the hand-rolled path** (documented in the patch):
   the engine Add has NO duplicate refusal (our callers register distinct
   objects once per process — acceptable); `l25_dl_add_done` /
   `l25_dl_remove_done` now carry elapsed-ms instead of post-Num (Num is
   engine-owned under the lock and deliberately not read); new event
   `l25_dl_engine_call` (0=Add, 1=Remove) for agent Y's harvest catalog.
3. **Pinning**: the two labels are pinned to image sha 3b6cfad5... exactly like
   the seed table; a different title/build requires re-derivation. The patch's
   resolver refuses cleanly if GUObjectArray is unbound or the derived base
   looks wrong. Optional hardening for review: verify the 16-byte prologues at
   seed-consume time via the existing admission `code_identity` machinery
   (UnrealInitializer TU, where text reads are already legal) — deliberately
   NOT done in UObjectArray.cpp to honor the no-naked-text-reads law.
4. **Rebase hazard — LIVE**: the parallel item-4 agent's edits landed in the
   candidate DURING this analysis (file sha moved 8f45e6ce... ->
   4f4afddf...; GMalloc-snapshot instrumentation + refusal code 6). This diff
   is cut against 4f4afddf and already re-based once (enum value 7 to avoid
   colliding with their code 6). If item-4 keeps editing, re-cut again; all
   hunks are additive and anchored on stable refusal/markers text.
5. **Variant composition**: variant B (`MODBRIDGE_L25_SKIP_DELETE_LISTENER`)
   guards the CALL SITES in UnrealInitializer.cpp/LuaMod.cpp; variant C
   (`MODBRIDGE_L25_ENGINE_ADD_LISTENER`) replaces the CALLEE. They compose:
   B+C = no calls at all (B wins); A+C = engine-side registration (the
   recommended production shape once the profile is fixed); A alone with the
   CURRENT profile is known-broken (section 2).
6. **Not done here (deliberately)**: promoting the labels into SEED-TABLE.json
   slots + SeedArming consumption (the architecturally-consistent long-term
   home, where code_identity prologue checks belong). The surgical TU-local
   resolver is the review draft; the seed-slot promotion is a one-file
   follow-up for Claude's decision.

## 7. Recommendation

1. Fix the FUObjectArray profile offsets first (section 2) — variant C moots
   the Append path but O's instrumentation and future reads use the same
   offsets. Reconcile with agent AA (report absent at writing time).
2. Build variant C as the third arm of the l25 A/B launch (or as the successor
   if the A/B pair already flew): expected outcome per the Z11/Z6 model —
   registration succeeds through the engine's own locked+allocator path, init
   proceeds past `:528`, the `[l25]` phase tags fire, and the save-load crash
   either disappears (family confirmed) or persists (bisect moves outside the
   family, exactly as the Z6 decision tree prescribes).
3. If C's `l25_dl_refused` code 7 ever fires on-console, capture the logged
   derived base before any retry; that is a seed/pin problem, not a code
   problem.

## 8. Current-state note (agent BC, 2026-10-08): rebased onto ea7221e8, ready for adoption

The section-6 risk 4 rebase hazard fired once more and is now CLOSED for this
cycle: after AE's cut (against `4f4afddf`), agent AT hoisted the refusal-5/6
growth screen above `Contains` (pre-read guard; file -> `69463958`) and agent AU
extended the GMalloc snapshot + both GMalloc log lines with the AB §7 vtable
slot reads (file -> `ea7221e8d7537e9b0dd36ddc0ac26a2c76050f343fdf7e8e2b8b03f297281f29`,
the current tree state).

- **`variant-c-patch-rebased.diff` is cut against ea7221e8** and is the adoption
  artifact. All 8 hunks, purely additive (142 added lines, 0 removed); added
  content is line-for-line identical to AE's original draft (only context
  windows shifted). `patch --dry-run --fuzz=0` applies cleanly to a copy of the
  current tree file; the applied result sha256 is
  `72d17fc8483d5426484ac3f7d5acebec7b27244922fca039212d6907c30f6782`.
- **Ready for adoption post-B** per Claude's plan: B+C still compose at the
  call sites (B defined => registration never called => C never reached);
  recommended production shape remains A-off + C-on (engine-side registration).
- **Verification already done (host)**: `/usr/bin/clang++ -fsyntax-only`
  (the `/tmp/l25-combined-smoke` recipe with `-D__PROSPERO__ -D__SCE__`) exits 0
  with 0 errors in all three states — current tree control, patched with the
  switch OFF (hand-rolled path active, AT guard + AU slot reads inside `#else`),
  and patched with `-DMODBRIDGE_L25_ENGINE_ADD_LISTENER` (engine-call path
  active; preprocessor proof that the resolver is in the TU only under the
  switch). Only the 2 known inherited Atomic.hpp -Wswitch warnings appear.
- Refusal codes coexist without collision: enum is contiguous 1-7 — code 6
  (AT, `ImplausibleDataForGrowth`, hand-rolled growth screen) and code 7 (AE,
  `EngineListenerCallUnavailable`, engine address unresolved/misaligned) are
  path-disjoint by construction (6 only reachable in `#else`, 7 only in the
  engine-call branch).
- **Address pinning**: the AA-corroborated labels Add `0x4841A0` / Remove
  `0x484240` (runtime `0x8841A0` / `0x884240` at base 0x400000) are pinned to
  image sha256 `3b6cfad4...` — the same sha as the seed table; a different
  title/build requires re-derivation per section 6 risk 3.
- **Status: variant C remains PROPOSAL-STAGE.** It is not in the twin build,
  not in typescan-r4, and not in CHANGES.json; nothing outside
  `variant-c-proposal/` was modified by AE or BC. Adoption (staging the
  candidate file + CHANGES.json rows + the third build arm) is Claude's call.
