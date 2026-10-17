# ROLLBACK-RUNBOOK — ploader slot /data/plugins/modbridge_supplier_composition.prx

Worker: worker-rollback-runbook, 2026-10-04. Console: ps5-host-not-configured.invalid (Townfall
= PPSA33286). Authority: user-authorized reversible mod ops on console; game
files need backups. This runbook is documentation only — every command below
is drafted from controller evidence and the pinned ploader source; none has
been executed by this worker. Do not contact the console until an incident
actually calls for it.

Sources of record (all read in full for this runbook):

- `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` — steps 1–15 (slot
  lineage, staging law, backup events).
- Checkpoint `zcode_sdg2_staged` (transport-ledger.composition5-mirror.json):
  slot staged with sdg2, verified, `ploader_ini unchanged (slot already =2;
  aligned_entry + path_test also =2)`. Checkpoint
  `zcode_composition_5_gate_go_staged` is referenced by the controller's live
  ledger; the local mirror carries its underlying evidence directly:
  CONSOLE-LOG step 15 (gate ALL 8 PASS → staged, readback verified) +
  `worker-composition5-gate/gate-receipt-composition5-r2.json` (verdict GO).
- Pinned ploader source:
  `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`.
- Policy: `outputs/ps5-mod-bridge/EXPERIMENTS.md` (promotion-boundary review,
  cleanup/rollback required, never control the user's FTP service).
- Staging command precedents: `worker-final-staging-rehearsal/REHEARSAL-RESULT.md`
  §3/§6, `worker-prx-packaging/STAGING-CHECKLIST.md` §2–§4,
  `worker-preflight/REPORT.md` (live ini bytes caveat).

---

## 0. Endpoints, inventory, and console state at time of writing

```sh
HTTP=http://ps5-host-not-configured.invalid:8084        # payload manager (reachability + dispatch)
FTP=ftp://ps5-host-not-configured.invalid:2121          # anonymous FTP (STOR/RETR/DELE/RNFR/RNTO/MDTM)
SLOT=/data/plugins/modbridge_supplier_composition.prx
R=/data/modbridge/roots/PPSA33286    # activation root (runtime-status.txt lives here)
W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
C=$W/outputs/2026-10-04-zcode-controller/console
```

Reachability check (always the first and only probe after any incident):
`curl -s "$HTTP/list_payloads"` → expect 200 + catalog.

| Artifact | Where (local, verified this session) | Bytes | sha256 |
|---|---|---|---|
| composition-5 fself (CURRENT slot content as staged) | `$T/worker-composition-5-rebuild/prx-candidate-composition5-2/modbridge_supplier_composition.prx` (determinism twin `-1` byte-identical) | 30786775 | `04869ab3229bffe1991de3e3a6cd4f53ac03958d3a3ac1b96f9f45309373f1bf` |
| composition-4 staged fself (original upload) | `$T/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx` | 30778567 | `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f` |
| composition-4 console READBACK backup | `$C/composition4-console-backup.prx` (CONSOLE-LOG step 11) | 30769672 | `d4fc0acf6a3ab8df9e8e9882a273d601c614591bc8d23111adf20a4acd48ba39` |
| sdg2 staged fself | `$C/pathtest-sdg2-run/sdg2-staged-fself.prx` | 22087 | `0e5b36bc03d97619990a061baafd077fb331d3c0fd50249488aaaace50d32d86` |
| sdg2 console READBACK backup (controller copy) | `/tmp/sdg2-final-backup.prx` — byte-identical to `$C/pathtest-sdg2-run/sdg2-readback.prx` | 81368 | `182f5d3b48611e94155c16ff4eb3169b456fa8474b6049432bccf35f61bc8a0d` |

Known console state (CONSOLE-LOG steps 11–15 + task context): slot holds
composition-5 (gate GO, 8/8; determinism pair 04869ab3…; imports 196 exact);
live alongside it: `modbridge_runtime_aligned_entry.prx` (=2, console-proven,
logs `/data/townfall_runtime_aligned_entry.log`), `modbridge_path_test.prx`
(=2), `modbridge_ingame_facts.prx` (=0, disabled). ploader.ini unchanged
through steps 12–15.

Discrepancy to reconcile at incident time (do not silently "fix"): CONSOLE-LOG
step 15 logs the staged composition-5 fself as 30786663 B; every local
composition-5 candidate is 30786775 B and no 30786663-byte file exists locally.
The console RETR (fresh connection) is the only ground truth for slot bytes;
`04869ab3…` is the uploaded-artifact sha, and 30786663 is plausibly the
console-side readback size (see §1). Record, do not resolve by re-upload.

---

## 1. The SHT-strip readback law (prevents false alarms)

 ftpsrv on this console transforms fself `.prx` content served over RETR.
Verified facts (CONSOLE-LOG steps 11–12, sdg2 artifacts):

- UPLOAD of sdg2 fself: 22087 B, sha `0e5b36bc…`.
- FRESH-CONNECTION READBACK of the same slot: 81368 B, sha `182f5d3b…` — the
  decrypted/decompressed inner ELF with section headers stripped. Documented
  artifact classes, and ONLY these may differ: `e_shnum=0` at offset 60,
  last 24 bytes zeroed, 1560 B tail (sdg2 case).
- Acceptance rule used at every verified staging (steps 12 and 15): all
  PT_LOAD segments of the readback must be byte-identical to the
  payload-of-record inner ELF; SHT-strip artifacts are expected and allowed.

Therefore:

- NEVER verify an fself slot by whole-file sha against the uploaded artifact —
  it will never match. False alarm, and blind re-upload churn follows.
- PLAIN files read back byte-exact: `ploader.ini`, `.log`, `.txt`, `.jsonl`.
  Whole-file sha256 IS the verification for those.
- A readback artifact (e.g. `composition4-console-backup.prx`) is already the
  console-side transformed view; re-RETR of unchanged content must reproduce
  it byte-exactly, making it a clean expected-readback oracle.

"Fresh-connection readback" = a NEW FTP session for the RETR (each `curl` is a
new session). Never trust the upload connection's own echo.

---

## 2. ploader mechanics (evidence: pinned `original/main.c`)

Facts needed for rollback decisions, each pinned to the source:

| Behavior | Evidence in main.c |
|---|---|
| ini read ONCE at arm time, path `/data/plugins/ploader.ini` | `DEFAULT_INI_PATH` (L25), `read_ini(DEFAULT_INI_PATH)` in `main` (L839) |
| mode values: `0 = disabled`, `1 = early`, `2 = late`; inline comments after value ignored | `parse_mode` (L600–610) |
| mode 0 (and invalid −1) entries are SKIPPED at parse — never registered, never attached, never injected | `read_ini`: `if (mode <= 0) continue;` (L672) |
| mode 0 is SKIP, not inject-early; early is mode 1 | `get_mode_params` (L471–476): mode 1 → early timings, else late |
| missing ini file → clean exit, nothing injects, notification "Config empty" | `read_ini` fopen fail (L652–653) → `g_plugin_count == 0` branch (L840–844) |
| malformed lines safely skipped (no `=`, bad mode, path >255 chars, contains `..`) | `read_ini` (L667–676), `normalize_path` (L613–623) |
| missing/renamed `.prx`: entry IS registered → ploader waits (mode 2 up to 25 s), PT_ATTACHes the game, saves + jacks ucred (jb_pid), then `access(path, R_OK)` fails → notification "[TITLE] FAIL", restores creds, detaches; entry never marked injected; injector keeps polling until the 120 s timeout ("Timeout, shutting down.") | `run_plugins` (L776–822), `inject_prx` access check (L424–425), `INJECTOR_TIMEOUT_S` (L735) |
| per-launch re-send: mode-2 injection is done by the armed ploader for THAT launch only | lifecycle + `STAGING-CHECKLIST.md` §4 (runbook line 77) |
| stale lock recovered; duplicate arm exits quietly | `lock_acquire` (L690–713) |

Launch ritual this console actually uses (REHEARSAL-RESULT §4, CONSOLE-LOG
step 12): game fully CLOSED → user arms ploader (Payload Manager UI or
`GET $HTTP/loadpayload:...`) → user launches Townfall within the injector
window → collect logs ~2 min later. Consequence: an ini edit only takes effect
at the NEXT arm — edit while the game is closed, before re-arming.

---

## 3. SEVERITY A — composition-5 loaded but the RUNTIME failed; game runs fine

Trigger: `runtime-status.txt` shows a `failed <code> <detail>` line and/or
UE4SS.log carries a composition-5 failure line (the entry writes three-channel
failure evidence and is fail-open BY DESIGN: `module_start` returns 0 on any
wiring/activation failure → kernel continues → game runs unmodded).

DO NOTHING to the console. This is the designed outcome of a wiring failure,
not a fault state. No re-upload, no ini change, no dispatch, no "retry" —
resubmitting identical bytes is prohibited.

Collect evidence ONCE (plain files — readback law does not apply):

```sh
TAG=$(date +%Y%m%d-%H%M%S); mkdir -p "$C/comp5-failure-$TAG"
curl -s -Q "MDTM $R/runtime-status.txt" "$FTP/"                     # freshness
curl -s "$R/runtime-status.txt" -o "$C/comp5-failure-$TAG/runtime-status.txt" \
  || echo "runtime-status.txt absent"
curl -s "$FTP/data/plugins/UE4SS.log" -o "$C/comp5-failure-$TAG/UE4SS.log"
curl -s "$FTP/data/townfall_runtime_aligned_entry.log" -o "$C/comp5-failure-$TAG/aligned.log" || true
curl -s "$FTP$SLOT" | shasum -a 256                                  # slot readback sha (law-aware)
shasum -a 256 "$C/comp5-failure-$TAG"/*
```

Then decide the NEXT REVISION offline: distinct build ID, distinct gate run,
distinct staging log — never re-rate unchanged evidence. Old logs are not
freshness-reset candidates here; the freshness reset (DELE) belongs to the
staging procedure, not to failure triage.

---

## 4. SEVERITY B — game unstable / frozen AFTER composition-5 started (tick detour suspected)

### 4.1 User action first

The user closes Townfall from home (do **not** rely on the mod to unfreeze).
The tick detour lives in the game process only; closing the game fully removes
it from memory. Nothing on console persists the detour. Keep the console up;
do not touch payloads while the game is in a bad state.

### 4.2 Disable mechanism — DECIDED: ploader.ini value edit (2 → 0), not rename

Evidence (pinned main.c, §2 above):

- **ini edit `= 0`**: `read_ini` skips the entry at parse (L672). The plugin is
  never registered → ploader never PT_ATTACHes the game for it, no FAIL
  notification, no 120 s timeout wait; remaining plugins inject and ploader
  exits cleanly when done (`all_injected`). One byte changes (`2` → `0`), the
  `.prx` file on disk is untouched, and flipping `0` back to `2` restores.
- **rename / missing file**: the entry still registers and ploader runs the
  full failure path on EVERY subsequent launch — up-to-25 s mode-2 wait,
  PT_ATTACH + ucred save/jack (`jb_pid`) against a game that may already be
  unstable, FAIL notification, then polling to the 120 s timeout. It also
  leaves the ini claiming `=2`, which corrupts future audits. Use only if FTP
  STOR to ploader.ini is impossible.

### 4.3 Exact verified-safe ploader.ini layouts

Parser-verified shapes (pinned §2; STAGING-CHECKLIST §2; REHEARSAL §3/C11):
`[PPSA33286]` section header; entries are `full/path = mode`; any order;
`0`/`1`/`2` modes; `;` or `#` comments; blank lines fine; a path without `/`
gets `/data/plugins/` prefixed; inline comments after the mode are safe.

Expected current content (from checkpoint `zcode_sdg2_staged` — slot =2,
aligned_entry + path_test also =2 — plus task context ingame_facts =0):

```ini
[PPSA33286]
/data/plugins/modbridge_runtime_aligned_entry.prx = 2
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
```

HONESTY CAVEAT: the exact console bytes were never recorded host-side
(`worker-preflight/REPORT.md` flags this; history staged ingame_facts as `=2`,
never `=0`; line order unrecorded). The Step-0 RETR below is mandatory and
authoritative. The ONLY verified-safe edit is the single-character mode change
on the composition line — never recreate the header, never touch other lines,
no whitespace/comment/reorder drift (§C rules).

Variant layouts (line shown = the only line that may differ):

- CURRENT (live): `/data/plugins/modbridge_supplier_composition.prx = 2`
- DISABLED:       `/data/plugins/modbridge_supplier_composition.prx = 0`
- RE-ENABLED:     `/data/plugins/modbridge_supplier_composition.prx = 2`

(If a live RETR shows the composition line absent, ADD the disabled line
exactly as above under the existing header — do not "normalize" anything else.)

### 4.4 Disable procedure (game CLOSED, before next arm)

```sh
TAG=$(date +%Y%m%d-%H%M%S); mkdir -p /tmp/ppsa33286-rollback-$TAG
# Step 0 — pre-image RETR + hash (authoritative bytes; also the audit record)
curl -s "$FTP/data/plugins/ploader.ini" -o /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre
shasum -a 256 /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre
grep -n "supplier_composition" /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre
# Step 1 — on-console backup of the pre-image
curl -s -T /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre "$FTP/data/plugins/ploader.ini.$TAG.bak"
# Step 2 — local edit: change ONLY the composition line's mode 2 -> 0
sed '/modbridge_supplier_composition\.prx/ s/ = 2$/ = 0/' \
    /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre > /tmp/ppsa33286-rollback-$TAG/ploader.ini.new
diff /tmp/ppsa33286-rollback-$TAG/ploader.ini.pre /tmp/ppsa33286-rollback-$TAG/ploader.ini.new
#    ^ diff must show exactly ONE changed line. If the pre-image lacks the line,
#      append "/data/plugins/modbridge_supplier_composition.prx = 0" after the header.
# Step 3 — STOR + fresh readback (plain file: byte-exact law)
curl -s -T /tmp/ppsa33286-rollback-$TAG/ploader.ini.new "$FTP/data/plugins/ploader.ini"
curl -s "$FTP/data/plugins/ploader.ini" | diff - /tmp/ppsa33286-rollback-$TAG/ploader.ini.new && echo INI-DISABLED-OK
```

Any diff/halt here → STOP, leave console as-is, reconcile against the `.bak`.
The `.prx` stays staged on disk the whole time; only its config line flips.

### 4.5 Verification launch

User re-arms ploader, launches Townfall. Expected: ploader notification shows
one FEWER plugin injected than before ("Injecting 2 plugin(s)" instead of 3),
composition never attempted (no FAIL line for it), game stable/unmodded.
If still unstable with composition-5 disabled, the tick detour was not the
(only) culprit — freeze further slot work, collect Severity-A-style evidence.

### 4.6 Rename fallback (only if ini STOR is impossible)

```sh
curl -s -Q "RNFR $SLOT" -Q "RNTO ${SLOT}.disabled" "$FTP/"    # rename away
# restore later: RNFR ${SLOT}.disabled -> RNTO $SLOT
```

Accepted costs (from main.c, §4.2): one FAIL notification + one attach/detach
cycle + 120 s injector timeout per launch, and an ini that still says `=2`.
Always rename back (or delete the `.disabled` copy only after a verified
re-stage) at restore time.

---

## 5. SEVERITY C — console crash / power loss (recovery ritual)

Precedents (all 2026-10-04, ledger mirror): `zcode_console_unreachable`
(ports 8084/2121 refused + 100% ping loss = crashed; user power-cycled;
"DO NOT re-dispatch until root-caused"), `zcode_outage_recovery` (power outage
killed PS5; came back; **staged /data intact**), `zcode_console_crash_2`
("DO NOT re-dispatch ploader or attempt any console contact until fully
restarted and verified"). /data persistence across power loss is twice-proven.

1. STOP all console contact immediately. Do not replay whatever ran before the
   crash — the crash is the evidence; identify the suspect first.
2. User fully restarts the PS5 (full boot, not rest mode). Nobody dispatches
   anything until the user says it is back.
3. Reachability: `curl -s "$HTTP/list_payloads"` → 200 + catalog. Repeat
   politely; a half-booted console must not receive dispatches.
4. FTP sanity (plain-file law): RETR `/data/plugins/ploader.ini` and sha it —
   compare to the last recorded ini sha; RETR the slot and check magic bytes
   (`curl -s "$FTP$SLOT" | xxd -l 4` → `7f 45 4c 46`, bare-ELF view per law).
5. Confirm staged /data intact: the ini + slot readback + `$R/loadlist.txt`
   shas vs the last logged values. Power loss has preserved /data every time;
   a mismatch here means the crash happened mid-STOR — treat the slot as
   unverified and go to §6 (full restore) before the next launch.
6. Re-arm ploader only as part of the normal launch ritual (§2) — never as a
   "test". If the crash suspect was the slot content, run §4 (disable) BEFORE
   the next launch.
7. Log the incident in CONSOLE-LOG.md style: timeline, suspect, evidence read,
   actions. No automatic retries, no scheduled re-checks.

---

## 6. SEVERITY D — full restore to a known-good slot

Both known-good states are restorable from local, sha-verified artifacts.
Restores happen with the game CLOSED; each is followed by a fresh-connection
readback per §1 and the ini-unchanged check.

### 6.1 Restore composition-4 (preferred known-good)

Upload the ORIGINAL staged fself (the byte class proven to load), then use the
console readback backup as the expected-readback oracle:

```sh
# a) STOR the original composition-4 fself (sha de372302…, 30778567 B, 17/17 gate)
curl -s -T "$T/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx" "$FTP$SLOT"
# b) fresh-connection readback; PASS = byte-identical to the step-11 backup
curl -s "$FTP$SLOT" -o /tmp/slot-after-c4-restore.prx
shasum -a 256 /tmp/slot-after-c4-restore.prx
#    expect d4fc0acf6a3ab8df9e8e9882a273d601c614591bc8d23111adf20a4acd48ba39 (30769672 B)
#    (both are the console-side transformed view of the same underlying payload)
cmp /tmp/slot-after-c4-restore.prx "$C/composition4-console-backup.prx" && echo C4-RESTORE-OK
```

If the readback differs, DIFF it and classify per §1 before any judgment:
only `e_shnum=0 @60`, last-24B-zeroed, tail-padding classes may be dismissed;
anything else means the slot does not hold composition-4 — reconcile, do not
blindly re-STOR. Never compare the readback to `de372302…` (upload sha) — that
comparison is meaningless under the law (§1).

Fallback (only if the fself artifact were lost): STOR the backup readback
itself (`$C/composition4-console-backup.prx`). Honest limit: that artifact is
a bare stripped inner ELF (ELF magic `7f 45 4c 46`), and every module that has
ever run on this console was staged as an fself — there is NO console proof a
re-uploaded bare ELF passes the kernel loader. If the loader rejects it, the
observable outcome is the fail-safe ploader FAIL path (§2: FAIL notification,
unmodded game) — recoverable, not a crash risk. Record whatever happens.

### 6.2 Restore sdg2 (one-line-per-launch diagnostic state)

```sh
curl -s -T "$C/pathtest-sdg2-run/sdg2-staged-fself.prx" "$FTP$SLOT"   # 22087 B, 0e5b36bc…
curl -s "$FTP$SLOT" -o /tmp/slot-after-sdg2-restore.prx
shasum -a 256 /tmp/slot-after-sdg2-restore.prx
# expect 182f5d3b48611e94155c16ff4eb3169b456fa8474b6049432bccf35f61bc8a0d (81368 B)
cmp /tmp/slot-after-sdg2-restore.prx /tmp/sdg2-final-backup.prx && echo SDG2-RESTORE-OK
```

Note the law in action: uploaded 22087 B vs expected readback 81368 B — the
whole-file sha that PASSES is the READBACK sha (`182f5d3b…`), never the
upload sha (`0e5b36bc…`).

### 6.3 After any restore

1. ini check: `curl -s "$FTP/data/plugins/ploader.ini" | diff - <expected>` —
   unchanged unless this incident intentionally flipped the composition mode
   (then flip `= 0` back to `= 2` via §4.4 with a fresh pre-image).
2. Remove nothing else. Do not delete on-console `.bak` copies.
3. Next launch follows the §2 ritual; collect logs once; one launch per staged
   state; record readback shas in CONSOLE-LOG style.

---

## 7. PRE-FLIGHT — 3 checks before ANY future slot swap

1. **Backup exists + sha.** RETR the current slot to local and sha256 it; STOR
   a `.bak` copy on-console; record BOTH hashes (plus the incoming artifact's
   upload sha and size) in the log BEFORE the overwrite. An fself slot has two
   distinct legitimate hashes (upload vs readback view) — record both, per §1.
2. **Readback law expectation written down before upload.** State in advance
   whether the incoming file is an fself (readback WILL differ in size/sha;
   PASS = PT_LOAD equality vs its payload-of-record inner ELF with only the
   §1 artifact classes) or a plain/bare file (PASS = byte-exact). Write the
   expected post-upload readback sha/size into the log first; an unplanned
   mismatch afterward is a STOP, never a retry loop.
3. **ploader.ini unchanged.** RETR + sha256 immediately before the swap and
   again after; only a single intended mode byte may differ when a mode change
   is part of the operation. Never recreate the header, other lines,
   whitespace, or ordering; keep the §4.4 one-line-diff discipline with an
   on-console `.bak` of the pre-image.

Plus the standing discipline that frames all three: game CLOSED for slot/ini
operations; one dispatch per verification; no replay of failed or reserved
jobs; any unexpected readback halts the pass (REHEARSAL §6 rule); the user
owns launch, power, and sign-in.

---

## 8. Evidence index

- CONSOLE-LOG steps 11–15: slot lineage + staging + readback law + backups.
- `zcode_sdg2_staged`, `zcode_sdg2_target_run`, `zcode_supplier_composition_4`,
  `zcode_supplier_composition_5`, `zcode_wiring_delivered_controller_attested`,
  `zcode_path_test_target_run`, `zcode_allfam_dll_console_run`,
  `zcode_console_unreachable`, `zcode_console_crash_2`,
  `zcode_outage_recovery` — in
  `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/transport-ledger.composition5-mirror.json`
  (PROVISIONAL mirror; controller ledger is authoritative).
- Gate GO: `outputs/2026-10-04-zcode-team/worker-composition5-gate/gate-receipt-composition5-r2.json`.
- ploader behavior: `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`.
- Staging/ini procedure precedents: `worker-final-staging-rehearsal/REHEARSAL-RESULT.md`,
  `worker-prx-packaging/STAGING-CHECKLIST.md`, `worker-post-promo-exec/EXECUTION-GUIDE.md`,
  `worker-preflight/REPORT.md` + `PREFLIGHT-CHECKLIST.md`.
- Policy: `outputs/ps5-mod-bridge/EXPERIMENTS.md`.
