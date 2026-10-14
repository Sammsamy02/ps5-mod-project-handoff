# LAUNCH-PLAN.md — l25 PS5 UE4SS runtime testing (Claude-approved, 2026-10-08)

Operational procedure for root, Mac-local, console at ps5-host-not-configured.invalid. Review
directive receipt: `claude-review-20261008-l25.md` (same directory). Console
state at plan time: game CRASHED on the last run → kstuff paused → REBOOT
REQUIRED before any launch; v2 settings (both hooks false, admission-legal)
still on console; ploader spent (exits per launch cycle). No launch happens
without explicit user OK — one launch per test, clean exit between launches.

Console endpoints (as used in prior sessions):
- HTTP: `http://ps5-host-not-configured.invalid:8084/processes_list` (alias `/api/processes`);
  pldmgr load dispatch `…/loadpayload:<url-encoded-path>`.
- FTP: `ftp://ps5-host-not-configured.invalid:2121` (same parameters as prior sessions).
- Klog tap: TCP `ps5-host-not-configured.invalid:3232` — connect BEFORE the launch; every capture
  to a NEW file (l16 pattern:
  `outputs/2026-10-07-l16-full-build/console-test-20261007T183202Z-2318a17f/`).

Variant definitions (mission STATUS 2026-10-08):
- **Variant A** — l25 build, NORMAL registration (`AddUObjectDeleteListener`
  at `UnrealInitializer.cpp:528` kept), instrumented with post-:528 markers
  (agent O) + runtime-phase tags (agent P) + copyin-probe liveness (agent Q).
- **Variant B** — l25 twin build, registration SKIPPED. Diagnostic-only,
  unsafe long-term: without the delete listener, stale object pointers are
  never cleared. Run ONLY if A stalls or crashes; never leave B resident.

## STEP 0 — UPLOAD GATE (before PRE-LAUNCH; blocking)

1. l25 build complete per `outputs/2026-10-08-l25-runtime/build_runtime.py`
   (writes `outputs/2026-10-08-l25-build-r2/`; run from the worktree root),
   `target_compile.py` executed (all exit 0, `results/TARGET-COMPILE.json`),
   item-6 suite green against l25 headers
   (`2026-10-08-l25-runtime/tests/run_tests.py` → PASS_HOST_ONLY_MAC; compare
   `tested_headers` against `tests/PRE_RUN_HASHES.txt` for drift).
2. Twin variant A/B artifacts fake-signed (see SAFETY BOX) and their sha256 +
   sizes recorded; twin static-gate tables and import-identity checks
   (import-symbol delta == 0, twin identity) recorded.
3. **Packet Stage 1 assembled and delivered to Claude; upload only per that
   approval.** Packet template: section 6 below.
4. Upload variant A PRX to `/data/plugins/` (plain FTP PUT, our own loader
   PRX; overwrite the resident composition only after a decrypt-OFF backup
   readback of the current file), then verify with decrypt-OFF readback:
   magic `4f153d1d` (or `5414f5ee`) + full sha256 match against the local
   artifact BEFORE any launch. No other console file changes in this step.

## 1. PRE-LAUNCH (root, after console reboot)

Order matters; do not skip the readbacks.

1.1 **Verify kstuff not paused + pldmgr up.**
```
curl --disable --fail --silent --show-error --noproxy '*' \
  --connect-timeout 5 --max-time 15 \
  -o processes.json http://ps5-host-not-configured.invalid:8084/processes_list
```
Confirm in `processes.json`: pldmgr (payload manager) present and responsive;
kstuff NOT in a paused state (post-reboot it must be healthy; a paused kstuff
after a failed launch resumes only on clean game exit or reboot — if still
paused, stop and reboot again; do not launch). Record the pre-arm pid list.

1.2 **Restore ORIGINAL UE4SS-settings.ini (byte-exact 1947 B).**
Source: `/tmp/l24-settings-backup.ini` (legacy, now durable at
`evidence/l24-settings-backup.ini` — same bytes, sha
`b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`;
prep_modded_launch.py resolves durable-first since 2026-10-10) — 1947 bytes, sha256
`b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`
(HookLoadMap=true + HookInitGameState=true, the as-run l24 configuration).
```
# $EV = outputs/2026-10-08-l25-prep/evidence/ ; $EVR = .../evidence/runtime/<label>/
# (legacy /tmp paths superseded, agent-HA 2026-10-10 evidence law)
curl -T $EV/l24-settings-backup.ini \
  "ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"
curl -o $EVR/l25-settings-readback.ini \
  "ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"
cmp $EV/l24-settings-backup.ini $EVR/l25-settings-readback.ini \
  && stat -f %z $EVR/l25-settings-readback.ini   # MUST print 1947
shasum -a 256 $EVR/l25-settings-readback.ini      # MUST print b980cea7…
```
Why size-exact: modforge MFA admission for role=config checks ONLY the byte
size declared in `loadlist.txt` (`config ue4ss/UE4SS-settings.ini 1947`) —
the 1949-byte A/B v1 file was rejected with `mfa_activate_failed code=-8`
"file is longer than the load list says", and the game ran unmodded. The file
must stay EXACTLY 1947 bytes. Plain-text file: no `SITE SELF` toggle needed
(ftpsrv executable decryption does not apply to .ini).

1.3 **Re-arm ploader** (it exited with the last launch cycle).
```
curl "http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf"
```
Then re-pull `processes_list` and confirm a FRESH `payload.elf` pid appears
(compare against the 1.1 list). Dispatch ONCE — read the receipt, never
duplicate-send; if a ploader instance from autoload is already up and the
dispatch is refused, record that observation and treat the running instance
as armed only if its pid is live in `processes_list`. Verify
`/data/plugins/ploader.ini` still has the inject row for `[PPSA33286]`.

1.4 **Arm the klog capture** (connect BEFORE the launch; new file; 7200 s
window; l16 pattern). Start it now if this launch is to be klog-covered.

1.5 **Ploader arming law (agent Z2, 2026-10-08 — applies to EVERY modded
launch).** ploader reads `ploader.ini` EXACTLY ONCE, at arm time, BEFORE the
launch-wait loop (pinned source `main.c:839` — the only `read_ini` call site;
no re-read in the 50 ms poll loop). The resident window is bounded by the
120 s timeout (`INJECTOR_TIMEOUT_S`), and a `loadpayload` dispatch while an
instance is resident is a lock-guarded NO-OP ("Already running") that CANNOT
refresh the resident's frozen in-memory list. THEREFORE: for every MODDED
launch, after ANY `ploader.ini` change, dispatch `loadpayload` ONCE — only
after the prior instance's confirmed exit (lock file gone / pid dead) — and
confirm a FRESH `payload.elf` pid in `processes_list` (compare against the
1.1 list) BEFORE launching. Never rely on an ini edit reaching a resident
ploader; an edit takes effect only at the NEXT arm. (Verified corollary from
today's vanilla test: the boot-armed instance read the ini at BOOT — before
the =0 flip — and had already timed out pre-launch, which is why that run
was injection-free; see `agent-Z2-ploader-timing.md` and
`evidence/VANILLA-VERDICT.md` post-hoc note.) Step 1.3 satisfies this law
when no ini edit happened after the last arm; if the ini changed at all
since the resident's arm, repeat 1.3 after the change.

## 2. VARIANT A LAUNCH (user does; root observes read-only)

Get explicit user OK, then exactly this sequence:

1. Launch the game (Townfall, PPSA33286).
2. **Main menu: wait 90 s.** Watch: does init pass `:528`? Does the mod
   start? (User-visible signal: none at this stage — root may do an optional
   read-only FTP pull of `/data/plugins/UE4SS.log` mid-window to check for
   lines past "Adding shutdown listener"; definitive check is at harvest.)
   The 90 s also covers the PS5Pad ≥10 s uptime needed for the pad borrow.
