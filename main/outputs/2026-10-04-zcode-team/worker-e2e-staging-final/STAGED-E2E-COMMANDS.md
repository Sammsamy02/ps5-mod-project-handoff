# STAGED-E2E-COMMANDS.md — literal controller sequence, config-file entry (MODE B), D1-fixed payload family

Origin: worker-e2e-staging-final, 2026-10-04. Implements
`worker-e2e-console-dry/CONSOLE-E2E-EXECUTION.md` (execution plan) and
`worker-e2e-console-dry/E2E-CMD-SCHEMA.md` (config-file entry design) as one
copy-paste, in-order command script. The 13 dispatch files are materialized
byte-exact under `cmd/` in this directory (sha receipts in `cmd/SHASUMS.txt`,
`cmd/SHASUMS.txt` files carry NO trailing newline, exactly the compact §4
bytes). Host proof: `host-test/HOST-TEST-RECEIPT.md` (OVERALL PASS).

## 0. READ FIRST — the payload gate (hard, non-negotiable)

Recomputed this session: the only existing payload is the D1-fixed merged-chain
build `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436`
(681528 B, 116 imports, D1 + F-R1 + F-R2). It contains **NO `e2e-cmd.json`
reader** (vendored-source grep negative; E2E-CMD-SCHEMA.md §Status; no
implementing revision exists anywhere under this team's outputs). A bare
dispatch of THAT build lands in `usage()`/`EX_BAD` by construction.

Therefore exactly one of two run configurations is lawful:

- **RUN-CONFIG A (today, payload e3489297…)**: MODE A / argv dispatch per the
  G1 probe grammar (`worker-argv-probe/REPORT.md`). Same stage order and
  expected lines as below; skip every `cmd_put`/`cmd_gone` line; the payload
  gate will resolve MODE=A automatically.
- **RUN-CONFIG B (config-file entry — the sequence below)**: requires the
  config-entry payload revision to EXIST and be re-pinned (new tuple IDs
  `g1-cmd-entry-*`, suites re-run, new build receipt + STAGING re-pin — schema
  §8). Set `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`/`681888` from THAT re-pin receipt.
  The gate refuses to run MODE B against any other bytes, and can never pass
  with `e3489297…`.

Never dispatch unverified bytes. Never widen the gate to make it pass.

## 1. Conventions (paste as one block; `set -e` — any mismatch aborts the run)

```sh
set -e
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
WS=/WORKSPACE/.codex/worktrees/translation-core/what
CMDDIR=$WS/outputs/2026-10-04-zcode-team/worker-e2e-staging-final/cmd
PL=$WS/outputs/2026-10-04-zcode-team/worker-d1-chain-relink/composition/build/console/modbridge-installer.payload.elf
EV=$WS/outputs/2026-10-04-zcode-team/worker-e2e-staging-final/staged-run-evidence   # controller-owned
CMDFTP=data/modbridge/intake/e2e-cmd.json
CMDFTPDONE=data/modbridge/intake/e2e-cmd.json.consumed
D1REL_SHA=e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436
D1REL_BYTES=681528
a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054=REPLACE_WITH_REPINNED_SHA_OF_CONFIG_ENTRY_REVISION   # RUN-CONFIG B only
681888=REPLACE_WITH_REPINNED_BYTES
mkdir -p "$EV/cmd"
```

MODE B helpers (verbatim from CONSOLE-E2E-EXECUTION.md §0):

```sh
cmd_put() {  # $1 = local command json; STOR only if no stale file, then verify readback sha
  curl -s "$FTP/$CMDFTP" -o /tmp/stale.chk && { echo "STALE e2e-cmd.json — DELE it, do not overwrite"; return 1; }
  curl -s -T "$1" "$FTP/$CMDFTP"
  a=$(shasum -a 256 "$1" | cut -d' ' -f1)
  b=$(curl -s "$FTP/$CMDFTP" | shasum -a 256 | cut -d' ' -f1)
  [ "$a" = "$b" ] || { echo "CMD READBACK MISMATCH $a != $b"; return 1; }
  echo "$a" >> "$EV/cmdfiles.log"
}
cmd_gone() {  # consumption proof: original gone, .consumed present and hash-equal
  curl -s "$FTP/$CMDFTP" -o /tmp/gone.chk && { echo "NOT CONSUMED"; return 1; }
  curl -s "$FTP/$CMDFTPDONE" | shasum -a 256 | cut -d' ' -f1   # must equal last cmdfiles.log line
}
dispatch() { curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf"; }
evid() {  # per-dispatch evidence readback
  curl -s "$FTP/data/modbridge/state/e2e-status.jsonl" > "$EV/e2e-status.jsonl"
  curl -s "$FTP/data/modbridge/state/records.txt"      > "$EV/records.txt"
  tail -n "${1:-2}" "$EV/e2e-status.jsonl"
}
```

## 2. Pre-flight — verify the staged cmd files (runs on the controller host)

```sh
( cd "$CMDDIR" && shasum -a 256 -c SHASUMS.txt )        # all 13 must say OK
cat "$CMDDIR/SHASUMS.txt" >> "$EV/cmdfiles.log"          # receipt: expected shas
```

Expected receipt (recomputed over the exact staged bytes):

```
a414d015e3d5bfd2f6614090e2940c19522c44e03c5ecde6e8adb1c81e2dcab1  recover.json
ef2e203663e002fa311995c37a6cd5ae5060cf1ff5b07f6b8a8adfed0f919ac3  stage0-records.json
a7be7864a7c44c0739b2afadf8944b421846eddcb6cb133ccd83040b222f97e3  stage1-register.json
77e7e03a801e75327ce856f6c9afda1635b8733cc5fe41666924231957f714b0  stage2a-prepare-archive.json
7c4f9d8045a8ad6979f848edbeb3d0b10352aaed5c27b7169d19a9755a296827  stage2b-1-register.json
140c6bbebcca323eccc36de363cc82fd5ff740d268f4baf5811fcf8ffb4a1c2a  stage2b-2-register.json
1ce4f601d71e31e74ba620761d7e1bea1af63f489de1204e1f30aa376cd4b509  stage2b-3-prepare.json
d7834dd599fb3f36c846756a49563ee1dc3206e214751ee774275d713b21af52  stage3-install.json
f09a87ba8648942aad16d25c1e53b2b5e1de0b42a62679837ec0fe8ef6fa42ef  stage4-activated.json
a63cf9c5ee5e4a667a49d9a983da949890d83538e50e91cf4e3ca4499ee3e8f5  stage5-mods-started.json
789474c8972d100ce862fa1560fd294b3c2f76a5b807fe60946f028086a6cb81  stage6-1-remove.json
d35255483210bdb919073ef0071c0dd41e27aec01611ead021046f6883026407  stage6-2-remove.json
780964191a76a047b8cd72d83f25c6397cb7f6f7f088ffabe5f002a2217ca009  stage6-3-remove.json
```

All 13 validated against the schema §2/§3/§5/§6 grammar host-side (14/14
refusal cases refused; see HOST-TEST-RECEIPT.md §A/§D).

## 3. Step P — stage the payload ONCE, then the identity gate picks the mode

```sh
curl -s "$HTTP/list_payloads"                                   # G0 reachability, 200 expected
curl -s -X POST --data-binary @"$PL" \
     "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"      # upload ONCE
curl -s "$HTTP/manage:check"
curl -s "$FTP/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" -o /tmp/payload.readback
RB_SHA=$(shasum -a 256 /tmp/payload.readback | cut -d' ' -f1)
RB_SZ=$(stat -f %z /tmp/payload.readback)
head -c 4 /tmp/payload.readback                                  # must be .ELF magic 7f 45 4c 46
if   [ "$RB_SHA" = "$a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054" ] && [ "$RB_SZ" = "$681888" ]; then MODE=B
elif [ "$RB_SHA" = "$D1REL_SHA" ] && [ "$RB_SZ" = "$D1REL_BYTES" ]; then MODE=A
else echo "PAYLOAD IDENTITY NO-GO: $RB_SHA $RB_SZ"; exit 1
fi
echo "MODE=$MODE"
```

- MODE=B: continue with §4 exactly as written (config-file entry).
- MODE=A: the payload is the D1-fixed argv-flavor build. Do NOT run the §4
  cmd-file blocks against it (bare dispatch = usage()/EX_BAD). Run the same
  stage order with argv dispatch per the G1 probe grammar; the expected-line
  blocks in §4 still apply verbatim. Stage 6 under MODE A: expect the SAME
  corrected lines (see §4.7 CORRECTION).
- Re-upload rule: if upload failed, re-upload to the SAME alias
  `MODBRIDGE_INST_E2E.elf`; never overwrite any pre-existing payload name.

## 4. The dispatch sequence (MODE B; in this exact order)

Rollback ownership (unchanged): this run owns ONLY `/data/modbridge/**` and the
payload alias `MODBRIDGE_INST_E2E`; every DELE below stays inside
`/data/modbridge/intake/`.

Expected exits (AMENDED 2026-10-04 by worker-e2e-promotion-apply, per
`worker-e2e-suite-completion/EXCONFIG6-MEMO.md` + `worker-e2e-console-dry/E2E-CMD-SCHEMA.md`
§6 AMENDMENT): a cmd file that is PRESENT but fails validation exits
**6 (`EX_CONFIG`)** with the file LEFT in place (inspect, then DELE; never
auto-retry); absent-file and verb-level refusals still exit 2 — the
`{"event":"end","exit":2}` = refusal-path STOP guidance below is unchanged.

### 4.0 Corpus-independent probes + stage 0 baseline

```sh
curl -s -T "$WS/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip" \
     "$FTP/data/modbridge/intake/movspeed.zip"                   # STOR probe gate (PREREQS P1/G3)
curl -s "$FTP/data/modbridge/intake/movspeed.zip" | shasum -a 256
# -> 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad  (else STOP)

cmd_put "$CMDDIR/stage0-records.json"
dispatch
cmd_gone        # printed hash must equal the last cmdfiles.log line
evid 2
```

Expected NEW lines (this IS stage 0 complete + the MODE B G1 verdict):
```
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
- records.txt contains only the header `modforge-records	1`.
- File absent + dispatch OK = payload-run/write failure, NOT a verdict.
- Two bare dispatches with no status file ⇒ staged payload is not the
  config-entry revision — STOP, re-check §3 identity (MODE=B can only be
  reached with the re-pinned sha, so this means corrupted staging).

### 4.1 Stage 1-owner — register the owner-placed archive

```sh
cmd_put "$CMDDIR/stage1-register.json"; dispatch; cmd_gone; evid 2
```
Expected NEW lines:
```
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
```
records delta: new row `mod	movspeed	PPSA33286	downloaded	/data/modbridge/intake/movspeed.zip`.

### 4.2 Stage 2a — prepare-archive, auto-bridge expected

```sh
cmd_put "$CMDDIR/stage2a-prepare-archive.json"; dispatch; cmd_gone; evid 4
```
Expected NEW lines (zb-pak merged-chain lines 4-8, verbatim):
```
{"event":"begin","cmd":"prepare-archive","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"auto-bundle composed mfb"}
{"event":"end","exit":0}
```
- Missing `auto-bundle composed mfb` with exit 0 = no recognized roles — STOP,
  do not install, report distinctly.
- Last line `{"event":"end","exit":2}` = refusal path — STOP, rollback
  (DELE `movspeed.q` + `state/bundles/movspeed.mfb` + cmd files), never replay
  the identical request.

### 4.3 Stage 2b — bundle flow (three dispatches)

```sh
curl -s -T "$WS/outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb" "$FTP/data/modbridge/intake/mm-skin.mfb"
curl -s -T "$WS/outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb"  "$FTP/data/modbridge/intake/mm-lua.mfb"
curl -s "$FTP/data/modbridge/intake/mm-skin.mfb" | shasum -a 256   # 7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af
curl -s "$FTP/data/modbridge/intake/mm-lua.mfb"  | shasum -a 256   # eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec

cmd_put "$CMDDIR/stage2b-1-register.json"; dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage2b-2-register.json"; dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage2b-3-prepare.json";  dispatch; cmd_gone; evid 2
```
Expected NEW lines (6 total):
```
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"prepare","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"end","exit":0}
```
records: mm-skin/mm-lua rows `prepared`, bundle columns at their intake paths.

### 4.4 Stage 3 — transactional install (precondition: game NOT running, C4)

```sh
cmd_put "$CMDDIR/stage3-install.json"; dispatch; cmd_gone; evid 3
```
Expected NEW lines:
```
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}
{"event":"end","exit":0}
```
records: three rows `installed`; game row generation 1 with loadlist sha (the
only commit truth); root `/data/modbridge/roots/PPSA33286/` gains
`.modforge-root`, `loadlist.txt`, staged files; journal gone after commit.
If interrupted (last end line absent or journal remains): dispatch
`cmd_put "$CMDDIR/recover.json"; dispatch; cmd_gone; evid 2` and expect
```
{"event":"begin","cmd":"recover","title":"","stage":"recovery"}
{"event":"end","exit":0}
```
If the game is running: STOP, do NOT retry live. Rollback for a COMPLETED
install is stage 6 itself.

### 4.5 Stages 4-5 — runtime ack, SIMULATED ONLY (label every report SIMULATED)

```sh
cmd_put "$CMDDIR/stage4-activated.json"; dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage0-records.json";   dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage5-mods-started.json"; dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage0-records.json";   dispatch; cmd_gone; evid 2
```
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
records deltas: `activated`/running=0, then `mods-started`/running=1.
**SIMULATED ack-file proof — NOT a real runtime acceptance.**

### 4.6 Stage 6 — uninstall (three dispatches, fixed order mm-lua → mm-skin → movspeed)

```sh
cmd_put "$CMDDIR/stage6-1-remove.json"; dispatch; cmd_gone; evid 3
cmd_put "$CMDDIR/stage6-2-remove.json"; dispatch; cmd_gone; evid 3
cmd_put "$CMDDIR/stage6-3-remove.json"; dispatch; cmd_gone; evid 2
cmd_put "$CMDDIR/stage0-records.json";  dispatch; cmd_gone; evid 2   # closing baseline
```
Expected NEW lines — **CORRECTION vs CONSOLE-E2E-EXECUTION.md §Stage 6**:
the doc claims all three removals emit NO status line. That over-generalized
the merged-chain zb-pak tail, whose single removal emptied the mod set (silent
because `mods_out` was empty — zb-pak e2e-status.jsonl line 17). The same
merged chain, replayed on host with THIS exact dispatch set
(host-test/scratch/a-config-synth/state/e2e-status.jsonl, sha `31a4764a…`,
35/35 verbatim), shows a removal emits the F-R2 status line WHILE MODS REMAIN,
with detail = remaining mods in registration order; only the final removal is
silent. Corrected expectation:
```
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin"}
{"event":"end","exit":0}
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed"}
{"event":"end","exit":0}
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"end","exit":0}
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
records: generations advance 2, 3, 4; each removed mod drops to `prepared`;
after the last removal the root is the empty managed set (`.modforge-root` +
header-only `loadlist.txt`), game row runtime resets.
Deviations: a MISSING status line on 6-1/6-2 or an unexpected detail
(must be exactly the remaining mods, in order) = STOP, collect, report
distinctly.

