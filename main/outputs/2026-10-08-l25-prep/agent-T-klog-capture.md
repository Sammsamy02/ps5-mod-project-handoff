# agent-T — klog capture procedure for the VANILLA save-load test (l25)

Read-only research, 2026-10-08. Deliverable: how to get the kernel log around a
vanilla (no-plugin) Townfall save-load, to test whether the
"copyin: has nonsleeping lock" / libkernel RIP 0x80004920 crash signature also
occurs WITHOUT our runtime.

## 0. Headline answers

- The klog on this console is a **live TCP stream, not a file**: `klogsrv-ps5.elf`
  (started by `/data/pldmgr/autoload.txt`) serves the kernel log read from
  `/dev/klog` on **TCP ps5-host-not-configured.invalid:3232**, with **no history replay** — you must
  be connected BEFORE the event you want. There is nothing to "pull" afterwards.
- Today's crash signature came from the l23 session's TCP tap:
  `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log`
  — fatal-signal dump for `eboot.bin` with `# rip: 0000000080004920`,
  `copyin:  has nonsleeping lock`, and `libkernel.sprx` in the dynamic-libraries
  list (block at lines ~15097–15160; 7 further eboot.bin fatal blocks follow at
  18357/18620/22470/25979/30503/42529/45404).
- Differential evidence already in hand: clean sessions (l13, l16, l18 klogs)
  contain **zero** "nonsleeping"/fatal-signal lines; every crashed session (l14,
  l15, l23) shows them. The "copyin: <thread> has nonsleeping lock" line is
  emitted by the kernel's fatal-signal dump path when the faulting user thread
  holds a spinlock — it appears inside crash dumps (game `eboot.bin` aborts AND a
  `SceShellUI` privileged-instruction fault in l15), not as a standalone message.
  That makes the vanilla test decisive: no fatal block + no copyin line ⇒ the
  signature is ours; same signature in vanilla ⇒ it is native.

## 1. Ranked klog source candidates

1. **klogsrv TCP stream `ps5-host-not-configured.invalid:3232`** (PRIMARY, proven — this is where
   every existing klog file came from).
   - `HANDOFF-MOD-STATUS.md` (2026-10-06-recovery-fakesign), lines 129–131:
     "klog capture: klogsrv streams on TCP 3232, no history replay — connect
     BEFORE the event you want."
   - Verified capture script `klog_capture.py` (l18 session) implements the tap:
     connect, send a single `b"\n"` to kick the stream, recv-loop into a NEW
     file, reconnect every ~5 s, 2 h window.
   - `autoload.txt` lists `klogsrv-ps5.elf`; `processes_list` shows the daemon.
   - etaHEN 13.60 binary strings contain the klog server (`klog_server_thread`,
     `/dev/klog`, "klog client disconnected") — stream-only design, no file sink.
2. **`/data/etaHEN/*.log` via FTP** (secondary, corroboration only — etaHEN's own
   logs, not the kernel log). Observed present in the l23 FTP listing:
   `etaHEN.log`, `etaHEN_util_daemon.log`; binary strings also reference
   `etaHEN_crash.log`, `startup-1360-port.log`, `bootstrap-1360-port.log`.
3. **pldmgr HTTP 8084** — confirmed NO `/klog` endpoint (404). The only useful
   endpoint is `GET http://ps5-host-not-configured.invalid:8084/processes_list` (precondition
   checks; used 14x in l23 receipts).
4. **FTP file guesses** (lowest): `/data/klog.txt` (confirmed absent),
   `/data/klogsrv.log`, `/mnt/usb0/klog.txt`, `/mnt/usb1/klog.txt` (USB sink
   modes exist in other klogsrv builds; none observed on this console).
   No UDP klog mode evidence on this console (no UDP port strings in the etaHEN
   binary; "BROADCAST" hits are SIMD mnemonics).

## 2. Exact capture commands (run BEFORE the user loads the save)

Copy the verified script once (read-only on the console):

```sh
mkdir -p /tmp/l25-vanilla
cp /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py \
   /tmp/l25-vanilla/klog_capture.py
# start the tap AFTER the pre-test reboot, BEFORE the user launches the game:
python3 /tmp/l25-vanilla/klog_capture.py --out-dir /tmp/l25-vanilla --window-seconds 7200
# status goes to stderr; raw stream + "[klog-capture UTC]" markers go to a NEW
# klog-<UTC>.log file (create-only, never truncates). Ctrl+C ends the capture.
```

