# CONSOLE-E2E-PLAN.md — console acceptance test, no developer intervention

Origin: worker-installer-ps5io, 2026-10-04. For the controller to execute
once the components below are composed. End-goal (user-fixed):
download -> prepare -> install -> activate -> uninstall on the console with
NO developer intervention; `downloaded`/`prepared`/`installed`/`running`
stay distinct; `running` only from real runtime acknowledgment
(RUNTIME-ACK.md).

## Composition preconditions (controller, before the run)

- C1. Installer service linked with this worker's `ps5_io.ps5.o` bound to
  worker-7's vendored core (`ps5io_mfb_bind` / `ps5io_mfx_bind` /
  `ps5io_mfa_bind` replace the host POSIX adapters in `modinst.c`'s `bio_*`).
- C2. Archive prepare path linked with `ps5_prepare_bridge.ps5.o` plus the
  pinned intake objects (libarchive 3.8.7 closure per
  `native-host/archive/dependency_build.py`, link order libarchive/liblzma/
  zlib) installed as the `ps5_intake_api` vtable
  {mb_archive_limits_default_v1, mb_archive_budget_default_v1,
  mb_archive_scan_v1, mb_archive_materialize_v1}.
- C3. Injected runtime carries the RUNTIME-ACK.md emissions (mfa_status_*
  calls at module_start). Without C3, stage 5 must fail as "not running".
- C4. Console environment: jailbroken PS5, `/data/modbridge` writable,
  payload deployed via the pinned SDK loader path; title under test known
  (e.g. PPSA33286-class title id) with the game NOT running at stages 1-4.

Stages below are operator steps (single scripted sequence; no developer
tooling between them). Expected states cite worker-7's records model.

## Stage 0 — baseline

- Command: `<service> records <TITLE>`
- Expected: empty records (or prior state), exit 0. Mod root absent, no
  `.next`/`.prev`/journal leftovers, no `runtime-status.txt`.

## Stage 1 — download (owner interface, not this worker)

- Action: owner interface places the downloaded archive at
  `/data/modbridge/intake/<id>.zip` (content-addressed; sha256 recorded).
  For bundle-only flow, place the `.mfb` directly and jump to stage 2b.
- Expected state: `downloaded` in records; archive file present; nothing
  extracted, nothing installed.

## Stage 2 — prepare

- 2a archive flow: `<service> prepare <TITLE> --archive /data/modbridge/intake/<id>.zip --quarantine /data/modbridge/intake/<id>.q`
  - Calls ps5_prepare_archive: scan -> select regular rows -> materialize
    into quarantine with retained journal.
  - Pass: exit 0; records show the mod `prepared` with quarantine manifest
    name + inventory sha256; quarantine contains extracted files; journal
    file present (retained per intake rule).
  - Fail (honest): `requirement-unavailable`/intake status refusal, nothing
    installed.
- 2b bundle flow: `<service> register-mod <TITLE> --bundle <file.mfb>` —
  records `downloaded`; `prepare` computes plan (conflicts/requirements).
- Expected: `prepared` distinct from `downloaded`; no files under the mod
  root yet.

## Stage 3 — install (transaction)

- Command: `<service> install <TITLE>`
- Expected sequence (worker-7 semantics through ps5_io callbacks): marker +
  files staged under `<TITLE>.next`, loadlist written LAST, journal swap,
  rename root->`.prev`, `.next`->root, commit records generation++.
- Pass: exit 0; mod root exists with `.modforge-root`, `loadlist.txt`,
  staged files; records `installed`; `running=0` (runtime-status.txt absent
  or not `mods-started`); journal gone.
- Cancel/crash probe (optional): `--cancel-after N` / kill during swap; rerun
  opens recovery; root is exactly old or new set, never mixed.

## Stage 4 — activate (game launch)

- Action: operator launches the game normally (no developer attach). The
  injected runtime runs `module_start`: mfa_activate over the installed root
  via the `ps5io_mfa` callbacks (only moment /data is readable).
- Expected: `<mod root>/runtime-status.txt` appears with
  `session`, then states per RUNTIME-ACK.md: `activated` -> `engine-ready`
  -> `mods-started`.

## Stage 5 — running (acknowledgment only)

- Command (from the app side after game start): `<service> records <TITLE>`
- Pass: report shows `installed` mods AND `running=1` with
  `runtime mods-started` — derived solely from the last complete state line.
- Fail honestly: any of `failed`/`stopped`/absent file keeps `running=0`
  with the reason. Never fabricate running without `mods-started`.

## Stage 6 — uninstall

- Command: `<service> install <TITLE> --remove <MOD>` then to empty if
  desired.
- Pass: files gone, remaining mods re-composed (conflict winner reverts),
  records show mods `prepared` (not downloaded), generation advances;
  uninstall-to-empty leaves marker + minimal loadlist and the root still
  passes `mfa_activate`; `runtime-status.txt` removed with the root.
- Optional rollback of stage 5: relaunch game; runtime reports
  `activated`/`engine-ready` only, `running=0`.

## Acceptance

E2E passes iff stages 0-6 all pass in one uninterrupted operator sequence on
the console, every stage transition observable in records, and stage 5's
`running=1` came only from the runtime-written status file. Any stage may
fail; the plan requires the failure to be reported distinctly, not worked
around live.