### 4.7 Line-count ledger (all-green run, CORRECTED)

New e2e-status lines per stage: D0/0: 2 · 1-owner: 2 · 2a: 4 · 2b: 6 · 3: 3 ·
4-5: 8 · 6: 8 (6 begins/ends + 2 removal status lines) · closing records: 2 ⇒
**35 total**, 0 unmatched begins, 0 failed ends. (The dry doc's 33 predates
this correction.) Any other count: stop, collect, report distinctly.

## 5. Step C — collect evidence (read-only)

```sh
cp "$EV/e2e-status.jsonl" "$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl"
cp "$EV/records.txt"      "$EV/MODBRIDGE_INST_E2E-records.txt"
curl -s "$FTP/data/modbridge/state/bundles/movspeed.mfb" | shasum -a 256   # on-console bridge artifact identity
shasum -a 256 "$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl"                    # receipt
cat "$EV/cmdfiles.log"
```

Store next to them a receipt JSON: dispatch list (MODE per stage, cmd-file sha
per dispatch from `cmdfiles.log`), payload sha/bytes, readback shas, per-stage
pass/fail vs the verbatim blocks in §4, the SIMULATED label on 4-5, the
stage-6 CORRECTION reference, and the F-R2 notes (install success carries a
terminal status line; removals carry one only while mods remain; controlled
refusals close with a failed end and the cmd file left in place for inspection).

