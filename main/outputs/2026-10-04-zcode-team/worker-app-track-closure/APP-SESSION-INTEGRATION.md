# APP-SESSION-INTEGRATION.md — bringing the app live against the console in the return window

Origin: worker-app-track-closure, 2026-10-04 (read-only synthesis; nothing here
has executed on a console; no code changed). This is the exact, receipt-cited
recipe for riding the app track's console-session closure onto the SAME return
window as the UE4SS session (RETURN-SESSION-MASTER-PLAN.md). Every console step
stays controller-only; the app-side halves are host-proven by the four
app-track workers.

Abbreviations: `W = outputs/2026-10-04-zcode-team` (under the workspace root),
manager HTTP `ps5-host-not-configured.invalid:8084`, anonymous FTP `ps5-host-not-configured.invalid:2121`.
Laws inherited unchanged: dispatch EXACTLY ONCE; >=30 s gaps between
dispatches; timeout = crash-signal; any gate mismatch = STOP + preserve +
reconcile, never re-send, never replay.

---

## 1. The three moving parts (as evidenced, with pins)

| Part | Artifact | Identity pin | Evidence |
|---|---|---|---|
| Host app binary (serves UI + API, fixture OAuth compiled in) | `W/worker-app-ui-flows/build/final/desktop_fixture-download/modbridge-native-host-desktop-download` | sha256 `a677e7b273019f3accf9d29bb072bc44cf1b2061c2db9875fffaf29e272e91a9`, build_id `9b46b161…` (re-hashed on disk this session) | FIXES.md FIX 1+ENABLER 3; UI flows 52 checks green |
| Core-loop driver + console emitter | `W/worker-app-ui-flows/vendored/repo/outputs/ps5-mod-bridge/native-host/app_core/{modmgr.py,console_link.py,modmgr_api.py}` | modmgr/console_link sha `2590e0e4…` = worker-app-modmanager bytes | backend-integration REPORT §1 |
| Proven installer binary (the only mutation engine) | `W/worker-app-ui-flows/vendored/bin/modinst_composed` | sha256 `f2215f2f…` = CONSOLE-CONTRACT §7 pin (re-hashed on disk) | worker-app-modmanager |
| Console payload that READS staged command files (MODE=B config-entry) | `MODBRIDGE_INST_E2E.elf` alias | sha256 `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`, 681888 B | console_batch.py `mode_gate`; worker-e2e-payload-build; suites 348 checks 0 failed |
| Old D1 payload (argv-only, REFUSES cmd files) | — | sha256 `e3489297…`, 681528 B | mode_gate: "MODE=A … STOP — do not run cmd files against it" |
| Console tree | `/data/modbridge/{intake,state,roots}` | layout per CONSOLE-CONTRACT §2 | CONSOLE-CONTRACT §2 |

The app's queued commands are MODE-B command files. The batch tool's `mode_gate`
is explicit: cmd files run ONLY against `a21771f8…/681888`; the D1 payload on
the alias is a STOP. This is why P0 below exists.

## 2. Pre-window host prep (controller/host-side; do BEFORE the user returns)

- **P0 — re-pin the app's payload identity (the ONE required code change; NOT
  yet applied).** `app_core/modmgr_api.py` `PAYLOAD_IDENTITY` still pins the D1
  MODE-A payload (`e3489297…/681528`, "CONSOLE-CONTRACT.md §7 pin"). Every
  staged manifest row carries it, and the drive protocol's payload-identity
  gate refuses any other bytes. Change the pin to the MODE=B payload
  `a21771f8…/681888` (source: worker-e2e-payload-build build-r1 + the batch
  tool's own gate), re-run the backend 42/42 suite
  (`worker-app-ui-flows/test/regression/run_api_tests.py`) and the 52 UI-flow
  checks so the staged manifests and the batch tool agree BEFORE anything is
  queued. Until this lands, the controller must treat manifest
  `payload_identity` rows as stale and gate dispatch identity on the batch
  tool's readback instead (the tool already refuses both NO-GO bytes and
  MODE-A-on-cmd-files).
