# REPORT.md — worker-real-input (rung 5 preparation)

2026-10-04, ZCode team. Scope: prepare acceptance-ladder rung 5 "visible spawn
through real input" — real pad input reaching Item Spawner's registered keybinds
through the EXISTING shared configurable bindings. No console contact; originals
immutable; vendored copies + proposals only. Tool calls used: ~45 (within bound).

## Files (all under outputs/2026-10-04-zcode-team/worker-real-input/)

- `CLAIM.json` — written first (role, scope, bounds).
- `INPUT-MAP.md` — the bridge design: default profile mapping (OPTIONS=F7,
  D-pad=arrows, CROSS=RETURN) for Item Spawner's 6 binds at main.lua:694-729; where
  edge detection lives (retained provider rising-edge + vendored hardening); the
  dispatch path pad → PS5PadSource::poll → Handler::process_event → Lua callback →
  mod's own ExecuteInGameThread → game-thread refusal gate; user-facing config
  surface (shared `ue4ss/PS5Pad.ini`, profile-driven, no per-mod recipes).
- `vendored/PadEdgeSource.{hpp,cpp}` — vendored adaptation of the retained provider
  (PS5PadSource, spike ue4ss-native-pad-provider-1) with the three rung-5 additions:
  per-key re-arm debounce (min_rearm_us, pad-timestamp domain), drop-not-queue of
  suppressed edges, backwards-timestamp re-baseline. VENDORED vs ADDED lines marked.
- `vendored/KeybindDispatchGate.{hpp,cpp}` — host-testable halves of the dispatch
  leg: KeybindDispatch (matching rules vendored verbatim from
  Handler::process_event, Handler.cpp:66-99) and GameThreadGate (refusal contract
  vendored from NativeThreadDetail::Identity: unknown identity / other thread refuse,
  nothing queued).
- `vendored/keydef_modifier_ops.cpp` — verbatim ModifierKeys operators from the
  retained KeyDef.cpp (its full TU needs fmt/String.hpp; only these are referenced).
- `vendored/host_test.cpp` + `vendored/build_and_test.sh` — 11 host tests, built with
  the retained provider + Handler sources read-only, ASan+UBSan.
- `vendored/build_and_test_receipt.txt`, `vendored/RECEIPT.sha256` — run receipt
  ("ALL PASS") + hashes of sources and binary.
- `vendored/PROPOSED-BACKEND-DIFF.md` — the one-call-site composition proposal at
  `ue4ss_backend.cpp:21-22` when rung-5 integration lands.
- `VISIBILITY-TEST.md` — rung-5 acceptance design (counts / does-not-count /
  9-step controller console procedure / evidence boundaries).

## Test results (host, ASan+UBSan, Apple clang++ -std=c++20): ALL PASS

1. single-press parity vs the RETAINED PS5PadSource as oracle (identical events).
2. debounce: 4 physical edges (bounce + 10 ms repeat + real 60 ms press) → adapter
   emits 2, drops exactly 2; retained emits all 4.
3. no-repeat while held (10 samples) → 1 event; cached duplicate read → no re-fire.
4. simultaneous CROSS+UP in one sample → exactly 2 events, once each.
5. chord L1+R1 requires both buttons; parity with retained.
6. unsubscribed / allow=false gating; no phantom edge on re-enable (parity).
7. disconnect clears state; reconnect baseline swallow (parity).
8. backwards timestamp: adapter drops the fabricated edge retained would emit.
9. KeybindDispatch: modifier equality exact, out-of-range key skipped,
   registration-inside-callback safe (snapshot-then-invoke), next-round fire.
10. GameThreadGate: unknown identity refuses (action never runs); game thread
    delivers; other thread refuses; second publication cannot steal identity.
11. rung-5 mini-chain: pre-identity press refused with zero menu toggles; the same
    press after identity publication delivers exactly one toggle.

Debugging note (honesty): two intermediate versions failed — the scripted backend
initially read an unpopulated sample global (all polls errored) and an unguarded
`adapter[0]` on an empty vector crashed. Both fixed; final run is clean under
sanitizers.

## Gaps / blockers (rung 5 cannot pass yet)

- **Game-thread delivery (A9) pending on console** — worker-3's EngineTick seam is
  the single hard gate already observed failing at main.lua:695
  (`ExecuteInGameThread` nil). This bridge preserves its refusal semantics but does
  not implement delivery.
- **Pad provider never ran on hardware** (`native_ps5_pad_provider`:
  hardware_runtime_validated=false). First real ScePad open/read happens during the
  VISIBILITY-TEST console attempt.
- **Downstream rungs**: live PlayerController/Pawn (C5), LineTraceSingle (C1),
  spawn callables + ownership (C2), and the UMG menu family (D1–D5) are all
  missing-new per worker-modreqs; rung 5 waits on their acceptance.
- **Integration not applied**: PadEdgeSource is host-validated only; the composition
  is the proposed diff in `vendored/PROPOSED-BACKEND-DIFF.md` (originals immutable).
- The toolchain note: the pinned LLVM 21.1.8 distribution has no C++ standard
  library; host builds used /usr/bin/clang++ with the macOS SDK (documented in
  build_and_test.sh). SDK is MacOSX26.2 on macOS 15 — binary ran cleanly, but for
  future reproducibility record host clang version in RECEIPT if reused.

## Verdict

Bridge design complete, host-validated against the actual provider API with the
retained provider as parity oracle; dispatch and refusal discipline preserved
verbatim. Rung 5 acceptance remains blocked on worker-3 delivery + C/D-family
capabilities per VISIBILITY-TEST.md §4.
