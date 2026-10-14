# Agent Z2 — WHEN does ploader read ploader.ini? (start vs per-launch)

Date: 2026-10-08. Scope: READ-ONLY Mac-local research. No console/network contact.
Question: does an ini edit made while a resident ploader waits take effect on the
NEXT launch, or only on the next ploader (re)start?

## VERDICT

**The ini is read EXACTLY ONCE, at ploader START (arm time), BEFORE the launch-wait
loop. It is NEVER re-read per launch.** An edit to `/data/plugins/ploader.ini` made
while a ploader instance is resident does NOT affect that instance; it takes effect
at the NEXT arm (next `pldmgr /loadpayload` dispatch, or next boot's autoload).

**Confidence: HIGH.** The deployed console binary is byte-traceable to a pinned
upstream source we hold locally, whose `main()` makes the single `read_ini()` call
site unambiguous; independent session receipts corroborate the exit/re-arm cycle.

## Source-of-truth chain (deployed binary identity)

1. Console `/data/pldmgr/payloads/ploader/ploader.elf` = 142968 bytes
   (task context) = `diagnostic/ploader.elf` built in
   `.superpowers/native-dependency-spike/ploader-return-message-1/` — 142968 B,
   sha256 `009445ab35ecf447696a325c88c617d469d5b6cc966ccece914bdc09577f706d`
   (REPORT.md artifact table). That build = pinned upstream **Ploader v1.1.0,
   rev `1409a94aa6a5851acde72f85d3a1f6ae64a1b0a6`** with EXACTLY ONE source line
   changed (FAIL notification format `rc=0x%08x`); "inject_prx, credentials,
   ptrace, temporary pages, registers, permissions, offsets, cleanup, game-memory
   logic, and OK notification are unchanged" (same REPORT).
2. The controller ledger records the console payload as
   `payload_sha256: 009445ab…` (transport-ledger.composition6-mirror.json L5499;
   mirrored in outputs/2026-10-08-l21-audit/state-backups/) with readback receipt
   `.superpowers/native-observations/ploader-diagnostic-message-check-…/receipt.json`.
3. Strings dumps of the deployed binary contain the four lifecycle toasts —
   `Injector started`, `Config empty`, `Timeout, shutting down.`,
   `Already running (pid %d), exiting` (worker-staging-next/structural-checks/
   baseline.strings L583/411/1265/301) — matching the pinned source exactly.

Pinned source (authoritative for the deployed logic):
`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`
(sha `2b521cd7…`, 851 lines).

## Mechanism (pinned original/main.c, line-precise)

`main()` L831-850, in execution order:

| Step | Code | Lines |
|---|---|---|
| 1. Lock first | `if (!lock_acquire()) return 0;` | L836-837 |
| 2. **Read ini ONCE** | `read_ini(DEFAULT_INI_PATH);` — the ONLY call site in the whole program (grep-verified) | L839 (def L651; path L25 `/data/plugins/ploader.ini`) |
| 3. Empty config | `g_plugin_count == 0` → notify "Config empty" → `lock_release()` → exit | L840-844 |
| 4. Wait loop | `run_plugins()` — 50 ms poll (`usleep(50000)` L825) matching NEW pids' `sceKernelGetAppInfo` title_id against the **in-memory** `g_plugins[]` via `find_entry`; no file I/O, no `read_ini` anywhere in the loop | L738-826 |
| 5. Exit | all entries injected ("Finished") OR **120 s wall timeout** ("Timeout, shutting down.") → `lock_release()` → `return 0` | L735 `#define INJECTOR_TIMEOUT_S 120`, L745-748; L846-849 |

Consequences:
- **Per-instance list is frozen at arm time.** The =2→=0 flip is invisible to an
  already-running instance.
- **=0 entries are skipped at parse, not at inject** — `read_ini` L672
  `if (mode <= 0) continue;` (cited in worker-rollback-runbook/ROLLBACK-RUNBOOK.md
  §2 table). Next arm, the flipped plugin is never registered/attached/injected,
  and the toast shows one fewer plugin ("Injecting N plugin(s)" — runbook §4.5
  verification signal).
- **The resident window is bounded at ~120 s from arm**, not infinite: with no
  matching game launch, the daemon exits by itself (timeout) and releases the
  lock. With a single `[PPSA33286]` section, one Townfall launch also completes
  the cycle ("Finished") and the process exits — the documented "exits per game
  launch cycle" (AGENTS.md / PS5-MODDING-MISSION.md L132-135).
- **Double-spawn is lock-guarded and harmless**: `lock_acquire()` L690-714 reads
  the pid from `/data/plugins/ploader.lock`; if that pid is alive it notifies
  "Already running (pid %d), exiting" (L700) and `main` returns 0 — **before
  `read_ini` ever runs**. A stale lock (owner dead) is auto-unlinked (L704).
  So a `pldmgr /loadpayload` while resident is a NO-OP toast, never a second
  injector — but it also CANNOT refresh the resident's list.

## Historical behavioral evidence (receipts)

1. **Live exit-without-launch + re-arm-after-confirmed-exit (2026-10-08, menu-complete
   console-test-20261008T140903Z-a0626b):** armed 14:15:11 UTC, lock pid 182
   (PLOADER-ARMING.json). No game launched. FINAL-READINESS.json at 14:19:26:
   `ploader.lock` GONE (`curl rc 78, file does not exist`), zero game processes —
   i.e. the resident exited on its own inside ~2-5 min of arm with no launch
   (120 s timeout: 14:15:11+120 s = 14:17:11). PLOADER-REARM-AFTER-CONFIRMED-EXIT.json
   14:20:15: "Previous loader exited with missing lock and no game; not a blind
   retry or overlapping dispatch" — exactly ONE new dispatch, new pid 183.
2. **l22 ritual (console-test-20261008T155200Z-634c29):** lock-check BEFORE arm
   (RETR 550 — no resident), ONE loadpayload, lock pid 195 confirmed
   (PLOADER-LOCK-CHECK.json / PLOADER-ARMING.json). Same one-dispatch-after-
   confirmed-exit discipline, 2026-10-08.
3. **Second-launch receipt (2026-10-04, transport-ledger checkpoint
   2026-10-04T17:06:38):** after an ini append (`modbridge_ingame_facts.prx = 2`),
   "ploader WORKED on the second launch … ploader.lock fresh 17:03" — the ini edit
   was followed by a RE-ARM (fresh lock 17:03) before the launch that used it.
4. **Standing approval (2026-10-07-standing-routine-approval/20261007T230213Z/
   STANDING-APPROVAL.json L17):** "existing_ploader_reload: Via pldmgr
   /loadpayload **before a user launch**…" — the approved ritual always places the
   arm between the edit and the launch.
5. **Rollback runbook (worker-rollback-runbook/ROLLBACK-RUNBOOK.md §2, L105 +
   L115-119):** states it as law from the pinned source — "ini read ONCE at arm
   time … Consequence: an ini edit only takes effect at the NEXT arm — edit while
   the game is closed, before re-arming." §2 table also documents the 120 s
   timeout path (L111) and "stale lock recovered; duplicate arm exits quietly"
   (L113).
6. **Empty-ini disarm precedent (HANDOFF-MOD-STATUS.md §6):** "ploader.ini: EMPTY
   (mod plugins disarmed — mod is NOT loading until §3 step 1)" — the historically
   used vanilla mechanism: an empty ini makes even an armed ploader exit
   immediately ("Config empty").
7. **No counter-evidence:** no retained receipt shows an ini edit taking effect on
   a launch WITHOUT an intervening arm, and none edits the ini while a lock is
   live (the menu-complete session's ini was byte-identical before/after,
   diff-exit 0). The no-effect-on-resident claim therefore rests on the pinned
   source, which is unambiguous (single `read_ini` call site; no re-read in the
   poll loop).

## Application to TODAY's situation

Autoload armed ploader at boot (last line of autoload.txt) — that instance read
the ini AT BOOT, i.e. BEFORE root's =2→=0 edit. Therefore:

- The edit does NOT apply to the boot-armed instance. If Townfall were launched
  while that instance is still alive, it would inject per the OLD in-memory list
  (the flipped plugin INCLUDED).
- BUT the boot-armed instance only lives ≤120 s from its autoload execution
  (absent a launch). If root's edit came more than ~2 minutes after boot, the
  resident is almost certainly ALREADY exited ("Timeout, shutting down.",
  lock released) and today's `ploader.lock` observation is either a live
  still-in-window instance or a STALE lock from an unclean death (SIGSEGV/SIGBUS
  are SIG_DFL, L833-834, so a crash skips `lock_release`). A stale lock is
  harmless: the next arm auto-unlinks it (L704).

## SAFE OPERATOR PROCEDURE — guaranteed vanilla launch

Primary (zero dispatch, zero risk — RECOMMENDED):

1. Keep the game CLOSED. Establish resident state read-only:
   FTP RETR `/data/plugins/ploader.lock` (curl exit 78 / 550 = no resident) or
   pldmgr `/processes_list` — a resident shows as a `payload.elf` daemon whose
   pid equals the lock file's bytes.
2a. **No resident (most likely):** nothing more to do. The on-disk ini (=0 flip)
    is inert this boot — no injector exists — launch = vanilla. The flip's real
    effect lands at the NEXT arm/boot (plugin skipped at parse).
2b. **Resident alive (edit landed inside its 120 s window):** DO NOT launch yet —
    it holds the OLD list and would inject it once. Simply wait out the ≤120 s
    from its arm for the "Timeout, shutting down." toast / lock RETR turning 550,
    then launch = vanilla. The exit is guaranteed by `INJECTOR_TIMEOUT_S` with no
    game involvement.

Optional re-arm (only if a MODDED launch is wanted later this same boot): after
CONFIRMED exit (lock 550), dispatch ONCE
`curl http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf`
— the fresh instance reads the EDITED ini (flipped entry skipped; remaining =2
entries, if any, still inject; expected toast shows one fewer plugin). Never
double-dispatch: it is lock-guarded (second instance exits "Already running"
BEFORE reading the ini — a harmless no-op that does NOT refresh anything), but
the standing rule is one dispatch after confirmed exit, never overlapping.

Explicitly rejected alternatives (tradeoffs, for the record):
- **"Let the resident do one cycle on the OLD list, then restart cleanly"**
  (the start-only contingency from the tasking): unnecessary here — the natural
  120 s timeout gives a clean, game-free exit within ~2 minutes of arm, so there
  is no reason to spend one modded launch on the stale list. That path only
  makes sense if a launch happened to occur inside the window anyway.
- **`/process_kill?pid=<lock pid>`** (API exists per HANDOFF-MOD-STATUS §6):
  works in principle (lock holds the resident's pid) but violates the
  no-unrelated-termination discipline and risks nothing-needed — the timeout
  achieves the same exit safely. Do not use for this.
- **Editing autoload.txt / renaming prx files**: prohibited/unnecessary; the
  one-character ini mode edit is the verified-safe disable mechanism
  (ROLLBACK-RUNBOOK §4.2).

## Evidence index (absolute paths)

- Pinned deployed-logic source: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`
- Build identity: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ploader-return-message-1/REPORT.md` (+ diagnostic/ploader.elf 142968 B sha 009445ab…); console payload_sha256 in `…/outputs/2026-10-08-l21-audit/state-backups/transport-experiments-progress.json` L5499; strings: `…/outputs/2026-10-04-zcode-team/worker-staging-next/structural-checks/baseline.strings`
- Exit+re-arm receipts: `…/outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/{PLOADER-ARMING,FINAL-READINESS,PLOADER-REARM-AFTER-CONFIRMED-EXIT}.json`; `…/outputs/2026-10-08-l22-package-r2/console-test-20261008T155200Z-634c29/{PLOADER-LOCK-CHECK,PLOADER-ARMING}.json`
- Second-launch/re-arm checkpoint (2026-10-04T17:06:38): `…/outputs/2026-10-08-l21-audit/state-backups/transport-experiments-progress.json` (~L9245-9263)
- Runbook law: `…/outputs/2026-10-04-zcode-team/worker-rollback-runbook/ROLLBACK-RUNBOOK.md` §2, §4.2-4.5
- Standing ritual: `…/outputs/2026-10-07-standing-routine-approval/20261007T230213Z/STANDING-APPROVAL.json` L17
- Empty-ini disarm + pldmgr API (incl. /process_kill): `…/outputs/2026-10-06-recovery-fakesign/HANDOFF-MOD-STATUS.md` §6
- Lifecycle/restart phrasing: `…/outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md` L130-135; AGENTS.md PS5 rules
