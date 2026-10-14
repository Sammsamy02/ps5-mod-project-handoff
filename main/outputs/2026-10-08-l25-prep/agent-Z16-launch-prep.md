# Agent Z16 — one-command launch preparation for the l25 variant A/B tests

Date: 2026-10-08. Scope: Mac-local tool authoring + local `--dry-run` testing
only. **NO console contact was made at any point** (every network-capable
line in the tool is print-only under `--dry-run`; the local tests used
throwaway fixtures on 127.0.0.1 or pure file reads). The script is EXECUTED
LATER BY ROOT.

## Deliverable

- `harvest/prep_modded_launch.py` (Python 3, stdlib only; 1,236 lines;
  py-compile clean). Sits next to agent-Y's `harvest.py` and reuses its
  conventions: console `ps5-host-not-configured.invalid`, HTTP 8084 `/processes_list`, FTP
  `ftp://IP:2121`, `-Q "SITE SELF"` decrypt-OFF readbacks, receipt-style
  curl argv (`--silent --show-error --fail --connect-timeout 5 --max-time
  …`, `-T/--upload-file` + `SITE SELF` per the l22 UPLOAD-RECEIPT.json
  precedent, `--max-filesize 67108864` on executable pulls).

Sources of truth encoded (each constant cites its origin in-file):
LAUNCH-PLAN.md (sections 1, 4, STEP 0.4, SAFETY BOX), INTEGRATION-CHECKLIST
"Console preconditions", agent-Z2-ploader-timing.md (arming law + lock/120 s
mechanism), evidence/VANILLA-VERDICT.md (post-hoc operator law, ini backup
hash), agent-T-klog-capture.md (tap procedure + pinned klog_capture.py
path), harvest.py/markers.json (exact variant-B SKIPPED string).

## Usage

```
# Plan-only (no network, no writes, exit 0) — root sanity pass:
python3 outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py \
    --variant A --prx-path <path-to-variant-A.prx> --dry-run

# LIVE prep (root, after the console reboot, before the user launch):
python3 outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py \
    --variant A --prx-path <path-to-variant-A.prx>
python3 outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py \
    --variant B --prx-path <path-to-variant-B.prx>   # only if A stalled/crashed

# Options: --skip-klog (records "no klog evidence" as a gap; not silent),
#          --console-ip (default ps5-host-not-configured.invalid), --dry-run.
# Exit codes: 0 = dry-run done / live all-gates-passed; 1 = verification
# ABORT (durable receipt written); 2 = usage error.
```

Outputs on success: `LAUNCH-READY-<variant>-<timestamp>.md` beside the
script (verification ledger, hashes, pids, capture paths, the player
instruction sequence, variant-specific harvest expectations) plus the
session dir `/tmp/l25-variant-<X>/` holding every readback, the arm
response, processes snapshots, the klog tap stderr and the monitor log. On
abort: `LAUNCH-PREP-FAILED-<variant>-<timestamp>.md` with the same ledger
plus the exact abort reason — nothing is retried or bypassed.

## Enforced procedure (verbatim from the tasking; every gate aborts)

1. **Preconditions** — processes_list must contain kstuff/pldmgr/ftpsrv/
   klogsrv (matched as `<base>.elf`, format proven against l22
   processes.before.json); no `eboot.bin` process (name proven in the l23
   crash-session list, pid 102); ploader residency via FTP RETR of
   `/data/plugins/ploader.lock`: curl rc 78/RETR 550 = none; **rc 0 + LIVE
   lock pid → ABORT with the operator instruction to wait ≤120 s for the
   INJECTOR_TIMEOUT_S exit, then re-run** (Z2 law — a resident holds the
   frozen in-memory ini list; a dead-pid lock is recorded as stale and
   passed, since the next arm auto-unlinks it, Z2 main.c L704).