- **P1 — stand the app up durably.** Run the P1 binary at
  `--port 62283` (matches the fixture's registered redirect URI
  `http://127.0.0.1:62283/auth/callback`) with the runtime env the routes are
  gated on: `MB_MODMGR_BIN` (pinned installer above), `MB_MODMGR_SCRIPT`
  (app_core/modmgr_api.py), `MB_MODMGR_STATE` + `MB_MODMGR_ROOTS` (DURABLE
  dirs this time — not the workers' /tmp fixtures; the queue must survive to
  the window), `MB_MODMGR_PYTHON`. Without the env the routes 404-fall-through
  (upstream behavior); with it, `GET /api/capabilities` reports
  `installed_records:true`.
- **P2 — load the queue before the window (optional but recommended).** Walk
  the UI install flow once against the durable state so
  `<state>/modmgr/console-queue/` holds the real commands
  (`NNN-<name>.json` slots + append-only `manifest.jsonl` rows: command sha256,
  bytes, verb, title, `console_executed:false`, `consumed:false`). Every
  mutation endpooint stages its own console-reproducing commands (install zip =
  register-mod + prepare-archive + install; disable = install WITHOUT the mod;
  enable = full desired set; uninstall = install `--remove`). Enumerate with
  `modmgr_api.py queue --state …` or read the manifest directly.
- **P3 — live cookies (already satisfied by the fixture; real Nexus is
  user-only).** The P1 binary has the fixture OAuth config COMPILED IN
  (`build/final/desktop_fixture-download/oauth_config.h`:
  `MB_HOST_OAUTH_CONFIGURED 1`, client_id `owned_native_jobs_client`, scope
  `fixture_scope`, loopback `*_resolve` pins, fixture CA). That is enough for
  the REAL session admission the browser experiences: `GET /api/capabilities`
  issues `Set-Cookie: mb_session=…; HttpOnly; SameSite=Lax`, mutations require
  session cookie + `Origin` + `X-CSRF-Token`. It contacts NO provider (all
  Nexus hosts resolve to 127.0.0.1). When the user supplies the real
  client_id/scope/CA, the only change is regenerating that header + one rebuild
  — no code path changes.
- **P4 — stage the app's fixture zip(s) into the durable intake dir** (sibling
  `intake/` next to the state dir). The server enforces realpath containment:
  `POST /api/install` accepts only archives already in intake (the 4096-byte
  JSON gate rules out raw upload by design). For the window, the zips the UI
  will "install" must be on the host AND their bytes go to the console intake
  by FTP (step 3.1 below).

## 3. The console-session slot and the drive protocol

### 3.1 WHERE the app drive rides the batch session

Insert the app drive as a new flow BETWEEN the batch tool's `e2e` stage and
`stage-ridealongs`, inside the SAME game-CLOSED payload-only window:

```
Phase 0 sanity  ->  dll8  ->  e2e (canned 13-cmd cycle, ends with §7 full-tree
reset of /data/modbridge/**)  ->  [APP DRIVE — this section]  ->
stage-ridealongs  ->  stage-settings  ->  game-evidence (arm + launch)
```

Why this slot, exactly (each reason is a law already on disk):

- **After `e2e`**: the `e2e` stage ends with the §7 full-tree reset ("after
  green+reset the console is back to the pre-E2E payload-only state", and the
  `MODBRIDGE_INST_E2E` payload alias STAYS). Running the app drive before it
  would have the reset wipe the app's freshly proven tree. The alias being
  staged means the drive needs NO new payload upload — the per-dispatch
  readback gate re-verifies `a21771f8…/681888` each time.
