# CONSOLE-STAGE-PLAN — MODBRIDGE_INST_E2E.elf (config-entry payload, MODE B)

Prepared by worker-e2e-console-stage-prep, 2026-10-04. NO console contact was
made by this worker; every command below is controller work, frozen to be
zero-decision. Sources: `worker-e2e-payload-build/PAYLOAD-DELIVERY.md` (payload
pin + Step P), `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` (the 13 cmd
files + gate, applied verbatim), `worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md`
sec 9 (re-pin), plus the payload-upload precedents (ALLFAM_DLL_TEST plan,
6d295f56/1a6b37b7 receipts). Expected shapes live in `EXPECTED-LEDGER.md`
(same dir); the failure/recovery table is its §F.

## 0. Pinned identity (verify locally BEFORE touching the console)

| item | value |
|---|---|
| artifact | `outputs/2026-10-04-zcode-team/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf` |
| sha256 | `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054` (RE-VERIFIED locally this session, matches PAYLOAD-DELIVERY) |
| bytes | `681888` (re-verified) |
| ELF magic | `7f 45 4c 46` (re-verified) |
| upload alias | `MODBRIDGE_INST_E2E.elf` (ONCE; re-upload only to the SAME alias) |
| report path | `/data/modbridge/state/e2e-status.jsonl` |
| cmd files | `worker-e2e-staging-final/cmd/` — 13 files, `shasum -a 256 -c SHASUMS.txt` re-verified 13/13 OK this session |
| run inputs | movement-speed-2x.zip `3850a2b5…`, mm-skin.mfb `7b9888b9…`, mm-lua.mfb `eefc774a…` — all three re-hashed this session, match STAGED §4.0/§4.3 |

Build-id law: relinks of identical sources differ in the 16-byte
`.note.gnu.build-id`. NEVER substitute a relink (r2 `ec8acdfc…` is proof-of-
determinism, not the artifact) and NEVER re-derive the sha. If the local file
hashes to anything but `a21771f8…` / 681888: STOP, no console step runs.

Readback law (verified from precedent, not assumed): payload ELFs staged via
`manage:upload` are stored VERBATIM under `/data/pldmgr/payloads/<NAME>/` —
6d295f56 read back byte-identical; 1a6b37b7 recorded
`upload.ftp_readback_sha256 == artifact_sha256`, `attempts: 1`; ALLFAM_DLL_TEST
froze the same POST-upload → `/manage:check` → FTP readback sha gate. This ELF
is a plain dynamically-linked payload (not fself, not a packaged PRX), so the
PRX packaging SHT-strip law (section headers stripped, `e_shnum=0` — a
PRX/fself readback can never equal pre-package bytes) does NOT apply.
Therefore: readback sha MUST equal `a21771f8…` EXACTLY and size 681888. Any
mismatch = corrupted staging; STOP.

### Port erratum (ack before running)

PAYLOAD-DELIVERY.md §3 writes `HTTP=http://ps5-host-not-configured.invalid:9223`. Every proven
source says the payload manager is on **8084**: `console-context.json`
(`manager_version_endpoint: http://ps5-host-not-configured.invalid:8084/version` → 200, "0.5.2"),
STAGED-E2E-COMMANDS.md §1, the prior CONSOLE-STAGE-PLAN, and all controller
receipts (no receipt or doc outside PAYLOAD-DELIVERY ever used 9223; the only
"9223" hits in the tree are sha-substring coincidences). This plan pins 8084.
Do not send anything to 9223; if 8084 does not answer, stop — do not probe
alternate ports.

## 1. Sequencing vs the UE4SS critical path (recommendation)

**Run THIS stage first, in payload-only state: game closed, manager 8084 up,
no ploader arm, no PRX-slot interaction.** Same rule as ALLFAM_DLL_TEST:
"all four family payload runs executed via pldmgr /loadpayload with
game_launched: false … the payload route needs no game and no ploader arm."

