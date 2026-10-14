# REPORT.md — worker-real-input-pad (rung 5: PS5 pad delivery preparation)

2026-10-04, ZCode team. Scope: prepare the PS5 pad input delivery for the Item
Spawner keybind path — read the retained provider's actual API + composed-runtime
integration, design the FIRST hardware test, write the pad-to-keybind mapping
config, and prepare the standalone probe module (host-tested; PRX build route
documented). No console contact; no network; originals immutable. Tool calls
used: ~26 of 60.

## Answers to the task (all sourced from actual retained code)

1. **API + integration** (`PAD-API-AND-INTEGRATION.md`): `PS5PadSource::poll` is
   called once per program tick via `Handler::process_event()` →
   `m_platform_handler->process_event(this)` (UE4SSProgram.cpp:1660 →
   Handler.cpp:75 → PS5PadSource.cpp:65-68), forwarding the handler's
   subscribed-keys bitmap and `allow_input` flag. Edge detection lives INSIDE
   `poll` (PS5PadSource.cpp:69-94): rising-edge `down && !m_down[i]`,
   cached-timestamp guard, post-attach baseline swallow, held-state tracking
   while disabled, disconnect clearing. The dispatch gate reaches Lua via exact
   key+modifier match against `m_key_set` (Handler.cpp:80-95), snapshot-then-
   invoke outside the mutex (:97-102) — callbacks registered by
   `RegisterKeyBind` through UE4SSProgram.cpp:2885 fire on the processing
   thread; the mod's own `ExecuteInGameThread` wrapper is the only route into
   the game thread (worker-3 seam, still blocked — preserved, not touched).
   Composed-runtime wiring: `ue4ss_backend.cpp:26` calls
   `ConfigureRetainedPS5PadInput` (NativePadInput.cpp) during backend
   construction — reads `ue4ss/PS5Pad.ini`, parses with the retained config
   parser, opens the real provider eagerly, and THROWS on failure, so a bad
   pad blocks the whole activated-runtime start. That is why the first
   hardware contact is the standalone probe, not the composed runtime.
2. **First hardware test** (`pad_probe/` + `HARDWARE-TEST-PLAN.md`): a
   standalone module registering exactly one keybind (F7) through the REAL
   retained `RC::Input::Handler`, polling through the REAL retained
   `PS5PadSource`, and writing JSONL records (`probe_start` / `key_fired` /
   `phase_change` / `probe_end` / `probe_failed`) to an injected sink —
   console wires a `/data/pad_probe/events.jsonl` append sink and the real
   `scePad*` factory; host tests wire a scripted `PadBackend` (the provider's
   explicit owned-test seam). Console ordering constraint found and handled:
   `__PROSPERO__` defaults `Handler::m_source_injected_mode=true`, so the probe
   must `init(source)` BEFORE `register_keydown_event` or registration throws.
3. **Mapping config** (`PS5Pad.ini`): exact shared profile for the staged
   `ue4ss/PS5Pad.ini`, matching worker-real-input's INPUT-MAP: OPTIONS=F7,
   UP=UP_ARROW, DOWN=DOWN_ARROW, CROSS=RETURN, LEFT=LEFT_ARROW,
   RIGHT=RIGHT_ARROW; syntax notes cite the retained parser's hard refusals.
4. **Hardware plan** (`HARDWARE-TEST-PLAN.md`): 6 user steps (~2 min), PASS =
   exactly one `key_fired` per press, hold = one, CROSS = zero, disconnect/
   reconnect phase records with baseline swallow, `probe_end` at budget; a
   12-row failure-diagnosis table mapping each evidence shape to its first
   suspect (including "process crashes on first scePad call" = provider
   cannot run in that process, record and stop, no blind replay).
5. **Standalone module**: prepared, because the provider API is sufficient
   (`PadBackend` injection + real Handler link). `pad_probe/PadProbe.{hpp,cpp}`
   (logic, sink-injected), `pad_probe_main.cpp` (`__PROSPERO__`-only exported
   `modbridge_pad_probe_run` + `/data` append sink), vendored
   `keydef_modifier_ops.cpp` (verbatim from worker-real-input, origin labeled),
   `host_test.cpp`, `build_and_test.sh`, `build_and_test_receipt.txt`,
   `PROBE-PRX-BUILD.md` (console compile/link route mirroring the proven
   `native-coherent-pad-runtime-1/build-1` commands, two link routes, gaps).

## Host test results (ASan+UBSan, /usr/bin/clang++ Apple clang 17.0.0, -std=c++20)

ALL PASS — 9 scenarios, 17 assertions: no-press silence; single
press+hold+cached-read → one `key_fired`; press/release/press → two; unbound
CROSS silent while exact-chord OPTIONS|CROSS fires once; disconnect/reconnect
phase records + baseline swallow + post-reconnect single fire; failed
`scePadOpen` → `probe_failed stage=activate phase=Open` with no `probe_end`;
null factory → `probe_failed stage=factory`; stable session → zero
`phase_change`; throwing factory contained. Receipt with SHA256 hashes in
`pad_probe/build_and_test_receipt.txt`. One intermediate failure during
development (test-5 fixture made the pre-disconnect press land on the baseline
sample; fixture corrected, probe code unchanged).

## Gaps / honesty

- Nothing ran on console or with the Prospero toolchain; `PROBE-PRX-BUILD.md`
  documents the exact route but the `-r` link is unexecuted by this worker
  (bounds). Route A (reuse the composed payload link) inherits output.o/fmt.o
  symbols; Route B (standalone) may need to add them — flagged in the doc.
- The composed-runtime pad path (`PS5Pad.ini` via `ConfigureRetainedPS5PadInput`)
  is designed and staged-config-ready but is deliberately the SECOND hardware
  step; rung-5 full acceptance additionally needs worker-3 game-thread
  delivery plus C/D-family capabilities per worker-real-input/VISIBILITY-TEST.md.
- `PS5_PadData` layout statics re-verify at console compile time
  (PS5PadNative.cpp:7-9) — desirable, not yet exercised.

## Files (all under outputs/2026-10-04-zcode-team/worker-real-input-pad/)

`CLAIM.json` (first) · `PAD-API-AND-INTEGRATION.md` · `PS5Pad.ini` ·
`pad_probe/{PadProbe.hpp,PadProbe.cpp,pad_probe_main.cpp,keydef_modifier_ops.cpp,host_test.cpp,build_and_test.sh,build_and_test_receipt.txt,PROBE-PRX-BUILD.md}`
· `HARDWARE-TEST-PLAN.md` · `REPORT.md` (this file, last).
