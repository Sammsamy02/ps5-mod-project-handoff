# CONSOLE-E2E-EXECUTION.md — exact controller sequence, D1-rel chain, stages 0 / 2a / 2b / 3 / 4-5 / 6

Origin: worker-e2e-console-dry, 2026-10-04. Supersedes
`worker-e2e-console-staging/STEP-BY-STEP.md` for the D1-rel payload
(`e3489297…`, 681528 B, `STAGING-MANIFEST-D1REL.md`). HTTP/FTP patterns follow
`outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md`; grammar follows
`worker-e2e-final/E2E-RUNBOOK-V2.md`. Every expected line below is a VERBATIM
merged-chain transcript fact (read from
`worker-d1-chain-relink/test/run/zb-pak/state/e2e-status.jsonl` and the fr2-87
run states, and from `worker-f-r2-fix/owned/e2e_status.c` key order) — console
results remain [TARGET] until this run executes.

## 0. Conventions

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
WS=/WORKSPACE/.codex/worktrees/translation-core/what
PL=$WS/outputs/2026-10-04-zcode-team/worker-d1-chain-relink/composition/build/console/modbridge-installer.payload.elf
CMDFTP=data/modbridge/intake/e2e-cmd.json            # fixed config path (schema: E2E-CMD-SCHEMA.md)
CMDFTPDONE=data/modbridge/intake/e2e-cmd.json.consumed
EV=<run evidence dir>/console                        # controller-owned
```

Dispatch modes (set exactly one before the run):

- **MODE A — argv (G1 GO)**: pldmgr forwards arguments; `$DISPATCH <args>` is
  `curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf?<args-grammar per G1 probe>"`.
  No cmd files are used; skip every `cmd_put` block. The G1 probe's verbatim
  grammar binds this mode.
- **MODE B — config-file entry (G1 NO-GO fallback)**: bare dispatch
  `$DISPATCH` = `curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf"`,
  preceded per dispatch by `cmd_put` of that stage's command file
  (`E2E-CMD-SCHEMA.md` §4) and followed by `cmd_gone`. **MODE B REQUIRES THE
  CONFIG-ENTRY PAYLOAD REVISION** (new sha re-pin) — the current
  `e3489297…` build has no `e2e-cmd.json` reader and bare-dispatches to
  `usage()`/`EX_BAD`. Until that revision exists, MODE B stages nothing.

Every `$DISPATCH` (both modes) is followed by the evidence readback:
```sh
curl -s "$FTP/data/modbridge/state/e2e-status.jsonl" > "$EV/e2e-status.jsonl"
curl -s "$FTP/data/modbridge/state/records.txt"      > "$EV/records.txt"
tail -n <expected> "$EV/e2e-status.jsonl"             # compare verbatim below
```

MODE B helpers (run in `set -e` shell; any mismatch ABORTS the run):
```sh
cmd_put() {  # $1 = local command json; STOR only if no stale file, then verify readback sha
  curl -s "$FTP/$CMDFTP" -o /tmp/stale.chk && { echo "STALE e2e-cmd.json — DELE it, do not overwrite"; return 1; }
  curl -s -T "$1" "$FTP/$CMDFTP"
  a=$(shasum -a 256 "$1" | cut -d' ' -f1)
  b=$(curl -s "$FTP/$CMDFTP" | shasum -a 256 | cut -d' ' -f1)
  [ "$a" = "$b" ] || { echo "CMD READBACK MISMATCH $a != $b"; return 1; }
  echo "$a" >> "$EV/cmdfiles.log"                       # receipt: file sha per dispatch
}
cmd_gone() {  # consumption proof: original gone, .consumed present and hash-equal
  curl -s "$FTP/$CMDFTP" -o /tmp/gone.chk && { echo "NOT CONSUMED"; return 1; }
  curl -s "$FTP/$CMDFTPDONE" | shasum -a 256 | cut -d' ' -f1   # must equal last cmdfiles.log line
}
dispatch() { curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf"; }
```
Local command files are staged once under `$EV/cmd/` with the exact bytes
from `E2E-CMD-SCHEMA.md` §4: `stage0-records.json`, `stage1-register.json`,
`stage2a-prepare-archive.json`, `stage2b-1-register.json`,
`stage2b-2-register.json`, `stage2b-3-prepare.json`, `stage3-install.json`,
`stage4-activated.json`, `stage5-mods-started.json`, `stage6-1-remove.json`,
`stage6-2-remove.json`, `stage6-3-remove.json`, `recover.json`.

