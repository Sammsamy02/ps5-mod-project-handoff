# CONSOLE-CONTRACT.md — worker-app-modmanager: how the app drives the console

Origin: worker-app-modmanager, 2026-10-04. This document defines the
app<->console contract for the PS5 mod-manager: the console drive protocol,
the artifact formats on both sides, and the safety gates that must hold
before and after every console operation. **Console ops remain
controller-only and PENDING** — nothing here has executed on a console; the
app-side halves are implemented and host-proven in `app/console_link.py`
(13/13 staged files reproduced byte-identically, receipts in
`receipts/staged-byte-compare.log`).

## 1. Division of labor

| Side | Owns | Never does |
|---|---|---|
| App (host) | browse/auth/download, records projection, install decisions, command-file authoring, transcript verdicts, the core loop (`app/modmgr.py`) | touch `/data` directly; run curl at console dispatch time (controller pastes the script); retry a failed dispatch |
| Controller (human) | FTP/HTTP transfer of payloads + command files, payload dispatch, evidence readback | edit command JSON by hand at dispatch time; widen any gate to make it pass |
| Console payload | the only `/data` writer: installer verbs, journaled root swap, e2e-status.jsonl, `.consumed` rename | network, credentials, anything outside `/data/modbridge` |

## 2. Layout (real, console paths)

```
/data/modbridge/
  intake/<id>.zip            downloaded archives (transport -> sink -> intake)
  intake/<id>.q/             quarantine (libarchive intake; RETAINED, never erased)
  state/records.txt          durable mod/game store   (modforge-records\t1)
  state/bundles/<id>.mfb     installable bundles (bridged or supplier)
  state/journal/<TITLE>.*    install swap journal (stage|swap phases)
  state/e2e-status.jsonl     append-only operation transcript
  state/runtime-status.txt   runtime ack (last state; mods-started = running)
  state/caps.tsv             capability table (available/integrating/planned)
  roots/<TITLE>/             THE installed root (exactly one loadlist.txt)
    .modforge-root           ownership marker — installer refuses unmanaged roots
    loadlist.txt             written LAST in every install; runtime reads it at module_start
    <mod files>...
```

## 3. Drive protocol (one console operation)

Each mutating step = one command file + one payload dispatch + one evidence
readback. The loop:

1. **Command authoring (app).** `console_link.build_command(verb, state,
   roots, mods, options)` emits `{"schema":1,"verb":...,"state_root":{"state",
   "roots"},"mods":[...],"options":{...}}` — compact JSON, key order fixed.
   Verb allow-list: `records, register-mod, prepare, prepare-archive, install,
   simulate-runtime, recover`. Options keys are the snake_case argv flags
   (E2E-CMD-SCHEMA.md §3); unknown keys, wrong types, verb-illegal keys,
   `mods` outside prepare/install, `mods`+`remove` together, and the
   `make-bundle`/fault-injection surface are REFUSED app-side before any
   transfer (tests `test_16`).
2. **Transfer gate (controller, gated).** `cmd_put`: STOR only if no stale
   `e2e-cmd.json` exists at `data/modbridge/intake/e2e-cmd.json`; verify FTP
   readback sha256 == local sha256; append to the evidence log.
3. **Dispatch (controller).** `loadpayload:` of the ONE staged payload, whose
   sha256+byte-size must equal the pinned identity (today:
   `e3489297193a639a...`, 681528 B — see §7). Never dispatch unverified bytes.
4. **Consumption proof.** `cmd_gone`: original file gone via FTP AND
   `e2e-cmd.json.consumed` present with hash-equal bytes. The payload
   `rename()`s the file to `.consumed` BEFORE executing the verb; a
   parse/refusal leaves the file in place for inspection (never auto-retried).
5. **Transcript verdict (app).** Read back `state/e2e-status.jsonl` and
   `state/records.txt`; `E2eTranscript.validate()` must report:
   begins == ends (0 unmatched — any unmatched begin is a DEAD command),
   no `{"event":"truncated"}` cap hit, no unparseable lines. The step's
   `end` line carries the CLI exit contract
   (`0` ok / `1` io_error / `2` bad_request / `3` not_ready / `4`
   needs_choices / `5` cancelled). Stage-1/2/3 success additionally emits a
   terminal `status` event (`stage-1-download` / `stage-2-prepare` /
   `stage-3-install`, F-R2).
