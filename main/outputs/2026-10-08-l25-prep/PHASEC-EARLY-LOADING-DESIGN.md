# PHASE C — EARLY LOADING (#4) DESIGN + TIMING VERDICT

Agent GD (ZCode), 2026-10-10. Claude round-5 Part 2, packet PHASE C. Companion:
`PHASEB-PLAN.md` (same round). All paths relative to
`/WORKSPACE/.codex/worktrees/translation-core/what/` unless absolute.

## 0. LAW (binding)

1. **NOTHING runs on the console without Claude review + explicit user OK for
   that exact step.** This document is a DESIGN/ANALYSIS only. Mac on battery
   during authoring: no builds, no console contact, no tree edits.
2. **Phase C hard law (Claude round-5): this design NEVER modifies or uploads
   a game executable.** No eboot.bin/`*.self`/`*.sprx`/`*.prx` file of any
   game is edited, patched, re-signed, uploaded, or replaced. If early
   loading turned out to be impossible under this law, the instruction is to
   STOP and escalate to Claude — §6 shows that branch is NOT reached.
3. The original mission framing of #4 ("early-load **eboot patching**",
   SUSPENDED, "needs fake-sign rework if ever revived") is a TECHNIQUE, not
   the goal. This doc evaluates the GOAL — UE4SS "early" semantics — under
   the law. The eboot-patching technique is REJECTED here both as
   unnecessary (§5) and as an executable-file modification (law 2).
4. kstuffSharp stays QUARANTINED; nothing in Phase C touches kernel
  resident state. No rapid launches; one user-launched test per boot.

## 1. What "early loading" means for this port

**PC reference (UE4SS on Windows):** UE4SS loads as a `d3d11.dll` /
`dxgi.dll` / `steam_api*.dll` proxy — the game process loads the proxy DLL
while the engine brings up the renderer module, i.e. DURING engine module
init and BEFORE the engine finishes constructing its singletons. UE4SS's own
initializer then deliberately WAITS for the engine: the upstream code (same
source our port carries) logs *"Delay until enough elements have been
constructed by the engine to the point where we know we can start
constructing FNames"* followed by a **"Waiting for object construction..."**
poll loop (`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/
UnrealInitializer.cpp`, the wait block after `Using engine version`). So
even on PC, UE4SS's bar is NOT "resident before any engine instruction"; it
is **resident IN-PROCESS while the engine is still initializing**, so that
(a) engine module memory is scannable, (b) listeners can be registered on
the live GUObjectArray arrays during engine init, and (c) mods install
before gameplay begins. That is the semantics Phase C must reproduce.

