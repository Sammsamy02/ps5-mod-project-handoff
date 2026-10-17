# STEP-BY-STEP.md — staged console E2E run (controller-executed)

Origin: worker-e2e-console-staging, 2026-10-04. HTTP patterns follow
`outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md`; CLI grammar follows
`worker-e2e-final/E2E-RUNBOOK-V2.md` and was re-executed host-side against the
CURRENT bridge-linked build by this worker (probe evidence in REPORT §3;
console-side results remain [TARGET-OPEN] until this run).

Conventions:
- `HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121`
- Console service invocation placeholder: `$SVC` = the argv mechanism resolved
  by step G1 (e.g. `GET /loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf`
  plus arguments per G1 outcome). Every `$SVC` dispatch is followed by an FTP
  RETR of `/data/modbridge/state/e2e-status.jsonl` and `/data/modbridge/state/records.txt`.
- All e2e-status lines below were produced by the CURRENT bridge-linked
  binary (host probe, REPORT §3 probe B) with title `PPSA33286`.
- Rollback principle: this run owns ONLY `/data/modbridge/**` and the new
  payload alias `MODBRIDGE_INST_E2E`; nothing else is ever written/deleted.

## Step P — stage the payload (once)

```sh
curl -s "$HTTP/list_payloads"                                   # reachability, 200 expected
curl -s -X POST --data-binary @outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/composition/build/console/modbridge-installer.payload.elf \
     "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"      # upload ONCE (CONSOLE-LOG item 2 pattern)
curl -s "$HTTP/manage:check"                                    # ok/folder_exists/file_exists (item 3)
curl -s "$FTP/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" | shasum -a 256
```
Expected: readback = 681632 bytes, sha256 `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9`.
Rollback: none needed; if upload failed, re-upload is to the SAME new alias we
just created (it is ours). Never overwrite any pre-existing payload name.

## Step G1 — resolve the argv dispatch gate (bounded, before anything else)

Issue the `records` command through pldmgr using whatever argument mechanism
it actually supports (query parameter, args field, or none — to be verified
against the running pldmgr; no workspace evidence exists either way):
`$SVC --state /data/modbridge/state --roots /data/modbridge/roots records`

Then `curl -s "$FTP/data/modbridge/state/e2e-status.jsonl"`.
- PASS: file contains `{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}` + `{"event":"end","exit":0}` -> argv reached the payload; proceed. (These exact two lines = stage 0 done; skip Step 0.)
- FAIL: no file / no begin line -> NO-GO per PREREQS G3 until an argv-capable
  dispatch exists. Do NOT improvise wrappers.
Rollback: delete `/data/modbridge/state` if created (we own the fresh tree).

## Step 0 — baseline (skip if G1 already produced the two lines above)

`$SVC --state /data/modbridge/state --roots /data/modbridge/roots records`
Expected: exit 0, empty output (fresh tree); e2e-status exactly:
```
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
(rehearsal S0 + probe B line 1). Rollback: n/a (read-only).

## Step 1-owner — owner-placed archive (native download DEFERRED)

```sh
curl -s -T .superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip \
     "$FTP/data/modbridge/intake/movspeed.zip"                  # P2 STOR probe gate first (PREREQS P1)