- Current critical path state: composition-5 (UE4SS) is still pre-console —
  worker-composition5-gate delivered an OFFLINE gate (negative control NO-GO on
  comp4), worker-anchor-host-test just returned GREEN for the window-anchor
  bridge logic; the launch session (ploader + PRX slot + game running,
  CONSOLE-LOG steps 10-12) has not happened. The console is therefore expected
  to be idle/payload-only now — the correct window for this E2E stage.
- Why before, not after: the E2E stage-3 install commits a managed mod set for
  PPSA33286 under `/data/modbridge/**`. If a UE4SS/bridge session ran between
  stage 3 and stage 6, the live runtime bridge could observe the staged mods —
  an uncontrolled variable for BOTH tests. The stage-6 uninstall returns the
  tree to the empty managed set, and the final reset (§7) removes the state
  tree entirely, so afterwards the UE4SS session starts from the same
  pre-E2E state.
- Conflict rules:
  - If Townfall (or any game) is running: DO NOT run stage 3 (its precondition
    is explicitly "game NOT running, C4"). Defer the whole stage; do not run a
    partial sequence (stages 0-2 leave registrations behind).
  - If the console is inside the composition-5/UE4SS session when this stage
    is scheduled: defer until that session ends and the console is back to
    payload-only. Never interleave the two dispatch mechanisms.
  - Stage 4-5 acks are SIMULATED (cmd-file driven), NOT a real runtime
    acceptance; they neither require nor substitute for the UE4SS session.
- Rollback ownership (unchanged from STAGED §4): this run owns ONLY
  `/data/modbridge/**` and the payload alias `MODBRIDGE_INST_E2E`. Every DELE
  stays inside `/data/modbridge/intake/` plus the owned `state/`+`roots/`
  trees. No game files, no autoload changes, no `/data/plugins/` writes, no
  other payload alias touched.

## 2. Conventions block (paste as one; `set -e` — any mismatch aborts)

STAGED §1 with the two Step P lines substituted (CONFIG-ENTRY-DESIGN sec 9 /
PAYLOAD-DELIVERY §2) and `$PL` overridden to the revised artifact (the ONLY
other change):

```sh
set -e
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
WS=/WORKSPACE/.codex/worktrees/translation-core/what
CMDDIR=$WS/outputs/2026-10-04-zcode-team/worker-e2e-staging-final/cmd
PL=$WS/outputs/2026-10-04-zcode-team/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf
EV=$WS/outputs/2026-10-04-zcode-team/worker-e2e-console-stage-prep/staged-run-evidence   # controller-owned
CMDFTP=data/modbridge/intake/e2e-cmd.json
CMDFTPDONE=data/modbridge/intake/e2e-cmd.json.consumed
D1REL_SHA=e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436
D1REL_BYTES=681528
CONFIG_ENTRY_SHA=a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054
CONFIG_ENTRY_BYTES=681888
mkdir -p "$EV/cmd"
```

MODE B helpers — verbatim from STAGED §1 (`cmd_put`, `cmd_gone`, `dispatch`,
`evid`); copy them unchanged from that doc, including the stale-file guard in
`cmd_put` (STOR only if no stale `e2e-cmd.json`, then readback-sha verify) and
the `cmd_gone` consumed-hash check (must equal the last `cmdfiles.log` line).

## 3. Pre-flight (controller host, no console)

```sh
( cd "$CMDDIR" && shasum -a 256 -c SHASUMS.txt )        # 13/13 OK, else STOP
shasum -a 256 "$PL"                                      # a21771f8…, else STOP
stat -f %z "$PL"                                         # 681888, else STOP
head -c 4 "$PL" | xxd                                    # 7f 45 4c 46, else STOP
# expected receipt into evidence:
cat "$CMDDIR/SHASUMS.txt" >> "$EV/cmdfiles.log"
```