Rollback principle (unchanged from STEP-BY-STEP): this run owns ONLY
`/data/modbridge/**` and the new payload alias `MODBRIDGE_INST_E2E`; nothing
else is ever written or deleted. All DELEs below stay inside
`/data/modbridge/intake/`.

## Step P — stage the payload (once)

```sh
curl -s "$HTTP/list_payloads"                                   # G0 reachability, 200 expected
curl -s -X POST --data-binary @"$PL" \
     "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"      # upload ONCE (CONSOLE-LOG item 2)
curl -s "$HTTP/manage:check"                                    # ok/folder_exists/file_exists
curl -s "$FTP/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" -o /tmp/payload.readback
shasum -a 256 /tmp/payload.readback; stat -f %z /tmp/payload.readback
head -c 4 /tmp/payload.readback                                  # must be .ELF magic 7f 45 4c 46
```
Expected: 681528 bytes, sha256 `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436`,
ELF magic. Acceptance law is F-R3 size law + identity receipts
(STAGING-MANIFEST-D1REL.md) — a 1:1 re-read hash equal to the local file is
sufficient; a DIFFERENT sha with correct size/magic is a NO-GO (never
dispatch unverified bytes).
Rollback: none needed; the alias is ours and new. If upload failed, re-upload
to the SAME alias. Never overwrite any pre-existing payload name.

## Step D0 — G1 resolution + stage 0 baseline (one dispatch does both)

Stage the two corpus-independent probes first (G3 FTP-write gate):
```sh
curl -s -T "$WS/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip" \
     "$FTP/data/modbridge/intake/movspeed.zip"                   # STOR probe gate (PREREQS P1/G3)
curl -s "$FTP/data/modbridge/intake/movspeed.zip" | shasum -a 256
# -> 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad
```

