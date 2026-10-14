# PS5 MODDING MISSION — live problem, plan, and exact playbook
# (persistent state for any future session; companion to POSTMORTEM.md and the
#  PS5 CONSOLE RULES in ai-game-studio/AGENTS.md)

## AUTHORITATIVE UPDATE — l15 / unmodified PC Lua goal (2026-10-07)

Latest user goal overrides the historical spawner-only wording below: prove that PC UE4SS Lua mods run on PS5 **exactly as downloaded**. TownfallItemSpawner is the first case, not the compatibility goal. Mod release files are immutable; every PS5 difference belongs in runtime or explicit user config. Keep per-mod release/console SHA-256 manifests. Game/build facts belong in profiles, not per-mod edits.

The l15 PRX-only upload was approved and completed: 30,924,039 bytes, SHA-256 `1cb1b27a2771a9178b6e5725665d398b81b56e8e84685903a67aa407b5585acb`, signed SELF wrapper `4f153d1d`, raw/decrypt-off readback verified. This supersedes the historical “PRXs are PLAIN” claim below. Own loader PRXs also use the proven fake-sign pipeline on this setup. No configuration/game/kernel changes were made.

One user launch, PID108, harvested before another launch. All13 pool checks passed, admission22ms / shared60,000ms. Native FName call was **not reached**: fresh32-byte prologue probe sink creation `_openat` returned -1/EPERM after module_start; zero bytes copied. Runtime refused and stopped ItemSpawner. No Townfall crash observed, game alive at harvest; separate prelaunch SceShellUI SIGILL preserved. New FName ABI remains Mac-only; controller, mod-owned menu and spawn remain unproved.

Proposed ONLY, awaiting user approval: fully open bounded game-created probe-file handles during startup, retaining them for delayed fresh kernel-copyin/same-handle readback. Keep exact prologue match, EFAULT/short/timeout refusal and cleanup. No new console action or code implementation is authorized by this note.

TownfallItemSpawner V1.0, Nexus mod95/file218, original ZIP SHA `a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb`: both release files exactly match the console copy; no mod edits/extra files. User owns PS5 only; same-game PC reference is unavailable. Do not invent PC parity from host tests.

Latest report: `outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/CONSOLE-REPORT.md`; provenance manifest and API-gap census are beside it. Remaining order: resolve this refusal → off-by-default runtime tooling (crash report chaining previous handler, LAN Lua console, read-only debugger) → runtime/config controller-to-key translation → ORIGINAL mod's UMG menu → spawn → conformance + more unmodified mods → deferred B/C/A capability phases. Those phases retain three console passes across reboots and per-step user approval. Historical sections below are retained context, not current authorization/readiness.

## THE GOAL (historical; latest goal/status above)
Turn the PS5 into a PC for game modding (capabilities 1-8). Proving ladder:
Townfall Item Spawner — runtime starts → mod loads → game-thread work →
controller input → MENU OPENS → item spawns. Current position: everything up to
"mod loads" works; blocked at the FName-pool admission (GATE-1).

## THE DELIVERY STACK (all proven working end-to-end Oct 6-7)
- ploader (console plugin injector; /data/pldmgr/payloads/ploader/ploader.elf)
  reads /data/plugins/ploader.ini `[PPSA33286]` `<prx>=<2=inject|0=off>` and
  injects at game launch. NOW IN autoload.txt (last line; exits after each game
  launch cycle — restart via
  `curl http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf`
  between multiple launches in one boot).
- modbridge_supplier_composition.prx (30.9MB, sha 24d254f3…, /data/plugins/) =
  the UE4SS PS5 runtime composition (built by
  outputs/2026-10-04-zcode-controller/launch11-combined/launch11_one_pass.py;
  EDITIONS.json pins every unit; live units in launch11-combined/units/).
- Mod content: /data/modbridge/roots/PPSA33286/ (loadlist, ue4ss/ settings +
  layout inis, Mods/TownfallItemSpawner/Scripts/main.lua).
