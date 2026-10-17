# STAGE-PLAN — modbridge_pad_probe.prx (padp-79947e016dfe)

Worker: worker-pad-hardware-probe, 2026-10-04. Console: ps5-host-not-configured.invalid
(pldmgr HTTP 8084, FTP 2121). All console steps are user-coordinated in
procedure but may be executed by the controller via the standing console
authorization; the game LAUNCH and the CONTROLLER PRESSES are user actions.

Purpose: FIRST hardware test of PS5 pad input delivery — does a real
OPTIONS press on the controller reach a registered keybind (OPTIONS->F7)
inside the Townfall process? The module is STANDALONE (not the composed
runtime): entry marker -> startup evidence -> register one keybind ->
bounded 30s poll window -> JSONL records to `/data/pad-probe-events.jsonl`.

## 0. What gets staged (NEW module, one ini line added)

- Upload: `modbridge_pad_probe.prx` (this dir; sha256
  `deccca38e58ac76ea38e1a80b780eb3235fd4dfab7f81a32f776621cac75803a`,
  20427 B, build id `padp-79947e016dfe` — FROZEN; determinism run1==run2)
  to `/data/plugins/modbridge_pad_probe.prx` (new file; nothing replaced).
- ploader entry: `/data/plugins/ploader.ini`, section `[PPSA33286]` — ADD
  the line `/data/plugins/modbridge_pad_probe.prx = 2` (mode 2 = LATE load,
  the only evidenced mode shape). Take a pre-image backup first
  (`ploader.ini.padprobe.bak`) and change NOTHING else (no reordering, no
  whitespace edits — MASTER-STAGING-PACKAGE §C rules).
- **ISOLATION (required for this first run):** the composed runtime
  (modbridge_supplier_composition / modbridge_runtime_aligned_entry) must
  NOT load in the same launch: both it and this probe would race for the
  pad service lease (PROBE-PRX-BUILD.md precondition). For the probe
  launch, temporarily move the other mode-2 module lines out of the
  `[PPSA33286]` section (same .bak file), leaving ONLY the pad probe line.
  Restore the .bak after the run.
- No UE4SS/ini game-config edit is required or permitted. The ini mapping
  (PS5Pad.ini OPTIONS=F7) belongs to the composed-runtime step AFTER this
  probe passes.

## 1. Pre-stage checks (host side)

1. `shasum -a 256 modbridge_pad_probe.prx` ==
   `deccca38e58ac76ea38e1a80b780eb3235fd4dfab7f81a32f776621cac75803a`;
   refuse to stage on any mismatch.
2. Determinism dirs exist (`determinism/run1`, `run2`) with identical
   receipts (`build_id padp-79947e016dfe` in both).
3. Delete any stale `/data/pad-probe-events.jsonl` on console (freshness
   oracle: file mtime must postdate this staging).

## 2. Upload + ftpsrv decryption-oracle (expected values)

Upload via FTP (2121), binary, to `/data/plugins/modbridge_pad_probe.prx`.

ftpsrv auto-DECRYPTS FSELF content on RETR. Verification readback:

- first 4 bytes `7f 45 4c 46` (`\x7fELF`), EI_CLASS=2 — the wrapped host
  file starts `4f 15 3d 1d`; a returned `4f 15 3d 1d` proves NO decryption
  (oracle FAIL).
- byte size: exactly `69920`.
- sha256: exactly
  `96fbdfbcfc6c95f5c18f9275e9c85ced769c3ea3d3af3c2d7666680b7eaba4fa`
  (the bare-sdk inner ELF; equality with the WRAPPED host file
  `deccca38…` is impossible by construction and must NOT be expected).
- `GET /` on the pldmgr surface to confirm the console is up; do NOT load
  anything outside the launch ritual.

## 3. Launch ritual + USER TEST (empirically nailed lineage; do not improvise)

1. Game fully CLOSED (no Townfall process, no suspended app).
2. ploader.ini contains ONLY the pad probe line under `[PPSA33286]`
   (isolation, §0).
3. ploader armed: user clicks `ploader.elf` in Payload Manager, or
   `GET /loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`.
4. User launches Townfall (never automated). Injection completes ~1 minute
   after launch ("injecting 1 plugin", then OK/FAIL).
5. **USER TEST (the actual hardware probe):** within ~30 seconds of the
   injection OK, PRESS OPTIONS ON THE CONTROLLER (2–3 presses, ~1s apart,
   then wait ~5s). The module's poll window is 30 s from plugin start and
   ends ~5 s after the FIRST accepted press (early-exit, cooperative —
   module_start then returns and the module stays loaded but inert).
6. Freshness oracle: mtime of `/data/pad-probe-events.jsonl`.
7. One launch per staged build. If FAIL: collect the ploader surface text
   and any jsonl tail via FTP; do NOT relaunch repeatedly without a changed
   implementation or a specific open question (standing rule).

## 4. First-look analysis (controller, after the run)

Entry-marker triage (lineage contract):

1. **File ABSENT** -> load or channel-open failure. With a NEW import set
   (the 7 pad rows inside a ploader plugin), a silent load-rejection is a
   possible and honestly expected failure mode — capture the ploader FAIL
   text; that is a real result (the pad rows' identity provenance is
   Townfall's own import table, but plugin-context loader acceptance was
   never proven).
2. **Only seq=1 `entry`** -> load + channel OK; failure localized after
   entry (check for a `startup` record; its absence = init-array walk or
   startup-record fault).
3. **entry + startup** -> module healthy; `startup.fields` carries pid,
   clock_rc (expect 0), monotonic_now_ms, sysctl_rc, kern_version,
   ctors_run=1. Clock rc != 0 -> window falls back to bounded iterations
   (recorded as end_reason=budget_fallback).
4. **`probe_start`** -> binding OPTIONS->F7, requested_user -1, window_ms
   30000, grace_ms 5000, poll_budget 200000000.
5. **`probe_failed` stage=activate** with detail/rc = the exact retained
   provider phase + SDK error: UserInitialize (0x80960003 would NOT appear
   here — it is accepted as already-initialized), PadInitialize, UserList,
   NoUser, Open. Each rc is real pad-service evidence from inside the game
   process.
6. **`register`** record present -> the one keybind (OPTIONS->F7) is
   registered through the dispatch gate.
7. **`key_fired`** records — THE ACCEPTANCE SIGNAL: real controller OPTIONS
   presses delivered through the poll path to the registered F7 binding
   (fields: user_id, handle, polls_so_far, ts_ms).
8. **`phase_change`** records -> pad connect/disconnect transitions
   (Active/Disconnected) with the retained provider's exact phase names.
9. **`probe_end`** -> polls, keys_fired, final_phase, user_id, handle,
   elapsed_ms, end_reason. `keys_fired: 0` + end_reason=window_elapsed
   means the pad opened but no OPTIONS was seen in the window (user
   timing) — rerun per rule 7 only with a concrete change (e.g. longer
   window build or corrected press timing).
10. **`teardown`** -> pad handle closed (handle -1) and owned user-service
    lease released (phase Inactive) — the game keeps a clean pad state.

Every non-truncated line must `json.loads` (v3 writer lineage; envelope
marker `modbridge.padprobe`, build_id `padp-79947e016dfe`).

## 5. Rollback

- Restore `/data/plugins/ploader.ini` from `ploader.ini.padprobe.bak`
  (removes the pad probe line, restores the prior module lines).
- The module file may stay: after the run it quiesces inert (no threads,
  no unload, channel closed, pad handle released) — v2/v3 lineage
  behavior. Optionally delete `/data/plugins/modbridge_pad_probe.prx`.