## 6. Full-tree reset (only if aborting BEFORE stage 3 commits)

```sh
curl -s -X DELE "$FTP/data/modbridge/intake/movspeed.zip"
curl -s -X DELE "$FTP/data/modbridge/intake/mm-skin.mfb"
curl -s -X DELE "$FTP/data/modbridge/intake/mm-lua.mfb"
curl -s -X DELE "$FTP/$CMDFTP" ; curl -s -X DELE "$FTP/$CMDFTPDONE"
# then delete /data/modbridge/state and /data/modbridge/roots (we own the fresh tree)
```
Never delete anything outside `/data/modbridge/**`; if FTP cannot remove dirs,
leave the dormant tree and RECORD it.

## 7. What is already proven host-side vs what stays [TARGET]

Proven on host this session (HOST-TEST-RECEIPT.md, OVERALL PASS):
all 13 staged files inside the schema grammar; the exact synthesized argv
vectors drive the real host binary (sha `8dffe3cf…`) through the full stage
0→6 sequence with exit 0 everywhere and a 35-line verbatim transcript (sha
`31a4764a…`); each dispatch byte-identical to its hand-written argv twin;
recover shape `begin/end` on a clean tree; 14/14 refusal-matrix cases refuse
with no state change.

Still [TARGET] until the console run: on-console STOR/readback/consumed-rename
behavior, console filesystem timings, and every expected line above on the
console payload — which requires the re-pinned config-entry revision for MODE
B (RUN-CONFIG A with `e3489297…` uses the same expected lines via argv).