Fallback one-liner if the script is unavailable (keeps stdin open so the `nc`
kick byte goes out and the connection persists; auto-reconnects):

```sh
F=/tmp/l25-vanilla/klog-manual-$(date -u +%Y%m%d-%H%M%S).log
while :; do (printf '\n'; sleep 7200) | nc ps5-host-not-configured.invalid 3232 >> "$F"; sleep 5; done
```

Rules learned from prior sessions:
- **Exactly one tap.** Stop any older local klog capture from earlier sessions
  first; a previous zcode tap had to be explicitly stopped (HANDOFF note).
- Start the tap after the console reboot: the stream ends when klogsrv goes
  down at shutdown/reboot (l23 log shows disconnect at 19:21:55Z → "Connection
  refused" during the console's reboot window). The script's reconnect loop
  rides this out if started before.
- Text logs need no `-Q "SITE SELF"` (decrypt-off toggle only matters for
  SELF/SPRX executables).

Corroboration pulls after the test (optional, FTP port 2121):

```sh
curl -s 'ftp://ps5-host-not-configured.invalid:2121/data/etaHEN/etaHEN.log'            -o /tmp/l25-vanilla/etaHEN.log
curl -s 'ftp://ps5-host-not-configured.invalid:2121/data/etaHEN/etaHEN_util_daemon.log' -o /tmp/l25-vanilla/etaHEN_util_daemon.log
curl -s 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini'           -o /tmp/l25-vanilla/ploader.ini.readback
```

## 3. Markers to grep and how to correlate with the save-load moment

Crash-signature greps (any hit = signature present in vanilla):

```sh
rg -n 'A user thread receives a fatal signal|# proc name:|# reason:|rip: |copyin|nonsleeping|fault address|libkernel\.sprx' /tmp/l25-vanilla/klog-*.log
```

Expected vanilla-NEGATIVE file matches the clean precedent
(`outputs/2026-10-07-l13-full-build/.../klog-20261007-154454.log`,
`.../2026-10-07-l16-full-build/.../klog-20261007-183310.log`,
`.../2026-10-07-l18-full-build/.../klog-20261007-205612.log` — all 0 hits).
The l23 positive block for comparison (lines 15097 ff.): reason
`abort is called`, `proc name: eboot.bin`, `# rip: 0000000080004920`
(= libkernel.sprx text), `copyin:  has nonsleeping lock` inside the backtrace
dump, `# /YBJS8crmSZ/common/lib/libkernel.sprx` in dynamic libraries.

Save-load window markers:

```sh
rg -n 'launchApp\(PPSA33286\)|Big App started|libSceSaveData|SaveData|OnionHEN|atime:|klog-capture' /tmp/l25-vanilla/klog-*.log
```

Anchors seen in the l23/l15 captures: `[SceLncService] launchApp(PPSA33286)`
(launch), `[onion_daemon.elf] [N.NNN] [OnionHEN] info: [lifecycle] Big App
started pid=... tid=PPSA33286` (game up, N.NNN = kernel uptime seconds),
`[libSceSaveData] api_id: init, titleId=PPSA33286` and the save-dialog scene
lines (`Sce.Cdlg.ReactNativeSaveData`, `kSaveDataDialog`) = the save-load
moment, `atime: 2026-10-08 17:36:52.155`-style lines carry wall clock.

Correlation method (the stream has NO per-line wall clock): bracket the test
with the script's `[klog-capture <UTC>]` start/end markers; find `launchApp`
then the `[libSceSaveData]` lines; cross-map uptime↔UTC using any `atime:`
wall-clock line plus the OnionHEN `[N.NNN]` uptime lines (in l23, e.g.
`[3750.930]` sat ~5 lines before the fatal block). Analyze the 30–60 s
(expand to 5 min) span after the save-data lines. Also note the CE-108255-1
dialog the user sees on a crash corresponds to these fatal blocks; a clean
vanilla run shows the game continuing past the save-load lines instead.

## 4. Fallback if TCP 3232 refuses: UDP listener

No UDP klog mode is evidenced on this console, so this is contingency only.
If TCP 3232 refuses connections after reboot (klogsrv failed to start), record
any UDP klog broadcast and prove/disprove its existence in one pass:

```sh
# catch-all (records from any UDP port; also captures mDNS noise — filter later):
sudo tcpdump -i en0 -w /tmp/l25-vanilla/klog-udp-$(date -u +%Y%m%d-%H%M%S).pcap host ps5-host-not-configured.invalid and udp
# conventional klogsrv UDP ports, if a text listener is preferred:
nc -lu 9999 > /tmp/l25-vanilla/klog-udp9999-$(date -u +%Y%m%d-%H%M%S).log &
nc -lu 3232  > /tmp/l25-vanilla/klog-udp3232-$(date -u +%Y%m%d-%H%M%S).log &
```

Run the listener during the whole test window; grep the same markers. If both
TCP and UDP yield nothing, klog is unobtainable for that boot — do not invent
substitutes; report the gap.

## 5. Console preconditions for the vanilla run

1. **Reboot the console first.** kstuff is paused after the prior crash
   (mission LAUNCH PLAN: "console REBOOT first — kstuff paused after crash";
   last known state line: "Console left with: v2 settings, ploader spent,
   kstuff paused"). A reboot also clears any resident modbridge copyin probe.
2. **Root must NOT arm ploader.** Never call
   `GET http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf`
   (the documented re-arm endpoint; l23 dispatched it exactly once at
   17:18:05Z — never re-send).
3. **ploader.ini STOP-check (silent vanilla-test breaker).** ploader.elf is in
   `/data/pldmgr/autoload.txt` (never touch autoload) and runs fresh at every
   boot; it injects any `[PPSA33286]` entries set to `2` (`<prx>=<2=inject|0=off>`
   per mission doc). The last observed state (l23 harvest, 17:42Z today) had
   `modbridge_supplier_composition.prx = 2` and `modbridge_path_test.prx = 2`
   under `[PPSA33286]`. BEFORE the launch, read it back:
   `curl -s 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini'`
   If any `= 2` remains under `[PPSA33286]`, STOP and surface it for explicit
   user approval (AGENTS rule: unmodded play = empty ploader.ini) — editing it
   is a console write, outside this read-only task.
4. **Verify daemons and no fresh payloads after reboot:**
   `curl -s http://ps5-host-not-configured.invalid:8084/processes_list` → expect `klogsrv.elf`
   present; expect no fresh ploader payload/generic-payload process; FTP LIST
   `/data/pldmgr/payloads/` shows no new `payload.elf` beyond the boot-time one.
5. **One capture client only** (section 2 rules); start the tap before the
   user launches Townfall from the shell, have them load the target save and
   play ≥60 s, then exit the game cleanly back to the shell.
6. Archive the capture (plus sha256, and the optional etaHEN logs) under a NEW
   directory `outputs/2026-10-08-l25-vanilla/` following the l23
   PACKET-MANIFEST.json pattern; never append to or truncate existing klog files.

## 6. Decision matrix

| Vanilla klog result | Meaning |
|---|---|
| Fatal-signal block(s) for `eboot.bin` + `copyin ... nonsleeping` in the save-load window (RIP 0x80004920 or not) | Signature is native to game/kernel save-load — not attributable to our runtime |
| Zero fatal blocks, zero copyin lines (l13/l16/l18 pattern) | Signature is runtime-attributed — our copyin machinery remains the prime suspect |
| System-thread fatal (e.g. SceShellUI) with copyin, game clean | l15-style system fault; record separately, does not answer the save-load question |

## 7. Provenance (all Mac-local)

- Crash signature source: `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log` (block ~15097–15160; more at 18357–45429); archived copy `harvest-20261008T174239Z-25929d/klog-snapshot.log`; filtered view `.../klog-filtered.log`.
- Prior-day precedents: l14 `.../2026-10-07-l14-full-build/console-test-20261007T161545Z-abde74a3/klog-crash-20261007T162131.110070Z.snapshot.log`; l15 `.../2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/klog-20261007-170208.log` (SceShellUI case); clean: l13/l16/l18 klogs.
- Capture script: `outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py` (also mirrored in test_robot.py `KlogCapture`, `ps5-pc-layer/src/testrobot/test_robot.py` lines 362–443, config `klog_port: 3232`).
- Discipline docs: `outputs/2026-10-06-recovery-fakesign/HANDOFF-MOD-STATUS.md` (TCP 3232, no replay); `outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md` (LAUNCH PLAN + vanilla test scope, lines ~390–410).
- Console state evidence: l23 harvest `autoload.txt`, `ploader.ini`, `PLOADER-ARMING.json`, `storage/directory-listings.jsonl`; etaHEN binary strings (`~/Downloads/Extra Stuff/etaHEN-13.60.elf`).