2. **PRX safety gate** — exists; sha256 recorded; magic must be `4f153d1d`
   or `5414f5ee`. Plain `7f454c46` → STOP: *"unsigned ELF -- run
   make_fself.py first (AGENTS.md PS5 rule 3 …)"*. Unknown magic → refuse.
3. **Upload + verify** — decrypt-OFF backup readback of the CURRENT
   resident `/data/plugins/modbridge_supplier_composition.prx` first
   (LAUNCH-PLAN STEP 0.4; absent-on-console rc 78/550 = first install,
   recorded and passed; an unverifiable resident aborts rather than being
   overwritten); then `curl -T` upload; then decrypt-OFF readback must
   match magic + FULL sha256 byte-exact. **On mismatch: ONE verified
   restore of the backup captured this run (its own decrypt-OFF readback
   check), then ABORT regardless — a broken executable is never left in
   place.**
4. **Settings restore** — local `/tmp/l24-settings-backup.ini` verified
   BEFORE upload: exactly 1947 bytes, sha256 `b980cea7…d3ce2` (both
   re-verified on this Mac today) → `/data/modbridge/roots/PPSA33286/ue4ss/
   UE4SS-settings.ini`; readback + byte-exact cmp + size re-check; any
   drift aborts (MFA size-exact law: the 1949-byte v1 file was
   `mfa_activate_failed code=-8` rejected).
5. **ploader.ini** — local `/tmp/ploader-ini-backup-20261008.ini` verified
   (sha256 `3eab9510…bb3d5`, 440 bytes, and the
   `/data/plugins/modbridge_supplier_composition.prx = 2` inject row must
   be present) → `/data/plugins/ploader.ini`; readback + cmp.
6. **Arm ploader** — exactly ONE GET
   `http://IP:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf`
   (ini-edit law: REQUIRED after any ini change), then poll processes_list
   every 2 s up to 10 s for a `payload.elf` pid ABSENT from the step-1
   baseline; none → abort with an explicit never-duplicate-send warning.
7. **Background captures** — klog tap:
   `python3 …/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py
   --out-dir /tmp/l25-variant-<X> --window-seconds 3600`, detached; the
   capture path is parsed from its `[klog-capture] … file=` stderr line and
   CONFIRMED non-empty on disk (abort otherwise; `--skip-klog` records the
   gap explicitly). eboot monitor: the script re-spawns ITSELF with the
   hidden `--run-eboot-monitor --out-dir … --poll-seconds 15
   --window-seconds 3600` mode — 15 s processes_list poll, logs
   `eboot.bin`/`payload.elf` pid-set transitions and required-daemon loss
   to `eboot-monitor-<ts>.log`, read-only, bounded (ends by itself).
8. **LAUNCH-READY-<variant>-<timestamp>.md** — full verification ledger
   (expected/actual/status per check), all hashes (PRX local/backup/
   readback; settings; ploader.ini), pids (daemons, pre-arm payload.elf
   set, fresh armed pid), capture paths/commands/pids, and the user
   instruction sequence: menu 90 s → load the SAME save → 30 s → L3+R3
   once → clean exit (crash note: CE-108255-1 + moment; harvest
   immediately; reboot before any next launch). Variant B additionally
   states the SKIPPED listener expectation using the landed agent-O
   vocabulary: NO `[L25][DeleteListener]` registration lines, ONLY
   `delete-listener registration SKIPPED (variant B; unsafe long-term:
   stale object pointers not cleared)` — plus the diagnostic-only /
   never-leave-B-resident warning (LAUNCH-PLAN 4.2/4.4).
9. **--dry-run** — prints every command (12 command lines for variant A)
   and every check with its expected value; local read-only checks (PRX
   magic/sha, both /tmp backups) really execute and show PASS/FAIL; no
   network, no writes; exit 0.

Standing rules baked in: the code never references `autoload.txt`
operationally (docstrings/reports only mention the prohibition), contains
no `/process_kill` or any process termination, and never proceeds past a
failed verification (single `Abort` path; even unexpected exceptions are
converted into an abort receipt).