- ploader prx uploads are PLAIN (our own loader — no fake-signing needed).
  Only game executables (eboot/sce_module/prx of the GAME) need signing rules.

## THE PROBLEM (GATE-1) — precise mechanics
runtime-status/UE4SS.log Terminal refusal:
`[Seeds] REFUTED: FName pool at 0x9765EC0: snapshot truncated … copyin-probe
resolution: kernel write-copyin probe unavailable (sink-open/policy/partial/
vacuous-sink class)`.
Facts established:
1. The pin is believed CORRECT: read_pool_words returns live values
   (CurrentBlock 82, Blocks[0] 0x22C0000000); the pool struct itself is
   readable via published spans; ONLY Blocks[0]'s heap block lacks coverage
   proof (a listing gap in the bridge's 64-span table).
2. The copyin oracle (bridge_kernel_write_copyin_probe: userspace
   sceKernelOpen/Write/Close to /data/plugins/modbridge-copyin-sink.bin) is
   the gap-filler. Its only full success (session 1047) moved 0x20000 bytes.
3. Current failures: SHORT WRITES (rc=64, rc=4096, errno=0 — legitimate-looking
   partials the one-shot accounting classes as Unavailable) and the
   0x7ffdfffe sentinel on the fresh re-probe after Control D (module_start)
   consumes the probe channel. Sink file state RULED OUT (4KB reset didn't
   help; behavior reproduces from the documented-good state).
4. NOT kstuff-related (source-verified: zero kekcall/kstuff dependency; 1047
   ran inside the kstuff-paused window).
5. Honest-refuse discipline: without the oracle, admission cannot prove
   Blocks[0] readable → refuse → mod stops → no menu.

## WHAT WE ARE DOING (l12 build — Claude-approved decision 4)
Workspace: /tmp/l12-staging (recreate from launch11-combined/units + the four
shadow units in outputs/2026-10-04-zcode-team/worker-shadow-integrate/).
Changes to UnrealInitializer.cpp (live unit):
a. SHADOW-CORROBORATION FALLBACK at the pool budget-expiry refusal: when the
   failure class is rescuable AND the oracle resolution was Unavailable (not
   REFUSED), run the staged shadow arm (rc::shadowarm::shadow_arm):
   identity gate = sha256(decrypted eboot) == SeedTable::kImageSha256;
   slide gate = module_view_base() == kBaseObserved (0x400000); 4/4
   required-slot pinned sigs (kGuaXref/kFNameCtor/kGMallocStore/kScoPrologue)
   must agree with seed labels. On Armed → ADMIT the pool under image
   corroboration, log loudly: source (shadow-corroboration), the slide, and
   the cross-check vs the pinned 0x9765EC0 (MATCH = expected). Reference
   implementation of the arm call: worker-shadow-integrate/patched/
   UnrealInitializer.cpp ::shadow_arm_stage (repin
   kDecryptedEbootPath → "/data/homebrew/Silent Hill Townfall
   PPSA33286/decrypted/eboot.bin" — that file is verified 3b6cfad4, the exact
   pinned fixture).
b. PROBE HARDENING: short-write loop in bridge_kernel_write_copyin_probe
   (advance by rc, repeat until complete/error — today's errno-0 partials
   look like legitimate short writes); treat 0x7ffdfffe as
   channel-consumed, not generic failure; capture-first path accepts a
   partial capture's usable prefix.
c. SETTINGS SWITCH: [Seeds] PoolAdmission = oracle|auto|shadow in
   /data/plugins/UE4SS-settings.ini (default auto = existing-first-then-
   fallback). Read via a small direct INI scan at init (no SettingsManager
   surgery).
d. DATA DISCIPLINE: shadow admission never authorizes direct block reads —
   sink-fed validation stays authoritative when captures exist.
e. PS5Pad.ini REBIND (ships in the SAME upload): OPTIONS is the game's pause
   button — rebind the spawner menu to L3+R3 (game doesn't use it).
Then: update EDITIONS.json units (new pins + initializer sha), run
launch11_one_pass.py verify + --full, pass gates, THEN (Claude's upload
conditions): show gate receipts, /data/plugins/ files only, verify on-disk
magic with decrypt-OFF after upload, ONE test launch on a fresh boot.

## TEST LADDER (one launch per test, user launches)
fresh boot (autoload arms ploader) → launch Townfall → in-game → L3+R3 →
harvest UE4SS.log + runtime-status + supplier-diag jsonl + klog
(TCP 3232, connect BEFORE the launch; captures always to NEW files).
Success signals: "pool admitted via …" line naming the source; mod stays
installed (no "Stopping mod" Terminal); menu appears; spawn works.

## CAPABILITY 1-8 STATUS (the PC-parity goal)
DONE/in-stack: injector (ploader), UE4SS runtime in-game, Sony-native IO,
memory-read primitive (copyin), #2 AOB scanner (38/38, staged into l12),
#5 GDB stub (host), #7 file overlay (host), #8 GNF asset conversion (host).
BLOCKED (path known): #1/#3/#6 kernel services — build on kstuff-lite's
kernel access (NOT the quarantined kstuffSharp module; see POSTMORTEM).
SUSPENDED: #4 early-load eboot patching (needs fake-sign rework if ever
revived).

## KEY PATHS
Mac worktree: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/
launch11-combined/ (build), worker-shadow-integrate/ (scanner units + host
tests 38/38), worker-code-scan/ (sig audit), 2026-10-06-recovery-fakesign/
(POSTMORTEM + recovery assets + this file).
Console: /data/plugins/ (prx + ini + sink + UE4SS.log),
/data/modbridge/roots/PPSA33286/ (mod + runtime-status),
/data/pldmgr/ (payloads + autoload.txt — ploader now last line),
/data/modbridge-supplier-diag.jsonl (probe records; tail for latest run).

## l12 UPGRADES (Claude review of the sink-reset run — binding additions)
A. 0x7ffdfffe = likely -(int32)0x80020002 (SCE ENOENT) or ~0x80020001 (EPERM)
   — a sink OPEN failure after Control D consumed the sink, NOT a memory
   result. Confirm the probe's rc conversion. FIX: each probe creates its OWN
   sink (unique path, O_CREAT|O_TRUNC, created by the GAME process). NEVER
   pre-create the sink via FTP — ftpsrv ownership/mode may EPERM the game.
B. errno-0 short writes may be REAL copyin faults (FreeBSD returns the
   partial count when copyin hits an unmapped/protected page). Log base+rc
   and whether rc stops on a page boundary. In the short-write loop, a
   follow-up EFAULT = "unreadable from base+rc", NEVER success. Shadow
   corroboration must NEVER admit a range the probe showed faulting.
C. Priority order of mechanisms:
   1. PRIMARY coverage proof via sceKernelVirtualQuery /
      sceKernelQueryMemoryProtection on Blocks[0] (0x22C0000000): mapping,
      extent, protection — directly, no sink. Check libkernel exports; keep
      copyin as the cross-check.
   2. Per-page readability bitmap for the 128 KiB block; admit if the pages
      actually needed are readable.
   3. Replace the terminal refusal with a bounded retry (every ~5 s for
      ~2-3 min, logged) — the pool may not be fully committed early.
   4. PoolAdmission switch + source logging as planned.
D. DONE 2026-10-07: kstuff_crash_detection=0 in /data/shadowmount/config.ini
   (backup config.ini.20261007-predev.bak, sha aed778f1 pre-change) so
   ShadowMount stops doubling Townfall's pause delay on crashes; otherwise
   check autotune.ini after every crash.
E. DONE 2026-10-07: this worktree has its own AGENTS.md (rules + mission
   pointer) so the studio stop-directive can't stall PS5 sessions.

## DEFERRED PLAN — CONSOLE VALIDATION OF CAPABILITIES 1–8

User-directed plan, added 2026-10-07. **NOT STARTED. Begin only AFTER the
Townfall Item Spawner successfully spawns an item on the PS5.** Runtime
startup, mod loading, or the menu opening alone does not satisfy that
prerequisite. Recording this plan does not authorize console changes.

### Acceptance and session rules

- "Done" means the capability passes **on the PS5 three times across
  reboots**, with the logs saved. Mac/host tests do not count toward those
  three passes.
- Test one capability per session. Each session needs a written pass/fail
  check and a revert path before testing.
- Report each capability as **console-proven / Mac-only / blocked**.
  `console-proven` requires the three console passes above; partial console
  evidence must be stated explicitly without marking the capability done.
- Nothing changes on the console without the user's explicit OK for that
  step: no uploads, payload loads, launches, configuration edits or
  deletions. Read-only checks are allowed. Kernel or game-executable work
  needs the user's OK at each step; **back up saves before any kernel test**.
- One game launch per test, launched by the user. Save klog captures to NEW
  files; never truncate an existing capture.
- Preserve the permanent console rules: download executables for backup,
  hashing or re-upload with `-Q "SITE SELF"` (decrypt-off); never upload
  unsigned or modified game executables outside the approved fake-sign
  pipeline; verify executable uploads with decrypt-off readback.

### User-chosen order: PHASE B → PHASE C → PHASE A (hardest first)

The capability sequence is **#1 → #3 → #6 → #4 → #5 → #2 → #7 → #8**.

### PHASE B — kernel access (#1, #3, #6)

Build kernel features on **kstuff-lite**, NOT kstuffSharp/InstallerApp.
The latter stays quarantined: its firmware 10.01 offsets crashed firmware
13.60, and it conflicts with kstuff's own hooks.

First determine whether kstuff-lite's **existing `kekcall` interface**
suffices for kernel reads. Only if it does not, extend the open source
**EchoStretch/kstuff-lite v1.11** implementation.

Rules for any modified kstuff build:

- Never overwrite `kstuff-lite_v1.11.elf`; give the modified build a
  different filename.
- Before the first test, prove the recovery path and write the exact
  restore steps down. A startup crash must not lock us out: FTP must still
  come up so the original can be restored. If that recovery requirement
  cannot be established, report the test as blocked.
- Read-only additions come first. Kernel writes may begin only after
  reads are console-proven under the three-pass rule.
- Every game depends on kstuff. After any kstuff change, confirm that
  **Astro Bot, Stellar Blade and Townfall** still launch before continuing.
  These regression launches remain separate, user-launched tests.

Test order within Phase B:

1. **#1 — read-only kernel reads:** no game running; several reboots
   without a panic, with the required three logged console passes.
2. **#3 — harmless in-game patch:** demonstrate the patch and a clean
   revert.
3. **#6 — kernel input path.**

### PHASE C — early loading (#4)

Test **#4 early loading** only with the fake-sign pipeline and decrypt-off
backups, first on a non-essential title. The existing suspension of eboot
patching remains in effect until the user explicitly approves the specific
step; this future plan does not lift it.

### PHASE A — remaining capabilities (#5, #2, #7, #8)

1. **#5 — debugger:** attach, breakpoint, read, continue.
2. **#2 — scanner:** demonstrate scanning in-game.
3. **#7 — file overlay:** overlay one harmless file and demonstrate revert.
4. **#8 — converted asset:** demonstrate the asset through the overlay;
   formalize the Leon recipe as a reproducible conversion procedure.

### Design rule for every phase

Game-specific values belong in a **per-game profile/configuration file**,
not shared code. Keep the mechanisms reusable so game #2, **Stellar Blade**,
uses the same implementation with its own profile.