curl -s "$FTP/data/modbridge/intake/movspeed.zip" | shasum -a 256   # must equal 3850a2b5…d6ad
```
Then register through the binary (host-proven: register-mod ACCEPTS a raw zip,
`registered … state=downloaded variants=0`; no sidecar needed — REPORT §3):
`$SVC --state /data/modbridge/state --roots /data/modbridge/roots register-mod --id movspeed --title PPSA33286 --bundle /data/modbridge/intake/movspeed.zip`
Expected stdout: `registered mod=movspeed title=PPSA33286 state=downloaded variants=0`; records row `mod movspeed PPSA33286 downloaded`; e2e-status:
```
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
```
Rollback: delete `/data/modbridge/intake/movspeed.zip`; fresh-tree reset =
delete `/data/modbridge/state` (only legitimate before stage 3 commits).

## Step 2a — prepare-archive (auto-bridge expected)

`$SVC --state /data/modbridge/state --roots /data/modbridge/roots prepare-archive --title PPSA33286 --archive /data/modbridge/intake/movspeed.zip --quarantine /data/modbridge/intake/movspeed.q --mod movspeed`
Expected (probe B; zb-pak e2e-status identical):
- stdout: `prepared title=PPSA33286 quarantine=…movspeed.q manifest=manifest-v1.bin files=… inventory=… token=… journal_retained=1 mod=movspeed` then `bridge bundle …/state/bundles/movspeed.mfb id=movspeed …` then `records prepared mod=movspeed title=PPSA33286`
- e2e-status:
```
{"event":"begin","cmd":"prepare-archive","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"auto-bundle composed mfb"}
{"event":"end","exit":0}
```
- records row artifact path becomes `/data/modbridge/state/bundles/movspeed.mfb`; nothing under `/data/modbridge/roots` yet.
Failure honesty: missing `auto-bundle` line with exit 0 = archive had no
recognized roles (REPORT §3 probe A) — STOP, do not proceed to install with
this mod; report distinctly. Refusal path: exit 2, quarantine retained.
Rollback: quarantine/`movspeed.mfb` removal + records has no per-row delete
verb; use the fresh-tree reset only before stage 3, otherwise continue to 6
and end the run honestly.

## Step 2b — bundle flow (register two .mfb, then plan)

Stage (after readback shas `7b9888b9…` / `eefc774a…`):
`mm-skin.mfb`, `mm-lua.mfb` -> `/data/modbridge/intake/`.
```sh
$SVC … register-mod --id mm-skin --title PPSA33286 --bundle /data/modbridge/intake/mm-skin.mfb
$SVC … register-mod --id mm-lua  --title PPSA33286 --bundle /data/modbridge/intake/mm-lua.mfb
$SVC … prepare --title PPSA33286 --mods mm-skin,mm-lua
```
Expected: two `registered mod=… state=downloaded` lines; prepare prints
`plan title=PPSA33286 mods=2` + `ready 2` (rehearsal S2b `ready` line pattern);
e2e-status per command: `begin cmd=register-mod stage=stage-1-download` + end;
`begin cmd=prepare title=PPSA33286 stage=stage-2-prepare` + end (probe B /
rehearsal transcript lines 19-24). Rollback: as 2a.

## Step 3 — install (transactional, all three mods)

`$SVC … install --title PPSA33286 --mods movspeed,mm-skin,mm-lua`
Expected stdout (probe B): `plan … mods=3` -> `staged files=… root=…PPSA33286.next` ->
`installed title=PPSA33286 generation=1 mods=movspeed,mm-skin,mm-lua loadlist=<sha256>`.
records: `installed` rows; `runtime not-started running=0`. e2e-status:
```
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"end","exit":0}
```
(no `status` line — install emits begin+end only, rehearsal INFO + probe B).
Root: `/data/modbridge/roots/PPSA33286/` with `.modforge-root`, `loadlist.txt`,
staged files; journal gone after commit. **Interrupted transaction**: rerun
opens recovery; hard rollback verb = `$SVC … recover` (rollback probes
`--cancel-at rename1` / `--crash-at staged` + `recover` all pass in the 26/26
suite, RUNBOOK-V2 §3). Completed-install rollback is Step 6 itself.
If game running: stop, do NOT retry live — C4 requires game closed for 0-3.

## Steps 4-5 — runtime ack, SIMULATED ONLY (UE4SS deferred)

```sh
$SVC … simulate-runtime --title PPSA33286 --rt-state activated
$SVC … records
$SVC … simulate-runtime --title PPSA33286 --rt-state mods-started
$SVC … records
```
Expected e2e-status (per simulate command):
```
{"event":"begin","cmd":"simulate-runtime","title":"PPSA33286","stage":"stage-4-5-runtime-ack"}
{"event":"end","exit":0}
```
Expected records: after `activated` -> `runtime activated running=0`; after
`mods-started` -> `runtime mods-started running=1` (probe B). **This
`running=1` is the simulated ack-file proof; the report must label it
SIMULATED. It is NOT a real runtime acceptance** (PREREQS P4 / READINESS §5).
Rollback: none (status file removed with the root at stage 6).

## Step 6 — uninstall

```sh
$SVC … install --title PPSA33286 --remove mm-lua
$SVC … install --title PPSA33286 --remove mm-skin
$SVC … install --title PPSA33286 --remove movspeed
$SVC … records
```
Expected: each removal prints `staged files=…` + `installed title=PPSA33286
generation=<n+1> mods=<remaining> loadlist=<sha>`; removed mods drop to
`prepared` (probe B: movspeed `prepared`, generation advanced, `runtime
not-started running=0`); final root is the empty managed set
(`.modforge-root` + header-only `loadlist.txt` remain; rehearsal S7).
e2e-status: one `begin cmd=install stage=stage-3-install` + `end exit 0` per
removal. Rollback: re-register + re-prepare the removed mod (2a/2b steps).

## Step C — collect evidence (read-only)

```sh
curl -s "$FTP/data/modbridge/state/e2e-status.jsonl" > console/MODBRIDGE_INST_E2E-e2e-status.jsonl
curl -s "$FTP/data/modbridge/state/records.txt"      > console/MODBRIDGE_INST_E2E-records.txt
curl -s "$FTP/data/modbridge/state/bundles/movspeed.mfb" | shasum -a 256   # on-console bridge artifact identity
```
Store under `outputs/2026-10-04-zcode-controller/console/` (controller dir,
CONSOLE-LOG item 6 pattern) with a console-receipt JSON: dispatch list, readback
shas, per-stage pass/fail, and the SIMULATED label on stage 4-5.

## Full-tree reset (only if aborting before stage 3)

`curl` FTP DELE of `/data/modbridge/intake/*` we staged + delete
`/data/modbridge/state` and `/data/modbridge/roots` (created by this run; if
FTP DELE for dirs is unsupported, leave the dormant tree and record it — do
NOT delete anything outside `/data/modbridge/**`).