**PS5 current mechanism:** our PRX is injected by **ploader** at GAME
PROCESS CREATE — the pinned deployed source (byte-traceable, agent Z2)
polls new pids every 50 ms (`usleep(50000)`), matches
`sceKernelGetAppInfo` title_id, and ptrace-injects the PRX; the PRX
`module_start` runs its lifecycle and hands off to the runtime's own init
thread (`outputs/2026-10-08-runtime-stop-candidate/runtime_module_entry.c`:
`module_start` → `start_composed_runtime()` → "the runtime continues on its
own init thread"). Process create is the EARLIEST legal in-process point
that exists for a PS5 game — before that, there is no process to be
resident in.

## 2. Current timing — evidence from tonight's menu-up session (frozen build)

Session: PRX efbfc656 (frozen known-good, `FROZEN-KNOWN-GOOD-20261010.md`),
2026-10-10. Sources (all retained on this Mac):

- Eboot process-create bound: `/tmp/l25-variant-A/eboot-monitor-20261010T051653Z.log`
  (15 s processes_list poll, read-only):
  ```
  2026-10-10T05:16:53Z # eboot-monitor start poll=15.0s ...
  2026-10-10T05:52:31Z TRANSITION eboot.bin [] -> [1271]
  ```
  The eboot process appeared in the poll window **(05:52:16Z, 05:52:31Z]**
  = local (00:52:16, 00:52:31] (UTC-5; monitor and ledger confirm the
  offset: ledger 05:16:53Z = local 00:16:53).
- Runtime startup: `outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log`
  (mixed narrow/wide; grep -a). Verbatim lines with log line numbers:

```
L1    [2026-10-10 00:53:05] Native console, GUI, crash dumping and filesystem watching are unavailable
L23   [2026-10-10 00:53:05] [BUILD] id=l26-20261010T043709Z changes=<12 scope ids> snapshot=24025929
L26   [2026-10-10 00:53:05] [Seeds] Required-slot seed table armed (slots 0/2/3/4); ...
L551  [2026-10-10 00:53:06] [Seeds] slot 1 fname pool 0x9765EC0: admitted via image-segments ...
L561  [2026-10-10 00:53:06] [Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified ...
L565  [2026-10-10 00:53:06] [Seeds][L26] engine delete-listener ops admitted: add=0x8841A0
      remove=0x884240; ... engine-call registration armed
L814  [2026-10-10 00:53:08] [Lua] [TownfallItemSpawner] Loaded v1.0. ...
L952  [2026-10-10 00:54:35] [Lua] [TownfallItemSpawner] 24 spawnable pickups found. Pages=3
L1049 [2026-10-10 00:55:14] [Lua] [zz] [DRY-1] pcs PASS ... (full 6/6 ladder)
```

Same lines quoted in `FROZEN-KNOWN-GOOD-20261010.md` §4 and
`CLAUDE-MENU-UP-SPAWN-BLOCKED.md` §1; the frozen PRX's 13/13 marker set
includes "slot 2 FName ctor bound" and "[Seeds][L26] engine delete-listener
ops admitted" (FROZEN §1).

**Reading of the chain (what is proven vs mechanism-argued):**

| Proposition | Class | Evidence |
|---|---|---|
| eboot process created in (00:52:16, 00:52:31] local | PROVEN (receipted) | eboot monitor transition, 15 s poll bound |
| PRX runtime startup output begins 00:53:05, i.e. 34–49 s after process create | PROVEN (receipted) | session log L1-L23 |
| Runtime bound engine structures (fname pool admission, slot-2 ctor, delete-listener ops) at 00:53:06, DURING engine initialization | PROVEN (receipted) | L551/561/565 — these admissions are of LIVE engine structures; engine world construction demonstrably continued for minutes after (item DB scan 00:54:35, DRY ladder 00:55:14, save loaded after that) |
| Mods installed before gameplay begins | PROVEN (receipted) | L814 at 00:53:08, menu visible before save load |
| PRX injection happened AT process create (before eboot main ran) | MECHANISM-PROVEN, not timestamp-receipted tonight | pinned ploader source: 50 ms new-pid poll + ptrace inject (agent Z2, confidence HIGH); the injection toast is untimestamped in tonight's retained artifacts |

The 34–49 s create-to-banner gap is honest open detail, two candidate
mechanisms, both benign and both consistent with the verdict: (i) ploader's
title-id match/injection landing some seconds after the pid first appears;
(ii) the runtime init thread's startup wait (loader facts / module source
readiness) before the first log line. Nothing in the gap contradicts
early-loading semantics — the SEMANTIC property is fixed by L551/561/565:
the runtime was resident and operating WHILE the engine was still
constructing. NOTE: the `[FFO2] q='GameEngine' num=0` line is deliberately
NOT used as instance-count evidence — `num` is the FName Number field in
that oracle's format (agent-FE-ffo2.md §field-spec), not an instance count.

## 3. Options assessment (design-level)

**(a) Earlier legal injection point via existing payloads (elfldr/plugins
order in autoload.txt): NO EFFECT — rejected as a no-op.** autoload.txt
order governs BOOT payload order only. Game-process injection happens when
the GAME launches, regardless of ploader's autoload position; moving
ploader earlier cannot make game injection precede process create (nothing
of the game exists before that). Today's order is also load-bearing:
kstuff-lite first (its crypt/sysent patches must be resident before other
payloads run), ploader last (documented; exits per launch cycle, re-arm via
pldmgr /loadpayload). Editing autoload.txt additionally requires explicit
user OK. Nothing to gain; risk for zero benefit; do not touch.