6. **State check (app).** `records.txt` projection must show the expected
   transition (`downloaded -> prepared` at prepare/prepare-archive;
   `installed` + generation++ + loadlist sha at install; `prepared` again on
   removal). `list_installed()`'s `loadlist_diverged` flag must be false
   after any console install (records sha == live root sha).

## 4. The staged 13-command sequence (whole-pipeline shape)

`console_link.staged_sequence()` reproduces the pinned files in
`worker-e2e-staging-final/cmd/` byte-identically (host proof in
`receipts/staged-byte-compare.log` + `test_15` against the published SHASUMS):

```
recover -> stage0-records -> stage1-register(zip) -> stage2a-prepare-archive
  -> stage2b-1..2-register(mfb) -> stage2b-3-prepare -> stage3-install
  -> stage4-activated -> stage5-mods-started -> stage6-1..3-remove
```

Semantics: `recover` first (crash-recovery reconciliation before anything
mutates), `records` baseline, then register -> prepare -> install with
readback between every step, runtime ack via `simulate-runtime`
(`activated`, then `mods-started` — the ONLY accepted evidence for
`running:true`), then staged removals. A MODE B (config-file entry) variant
requires a payload revision that does not exist yet; until it exists, argv
dispatch (MODE A) is the only lawful configuration
(STAGED-E2E-COMMANDS.md §0).

## 5. Safety gates (must hold; each is testable and tested host-side)

1. **Payload identity gate.** sha256 + byte size pinned before dispatch;
   gate refuses any other bytes; never widened to make a run pass.
2. **Stale-command gate.** A command file is STORed only into a clean
   intake; readback-verified; consumption proven by the `.consumed` rename.
   No blind re-PUT over a stale file.
3. **Transcript pairing gate.** Every begin must get its end. Missing end ==
   the command died; the run is FAILED, not retried.
4. **Ownership gate.** `.modforge-root` marker: the installer refuses to
   replace an unmanaged root (proven, worker-7); the app re-checks before
   any direct loadlist edit and refuses unmanaged roots too.
5. **Transactional root swaps.** Installs/uninstalls ONLY through the
   journaled staged-root swap (`<TITLE>.next` -> `.prev` dance, crash
   recovery on every open + explicit `recover` first). The game sees the old
   or the new set, never a mix; previous set preserved byte-for-byte on
   cancel points.
6. **Backup/restoration path.** Three layers: (a) the swap's `.prev`
   generation for full-installs (binary-owned); (b) the app sidecar
   `<state>/modmgr/backups/<TITLE>/<ts>-loadlist.txt` written before every
   enable/disable edit, from which any bad edit can be restored byte-exactly;
   (c) the disable overlay `<state>/modmgr/disabled-<TITLE>.tsv` that makes
   `enable` an exact inverse (test `test_10`: byte-equal restoration).
7. **Records-sha resync gate.** Any app-side loadlist edit recomputes the
   sha and rewrites ONLY the game-row sha field of `records.txt`, then
   VERIFIES through the binary's own `records` reader before reporting
   success (a mismatch is an io_error, never accepted silently).
8. **Capability honesty.** `running:true` only from a real `mods-started`
   runtime ack; `integrating` frameworks warn but never fake completion;
   unknown/planned capabilities block installs (exit 3) instead of guessing.
9. **No auto-retry.** A refused/failed command is inspected, then re-authored
   as a NEW decision; failed dispatches are never replayed blind.

## 6. What the app must never assume

- That `/data` is browsable after module_start (it is NOT — the loadlist is
  the only runtime window; hence preload-file semantics).
- That a Lua mod is disabled because its `mod` row is gone: the UE4SS
  startup list derives from the preloaded `enabled.txt` file row, so the
  disable operation removes the mod's whole attributed row set (bundle-index
  attribution), and `enabled` state is reported from live loadlist rows +
  overlay, never inferred.
- That the payload accepts MODE B; the config-entry revision does not exist.
- That Flash/normal generation capacity or console execution is proven;
  nothing console-side has run as of this document.

## 7. Current pins (change only with new receipts)

| Artifact | Pin |
|---|---|
| Console payload (D1+F-R1+F-R2) | sha256 `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436`, 681528 B, 116 imports, 0 missing |
| Host composed binary (vendored here) | sha256 `f2215f2ffd92b63c7d642089e60c37e10ba199ff279d0aaa201ff35324e21de6` |
| Staged command files | 13 shas per STAGED-E2E-COMMANDS.md §2 (reproduced byte-identically by this worker) |
| e2e-status caps | 512 lines / 262144 bytes, then `{"event":"truncated"}` and silence |