MODE A: `dispatch` with `records` arguments per the G1 probe grammar; if
e2e-status gains the two stage-0 lines below, G1 is GO — stay in MODE A.
MODE B (config-entry revision staged):
```sh
cmd_put "$EV/cmd/stage0-records.json"
dispatch
cmd_gone
curl -s "$FTP/data/modbridge/state/e2e-status.jsonl"
```
Expected NEW lines (either mode) — this IS stage 0 complete:
```
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
(records command emits no status line; merged-chain zb-pak transcript lines
10-11). records.txt contains only the header `modforge-records	1`.
Interpretation: file absent + dispatch OK = payload-run/write failure, NOT an
argv verdict (argv-probe INITONCE /tmp lesson). Rollback: MODE B —
`curl -s -X DELE...`/DELETE of `/data/modbridge/intake/e2e-cmd.json*` if
left; delete `/data/modbridge/state` if created (we own the fresh tree).
No-GO: MODE B bare dispatch yields no status file twice ⇒ the staged payload
is not the config-entry revision — stop, re-check row #1 identity.

## Stage 1-owner — owner-placed archive registration (native download DEFERRED)

`movspeed.zip` is already staged + sha-verified (Step D0). Then:
```sh
cmd_put "$EV/cmd/stage1-register.json"      # MODE B; MODE A: register-mod argv per grammar
dispatch; cmd_gone
```
Expected NEW lines:
```
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
```
Expected records delta: new row `mod	movspeed	PPSA33286	downloaded	/data/modbridge/intake/movspeed.zip`
(tab-separated store format; `records` stdout per host probe:
`registered mod=movspeed title=PPSA33286 state=downloaded variants=0` —
stdout observability on console is dispatch-mechanism-dependent; records.txt
+ e2e-status are the durable truth, never claim unobserved stdout).
Rollback: DELE `/data/modbridge/intake/movspeed.zip` + any `e2e-cmd.json*`;
fresh-tree reset (delete `/data/modbridge/state`) is legitimate only before
stage 3 commits.

## Stage 2a — prepare-archive, auto-bridge expected

```sh
cmd_put "$EV/cmd/stage2a-prepare-archive.json"; dispatch; cmd_gone
```
Expected NEW lines (zb-pak merged transcript lines 4-8, verbatim):
```
{"event":"begin","cmd":"prepare-archive","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"auto-bundle composed mfb"}
{"event":"end","exit":0}
```
Expected records: movspeed row flips to `prepared` with bundle
`/data/modbridge/state/bundles/movspeed.mfb`; nothing under `/data/modbridge/roots` yet.
Honesty stops: missing the `auto-bundle composed mfb` line with exit 0 = the
archive had no recognized roles — STOP, do not install this mod, report
distinctly. Refusal path = `{"event":"end","exit":2}` as the LAST line
(F-R2 matched failed end; quarantine retained) — STOP, rollback below, never
replay the identical request.
Rollback: DELE quarantine `movspeed.q` + `state/bundles/movspeed.mfb` +
cmd files; records has no per-row delete verb — fresh-tree reset only before
stage 3, otherwise continue to stage 6 and end the run honestly.

## Stage 2b — bundle flow (three dispatches: register mm-skin, register mm-lua, prepare)

First stage + verify the fixtures:
```sh
curl -s -T "$WS/outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb" "$FTP/data/modbridge/intake/mm-skin.mfb"
curl -s -T "$WS/outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb"  "$FTP/data/modbridge/intake/mm-lua.mfb"
curl -s "$FTP/data/modbridge/intake/mm-skin.mfb" | shasum -a 256   # 7b9888b9…70af
curl -s "$FTP/data/modbridge/intake/mm-lua.mfb"  | shasum -a 256   # eefc774a…9ec
```
Then per file, in order: `stage2b-1-register.json`, `stage2b-2-register.json`,
`stage2b-3-prepare.json` — each with `cmd_put … ; dispatch ; cmd_gone`.
Expected NEW lines (6 total; rehearsal S2b pattern, merged-chain shapes):
```
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"prepare","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"end","exit":0}
```
Expected records: mm-skin and mm-lua rows `prepared` (register lands
`downloaded`, prepare flips both; their bundle columns =
`/data/modbridge/intake/mm-skin.mfb` / `mm-lua.mfb`); `records` stdout
`plan title=PPSA33286 mods=2` + `ready 2` [HOST-PROVEN]. Rollback: as 2a.

## Stage 3 — transactional install (all three mods)

Precondition: game NOT running (C4). Dispatch `stage3-install.json`.
Expected NEW lines (merged-chain shape; the middle status line is the F-R2
`r2-install-status-ok` terminal event — NEW vs the old STEP-BY-STEP, which
predates F-R2):
```
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}
{"event":"end","exit":0}
```
Expected records: movspeed/mm-skin/mm-lua rows `installed`; game row
`game	PPSA33286	unreal	<runtime-col>	1	<loadlist-sha256>`
(generation 1; loadlist sha is the only commit truth). Root
`/data/modbridge/roots/PPSA33286/` gains `.modforge-root`, `loadlist.txt`,
staged files; journal gone after commit. `records` stdout per host probe:
`installed title=PPSA33286 generation=1 mods=movspeed,mm-skin,mm-lua loadlist=<sha256>`
+ `runtime not-started running=0`.
Interrupted transaction (process died mid-install; last end line absent or
a journal remains): rerunning ANY command opens recovery first (mfs_open
semantics); explicit hard rollback = dispatch `recover.json`, whose expected
lines are (`recover` maps to its own stage in `stage_for_cmd`):
```
{"event":"begin","cmd":"recover","title":"","stage":"recovery"}
{"event":"end","exit":0}
```
(rollback probes `--cancel-at rename1` / `--crash-at staged` + `recover` all
pass in the host suites). If the game is running: STOP, do NOT retry live.
Rollback for a COMPLETED install is stage 6 itself.

## Stages 4-5 — runtime ack, SIMULATED ONLY (UE4SS + runtime-ack runtime deferred)

Four dispatches in order: `stage4-activated.json`, plain `records`
(stage0 file reused), `stage5-mods-started.json`, `records` again.
Expected NEW lines per simulate dispatch (×2):
```
{"event":"begin","cmd":"simulate-runtime","title":"PPSA33286","stage":"stage-4-5-runtime-ack"}
{"event":"end","exit":0}
```
Expected NEW lines per records dispatch (×2):
```
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
Expected records deltas: after `activated`, game row runtime column shows
activated with running=0; after `mods-started`, mods-started with running=1
(`records` stdout `runtime activated running=0` / `runtime mods-started
running=1` [HOST-PROVEN]). **This `running=1` is the simulated ack-file
proof and MUST be labeled SIMULATED in every report. It is NOT a real
runtime acceptance** (PREREQS P4 / READINESS §5).
Rollback: none (status file is removed with the root at stage 6).