- **Before `stage-settings`**: the v3 settings restage FREEZE-checks the
  staged `roots/PPSA33286/loadlist.txt` against the recorded v2 inventory
  ("divergent/unowned = FAIL, never overwritten") and then STORs the fixed
  1448 B `d8faa4ec (447 B is the loadlist, `4d98ecd7)…` pair. An app-authored loadlist would either fail the
  freeze honestly or be clobbered by the fixed pair. Running the app drive
  before the restage keeps the settings law's pre-image exactly as recorded;
  what the app's install does to the loadlist is then a SEPARATE, explicitly
  reconciled step (§5, follow-on recipe — not silently mixed into this
  window's launch expectations).
- **Never during/after arm**: from the moment `game-evidence` prints its gate
  ("arm ploader", "launch Townfall"), NOTHING may mutate `/data/modbridge`:
  `/data` is not browsable after `module_start` (the loadlist is the only
  runtime window), the loadlist is preloaded once at `module_start`, and the
  master plan's own law is "Never interleave a game session with the payload
  window". The app UI goes READ-ONLY for the whole game session — no
  `POST /api/install`, no enable/disable/uninstall, no dispatches. If the
  console is already inside a game session when the window opens, defer ALL
  payload stages (including the app drive) until it ends.

### 3.2 The drive loop (per queued command; the modmgr console_link protocol,
executed with the batch tool's rituals)

For each row in `manifest.jsonl` with `consumed:false`, OLDEST SLOT FIRST:

1. **Clean-intake transfer gate.** FTP-read `/data/modbridge/intake/` first;
   STOR the command file as `intake/e2e-cmd.json` ONLY if no stale
   `e2e-cmd.json` exists; FTP readback sha256 must equal the manifest row's
   `command_sha256` (and byte size). Same for the archive itself: for an
   install of a zip, STOR the zip to `intake/<id>.zip` FIRST (readback sha
   must equal the host intake file's sha) before its register/prepare-archive
   commands.
2. **Identity gate (per dispatch).** FTP-readback the staged
   `MODBRIDGE_INST_E2E` alias; it must equal `a21771f8…/681888` (MODE=B).
   `e3489297…/681528` on the alias = MODE=A = STOP (do not run cmd files).
   Anything else = NO-GO = STOP.
3. **Dispatch EXACTLY ONCE.** `loadpayload:` the alias; wait the 15 s settle
   the batch tool uses AND keep >=30 s gaps between dispatches. No re-send,
   no retry — a timeout is a crash-signal, not a slow run.
4. **Consumption proof (`cmd_gone`).** Original `e2e-cmd.json` gone via FTP
   AND `e2e-cmd.json.consumed` present with hash-equal bytes. A refused
   command (parse/refusal) leaves the file in place — inspect, then re-author
   as a NEW decision (new queue slot), never a blind replay.
5. **Transcript verdict.** Fetch `state/e2e-status.jsonl` (fresh connection,
   15–30 s wait) and validate with the proven `E2eTranscript`
   (`modmgr_api.py transcript` against the fetched copy): begins == ends (0
   unmatched — any unmatched begin is a DEAD command = FAILED, not retried),
   no `{"event":"truncated"}` cap, and the step's `end` line carries exit 0
   (0 ok / 1 io_error / 2 bad_request / 3 not_ready / 4 needs_choices / 5
   cancelled / 6 stale-dispatch refusal).
6. **State check.** Fetch `state/records.txt`; the projection must show the
   expected transition (`downloaded -> prepared` at prepare-archive;
   `installed` + generation++ + loadlist sha at install; `prepared` on
   removal). The console's own `loadlist` (in `roots/<TITLE>/`, written LAST
   by every install) is the activation root update.
7. **Close the slot honestly.** Only after 5+6 are green, record the row as
   consumed in a LOCAL drive receipt (manifest rows are append-only; the
   console-side `.consumed` twin + transcript are the real proof). Update the
   receipt with `console_executed:<run-id>` — before that, every row stays
   `console_executed:false`.

Interrupted mid-install: if stage-3 commits incompletely, the ONE permitted
`recover.json` dispatch runs, then HALT for reconciliation — the all-green
ledger no longer applies and is not re-claimed.

## 4. End-user flow spec: install a Nexus archive zip through the UI

Step by step, who does what, and what each step's evidence is.

**Host half (game may be closed or open; no console involved yet):**

1. **User — get the archive into intake.** Put the downloaded Nexus zip into
   the durable `intake/` dir (today the app has no in-UI download: see gaps
   G3/G4). UI: Installed tab -> install panel: Title ID (e.g. `PPSA33286`),
   Mod ID, archive path -> Install. `POST /api/install` (session cookie +
   Origin + CSRF, <=4096-byte JSON body).
2. **Host — the real local install.** The proven binary performs the zip
   bridge (quarantine -> member classification -> `bundles/<id>.mfb`), the
   register -> prepare-archive -> install chain, the journaled staged-root
   swap under `.next`/`.prev`, and commits records `installed` + generation++
   + loadlist sha (verified through the binary's own reader). The UI shows the
   busy state, then the new confirmed row (`transaction_confirmed` + 64-hex
   sha256) with enabled/files/bytes.
3. **Host — the console commands get staged.** The same request appended
   `register-mod`, `prepare-archive`, `install --mods <full desired set>` to
   `<state>/modmgr/console-queue/` with sha-bound manifest rows
   (`console_executed:false`). The desired set is records-installed ∪
   disable-overlay ∪ new — one install can never silently drop other mods.

**Console half (game CLOSED; the §3.1 slot; controller drives, user authorizes):**

4. **Controller — transfer.** STOR the zip to `intake/<id>.zip`, then each
   queued command in order to `intake/e2e-cmd.json` (readback-verified, §3.2
   steps 1–2).
5. **Controller — dispatch.** One dispatch per command (§3.2 step 3). The
   payload consumes the command file (`.consumed` twin), runs the verb, and
   appends begin/end/status to `state/e2e-status.jsonl`.
6. **Activation root update.** The console `install` verb recomposes
   `/data/modbridge/roots/PPSA33286/` under the journaled swap and writes
   `loadlist.txt` LAST — exactly the shape the staged 13-command sequence
   proved. Records + generation + loadlist sha are the readback truth
   (§3.2 steps 5–6). The game has NOT run; nothing is "active" yet — this is
   the staged root waiting for the next launch.

**Launch half (the master plan's own gates, unchanged):**

7. **Settings/loadlist restage law (the part the modmgr already proved).**
   `stage-settings` lockstep-restages the UE4SS settings + loadlist pair:
   freeze-check against the recorded inventory, ONE lockstep upload, byte-exact
   readback of BOTH, never staged mismatched. IMPORTANT honest boundary: the
   v3 pair is a FIXED recording — an app-installed mod's rows are NOT in it.
   For the app mod to actually load at launch, the pair must be re-authored to
   include the app rows (see §5 follow-on). Without that step, this window's
   honest claim is: console EFFECT of the app install proven end-to-end
   (records + root + transcript); NOT in-game loading of the app mod.
8. **Arm ploader (user), launch Townfall (user).** Activation consumes the
   staged root at `module_start` (title/engine/framework gates, preload by
   role from the loadlist). The runtime ack `mods-started` in
   `state/runtime-status.txt` is the ONLY accepted `running:true` evidence —
   the UI derives `running` from it and never invents it.
9. **Enable/disable/uninstall on console** are the same protocol with their
   staged verbs: disable = `install --mods` WITHOUT the mod (the runtime sees
   only loadlisted bytes, and a Lua mod's startup list derives from the
   preloaded enabled.txt row — so row removal IS the disable); enable = full
   desired set (byte-exact restoration lineage, host-proven); uninstall =
   `install --remove`. Never mutate these while the game session runs.

## 5. Follow-on recipe (NOT this window's claim): making the app mod load at launch

After a green app drive, the activation root and the settings pair both live
under `roots/PPSA33286/`. To converge them lawfully: (a) fetch the
app-authored console `loadlist.txt` + `records.txt`; (b) re-author the
settings package pair host-side to include the app mod rows (the app's own
guarded loadlist-edit + records-sha-resync lineage is the recipe; the settings
`file` row and UE4SS rows stay byte-identical); (c) record the NEW pair
inventory (bytes + shas) as the successor to v2/v3 in the freeze-check; (d)
lockstep restage per the stage-settings law; (e) then launch. Each revision
gets distinct IDs and its own receipts — no silent overwrites, no re-rating
unchanged evidence.

## 6. Abort / failure laws (inherited, restated for the app drive)

Ctrl-C or `abort` at any gate stops; completed steps stay in receipts; nothing
re-dispatches automatically. A refused/failed command is inspected and
re-authored as a NEW decision in a NEW queue slot. Gate mismatch = STOP +
preserve + reconcile. No unrelated process or tree is touched: the drive only
ever writes `/data/modbridge/**` on the console and the worker-owned state on
the host.

## P0 STATUS (controller update): APPLIED — worker-app-payload-repin re-pinned PAYLOAD_IDENTITY to MODE-B a21771f8/681888 across all vendored copies; API 42/42 + UI 52/52 re-green; binary a677e7b2 byte-identical (pin lives in the runtime driver).