## Local test proof (all offline; no console traffic)

| Test | Result |
|---|---|
| `python3 -m py_compile prep_modded_launch.py` | OK |
| `--help` | prints usage; hidden monitor args suppressed |
| `--dry-run --variant A` (signed-magic fixture) | exit 0; 12 command lines printed; local checks PASS (magic 4f153d1d; settings 1947 B b980cea7…; ploader.ini 440 B 3eab9510… + inject row present); LAUNCH-READY-A skeleton printed, nothing written |
| `--dry-run --variant B` | exit 0; SKIPPED-listener expectation section exact |
| `--dry-run --variant B --skip-klog` | exit 0; gap recorded ("NO klog evidence … never rated") |
| `--dry-run` with plain-ELF fixture | exit 0; gate prints `FAIL: 7f454c46 -> STOP: unsigned ELF -- run make_fself.py first …` (live mode raises the Abort — unit-tested directly) |
| live run vs unreachable IP 127.0.0.1 | exit 1; `STEP 1 precondition FAILED …` on stderr; `LAUNCH-PREP-FAILED-A-<ts>.md` written with abort reason + ledger; no retry |
| unit: `step2_prx_gate` | unsigned→Abort(exact msg); unknown magic→Abort; signed→(size,sha,magic); missing→Abort |
| unit: `verify_local_ini` | sha-drifted 1947-byte file→Abort; real `/tmp/l24-settings-backup.ini`→PASS |
| unit: parsing vs real l22 receipt | kstuff=87 pldmgr=85 ftpsrv=89 klogsrv=96; eboot.bin=[]; payload.elf=[156] |
| unit: `spawn_and_confirm` | alive child→PASS pid; instantly-dead child→Abort |
| `--run-eboot-monitor` vs a LOCAL stub processes_list server on 127.0.0.1:8084 | logged `state baseline` → `TRANSITION eboot.bin [] -> [102]` → `TRANSITION payload.elf [195] -> []` → `TRANSITION daemon missing: kstuff` → `end reason=window-elapsed errors=0`, exit 0; honors explicit `--log-path` |

One bug was found and fixed during testing (recorded for provenance): the
monitor originally derived its own timestamped log path, so the step-7
readiness probe watched a different (empty) file; fixed by threading an
explicit `--log-path` from the spawner. Test artifacts (fixtures, stub
server, failed-run receipts, /tmp dirs) were removed afterwards;
`harvest/` contains only `harvest.py`, `markers.json`,
`prep_modded_launch.py`.

## Exact one-line commands root runs per variant launch

Variant A (run first):

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path outputs/2026-10-08-l25-package-A/modbridge_supplier_composition.prx
```

Variant B (ONLY if A stalls or crashes, per LAUNCH-PLAN section 4; reboot
first; never leave B resident):

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant B --prx-path outputs/2026-10-08-l25-package-B/modbridge_supplier_composition.prx
```

(Adjust the `--prx-path` to the FINAL-RECEIPT-pinned artifact if agent R's
packaging lands it under a different name — the B artifact is expected to
be labeled distinctly, e.g.
`modbridge_supplier_composition.l25B.diagnostic.prx`; the script pins
whatever path it is given by sha256 + SELF magic and records it.)

## Honest limits

- Not executed against the real console (by instruction); the live FTP/
  HTTP sequences reuse only receipt-proven curl argv shapes (l22/l23).
- The klog window is 3600 s per the prep contract (LAUNCH-PLAN 1.4 used
  7200 s for the vanilla test); extend only by editing `KLOG_WINDOW_S`.
- Depends on `/tmp/l24-settings-backup.ini` and
  `/tmp/ploader-ini-backup-20261008.ini` still matching their pinned
  hashes (both re-verified today; evidence copies exist under
  `2026-10-08-l25-prep/evidence/` if /tmp is lost).
- Variant PRX artifacts did not exist yet at authoring time (packaging
  pending per agent R); dry-run proof used a fixture with SELF magic.