**(b) Userland hook before eboot main via ptrace-like remote syscall
(kstuff-mediated, Phase B #1/#3 dependency): REJECTED — needless risk.**
This would use kernel-mediated writes to seize the process before its main
runs. But process create (option c's injection point) is already the
earliest legal in-process moment; the UE4SS semantic bar (resident during
engine init, §1) is already met (§2); and option (b) adds a Phase B #3
kernel-write dependency (its own ladder, authorization and risk) for zero
UE4SS-parity gain. Fail-closed bias says: don't.

**(c) Accept current timing: THIS IS THE STATE THE EVIDENCE SUPPORTS — see
§4/§5.**

## 4. UE4SS-semantics comparison table (current port vs PC reference)

| UE4SS "early" requirement | PC mechanism | PS5 port today | Verdict |
|---|---|---|---|
| In-process before engine finishes init | d3d11 proxy at renderer module load | PRX injected at process create; startup + engine-structure admissions at 00:53:05-06 while engine init continued for minutes (L551/561/565 vs L952/L1049) | MET (receipted) |
| Engine module memory scannable at startup | same-process AOB scan | runtime scan reads the eboot image via its proven image-segment/VirtualQuery admission paths (L519-551) | MET (receipted) |
| Listeners registered on live GUObjectArray during engine init | AddUObjectDeleteListener at init | `[Seeds][L26] engine delete-listener ops admitted … engine-call registration armed` (L565) | MET (receipted) |
| Mods installed before gameplay | mod loading after init | mod loaded 00:53:08, ~90 s before menu item scan, before save load | MET (receipted) |
| Resident before the engine's FIRST instruction | n/a on PC (proxy loads after process start too) | mechanism-proven (ploader ptrace inject at pid discovery), not timestamp-receipted | beyond-parity; tightening test §6 |

## 5. VERDICT — explicit, per the tasking

**#4 (early loading) = SATISFIED-ALREADY.**

- The GOAL of #4 — the runtime resident in the game process before engine
  init completes, with listener registration and mod startup sequenced
  around engine init (UE4SS semantics, §1) — is **console-proven on the
  frozen known-good build** from tonight's session evidence (§2, §4): every
  UE4SS-parity requirement in the table is receipted.
- The mission's original #4 TECHNIQUE (early-load via **eboot patching**) is
  **REJECTED AS UNNECESSARY** and would additionally violate the Phase C
  law (never modify or upload a game executable). The suspension of eboot
  patching stays in force permanently for this port; no fake-sign rework is
  needed for loading-timing reasons.
- **The IMPOSSIBLE-WITHOUT-EXE-CHANGE branch is NOT reached** — no
  escalation to Claude is required on feasibility grounds. (Per §0 law 1,
  the §6 tightening passes still need Claude review + user OK like any
  console session; that is ordinary authorization, not a blocked-design
  escalation.)
- One honest caveat, restated: the strict property "injected before the
  eboot's FIRST instruction" is mechanism-proven (pinned ploader source)
  but was not timestamp-receipted tonight (no retained injection-timestamp
  artifact). It is also BEYOND what PC UE4SS itself achieves (§4 last row).
  §6 receipts it optionally; if §6 ever showed injection landing after
  early engine instructions, the UE4SS-semantics verdict STILL stands on
  L551/561/565 (resident-during-init), and that contingency is recorded
  here so nobody mistakes the tightening test for the verdict.

## 6. Console test plan — 3 observe-only passes (per the law: only after Claude review + user OK)

Purpose: receipt the timing chain three times across three restarts, using
ONLY the frozen build and existing instrumentation — **no new build, no code
changes, no console state changes beyond the standard verified launch prep**
(the diagnostic events needed already exist in the frozen PRX:
`module_start_entered` with `t_ms` in the supplier-diag jsonl sink).

Per pass (fresh boot, one user-launched Townfall launch):

1. Standard verified prep (existing tooling, LAUNCH-READY ledger pattern):
   redeploy frozen PRX efbfc656 if absent (decrypt-off readback sha/magic),
   original 1947 B settings, ploader armed ONCE after confirmed exit
   (`prep_modded_launch.py --variant A --prx-path <frozen>`), klog tap on
   TCP 3232 to a NEW file, eboot monitor running BEFORE the launch.
2. User launches Townfall (single launch; clean exit via in-game quit).
3. Harvest, per pass:
   - eboot monitor: TRANSITION line -> process-create bound.
   - supplier-diag jsonl: `module_start_entered` (t_ms) -> injection
     receipt; first engine-side diagnostic events -> engine-first-activity
     receipt.
   - UE4SS.log: [BUILD] stamp line, `[Seeds] slot 1 fname pool … admitted`
     (L551-class), `slot 2 FName ctor bound` (L561-class), `[Seeds][L26]
     engine delete-listener ops admitted … armed` (L565-class), mod loaded
     line (L814-class) — with timestamps.
   - klog: zero hits on the crash watch list (Z1 patterns).
4. PASS criteria (all three passes):
   - ordering: [BUILD] + slot admissions + delete-listener arming all
     precede every world/gameplay-phase event (item DB scan, save-data
     activity, DRY-class lines);
   - injection receipt: `module_start_entered` t_ms within seconds of the
     monitor's process-create bound (receipts the at-create mechanism
     claim);
   - no crash signatures; clean game exit each pass.
5. Record 3/3 in a PHASEC-TIMING-RECEIPTS file (new file under this
   directory; pass/fail per pass, verbatim lines, absolute paths).

Failure contingency (any pass): do NOT iterate or relaunch (no-rapid-launch
law); save state, report the deviation to Claude with the three artifacts;
the §5 verdict stands or falls only on evidence-class grounds (if the
admission-ordering criterion itself fails on any pass — i.e. listener
arming NOT before world init — then #4 reopens as
FEASIBLE-WITHOUT-EXE-CHANGE analysis, which per §3(a)/(b) has no legal
earlier point than process create, and would then be escalated to Claude).

## 7. Boundary note (do not conflate)

Loading timing is NOT the port's exec-page hooking limitation. The known
constraint that the in-process detour verifier never succeeded on the
exec-only eboot text target is a CAPABILITY #3 (patching) question — if a
future mod needs first-call instruction hooks, the route is Phase B #3's
mdbg-mediated runtime-memory write path (see PHASEB-PLAN §3.2), NOT earlier
loading. Earlier loading would not make exec pages writable in either case.

— agent GD (ZCode), 2026-10-10. Design/analysis only; no builds, no console
contact, no tree edits. Nothing herein authorizes any console action.