3. **Load the SAME save** used in the l24 crash run.
4. **Wait 30 s** past save-load.
5. **Press L3+R3 ONCE** (PS5Pad rebind; OPTIONS stays the game's pause).
6. **Clean exit** the game (return to dashboard via in-game quit, not power
   events) — or, if it crashes, note the crash code (expected signature:
   CE-108255-1) and WHEN it happened (menu / save-load / L3+R3), then root
   harvests immediately; a reboot is required before any next launch.

Record per run: launch pid, timestamps of each step, crash code + moment if
any, L3+R3 result, clean-exit confirmation.

## 3. HARVEST (root, after clean exit — or immediately after a crash)

Pull, to a NEW dated harvest directory (never overwrite prior harvests):

| Evidence | Console path |
|---|---|
| UE4SS.log | `/data/plugins/UE4SS.log` |
| runtime-status.txt | `/data/modbridge/roots/PPSA33286/runtime-status.txt` |
| diag stream (supplier-diag) | `/data/modbridge-supplier-diag.jsonl` (append-only; split by pid, take this launch's session) |
| aligned-entry log | `/data/townfall_runtime_aligned_entry.log` (marker line `modbridge-runtime-aligned-entry/v1 build=…`; record the build id) |
| klog capture | local file from the 1.4 tap |
| processes_list | post-run snapshot (ploader exit, game pid gone) |

Also pull the run's `UE4SS-settings.ini` readback (prove it was still 1947 B
original at launch time).

### 3.1 New-marker grep lists

**Post-:528 markers (agent O — listener instrumentation):**
> `[PLACEHOLDER — agent-O-listener-instrumentation.md NOT PRESENT in
> 2026-10-08-l25-prep/ as of this writing. Before the first launch, fill in
> the exact post-AddUObjectDeleteListener marker strings (registration
> entered / completed, ABI verify result, listener-invoked trace) from that
> report. Do not invent strings.]`

**Event-loop / on_program_start / marshal markers (agent P — phase tags):**
> `[PLACEHOLDER — agent-P-phase-tags.md NOT PRESENT in 2026-10-08-l25-prep/
> as of this writing. Before the first launch, fill in the exact event-loop,
> on_program_start, detour-entry, queue-depth and pump tag strings from that
> report. Do not invent strings.]`

**Copyin-probe liveness tags (agent Q):** same rule — fill from the agent Q
report if/when present.

### 3.2 l24-era markers that remain in the build (known-good vocabulary)

In `UE4SS.log`: `Adding shutdown listener` (was the last line, `:527`),
`Starting callback garbage collector!` (`:542` — first proof init passed
:528), struct/module size prints, `Unreal Engine modules`,
`[l22] Lua mod started: … (main script returned)` (`LuaMod.cpp:7557`),
`[R3] FuncPtr-marshal armed on carrier`, `[R3] event carrier scan`,
`[R3] marshal is the game-thread pump`,
`[l22] ExecuteInGameThread queued method=…` /
`[l22] ExecuteInGameThread drained … (callback entry)`,
`[TypeScan] scan-complete … core-ready=true`,
`[Seeds]` pool admission line (`0x9765EC0`).
In the diag stream: `pad_l3r3_edge` → `pad_l3r3_f7_dispatched`,
`pad_key_edge`, (if adopted) the throttled `r3_marshal_pumped` heartbeat.
In `runtime-status.txt`: the terminal ack (absent while init stalls — the
RuntimeAckSink only reports after `on_engine_initialized`).

### 3.3 Interpretation matrix (dual hypothesis)

| Observation | Conclusion |
|---|---|
| Post-:528 markers present, "Starting callback garbage collector!" logged, init completes, `[l22] Lua mod started` | Registration does NOT hang → hypothesis (a) dead in A; watch save-load |
| A completes init, save-load CRASHES, listener-invoked/GC-phase markers show the listener firing | Hypothesis (b): listener added broken (ABI/race), engine calls it during save-load mass GC |
| Log still ends at `Adding shutdown listener`, no post-:528 markers | Hypothesis (a): registration hangs/faults at :528 → run VARIANT B |
| L3+R3 opens the mod menu | Pump armed and firing — full chain healthy |
| `runtime-status.txt` shows no terminal started-ack | init() never completed (corroborates stall) |

## 4. VARIANT B — only if A stalls or crashes

Same PRE-LAUNCH + LAUNCH + HARVEST procedure, with these additions:

1. Swap the resident PRX to the variant B twin (fake-sign → upload →
   decrypt-OFF readback verify magic + sha256 BEFORE launch; SAFETY BOX).
2. **The variant B receipt MUST state:** diagnostic-only; unsafe long-term —
   the skipped `AddUObjectDeleteListener` means the delete listener is never
   registered, so stale object pointers are never cleared; B exists solely to
   bracket the :528 fault and must never remain the resident runtime.
3. Decision value: B completes init (post-skip markers, event loop, mod
   start) where A stalled → the fault is in the registration call/write
   itself (hang or the racing TArray write). B surviving save-load where A
   crashed → the listener invocation path is implicated. B ALSO crashing at
   save-load → look away from the listener entirely (copyin-probe / detour
   attribution, §5's vanilla result becomes decisive).
4. After B testing: harvest, then restore variant A (or the l24 composition)
   as resident — never leave B installed.

## 5. VANILLA KLOG TEST (independent of the variants; may run FIRST)

Purpose: establish whether the kernel crash signature
`copyin: has nonsleeping lock` occurs in UNMODDED play. If it appears in a
vanilla klog too, the signature is not ours and the copyin-probe attribution
weakens sharply.

1. Console reboot. **Do NOT arm ploader** (no `loadpayload` dispatch).
2. Guarantee the run is actually vanilla: per standing rule, unmodded play =
   empty `ploader.ini` — back up `/data/plugins/ploader.ini`, neutralize the
   `[PPSA33286]` inject row (empty file / `0=off`), NEVER touch
   `autoload.txt`. (autoload may start ploader at boot; with an empty
   ploader.ini it injects nothing. Verify via `processes_list` that no game
   payload injection is pending; restore `ploader.ini` immediately after the
   test.)
3. Start the klog capture BEFORE the launch: TCP `ps5-host-not-configured.invalid:3232`, NEW
   file, 7200 s window (l16 pattern).
4. User: launch → menu 90 s → load the SAME save → 30 s → clean exit.
5. Root pulls/terminates the klog capture.

**Klog capture procedure (agent T):**
> `[PLACEHOLDER — agent-T-klog-capture.md NOT PRESENT in 2026-10-08-l25-prep/
> as of this writing. Until it exists, use the verified l16 procedure:
> connect a reader to ps5-host-not-configured.invalid:3232 BEFORE launch, capture to a NEW file
> per session (window 7200 s), stop after harvest; see
> outputs/2026-10-07-l16-full-build/console-test-20261007T183202Z-2318a17f/
> (klog-process.log, KLOG-COMPLETED.json). When the agent T report lands,
> replace this block with its exact procedure.]`

**Decision:** `copyin: has nonsleeping lock` present in the vanilla klog →
the crash signature is NOT ours (stock game + kernel produce it too); absent
in vanilla but present in modded runs → the signature tracks our residency
(copyin probe / detour attribution stands).

## 6. PACKET-TO-CLAUDE (template — Stage 1 MUST be complete before ANY upload)

Assemble under `2026-10-08-l25-prep/packet-<timestamp>/`; raw evidence
unchanged, hashes recomputed, honest gaps stated as gaps.

**Stage 1 — pre-upload (blocking; no console upload until delivered and
approved):**
- Build receipts: `2026-10-08-l25-build-r2` outputs, `TARGET-COMPILE.json`,
  host-suite results (item-6 `tests/RESULTS.json` + `PRE_RUN_HASHES.txt`,
  profile digest-gate PASS), build ids for the aligned-entry marker.
- Twin variant hashes: variant A and variant B PRX (bytes, sha256, SELF
  magic), source-tree CHANGES.json hash tables including the agent-N review
  fixes, twin-identity statement.
- Static-gate tables: BuildProfile / LayoutPreflight expectations
  (l24 baseline: 401/401 PASS, 66 keys, missing-required=0) reproduced for
  l25; [Seeds]/[TypeScan] admission expectations.
- Import checks: import-symbol delta == 0 vs the l24 composition; twin A/B
  import identity; `check_import_identity.py` outputs.

**Stage 2 — post-launch (appended as evidence lands):**
- Variant A harvest: UE4SS.log (+marker grep results against §3.1/§3.2
  lists), runtime-status.txt, supplier-diag session (split by pid),
  aligned-entry log, klog file, crash code + moment, L3+R3 outcome.
- Variant B harvest (if run) + its diagnostic-only/unsafe-long-term receipt.
- Vanilla klog result + the §5 decision line.
- Console-state handoff note (settings restored/original, resident PRX,
  ploader state, kstuff state).

Rules: raw logs never edited; pinned factual challenges block acceptance;
each launch requires explicit user OK; no replay of failed or unauthorized
runs; never claim untested behavior as proven.

## 7. SAFETY BOX (PS5 console rules — permanent; from AGENTS.md)

1. ftpsrv DECRYPTS SELF/SPRX executables on download BY DEFAULT (and fakes
   LIST/SIZE to the decrypted size). Toggle off per connection:
   `curl -Q "SITE SELF"`. `7F454C46` (plain ELF) in a download = decrypted
   view, not the real file.
2. NEVER upload a downloaded/modified executable back unsigned — it bricks
   the game (readHeader/kern_get_self_auth_info error 46 "can't start", or
   0xa0020102 module load crash). This broke Townfall AND Astro Bot on Oct 6.
3. Modified executables must be fake-signed first:
   `python3 make_fself.py in.elf out.self` (AGENTS.md canonical path
   `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/
   samples/install_app/`; verified working local copy:
   `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/
   native-sdk-build-check/ps5-payload-sdk/samples/install_app/make_fself.py`).
4. Verify EVERY executable upload with decrypt-OFF readback (magic
   `4f153d1d`/`5414f5ee` + FULL sha256 match) BEFORE launching anything.
5. NO rapid-fire launch tests: failed launches leave kstuff paused
   (ShadowMount pauses 15-25 s after launch, resumes only on clean game
   exit), and then ALL games fail auth until a clean exit or reboot. One
   launch per test; clean exit between launches; after any crash → harvest →
   reboot before the next launch.
6. kstuffSharp/InstallerApp kernel module is QUARANTINED (FW 10.01 hardcoded
   offsets, crashed the 13.60 kernel Oct 6 20:26 UTC). Never autoload it.
   Eboot DT_NEEDED patching stays suspended until reworked with fake-signing.
7. ploader exits per game launch cycle — re-arm via
   `pldmgr /loadpayload` between launches in one boot; unmodded play = empty
   `ploader.ini`; never touch `autoload.txt`.
8. Settings discipline: `ue4ss/UE4SS-settings.ini` must remain EXACTLY the
   loadlist-declared 1947 bytes after any edit (MFA size-exact admission).
9. Reference material: `outputs/2026-10-06-recovery-fakesign/POSTMORTEM.md`
   and `outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md`.

## Interpretation matrix v2 — 2026-10-08 evening

Appended by agent Z7; supersedes §3.3 BY ADDITION (§3.3 left unchanged for
provenance). New inputs incorporated:
- **agent-V-listener-abi.md** — CONFIRMED OOB call mechanism: engine is UE
  5.6.1; `FUObjectDeleteListener` slot 4 (`SIZE_T GetAllocatedSize() const`)
  was missing from our base, so the engine's virtual dispatch at
  `UObjectArray.h:1471` (`GetDeleteListenersAllocatedSize` ←
  `LogHashMemoryOverheadStatistics`, `UObjectHash.cpp:2416`, reachable during
  save-load bookkeeping) read one word PAST our vtable and called garbage.
  Registration ALSO writes the engine's live `UObjectDeleteListeners` TArray
  UNLOCKED and grows it with OUR allocator (`port/unreal/src/UObjectArray.cpp:379-387`),
  while the engine holds `UObjectDeleteListenersCritical` (THREADSAFE_UOBJECTS=1).
  Variant A as built carries the ABI repair:
  `2026-10-08-l25-runtime/typescan-r4/candidate/include/Unreal/UObjectArray.hpp`
  declares `virtual size_t GetAllocatedSize() const { return 0; }` LAST
  (slot 4). Variant B compiles its token units with registration skipped
  (`build_variants.py`: l25-B `delete_listener_registration: skipped`).
- **agent-Q-copyin-probe.md** — copyin sink bank is RETIRED at init (parent
  fd + 64 O_EXCL sink fds closed at `copyin_cleanup.complete()`, which
  precedes the UE4SS.log "slot 2 FName ctor bound" print); NO post-admission
  probe caller exists in any of the 374 composed units (all call sites are
  inside init's `consume_required_slot_seeds`). The "resident copyin probe
  races save-load" attribution is WEAKENED; residual post-admission writers
  are modbridge-owned-buffer copyins only (ack sink fd, UE4SS log fd, diag
  channel fd — transition-only writes).
- **evidence/VANILLA-VERDICT.md** — same-save vanilla load: ZERO
  fatal-signal/copyin/nonsleeping/libkernel klog hits. The CE-108255-1
  `copyin: has nonsleeping lock` signature REQUIRES our runtime's presence.
- **agent-Z2-ploader-timing.md** — arming law, now §1.5 above.
- **agent-P-phase-tags.md** — the `[l25]` phase-tag strings used below.

### Outcome table (summary)

| Outcome | Verdict | Immediate next action |
|---|---|---|
| A-FULL: A survives save load + mod starts | Listener family was BOTH the crash and the stall — full success path | Keep A resident; do NOT run B; fold V's lock/allocator registration fixes into l26 |
| A-STALL: A survives save load, mod still doesn't start | Crash = listener family; stall = something else past :528 | Localize with the [l25] phase ladder; run B only if rung 1 reproduces |
| A-CRASH: A still crashes at save load | Listener family insufficient | Residual-writer hunt (Q checklist below) + run B to bracket registration |
| B-SURVIVES: B survives save load where A crashed | Registration/write race (unlocked write) confirmed as the crasher; slot-4 OOB secondary | l26 = locked, engine-allocator registration (keep slot-4 fix); restore A resident |
| B-CRASH: B also crashes | Neither registration nor listener — resident machinery beyond the listener (rare given vanilla verdict) | Stop launches; escalate to Claude with phase-tag evidence |

### A-FULL — A survives save load AND the mod starts

Verdict: the listener family was BOTH the save-load crash (slot-4 OOB vtable
dispatch during save-load memory bookkeeping, agent V §3) AND the init stall
(unlocked/own-allocator TArray registration write, agent V §4). Full success
path.

Grep (UE4SS.log): `Adding shutdown listener` followed by
`Starting callback garbage collector!`, then `[l25] event loop started`,
`[l25] on_program_start entered`, `[l22] Lua mod started:`,
`[R3] FuncPtr-marshal armed on carrier`,
`[l25] marshal hook first invocation (carrier fired)`,
`[R3] marshal is the game-thread pump`. Grep (diag/aligned):
`event_loop_started=1`, `on_program_start_entered=1`, `r3_marshal_first_invoke`,
periodic `r3_marshal_pumped`, and the pad chain `pad_l3r3_edge` →
`pad_l3r3_f7_dispatched` (or `pad_key_edge` value 118 +
`[l22] keybind callback entry`). Grep (klog): ZERO `copyin`/`nonsleeping`
hits across the whole run including save-load + 30 s. L3+R3 opens the menu.

Next action: keep A resident; do NOT run variant B. l26 folds agent V's
remaining hazards into permanent fixes (registration under
`UObjectDeleteListenersCritical` with the engine allocator, or call the
engine's own exported `AddUObjectDeleteListener`; keep the slot-4 virtual).
Record the run in packet Stage 2 and advance the mission ladder (Item
Spawner menu prove-out).

### A-STALL — A survives save load but the mod still doesn't start

Verdict: crash = listener family (save load survived with the slot-4 fix);
stall = something ELSE past :528. Localize with the [l25] phase ladder —
each rung's ABSENCE localizes the stall:

1. `Adding shutdown listener` present but `Starting callback garbage
   collector!` AND `[l25] event loop started` absent → still stalled
   at/inside init's :528 region (registration-adjacent, not the vtable):
   agent-O listener-instrumentation markers are the tiebreaker (placeholder
   below); absent terminal ack in `runtime-status.txt` corroborates.
2. `[l25] event loop started` present but `[l25] on_program_start entered`
   absent → loop thread alive and scheduled, but on_program_start never
   reached (gating upstream of it).
3. `[l25] on_program_start entered` present but `[l22] Lua mod started:`
   absent → stall inside the on_program_start chain (install_lua_mods →
   LuaMod::on_program_start → start_lua_mods): check
   `[TypeScan] scan-complete … core-ready=true`, `[R3] event carrier scan`,
   `[R3] marshal arm failed`.
4. `[l22] Lua mod started:` present but L3+R3 dead → pad/marshal side:
   `[R3] FuncPtr-marshal armed`, `[l25] marshal hook first invocation
   (carrier fired)`, `r3_marshal_pumped`, `[l22] ExecuteInGameThread queued
   depth=` (depth stuck >0 with no drain = pump not draining),
   `pad_borrow_begin` / `pad_first_state_read` / `pad_key_edge`.

Next action: crash attribution is settled (listener); run variant B ONLY if
rung 1 reproduces (brackets the registration call itself); otherwise
instrument the localized rung in l26 and harvest per §3. Never claim a
rung passed without its marker actually present.

### A-CRASH — A still crashes at save load (CE-108255-1 / `copyin: has nonsleeping lock`)

Verdict: the listener family is INSUFFICIENT (slot-4 fix + normal
registration did not stop the crash). Next suspects, in order: (1) agent Q's
residual modbridge-owned-buffer copyin writers (ack sink fd, UE4SS log fd,
diag channel fd — transition-only but still resident copyin paths); (2) any
remaining unlocked live-state writes; (3) agent F's secondary candidate —
uninstrumented LoadMap detour / hook path. Note the attribution shift: the
resident copyin PROBE itself is retired at init (fds closed; zero
post-admission callers among all 374 composed units), so "resident probe
races save-load" is now a LOW-PRIOR hypothesis.

Check in A's harvest:
- `copyin_probe_fire_post_admission` (diag event + WARNING UE4SS.log line) —
  any post-admission probe dispatch attempt. Meaningful ONLY if agent Q's
  overlay is actually wired into this build (verify adoption per
  `2026-10-08-l25-runtime/copyin-probe/WIRING.md` and the build receipt);
  if not wired, its absence is NOT evidence.
- `[Seeds][L25] copyin probe state after admission: kernel-write dispatch
  STOPPED` (UE4SS.log) + `copyin_probe_liveness_state=3` and
  `copyin_probe_liveness_oracle_calls` (diag) — proves the bank was retired;
  nonzero oracle_calls near crash time reopens the probe hypothesis.
- Bridge-side diag: `copyin_probe_state`, `copyin_probe_sinks_open`,
  `copyin_probe_nonce_used`, `copyin_probe_attempts_total`,
  `copyin_probe_attempts_late_total` — sinks actually closed; any late
  attempts. (Overlay-dependent, same caveat.)
- Pre-existing non-overlay proof of retirement: `slot 2 FName ctor bound` in
  UE4SS.log (admission cleanup completes before that print, agent Q §2).
- Aligned-entry log: `modbridge-runtime-aligned-entry/v1 build=…` — record
  the build id; last state transitions before the crash timestamp.
- klog: `copyin: has nonsleeping lock` + RIP, and `fatal signal`/`libkernel`
  lines; the signature's presence still indicts our runtime (vanilla had
  zero hits). Record the crash moment (menu / save-load / L3+R3).

Next action: run variant B (§4 procedure; reboot first) to bracket
registration vs the rest; if B also crashes, see B-CRASH. Escalate to Claude
with the A harvest either way.

### B-SURVIVES — B (registration skipped) survives save load where A crashed

Verdict: the registration/write race is CONFIRMED as the crasher — the
UNLOCKED `Contains`/`Append`/`RemoveSingleSwap` on the engine's live
`UObjectDeleteListeners` TArray grown with OUR allocator (agent V §4),
racing a concurrent engine dispatch under `UObjectDeleteListenersCritical`
and/or corrupting the engine heap on save-load churn. The slot-4 OOB becomes
SECONDARY for this crash — still a real, confirmed OOB; keep the fix, since
any registered listener would hit it on memory-stats paths.

Grep: B's UE4SS.log shows post-skip markers (agent-O placeholder below),
`Starting callback garbage collector!`, `[l25] event loop started`,
`[l22] Lua mod started:`; the save-load + 30 s window completes; klog has
zero `nonsleeping`/`copyin` hits. Contrast against A's crash harvest
(same save, same steps).

Next action: l26 fix = registration must take the engine lock and use the
engine allocator (or call the engine's exported `AddUObjectDeleteListener`),
with the slot-4 virtual kept. After B testing, restore variant A (or the
l24 composition) as resident — B must NEVER remain (§4.2/§4.4).

### B-CRASH — B also crashes at save load

Verdict: neither registration nor the listener — points at resident
machinery BEYOND the listener. Rare given the vanilla verdict (the signature
requires our runtime, so the fault is still ours — but not the listener
family): widen to the residual modbridge-buffer copyin writers, the
uninstrumented LoadMap detour/hook path (agent F's secondary), and other
unlocked live-state writes.

Grep: run the full A-CRASH checklist against B's harvest, plus the [l25]
phase ladder up to the crash — the LAST reached phase tag brackets the fault
window (correlate its timestamp with the klog `nonsleeping`/RIP line).

Next action: STOP launching (SAFETY BOX 5: no rapid-fire tests; reboot
before any further launch). Escalate to Claude with BOTH harvests, the
phase-tag bracket, klog, and the vanilla-verdict contrast. No blind replays
of failed configurations.

### Listener-instrumentation placeholder (agent O)

`agent-O-listener-instrumentation.md` is STILL ABSENT from
`2026-10-08-l25-prep/` as of this writing (2026-10-08 evening). Until it
lands, the listener-side tiebreakers are: the pre-existing
`AddUObjectDeleteListener`-adjacent log family, the generic `listener-`
prefix patterns in `harvest/markers.json`, and the [l25] phase ladder above.
Before the first launch, fill in the exact post-registration marker strings
(registration entered/completed, ABI verify result, listener-invoked trace)
from that report — do not invent strings. (A-STALL rung 1 and B-SURVIVES
both depend on these markers.)

## Launch order v3 — B first (Claude review 2, 2026-10-08)

Appended by agent-AF from Claude's REVISED directive (review 2, relayed by
the coordinating session in response to `agent-Z11-stack-symbolize.md`).
This section supersedes the ORDER ONLY: variant B launches FIRST, variant A
SECOND — replacing §4's "only if A stalls or crashes" trigger and the
A-first ordering of §2 / the Interpretation-matrix-v2 outcome table. Every
other rule in this plan stands unchanged (upload gate, packet-to-Claude
Stage 1 before any upload, PRE-LAUNCH, SAFETY BOX, one launch per explicit
user OK, B never left resident). Z11's solved chain is the reason:
`AddUObjectDeleteListener` → `TArray Append → ResizeTo → FMemory::Realloc
→ RC::Unreal::FMalloc::Realloc` whose `call rax` is the FIRST live call
into the game's (repointed) GMalloc, and the in-allocator `call [rax+0x10]`
virtual dispatch landed on the game's fatal-error formatter → abort →
CE-108255-1 — a deterministic init-completion crash on the UE4SS init
thread, not save-load-timed. Variant B performs NO Append → no
`FMemory::Realloc` → no live GMalloc call from the registration path at
all, and is therefore predicted to survive init AND potentially reach mod
start — the path to the first real look at mod start, the pump, the pad,
and the menu.

### v3.0 — Claude's revised directive (faithful transcription)

- BEFORE any launch: Mac-only items 1-4 must land (offset verification,
  GMalloc audit, container audit, extended A logging) — reference the agent
  reports as they exist.
- LAUNCH ORDER: Variant B FIRST (it's the path to the first real look at
  mod start, the pump, the pad, and the menu). Then variant A second, only
  to confirm the mechanism (with item 4's logging).
- B test procedure: console restart first; restore ORIGINAL
  UE4SS-settings.ini byte-exact 1947 B; main menu 90 s; then load the save,
  wait 30 s, L3+R3 once. IF THE MENU OPENS: harvest, then D-pad + one spawn
  IN THE SAME SESSION, then harvest again.
- Variant C (proper fix) designed after the B result; packet + Claude
  review + user OK as always.

### v3.1 — Pre-launch gate: Mac-only items 1-4 (blocking, before ANY upload)

No console contact for any of these; they land as reports under
`2026-10-08-l25-prep/` and the twin tree before STEP 0 closes. Grounding in
the reports as they exist on disk today:

1. **Offset verification.** Verify every virtual-slot offset our port
   dispatches through against the real engine image — first target the
   allocator side: the game-side `call [rax+0x10]` vtable slot that turned
   the first live GMalloc call into the fatal formatter. Existing reports:
   `agent-Z11-stack-symbolize.md` (§3/§5/§6: the symbolized chain, the
   +0x10 dispatch site, the sharpened suspect) and
   `agent-Z8-listener-layout.md` (the compiler-verified layout-proof method
   to replicate for the FMalloc/GMalloc vtable).
2. **GMalloc audit.** Audit the GMalloc repoint (l24 log line
   "Post-initialization: GMalloc: 0x970add8 -> 0x97089b8") and our
   `RC::Unreal::FMalloc` vtable layout: is the pointer we repoint through
   the allocator object the engine actually uses, and does slot +0x10 mean
   what our port assumes? Existing reports: `agent-Z11-stack-symbolize.md`
   (sharpened suspect), `agent-V-listener-abi.md` (the same
   family of slot-mismatch bug, confirmed for the listener vtable),
   `agent-F-crash-forensics.md` / `agent-G-mod-start-gating.md` for the
   l21 "Virtual FMalloc::QuantizeSize unavailable" precedent.
3. **Container audit.** Enumerate every REMAINING init-time growth of an
   engine-owned container that goes through `FMemory`/GMalloc after the two
   listener registrations are skipped — the "crashes one line later" hazard
   (see v3.4). Claude review 2 cites an agent-AC audit for this; as of this
   writing NO `agent-AC` report exists in `2026-10-08-l25-prep/` — this
   item is that deliverable and must land before the launch gate closes.
   Its existing inputs: `agent-Z11-stack-symbolize.md` (the crash chain is
   generic: `Append → ResizeTo → FMemory::Realloc → FMalloc::Realloc`,
   any such growth reproduces it), `agent-Z6-getallocatedsize-reachability.md`
   (mechanism B: unlocked live-array write), `agent-V-listener-abi.md` §4
   (`port/unreal/src/UObjectArray.cpp:379-392`).
4. **Extended A logging.** Extend variant A's logging so the A run (which
   now FOLLOWS B) can confirm the mechanism rather than merely reproduce
   the crash. Existing reports: `agent-O-listener-instrumentation.md`
   (`[L25][DeleteListener]` log family + `l25_dl_` diag tags),
   `agent-P-phase-tags.md` (`[l25]` phase ladder),
   `agent-Q-copyin-probe.md` (probe-liveness lines). Whatever review 2
   specifies on top of these must be verified landed in the A tree before
   STEP 0.2's twin artifacts are hashed.

### v3.2 — B-FIRST launch procedure (replaces §2/§4 ordering; steps unchanged)

All of PRE-LAUNCH (§1 incl. 1.5 arming law), SAFETY BOX, and the
packet Stage-1 gate still apply. Get explicit user OK, then:

1. Console RESTART first (kstuff paused after the l24 crash run; §1.1).
2. Root: full PRE-LAUNCH per §1 with the variant B PRX as the staged
   artifact (STEP 0.4 upload + decrypt-OFF readback verify), and the
   ORIGINAL UE4SS-settings.ini restored byte-exact 1947 B
   (`/tmp/l24-settings-backup.ini` — legacy, durable at `evidence/l24-settings-backup.ini`, sha256 `b980cea7…`, §1.2) — never the
   A/B v2 residue. `harvest/prep_modded_launch.py --variant B --prx-path
   <variant-B.prx>` performs exactly this (v3.4).
3. User: launch the game (Townfall, PPSA33286) — ONE launch.
4. Main menu: WAIT 90 s. Watch for init passing :528 — in B the proof is
   the SKIPPED line plus what FOLLOWS it (v3.3). The 90 s also covers the
   PS5Pad ≥10 s uptime needed for the pad borrow.
5. Load the SAME save used in the l24 crash runs.
6. WAIT 30 s past save-load.
7. Press L3+R3 ONCE (hold L3, tap R3 or vice versa — no frame-perfect
   simultaneity needed; one chord completion = one F7; OPTIONS stays the
   game's pause).
8. **IF THE MENU OPENS** (on-screen: the dark red "TOWNFALL : ITEM
   SPAWNER" panel, top-right, and the game stops responding to look/move —
   the mod locks input on open): root HARVESTS IMMEDIATELY (read-only FTP
   pull of `UE4SS.log` + diag session + runtime-status into a NEW dated
   harvest dir; game keeps running), then the user performs the in-session
   spawn test (v3.3) IN THE SAME SESSION, then root harvests AGAIN (second
   pull, NEW files — the diff between the two pulls isolates the spawn
   proof).
9. Clean exit (in-game quit). If it crashes at any step: note the code
   (expected signature CE-108255-1) and WHEN (menu / save-load / L3+R3 /
   spawn); root harvests immediately; REBOOT before any next launch.
10. After B testing: restore variant A (or the l24 composition) as
    resident — B is diagnostic-only, unsafe long-term (stale object
    pointers never cleared), and must NEVER remain (§4.2/§4.4).
11. THEN variant A, SECOND, only to confirm the mechanism, run with item
    4's extended logging — same procedure as steps 3-9 with the A PRX.

### v3.3 — In-session spawn test ("D-pad + one spawn"), from the mod's actual code

Mod under test: TownfallItemSpawner `scripts/main.lua`, exactly 21415 B —
provenance copies of the console-resident file:
`outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/main.lua`
and the l22-package-r2 before/after/harvest triple (byte-identical). The
mod registers SIX keybinds (`main.lua:694-729`), every callback wrapped in
`ExecuteInGameThread(...)`; the pad layer maps pad chords onto them via
`ue4ss/PS5Pad.ini` (staged with the mod; l12 rebind; chord = conjunction
match, one event per rising edge — `PS5PadSource.cpp:154-171`):

| Pad input | UE4SS key | Menu action | main.lua |
|---|---|---|---|
| L3+R3 (chord) | F7 | open/close menu (`toggleMenu`) | :694 |
| D-pad UP | UP_ARROW | selection previous (`moveSelection(-1)`) | :698 |
| D-pad DOWN | DOWN_ARROW | selection next (`moveSelection(1)`) | :703 |
| CROSS | RETURN | activate selection — **spawn** on a pickup row (`activateSelection`) | :708 |
| D-pad LEFT | LEFT_ARROW | page previous | :713 |
| D-pad RIGHT | RIGHT_ARROW | page next | :722 |

OPTIONS is deliberately NOT bound (game's own pause). UE4SS key codes for
the diag `pad_key_edge` values (bounded 60 tags/boot): F7=118 (0x76),
RETURN=13 (0x0D), LEFT/UP/RIGHT/DOWN_ARROW=37/38/39/40 (0x25-0x28).

Menu row layout (`buildRows`, `main.lua:375-422`; selection starts at
row 1): row 1 "Quantity" (OPTIONS; default x1 — `quantities[1]=1`, so one
CROSS = one pickup), row 2 "Page" (OPTIONS), row 3 "Refresh pickup list"
(OPTIONS), rows 4-11 the eight PICKUPS slots of the current page, final row
STATUS (read-only). Therefore, operationally, with the menu open:

1. Press DOWN three times → selection lands on the first PICKUPS row
   (row 4). If it displays `---` (empty page), press RIGHT once (next
   page) and DOWN as needed.
2. Press CROSS ONCE → `spawnSelected` runs on the game thread: ground
   trace 150 cm ahead of the pawn, `BeginDeferredActorSpawnFromClass` +
   `FinishSpawningActor`, pickup configured, quantity = current Quantity
   (x1 by default). The STATUS row updates and UE4SS.log gains the proof
   line (below). One CROSS = one spawn; do not re-press while STATUS shows
   "Wait for the previous spawn." (`busy` guard).
3. F7-equivalent (L3+R3) closes the menu; do that only AFTER the second
   harvest pull if the STATUS line is the wanted live corroboration.

Mod log markers (UE4SS.log; the mod's `log()` prints the
`[TownfallItemSpawner]` tag — `main.lua:5-6`). NOTE: the F7 toggle itself
and selection moves log NOTHING; the only in-log menu-open evidence is the
FIRST-open pair (item scan + "Menu created."), so the first pull (step v3.2
#8) must happen while that session still owns the log:

- Mod start: `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only
  release. F7 opens the menu.` (`:746`)
- First menu open — scan: `[TownfallItemSpawner] <N> spawnable pickups
  found. Pages=<P>` (`:189`); failure variant `Scan failed: <err>` (`:194`,
  e.g. "Item database not ready. Load a save first." if pressed too early)
- First menu open — UI: `[TownfallItemSpawner] Menu created.` (`:643`);
  failures `UI creation failed: <err>` (`:636`),
  `WidgetBlueprintLibrary not found.` (`:502`)
- SPAWN SUCCESS: `[TownfallItemSpawner] Spawned <name> x<qty> ID=<id>`
  (`:335`) — the spawn proof
- SPAWN FAILURE: `[TownfallItemSpawner] Failed at <stage>. See UE4SS.log.
  <err>` (`:342`), stage ∈ {player, ground trace, load pickup class, spawn
  actor, configure pickup}

What the harvest tool should grep for the spawn proof (currently MISSING
from `harvest/markers.json` — add or grep manually): in UE4SS.log
`[TownfallItemSpawner] Spawned ` (success), `[TownfallItemSpawner] Failed
at ` (failure), plus the menu-open pair (`spawnable pickups found`,
`Menu created.`); in the diag session `pad_key_edge` value 13 (the CROSS →
RETURN edge) and `pad_l3r3_f7_dispatched` (the open); in UE4SS.log
`[l22] keybind callback entry key=118` (F7) then `key=13` (RETURN), each
followed by `[l22] ExecuteInGameThread queued`/`drained … (callback entry)`
pairs (LuaMod.cpp:2040/:5650/:4367) — those pairs prove the pump drained
the menu action, not just queued it.

### v3.4 — Harvest expectations for variant B

Expected UE4SS.log/diag marker SEQUENCE if all works (all strings are the
landed vocabulary in `harvest/markers.json`; never invented):

1. `Adding shutdown listener` (`:527` — last l24 line; B must pass it)
2. NO `[L25][DeleteListener]` registration lines — no
   `shutdown-listener registration begin`, no `registration returned in`,
   no `Add enter (GUObjectArray=`, no `header pre:/post:` — and no
   `l25_dl_add_*` diag tags; ONLY the SKIPPED line:
   `delete-listener registration SKIPPED (variant B; unsafe long-term:
   stale object pointers not cleared)` (`UnrealInitializer.cpp:557/:578`;
   BOTH registrations — class-cache and shutdown — are skipped under
   `MODBRIDGE_L25_SKIP_DELETE_LISTENER`, agent R §2)
3. `Starting callback garbage collector!` (`:542` — first proof init
   passed :528 in B)
4. `[l25] event loop started` (diag `event_loop_started=1`)
5. `[l25] on_program_start entered` (diag `on_program_start_entered=1`)
6. `[l22] Lua mod started: TownfallItemSpawner (main script returned)`
   followed by `[TownfallItemSpawner] Loaded v1.0. …`
7. `[R3] FuncPtr-marshal armed on carrier` → `[l25] marshal hook first
   invocation (carrier fired)` (diag `r3_marshal_first_invoke=1`) →
   throttled `r3_marshal_pumped` heartbeat
8. Pad chain (diag): `pad_available` → `pad_first_state_read` →
   `pad_key_edge` value=118 → `pad_l3r3_edge` (+ `_allow`/`_subscribed`)
   → `pad_l3r3_f7_dispatched`; then `[l22] keybind callback entry key=118`
   + `ExecuteInGameThread queued`/`drained` pairs
9. Menu markers (v3.3): `spawnable pickups found. Pages=` then
   `Menu created.`; on the in-session test: `[TownfallItemSpawner]
   Spawned <name> x1 ID=<id>`
10. klog: ZERO `copyin`/`nonsleeping`/`fatal signal` hits for the whole
    run INCLUDING save-load + 30 s + L3+R3 + spawn; clean exit confirmed by
    the eboot monitor and `Stopping mod TownfallItemSpawner for uninstall`.

B-specific FAILURE MODES to watch (beyond the B-CRASH § of the
Interpretation matrix v2, which still applies):

- **Another container growth crashing one line later** (the container-audit
  hazard, v3.1 item 3): if the true fault is the GMalloc
  dispatch/repoint/vtable rather than the registration per se, skipping the
  listener Append only removes the FIRST consumer — the next init-time
  engine-container growth through `FMemory::Realloc → FMalloc::Realloc`
  (e.g. later init allocations, GC-start structures, FName/mod-install
  growth) reproduces the identical `call rax` → `call [rax+0x10]` → fatal
  formatter → CE-108255-1 chain one line later. Per the agent-AC audit
  cited in Claude review 2 (report not yet on disk; existing grounding:
  `agent-Z11-stack-symbolize.md` §6 — the chain is generic — and
  `agent-Z6-getallocatedsize-reachability.md` mechanism B). The `[l25]`
  phase ladder localizes it: the LAST reached tag brackets the fault
  window; correlate its timestamp with the klog RIP line.
- **Mod start blocked elsewhere** (unrelated to the listener family): the
  A-STALL rung ladder of the Interpretation matrix v2 applies UNCHANGED to
  B past :528 — `event loop started` absent → still in init's :528 region;
  loop present but `on_program_start entered` absent → gated upstream;
  `on_program_start` present but `Lua mod started:` absent → stall inside
  install_lua_mods (check `[TypeScan] scan-complete … core-ready=true`,
  `[R3] event carrier scan`, `[R3] marshal arm failed`); mod started but
  L3+R3 dead → pad/pump side (`pad_borrow_begin`, `pad_first_state_read`,
  queue-depth stuck >0 with no drain).
- **Menu opens but spawn fails**: still a VALID B result (init + mod start
  + pump + pad all proven); the `Failed at <stage>` line names the stage —
  record it, do not retry-spam (one spawn per session per the directive).

### v3.5 — Prep script note (no mechanical change)

`harvest/prep_modded_launch.py` is UNCHANGED mechanically: `--variant B
--prx-path <variant-B.prx>` performs exactly the §1 procedure (precondition
daemons + ploader-lock check, PRX signed-SELF gate, decrypt-OFF backup +
upload + byte-exact readback, 1947 B settings restore, ploader.ini restore
+ single arm, klog tap + eboot monitor) and writes the LAUNCH-READY-B
receipt with the SKIPPED-line expectation. The v3 change is ORDER AND
DOCS ONLY: its usage text and the B receipt now state the v3 order (B
first, A second per Claude review 2) and the in-session spawn-test branch;
no gate, check, command, or exit-code behavior was touched. The twin-build
machinery (agent R) is likewise order-agnostic — build both variants as
before; only the upload/launch order changes.

## Interpretation matrix v4 — post-offset-fix (2026-10-08 night)

Appended by agent AR; supersedes the v2 outcome table and the v3.4 B-hazard
notes BY ADDITION (v2/v3 left unchanged for provenance). New inputs, all in
this directory: `agent-AA-offset-verify.md` (offset root cause PROVEN from
engine code in the authenticated image), `agent-AE-variant-c.md` (variant C,
patch ready), `agent-AC-container-audit.md` (post-:528 site table + the
G1–G4 plan), `agent-AD-item4-logging.md` (pre-Append GMalloc logging +
refusal code 6), `agent-AO-gate-extensions.md` (G1–G3 landed, nm/preprocessor
proven), plus Z11/Z6/Z10/O/P as cited. Every marker string below is the
landed vocabulary (O §5, P §1, AO §1–3, AD §6, v3.3) — none invented.

### v4.0 — The new baseline (what the offset fix changes)

AA proved the l24 profile wrong from the engine's own code: the generator
assumed a 0x28-byte critical section; the shipping Prospero build uses a
0x18-byte lazy-mutex form, shifting every field past ObjObjectsCritical. The
REAL members are UObjectCreateListeners @0x58, UObjectDeleteListeners @0x68,
UObjectDeleteListenersCritical @0x78, PrimarySerialNumber @0x90, total 0x98
(the l24 pins 0x68/0x78/0xB0/0xB8 are all wrong). The port's generated
accessor therefore treated the CRIT STATE WORD (GO+0x78) as "Data" and the
lazily-allocated mutex-pointer halves (GO+0x80/+0x84) as "Num"/"Max" — so
the Z11-symbolized chain (`AddUObjectDeleteListener → TArray Append →
ResizeTo → FMemory::Realloc → FMalloc::Realloc → call rax → game fatal
formatter → abort`, byte-identical in all six l24 crashes) handed the engine
allocator a NON-HEAP pointer and corrupted the crit it was simultaneously
misreading. With the profile corrected, `Data` at 0x68 is a genuine engine
heap block: the l24 crash mechanism (non-heap Data → allocator reject) is
eliminated AT THE SOURCE for variants A and C. For B it is moot — the
registration is compiled out entirely.

**PRECONDITION (verify at STEP 0, before any upload):** the l25 build
receipt / static-gate tables (BuildProfile / LayoutPreflight) pin the
CORRECTED values — create 0x58 / delete 0x68 / crit 0x78 / serial 0x90 /
total 0x98 — and no l24-era delete=0x78 pin survives. If the 0x78 delete pin
is still live, variant A is still the l24 crasher and this matrix does not
apply to it.

Residual risk map after the fix:

- **Variant A** (hand-rolled hardened+instrumented Add, now with real Data):
  (i) the UNLOCKED write vs the engine's locked dispatch — Z6 mechanism B
  (the engine's own Add locks the crit at +0x78; ours cannot — O §4);
  (ii) foreign-thread GMalloc use — now with a VALID block, accepted per the
  engine's own code path (AE §3: growth is the engine's GMalloc end-to-end
  via FMemory::Realloc → GMalloc@0x9340950 → vtable+0x40, and the engine
  itself registers listeners from 14 boot-time call sites);
  (iii) the R3-consumer GetFullName cluster — REMOVED by G2 (ToString-free
  `[n: X]` prints); (iv) the second registration site
  (`LuaMod::on_program_start`, audit line 7995 / tree line 8076-8091) —
  GATED by G1.
- **Variant B = the control**: both registrations + the cache block +
  the GetFullName prints all compiled out (AO: 0 symbol references in the
  B object; preprocessor counts create=0/class-delete=0/shutdown-delete=0).
- **Variant C = the proper fix**: call the engine's own locked
  `AddUObjectDeleteListener` — runtime 0x8841A0 (image 0x4841A0), rdi =
  &GUObjectArray (runtime 0x97BA110), rsi = listener; crit at +0x78 is
  RECURSIVE (the OnUObjectArrayShutdown self-Remove re-entry is legal);
  engine allocator end-to-end. Patch ready and review-clean:
  `outputs/2026-10-08-l25-runtime/variant-c-proposal/variant-c-patch.diff`
  (additive, 8 hunks, `MODBRIDGE_L25_ENGINE_ADD_LISTENER`, clean refusal
  code 7 `EngineListenerCallUnavailable`, keeps every instrument marker;
  `add_done`/`remove_done` carry elapsed-ms).

### v4.1 — Outcome table (B first, then A, then C)

| # | Outcome | What it proves | Immediate next action |
|---|---|---|---|
| **B-1** | B survives + mod starts + menu opens + spawn works | The l24 crash was ENTIRELY the listener-registration family (offset-corrupted header → non-heap Data → allocator reject); the rest of the chain (pump, pad, Lua, spawn) is healthy | Harvest both pulls; restore A as resident (B is NEVER left resident); run A second with item-4 logging; then C |
| **B-2** | B survives + mod starts + menu silent | Init, registration-independent path, and mod start all OK; fault is pump/pad/mod-side | Walk the B-2 ladder (v4.3): armed → first_invoke → pumped/eigt drained → pad edges → keybind entry; fix the last-reached rung |
| **B-3** | B survives + mod does NOT start | Stall past :528 OUTSIDE the gated family | Walk the B-3 ladder (v4.4); a main.lua-level fault shows NO `[l22] Lua mod started` marker with mod prints present — game alive = mod hang, game dead = mod crash; separate mod-level bisect |
| **B-4** | B still crashes (G1–G3 in place) | Points OUTSIDE the listener/registration family entirely — contradicts Z10/Z11; treat as a build/packaging error FIRST | The klog backtrace IS the evidence (B has NO pre-Append logging by design — v4.5); STOP, reboot, escalate |
| **A-5a** | A survives registration (EXPECTED now: real Data @0x68) | Corrected profile works at the source; hand-rolled path OK on console | Watch save-load + `engine called …` dispatch markers; proceed to C as the production shape |
| **A-5b** | A crashes AT registration | The item-4 pre-line names the fault — that is its purpose (v4.6) | Map the logged anomaly per v4.6; a live mechanism-B observation (RISK-realloc crash, Num ANOMALY) sends us straight to C |
| **C-6a** | C survives (locked engine Add, full run clean) | Production shape proven: locked + engine allocator; mechanism B retired by construction | Ship C — packet Stage 1 → Claude review → user OK first; labels pinned to image sha 3b6cfad4, re-derive per title |
| **C-6b** | C REFUSED(7) / crashes inside engine Add / crashes after registration | 7 = resolver failure (NOT an engine fault); in-Add crash = foreign-thread hostility → AE/Z11 fallback (deferred game-thread registration via the R3 pump); post-registration crash with dispatch markers = listener callback body | Respective bisects (v4.7); escalate with both A and C harvests |

### v4.2 — B-1: B survives + mod starts + menu opens + spawn works

Full success path. Proof markers, in order (UE4SS.log unless noted):

1. `Adding shutdown listener` then the skip line `[L25][DeleteListener]
   delete-listener registration SKIPPED (variant B; unsafe long-term: stale
   object pointers not cleared)` (both init registrations skipped;
   G3's suppression line appears only if the ini had the cache on — today's
   1947 B ini has it false, so its absence is expected).
2. `Starting callback garbage collector!` (first proof init passed the
   skip region in B).
3. `[l25] event loop started` (diag `event_loop_started=1`) and
   `[l25] on_program_start entered` (diag `on_program_start_entered=1`).
4. The G1 skip line `[L25][DeleteListener] Lua-mod delete-listener
   registration SKIPPED (variant B; unsafe long-term: Lua-held stale object
   pointers never cleared and Lua onDelete callbacks never fire)` — proves
   `LuaMod::on_program_start` was entered past the audit's rank-1 hazard.
5. `[R3] FuncPtr-marshal armed on carrier [n: …] @0x… (callback id …)`
   (G2 format) and `[l22] Lua mod started: TownfallItemSpawner (main
   script returned)` + `[TownfallItemSpawner] Loaded v1.0. Clean
   pickup-only release. F7 opens the menu.`
6. `[l25] marshal hook first invocation (carrier fired)` (diag
   `r3_marshal_first_invoke=1`) + `r3_marshal_pumped` heartbeat.
7. Pad chain (diag): `pad_available` → `pad_first_state_read` →
   `pad_key_edge` value=118 → `pad_l3r3_edge` (+`_allow`/`_subscribed`) →
   `pad_l3r3_f7_dispatched`; then `[l22] keybind callback entry key=118`
   with an `[l22] ExecuteInGameThread queued`/`drained … (callback entry)`
   pair (`eigt_queue_depth` back to 0).
8. Menu: `[TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>`
   then `Menu created.`; in-session spawn proof:
   `[TownfallItemSpawner] Spawned <name> x1 ID=<id>`.
9. klog: ZERO `copyin`/`nonsleeping`/`fatal signal` hits for the whole run
   including save-load + 30 s + L3+R3 + spawn; clean exit shown by
   `Stopping mod TownfallItemSpawner for uninstall`.

What it proves: with the entire listener family (both registrations, cache
block, GetFullName prints) removed, everything downstream works — so the
l24 crash was the listener-registration family, exactly as AA's root cause
and Z11's symbolized chain predict. Honest scope: B also removes the G2
prints and (via `R3MarshalPumpPending()==true`) skips the eager detour-arm
block, but AC ranked those non-crashy (log-text-only / soft-refuse by
composition) and no observed crash ever sat in them — the registration
attribution rests on Z11's frames, which sat inside
`AddUObjectDeleteListener → Append → Realloc`. Next: restore variant A as
resident (NEVER leave B — §4.2/§4.4, v3.2 step 10), then run A per v4.6,
then C per v4.7.

### v4.3 — B-2: B survives + mod starts + menu silent

Init, mod start, and the registration-independent path are all proven; the
fault is on the pump/pad/mod-input side. Ladder — the LAST rung whose
marker is present is the healthy prefix; the first ABSENT marker is the
broken link:

1. `[R3] FuncPtr-marshal armed on carrier [n: …]` — absent → read the four
   G2 refusal lines (`[R3] marshal arm refused: carrier [n: …] has a null
   FuncPtr / is a script function (Func == ProcessInternal) / does not have
   FUNC_Native / RegisterPostHook returned an invalid callback id`) and
   `[R3] event carrier scan`: arming never succeeded, the pump has no
   carrier. Note the two ranked candidates legitimately refuse on
   no-FUNC_Native; a THIRD refusal is the anomaly.
2. `[l25] marshal hook first invocation (carrier fired)` (diag
   `r3_marshal_first_invoke=1`) — absent = the carrier function has never
   run on the game thread (it may need real gameplay, not just the menu);
   retest after save-load before concluding the swap failed.
3. `r3_marshal_pumped` heartbeat + `ExecuteInGameThread queued/drained`
   pairs (`eigt_queue_depth`) — depth stuck >0 with no drain = pump armed
   but not draining (dispatch path blocked past `IsInGameThreadRaw`).
4. Pad chain (diag): `pad_available` → `pad_first_state_read` →
   `pad_key_edge` value=118 → `pad_l3r3_edge`(+`_allow`/`_subscribed`) →
   `pad_l3r3_f7_dispatched` — a break anywhere here is the pad side
   (borrow needs ≥10 s uptime; chord = rising-edge conjunction), not the
   pump.
5. `[l22] keybind callback entry key=118` + queued/drained pair present but
   no menu = MOD side: read the menu-open failure lines —
   `[TownfallItemSpawner] Scan failed: …` (e.g. "Item database not ready.
   Load a save first." if F7 was pressed before the save), `UI creation
   failed: …`, `WidgetBlueprintLibrary not found.`

Next: fix the localized rung in the next build; none of these rungs
touches the listener family, so B's diagnostic skips are irrelevant here.
B is still restored-off after the session.

### v4.4 — B-3: B survives + mod does NOT start

Stall (game alive) or crash before mod start, past the gated family. The
[l25] phase ladder localizes; each rung's absence means:

1. `Starting callback garbage collector!` absent AND the B skip lines
   absent → the B gates are not in the shipped PRX (build/packaging error);
   verify the twin hashes and import identity before interpreting anything.
2. `Starting callback garbage collector!` present but `[l25] event loop
   started` absent → init stalls PAST the skips. Per AC's audit, what
   remains on the init path is all-LOW/no-GMalloc: the GC thread start, the
   HashTables-unavailable warning, the modules listing, the
   `setup_unreal_properties` FNAME_Find lookups (proven call class), and
   the StaticConstructObject arm at UnrealInitializer.cpp:546-574, which
   by composition soft-REFUSES before any engine write (expect the PLH SEV
   line + a "[…] Failed to add hook" Error line — noise, not a fault). A
   stall here is a class AC's table does not contain — genuinely new
   information; corroborate with `runtime-status.txt` (no terminal ack =
   init never completed).
3. `[l25] event loop started` present but `[l25] on_program_start entered`
   absent → update-thread loop alive, on_program_start gated upstream.
4. G1 skip line present (we are inside `LuaMod::on_program_start`) but
   `[l22] Lua mod started:` absent → check `[TypeScan] scan-complete …
   core-ready=true`, `[R3] event carrier scan`, and the R3 arming/refusal
   lines before start_lua_mods.
5. **Mod boundary — how to distinguish a main.lua-level fault** (AC §4
   FLAG): main.lua executes BEFORE the `[l22] Lua mod started:` marker, and
   its engine API usage (FindObject, ExecuteInGameThread queueing,
   RegisterKeyBind) is gated by NO runtime switch. Signatures:
   - Game ALIVE + `[TownfallItemSpawner] Loaded v1.0` printed but NO
     `[l22] Lua mod started:` → main.lua hung at its tail (mod-level).
   - Mod prints present, neither `Loaded v1.0` nor the marker, game DEAD
     (CE-108255-1) → crashed mid-script; the log tail sits INSIDE lua
     startup and the klog backtrace carries lua/pcall frames — a mod-level
     crash, NOT a runtime stall (runtime stall = no marker + game alive).
   - NO `[TownfallItemSpawner]` lines at all + G1 skip line present →
     never reached start_lua_mods → runtime-side; stay on this ladder.
   Next for a mod-level fault: separate mod-level bisect (Claude flagged;
   do not gate mod code blindly, do not touch the runtime for it).

### v4.5 — B-4: B still crashes (G1–G3 in place)

Prior: LOW — Z10's residual ranking made the registration the only
crash-supported suspect, and Z11 proved all six l24 crashes byte-identical
INSIDE the registration chain, which B compiles out (AO nm-proof: zero
references). If it happens anyway, the fault is OUTSIDE the
listener/registration family entirely. Checks, in order:

1. **The klog backtrace is THE evidence** — B has NO item-4 pre-Append
   logging by design (the logging lives in the `AddUObjectDeleteListener`
   callee that B never calls). Pull the backtrace and compare the modbridge
   frames against Z11's chain:
   - STILL shows `FUObjectArray::AddUObjectDeleteListener → Append →
     ResizeTo → FMemory::Realloc → FMalloc::Realloc` frames → the B gates
     did not reach the shipped PRX (build/packaging error): confirm via the
     B skip lines' absence in UE4SS.log, the twin hashes, and import
     identity. Reconcile the build; do not interpret as a new fault.
   - DIFFERENT modbridge frames → genuine new site: symbolize against the
     l25 combined object (Z11's method) and read the [l25] phase ladder —
     the LAST tag before the crash brackets the window (correlate its
     timestamp with the klog RIP line).
2. Crash MOMENT vs the l24 signature (deterministic, ~1 s after the last
   log line, settings-independent):
   - init-time → AC's remaining table (all ranked non-crashy — a hit there
     CONTRADICTS the audit; re-verify rank 4's soft-refuse composition
     assumption first);
   - save-load → Z10 residual #2: the runtime FName thunk (pathological
     UTF16 from save data → `seed_refused` [[noreturn]] throw on the game
     thread — a terminate shape, not abort-in-copyin); #4: R3 marshal
     pump / LoadMap detour (l24 evidence: no trampoline frames,
     settings-independent); plus the MOD BOUNDARY: main.lua's own engine
     API usage (ungated);
   - L3+R3 or spawn → first live dispatch of the rank-3 SetFuncPtr store /
     keybind path.
   - Z10 theoretical residuals if nothing above fits: #5
     `bridge_io_controls` (needs a foreign module's NID resolution; zero
     diag evidence), #6 ack/diag/log writers (own-buffer,
     transition-only — not the crash shape).
3. The vanilla contrast still binds: the signature requires our runtime
   (`evidence/VANILLA-VERDICT.md`) — the fault is ours, just not the
   listener family.

Next action: STOP launching (SAFETY BOX 5), reboot, harvest everything,
escalate to Claude with klog + phase bracket + the B harvest. No replays of
failed configurations.

### v4.6 — A-5: variant A second (corrected profile + hardened + instrumented)

Expected outcome after the offset fix: A SURVIVES registration — `Data` at
0x68 is now a genuine engine heap block, so the sane-header gate passes,
the growth path (if any) hands a valid block to the engine's own Realloc
(O §4: our FMemory wrapper routes through the seed-bound ENGINE GMalloc
instance — storage comes from and returns to the engine heap), and the
item-4 refusal 6 should NOT fire. Marker sequence that proves it:
`shutdown-listener registration begin (pre-marker)` → `Adding shutdown
listener` → `Add enter (GUObjectArray=` → `header pre: Data= …
growth_required= GMallocVar= GMalloc= GMallocInst= vptr=` → (no REFUSED)
→ `header post: Data= … (path=no-realloc|fresh-alloc|realloc)` →
`GMalloc post: GMallocVar= …` → `shutdown-listener registration returned
in <ms>` (diag `l25_dl_add_done`, `l25_dl_gmalloc_pre`/`_post`). Then the
listener goes live: `engine called FShutdownDeleteListener::
NotifyUObjectDeleted` (diag `l25_dl_first_notify_shutdown`) during
save-load is the slot-2 dispatch proof (slot-4 fix in the build covers the
memory-stats path regardless — and Z6 proved that path dead-stripped here
anyway).

**If A crashes AT REGISTRATION, the item-4 pre-line says why — enumerate:**

- `growth_required=1` + `vptr=0x0` with `GMalloc=` non-null → the FMalloc
  instance died/changed between seed bind and the Add — the allocator-slot
  family; the crash is the engine's allocator rejecting use of a dead
  instance (GMalloc-repoint hypothesis (a), now with data).
- Pre vs post `GMallocInst`/`vptr` differ (`GMalloc post:` line absent or
  changed) → the Append itself mutated the allocator chain mid-call.
- `REFUSED (6)` firing UNDER the corrected profile → the Data at 0x68 STILL
  failed the arithmetic screen: either the corrected profile is not in the
  running build (re-verify the STEP 0 static-gate pins — the l24 0x78 pin
  would reproduce exactly this) or the GUObjectArray anchor moved. Do NOT
  proceed to C until reconciled. (Code 6 firing = the screen WORKED —
  registration cleanly skipped, no Realloc attempted, init continues.)
- `header pre:` logged but NO `header post:`/`GMalloc post:` after
  `growth_required=1` → the fault is INSIDE Contains/Append/Realloc with
  the full allocator state recorded beforehand — the pre-line values plus
  the klog backtrace close the case together.
- `RISK: capacity full` (realloc path) logged, then crash at/after it →
  the engine-owned buffer swap ran UNLOCKED against the game thread —
  Z6 mechanism B observed live → go straight to C.
- `ANOMALY: Num advanced by` ≠ 1 → a concurrent engine-side writer raced
  our write — mechanism B observed live → go straight to C.
- Registration clean but save-load crashes WITH dispatch markers → the
  registration is exonerated; the listener callback BODY (slot-2 path) is
  the suspect.
- Registration clean but save-load crashes WITHOUT dispatch markers →
  mechanism B (array visible to the engine while inconsistent) or an
  unrelated family; C distinguishes — C locks by construction.

### v4.7 — C-6: variant C (engine-locked registration) — the production candidate

A+C shape: variant A's instrumentation with the registration routed through
the engine's own `AddUObjectDeleteListener` (runtime 0x8841A0; rdi =
&GUObjectArray 0x97BA110, rsi = listener). The engine Add takes the crit
at +0x78 (recursive) and grows via the engine's GMalloc end-to-end —
mechanism B is retired BY CONSTRUCTION. Outcomes:

- **C survives registration + full run clean** (`add_done` with elapsed-ms,
  dispatch markers fire, save-load + L3+R3 + spawn all pass, klog zero
  hits): the production shape is proven; the whole l24 failure family is
  closed as {offset corruption + unlocked/allocator-mismatched write}.
  Ship C. Labels are pinned to image sha 3b6cfad4 — re-derive per title;
  seed-slot promotion + admission-time prologue verification are AE's open
  follow-ups for Claude.
- **`REFUSED (7)` (EngineListenerCallUnavailable)**: the TU-local resolver
  failed to derive the Add/Remove labels (module base = GUObjectArray −
  0x93BA110; alignment/null checks). A resolver/seed problem, NOT an
  engine fault — fix the promotion, never blame the engine path.
- **Crash INSIDE the engine Add** (klog chain entering the engine's
  0x4841A0 body): foreign-thread registration itself is hostile to this
  engine build — fall back to AE/Z11's design: deferred game-thread
  registration via the R3 marshal pump (register from inside the pump once
  it fires on the game thread).
- **Survives registration, crashes at save-load WITH dispatch markers**:
  the listener callback body (slot-2 `NotifyUObjectDeleted`) is the
  suspect — bisect the callback, not the registration.
- **A survived but C crashed** (or vice versa): the delta is exactly
  {lock + engine allocator + engine code path} vs the hand-rolled write —
  extremely diagnostic (GMalloc vtable+0x40 Realloc family); escalate with
  BOTH harvests and the item-4 lines.

### v4.8 — Standing rules (restated, unchanged)

- **Variant B is NEVER left resident** (§4.2/§4.4, v3.2 step 10): after any
  B run, harvest then restore variant A (or the l24 composition) before
  the session ends. B is diagnostic-only — no delete listener means stale
  object pointers are never cleared and Lua `onDelete` never fires.
- Variant C is proposal-stage until it clears the standing pipeline:
  packet Stage 1 → Claude review → explicit user OK → STEP 0 upload gates
  (fake-sign, decrypt-OFF readback, sha match).
- One launch per explicit user OK; after any crash → harvest → REBOOT
  before any next launch (SAFETY BOX 5). No replay of failed or
  unauthorized configurations; no outcome above may be claimed without its
  markers actually present in the pulled logs (v2 rule).

## Final artifact paths (post-build)

Appended by agent BA (launch rehearsal, 2026-10-08). Paths confirmed from
`2026-10-08-l25-runtime/l25_packaging_driver.py` (VARIANTS table + P9/P10
receipt rows use `pkg/pass-1/modbridge_supplier_composition.prx`; P9 pins the
pass-2 twin identical, pass-1 is the FINAL-RECEIPT canonical pin — matches the
Z18 manifest rows). Both PRX files are PENDING agent R `--execute` (root-gated).

**Signing**: no `sign_and_verify.sh` exists at `harvest/` (searched harvest/,
l25-prep/, l25-runtime/); the proposed convention for a future agent-AY script
is `<prx>.self` next to the PRX (`.../pass-1/modbridge_supplier_composition.prx.self`).
NOTE: a separate sign hop should normally be UNNECESSARY — the staged pipeline
`package_prx_l19.py` runs `make_fself.py` internally (its step 9), and the l24
artifact that pipeline produced (`2026-10-08-l24-package/pass-1/`
`modbridge_supplier_composition.prx`, 31332783 bytes) already carries SELF magic
`4f153d1d` (sha256 `2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4`).
The prep tool's `--prx-path` therefore points DIRECTLY at the pass-1 PRX; a
`.self` file is needed only if a re-sign/verify step is later added.

### Variant B (FIRST — registration SKIPPED, diagnostic-only; never left resident)

- prx path: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-B/pass-1/modbridge_supplier_composition.prx`
- signed path (fallback convention): `.../2026-10-08-l25-package-B/pass-1/modbridge_supplier_composition.prx.self`
- prep (one line): `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant B --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-B/pass-1/modbridge_supplier_composition.prx`
- harvest (one line): `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l25-launch-1`

### Variant A (SECOND — normal registration, confirms the mechanism)

- prx path: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-A/pass-1/modbridge_supplier_composition.prx`
- signed path (fallback convention): `.../2026-10-08-l25-package-A/pass-1/modbridge_supplier_composition.prx.self`
- prep (one line): `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-A/pass-1/modbridge_supplier_composition.prx`
- harvest (one line): `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l25-launch-2`

### Reminders

- **B FIRST** (launch order v3): B is diagnostic-only; after its run, harvest
  then restore A (or the l24 composition) before the session ends — B is never
  left resident (v4.8 standing rule).
- **In-session spawn steps** (once the menu is up and a page is loaded):
  press D-pad DOWN three times so the selection lands on the first PICKUPS
  row (rows 4-11 of the current page; use Page/Refresh via OPTIONS as needed),
  then press CROSS ONCE — `spawnSelected` runs on the game thread. One CROSS =
  one spawn; do not re-press while STATUS is busy (§ spawn procedure).
- Local backup dependency: RESOLVED 2026-10-10 (agent-HA evidence law): the
  prep tool now resolves the pinned ini backups DURABLE-FIRST
  (`evidence/l24-settings-backup.ini`, `evidence/ploader-ini-backup-20261008.ini`
  in this directory); the historical HARD-CODED /tmp paths
  (`/tmp/l24-settings-backup.ini`, `/tmp/ploader-ini-backup-20261008.ini`)
  remain only as legacy fallback (sha-gated either way).