## Stage 6 — uninstall (three dispatches, fixed order mm-lua → mm-skin → movspeed)

Dispatch `stage6-1-remove.json`, `stage6-2-remove.json`,
`stage6-3-remove.json` in order. Expected NEW lines per removal (×3 — note
NO status line: the remove commit shares the install say() with an empty
mods list, and `r2-install-status-ok` emits nothing when `mods_out` is empty;
verified verbatim in the merged-chain zb-pak transcript tail):
```
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"end","exit":0}
```
Expected records: generation advances 2, 3, 4; each removed mod drops to
`prepared` (bundle column restored to its intake/bundle path); after the last
removal the root is the empty managed set (`.modforge-root` + header-only
`loadlist.txt` remain; rehearsal S7), game row runtime column resets, final
`records` dispatch (stage0 file) adds the closing two baseline lines.
Rollback per removal: re-stage the removed mod's corpus + re-run its
register (stage 1-owner / 2b files) + prepare + install — distinct commands,
never a replay of the removal inverted.

## Step C — collect evidence (read-only)

```sh
cp "$EV/e2e-status.jsonl" "$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl"
cp "$EV/records.txt"      "$EV/MODBRIDGE_INST_E2E-records.txt"
curl -s "$FTP/data/modbridge/state/bundles/movspeed.mfb" | shasum -a 256   # on-console bridge artifact identity
```
Store under the controller's console dir with a receipt JSON: dispatch list
(mode per stage, cmd-file shas from `cmdfiles.log`), readback shas, per-stage
pass/fail against the verbatim blocks above, the SIMULATED label on stage
4-5, and the F-R2 notes (install success carries a terminal status line;
removals do not; controlled refusals close with a failed end).

## Full-tree reset (only if aborting before stage 3)

FTP DELE of every intake file this run staged:
`movspeed.zip`, `mm-skin.mfb`, `mm-lua.mfb`, `e2e-cmd.json`,
`e2e-cmd.json.consumed`, quarantine dirs `*.q` if created; then delete
`/data/modbridge/state` and `/data/modbridge/roots`. If FTP cannot remove
dirs, leave the dormant tree and RECORD it — never delete anything outside
`/data/modbridge/**`.

## Line-count ledger (all-green run)

New e2e-status lines per stage: D0/0: 2 · 1-owner: 2 · 2a: 4 · 2b: 6 · 3: 3
· 4-5: 8 · 6: 6 + closing records 2 ⇒ **33 total**, 0 unmatched begins, 0
failed ends. Any other count means a stage deviated: stop, collect, report
distinctly.