The 13 SHASUMS values are reproduced in STAGED §2; they were re-verified this
session. All 13 files are schema-validated host-side (HOST-TEST-RECEIPT:
14/14 refusal cases refused).

## 4. Step P — upload ONCE, then identity gate (mode resolution)

```sh
curl -s "$HTTP/list_payloads"                                   # G0 reachability, 200 expected; else STOP
curl -s -X POST --data-binary @"$PL" \
     "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"      # upload ONCE, expect OK
curl -s "$HTTP/manage:check"                                    # ok / folder_exists / file_exists for the new entry
curl -s "$FTP/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" -o /tmp/payload.readback
RB_SHA=$(shasum -a 256 /tmp/payload.readback | cut -d' ' -f1)
RB_SZ=$(stat -f %z /tmp/payload.readback)
head -c 4 /tmp/payload.readback                                 # 7f 45 4c 46
if   [ "$RB_SHA" = "$CONFIG_ENTRY_SHA" ] && [ "$RB_SZ" = "$CONFIG_ENTRY_BYTES" ]; then MODE=B
elif [ "$RB_SHA" = "$D1REL_SHA" ] && [ "$RB_SZ" = "$D1REL_BYTES" ]; then MODE=A
else echo "PAYLOAD IDENTITY NO-GO: $RB_SHA $RB_SZ"; exit 1
fi
echo "MODE=$MODE"
```

Expected outcome with the staged bytes: **MODE=B** (readback is byte-verbatim
per §0's readback law). MODE=A would mean someone uploaded the old D1 payload
instead — the gate refuses MODE B and no cmd file may run against it. NO-GO
= STOP, do NOT fall through to dispatching cmd files against unverified
bytes; never widen the gate. Re-upload rule: only to the SAME alias
`MODBRIDGE_INST_E2E.elf`, never overwrite any pre-existing payload name.

## 5. Dispatch sequence (MODE B; exact order; per-dispatch cycle is
`cmd_put → dispatch → cmd_gone → evid N`)

Dispatch URL (frozen helper): `GET $HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf`.
Exactly one dispatch per stage file. Never re-send on ambiguity — reconcile
first (`cmd_gone` + `evid` tell you whether the dispatch consumed and ran).

All expected NEW lines per stage, the consumed-twin proof, and the records.txt
deltas are frozen in `EXPECTED-LEDGER.md` §S0–S6 and §L (same dir) — they are
the STAGED §4 blocks verbatim (host-proven 35/35 transcript `31a4764a…`), with
MODE B additions: per dispatch, `data/modbridge/intake/e2e-cmd.json` must be
GONE and `e2e-cmd.json.consumed` present and hash-equal to the staged bytes
(`cmd_gone`), and a green run contains ZERO `stage:"e2e-cmd"` lines.

Order (do not reorder, do not skip the interleaved records baselines):

1. **§4.0 probe + stage 0** — STOR `movspeed.zip`, FTP readback sha
   `3850a2b5…` (else STOP); then `stage0-records.json`. Expect 2 new lines
   (begin records / end 0) — this is also the MODE B G1 verdict: the config
   entry proving itself with the re-pinned payload.
2. **Stage 1** — `stage1-register.json` (2 lines; records gains movspeed row
   `downloaded`).
3. **Stage 2a** — `stage2a-prepare-archive.json` (4 lines incl. both `status
   ok` lines; missing `auto-bundle composed mfb` with exit 0 = STOP).
4. **Stage 2b** — STOR `mm-skin.mfb` + `mm-lua.mfb` (readback shas
   `7b9888b9…` / `eefc774a…`, else STOP); then 2b-1 register, 2b-2 register,
   2b-3 prepare (6 lines total).
5. **Stage 3** — `stage3-install.json` (3 lines; precondition game NOT
   running). Commit truth = records generation 1 + loadlist sha. Interrupted
   transaction (last end absent or journal remains) → the ONE permitted
   recovery dispatch: `recover.json` (expect begin recover / end 0). If the
   game was running: STOP, never retry live. If install COMPLETED, rollback
   is stage 6 itself — not a reset.
