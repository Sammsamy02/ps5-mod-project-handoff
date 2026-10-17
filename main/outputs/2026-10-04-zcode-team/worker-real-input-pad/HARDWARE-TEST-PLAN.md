# HARDWARE-TEST-PLAN.md — first hardware test: pad input reaches the dispatch gate

worker-real-input-pad, 2026-10-04. Rung 5 sub-step 1 (probe), before the
composed-runtime rung-5 acceptance in worker-real-input/VISIBILITY-TEST.md.
NOTHING below has run; this is the procedure + evidence contract for whoever
holds the scoped console-execution exception.

## 0. What this test is and is not

- IS: the first-ever hardware contact for `native_ps5_pad_provider`
  (`hardware_runtime_validated=false` today). One keybind (F7), one binding
  (OPTIONS→F7), real `scePad*` calls, real retained `Handler` dispatch, JSONL
  evidence. Standalone module (`pad_probe/`), so the blocked game-thread seam
  (worker-3 `ExecuteInGameThread`) cannot mask the result.
- IS NOT: rung-5 acceptance. It does not involve Lua, the Item Spawner mod, or
  a visible spawn. Those need the composed runtime (`ue4ss/PS5Pad.ini` via
  `ConfigureRetainedPS5PadInput`) plus the unblocked game-thread delivery.

## 1. Preconditions

1. Probe built per `pad_probe/PROBE-PRX-BUILD.md` (route A payload-reuse or
   route B standalone) and callable in the game process.
2. One DualSense pad connected and activated to the logged-in console user.
3. The composed runtime is NOT running in the same process (pad service lease
   isolation, PROBE-PRX-BUILD.md §4).
4. `/data/pad_probe/events.jsonl` absent or empty at start (append mode).

## 2. What the user does (≈2 minutes)

1. Launch the probe payload; wait ~5 s for probe start (nothing visible is
   required — evidence is the JSONL file).
2. Press OPTIONS once, release, wait 1 s.
3. Press OPTIONS again, hold ~2 s (hold must produce exactly one record),
   release.
4. Press CROSS several times (must produce NOTHING — unbound button).
5. If practical: disconnect the pad (PS button > device list > off), wait 3 s,
   re-activate it, wait 2 s, press OPTIONS once.
6. Let the probe finish its bounded budget (or stop the payload) and pull
   `/data/pad_probe/events.jsonl`.

## 3. What the console-side evidence looks like (PASS)

One JSONL record per line, newest last, all from `pad_probe/PadProbe.cpp`:

```
{"ts_ms":0,"event":"probe_start","binding":"OPTIONS->F7","requested_user":-1}
{"ts_ms":3,"event":"key_fired","key":"F7","button":"OPTIONS","user_id":1,"handle":0,"polls_so_far":137}
{"ts_ms":1501,"event":"key_fired","key":"F7","button":"OPTIONS","user_id":1,"handle":0,"polls_so_far":1442}
{"ts_ms":6120,"event":"phase_change","from":"Active","to":"Disconnected","error":0}
{"ts_ms":9110,"event":"phase_change","from":"Disconnected","to":"Active","error":0}
{"ts_ms":9620,"event":"key_fired","key":"F7","button":"OPTIONS","user_id":1,"handle":0,"polls_so_far":58231}
{"ts_ms":60000,"event":"probe_end","polls":60000,"keys_fired":3,"final_phase":"Active","user_id":1,"handle":0}
```

PASS criteria (all must hold):

1. `probe_start` present with the exact binding string `OPTIONS->F7`.
2. Exactly one `key_fired` per discrete OPTIONS press (2 presses before the
   disconnect test → 2 records; +1 after reconnect → 3).
3. The 2-second HOLD produced exactly one `key_fired` (no auto-repeat: the
   retained rising-edge + cached-timestamp guards, PS5PadSource.cpp:80-91).
4. ZERO `key_fired` records attributable to CROSS.
5. Disconnect produced exactly one `Active→Disconnected` phase_change; the
   reconnect sample produced a `Disconnected→Active` phase_change and NO
   key_fired (baseline swallow); the first OPTIONS press after reconnect
   fired exactly once.
6. Final `probe_end` with `polls == poll_budget` and `final_phase:Active`.
7. Every record is valid single-line JSON (no interleaved/partial lines —
   single-threaded writer, per-line write()).

This is the missing `hardware_runtime_validated=true` evidence for
`native_ps5_pad_provider`, scoped exactly to: open/read/rising-edge/dispatch.

## 4. What failure looks like (diagnosis table)

| Evidence in events.jsonl | Meaning | First suspect |
|---|---|---|
| `probe_failed` `stage:activate` `phase:UserInitialize` err≠0 | `sceUserServiceInitialize` refused | service already initialized by another owner in-process (lease conflict) — check precondition 3 |
| `probe_failed` `stage:activate` `phase:NoUser` | login list empty / no match | user not signed in on the pad's side; `requested_user` wrong |
| `probe_failed` `stage:activate` `phase:Open` err<0 | `scePadOpen` refused | pad not activated for that user; wrong port type perms (privilege) |
| `probe_failed` `stage:factory` | factory null (build wiring) | Route B link missing `PS5PadNative.o` |
| `probe_failed` `stage:run` `detail:exception` | Handler init/registration threw | injected-mode ordering violated (init before register is in PadProbe.cpp — suspect a different Handler build) |
| `probe_start` + `probe_end` only, `keys_fired:0` despite presses | polls run but no edges | read_state returning error (check for `phase_change …to:"Read"`), pad reports `connected:false` (Disconnected records), or timestamps never change (cached-read guard eats all samples) |
| `key_fired` only for CROSS, none for OPTIONS | mapping wrong at the ScePadRead level | SDL PS5 button-bit mismatch — compare `PS5_PadData.buttons` bit for OPTIONS; this is exactly the SDL_ps5joystick.h static-assert surface |
| Two `key_fired` per press | duplicate detection failing | timestamp not advancing between samples (should be impossible: cached guard); capture raw sample timestamps via a debug build before touching the provider |
| No file at all | sink/open failed or crash before first record | `/data` writability; check payload log for `probe_failed stage:boundary`; crash before module return |
| `phase_change` to `Read` err≠0 mid-run | `scePadReadState` failed once | transient USB/BT hiccup — provider stays owner; subsequent presses still fire (retry is explicit contract) |

Non-JSONL failure mode: the process crashes on the FIRST `scePad*` call —
that is itself the finding: `native_ps5_pad_provider` cannot run in this
process/route; record the fault code and stop; do not retry in the same
session (no blind replay).

## 5. After the probe passes (next rungs, out of scope here)

1. Stage `PS5Pad.ini` (this directory) as `ue4ss/PS5Pad.ini` in the activation
   bundle and start the composed activated runtime: `ConfigureRetainedPS5PadInput`
   now parses it and opens the same provider (ue4ss_backend.cpp:26). Failure
   here throws at startup with the exact phase/error — that is the composed
   path's own evidence.
2. Register the mod's keybinds (Item Spawner main.lua:694-729) and continue
   with worker-real-input/VISIBILITY-TEST.md — which additionally requires the
   worker-3 game-thread delivery seam, still blocked.