6. **Stages 4-5 (SIMULATED)** — `stage4-activated.json`, `stage0-records.json`,
   `stage5-mods-started.json`, `stage0-records.json` (8 lines; label every
   report SIMULATED — ack-file proof, NOT runtime acceptance).
7. **Stage 6 (fixed order mm-lua → mm-skin → movspeed)** — `stage6-1-remove`,
   `stage6-2-remove`, `stage6-3-remove`, closing `stage0-records.json`
   (8 + 2 lines per the CORRECTION: removals DO emit the F-R2 status line
   while mods remain, detail = remaining mods in registration order; only the
   final removal is silent). Generations advance 2, 3, 4; after the last
   removal the root is the empty managed set.
8. Any deviation: STOP, collect, report distinctly. No identical-request
   replay, no re-rating unchanged evidence.

Line-count ledger, all-green run: **35 new e2e-status lines total**
(2+2+4+6+3+8+8+2), 0 unmatched begins, 0 failed ends, 0 `e2e-cmd` refusal
lines. Any other count: stop, collect, report.

## 6. Step C — evidence collection (read-only)

```sh
cp "$EV/e2e-status.jsonl" "$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl"
cp "$EV/records.txt"      "$EV/MODBRIDGE_INST_E2E-records.txt"
curl -s "$FTP/data/modbridge/state/bundles/movspeed.mfb" | shasum -a 256
shasum -a 256 "$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl"
cat "$EV/cmdfiles.log"
```

Receipt JSON (controller writes, in `$EV`): dispatch list (MODE per stage,
cmd-file sha per dispatch from cmdfiles.log), payload sha/bytes + readback
sha/bytes, per-stage pass/fail vs EXPECTED-LEDGER.md, the SIMULATED label on
4-5, the stage-6 CORRECTION reference, the port-8084 erratum ack, and the F-R2
notes (install success carries a terminal status line; removals carry one only
while mods remain; refusals close EX_CONFIG 6 with the cmd file left in place
— the controller-acked delta vs schema §6/§8 EX_BAD).

## 7. Full-tree reset at the end (this run owns `/data/modbridge/**`)

After stage 6 + Step C pass (or on any abort BEFORE stage 3 commits):

```sh
curl -s -X DELE "$FTP/data/modbridge/intake/movspeed.zip"     # if still present (2a moved it to movspeed.q on success)
curl -s -X DELE "$FTP/data/modbridge/intake/movspeed.q"       # stage-2a quarantine leftover, if present
curl -s -X DELE "$FTP/data/modbridge/intake/mm-skin.mfb"
curl -s -X DELE "$FTP/data/modbridge/intake/mm-lua.mfb"
curl -s -X DELE "$FTP/$CMDFTP" ; curl -s -X DELE "$FTP/$CMDFTPDONE"
# then delete /data/modbridge/state and /data/modbridge/roots (we own the fresh tree)
```

Rules (unchanged from STAGED §6): never delete anything outside
`/data/modbridge/**`; if FTP cannot remove directories, leave the dormant tree
and RECORD it in the receipt. After a green run + reset, the console is back
to the pre-E2E payload-only state — exactly what the UE4SS critical-path
session should find. The payload alias `MODBRIDGE_INST_E2E` stays (history;
never overwrite it later).

## 8. What remains [TARGET]

Proven host-side: 13/13 cmd-file shas, 14/14 refusal matrix, 35/35 verbatim
synthetic transcript, config-entry 32-check host receipt transferred to this
source revision by byte-exact host rebuild. Still [TARGET] until the controller
run: on-console STOR/readback/consumed-rename behavior, console timings, every
expected line on the console payload, and the 116-import census observed on
target (build-side census is 116/missing=[] by name-set identity).
