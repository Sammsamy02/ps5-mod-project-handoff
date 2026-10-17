# PS5 Mod Bridge — hard rules for Claude sessions in this worktree

Bypass-permissions mode is on, so these rules are the only guard. They win over
any older or looser wording elsewhere (including the standing routine approval in
AGENTS.md / PS5-MODDING-MISSION.md): where two rules differ, the stricter applies.
Console contact defaults (FTP / pldmgr / klog ports) are in AGENTS.md.

## GOALS

- Unmodified PC mods run on a jailbroken PS5 exactly as downloaded.
- Support is per engine (~95% of an engine's games), never per game or mod.
  Every platform difference lives in the generic runtime or config.
- The profile generator and the self-test run automatically at a new game's
  first launch.
- Order (user decision, 2026-10-12, latest; step (1) DONE 2026-10-13, R11 bfd95db6 = last clean build): (1) finish Townfall: current
  stutter fixes (B6p), then ONE launch carrying B7 (Lua errors unwind as C++)
  + the map-load / BeginPlay refused hooks + the 15 grandfathered engine fields
  (verified from two sources, added to the layout data, field_gate.py tightened
  to zero grandfathered reads; list in l25-prep/evidence/field-gate-20261012/
  B6n-fields.txt; Stellar Blade is UE4, hard-wired offsets will be wrong);
  (2) capabilities 1-12 BEFORE Stellar Blade, ONE launch per capability, each
  proven on Townfall (Leon for paks, #8 and #12; UE4SS C++ mods for #9; UE4SS
  GUI windows for #11); kernel capabilities #1/#3 keep the 3-pass rule; #10
  (C# runtime) BACK IN SCOPE (user, 2026-10-13): goes last and never holds Stellar Blade; (3) Stellar Blade
  as a fully blind test (Lua mods, outfit paks, CNS) with zero Stellar Blade-
  specific code or hand-written values. NO Stellar Blade prep until step 3.
  UPDATE (user, 2026-10-14): after the current round (#7 step 2, then one combined #12/#11/#10 launch) run the
  Stellar Blade CORE blind test next; the new capabilities (video, sound, saves, trainers, UE5 asset types,
  keyboard/mouse) are built on the Mac in parallel and tested on both games; ZCode #1/#3/#13 when ready.
  Then other engines -> mod manager and special-case kit.
- Out of scope: online mods, Windows-only mods, mods for a different game version.
- The 3-pass rule applies only to kernel work.

## 0. Parallel capability teams (user, 2026-10-13)

- Capabilities 1-12 are prepared in parallel on the Mac by teams A-F, each in its own git worktree/branch
  (`../caps-team-<X>`). Teams write only there: staged patch, host tests, one independent review, a launch card.
- Only the integrator (main session) merges into the candidate tree, runs gates, builds, deploys, and talks to the
  console. Teams never touch the console (not even reads), the candidate tree, frozen builds or mod files.
- One build at a time. Console tests one at a time, one launch per capability, in order: #7, #8, #2, #4, #5, #11, #9,
  #1/#3, #12, #10. Kernel payloads (#1/#3) only with the user's explicit OK; 3 passes across restarts.
- Memory low: pause Team F first, then Team E. Report to the user as one combined status table.
- Base inputs only from the game's own files; never Sony SDK material.

## 1. Console actions

- Console actions need the user's explicit OK for THAT action: FTP uploads,
  payload loads, ploader arming, game launches, config edits on the PS5,
  deletions. Approval for one action does not carry over to the next.
- Read-only FTP reads (LIST, RETR) are fine without asking.
- Before reading any console LOG, ask the user to confirm the game is closed.
  (FTP reads under game load come back incomplete.)
- One launch per test. Only one agent touches the console at a time.
- Keep modded games offline. Never expose FTP 2121, pldmgr 8084 or klogsrv 3232
  beyond the LAN. Never put the console's IP in reports meant for others.

## 2. Executables

- Never upload an unsigned or modified game executable (eboot.bin, sce_module/*,
  game prx). Download/back up executables with FTP decrypt OFF (`-Q "SITE SELF"`).
- Never touch the quarantined kstuffSharp / InstallerApp kernel module.
- No PRX upload without a fresh verify_build PASS and manifest gate for that
  exact sha. Decrypt-off readback (full sha256 + magic 4f153d1d) after every
  upload and after every power event.
- Run the automatic loadlist row-check before every launch.
- Diagnostic folders we own (ueselftest, zzdiag) deploy as exact mirrors of the
  local folder: stale files removed, then row-check.

## 3. Runtime code

- Mod files are never edited, not even for diagnostics (use zzdiag instead).
- Runtime code is generic for all UE4SS mods: no mod- or game-specific names,
  offsets or special cases.
- No patching or detours of game code on PS5 (execute-only). Hooks are data-only
  (vtable / function-pointer swaps) until Phase B is approved. The no-detour gate
  enforces this at build time.
- Values that ship need two independent sources. Missing values fail closed;
  wrong values crash.
- Lookup caches must stay correct for NEW objects (create AND delete listeners,
  or rebuild on miss). A create listener goes only through the engine's own
  AddUObjectCreateListener ("variant C" route), address confirmed from two sources.
- Epic source stays local: never committed or uploaded. Never seek Sony SDK or
  platform-extension material.

## 4. Builds and gates

- BUILD POLICY (user, 2026-10-12): no preemptive intermediate/backup builds.
  Make all changes for the next launch, run the automatic gates, build ONCE,
  launch. The last clean (launched, passing) build is the rollback. A split /
  bisect build only if a launch fails AND the logs don't show the cause.
  Build archives the pipeline makes are kept (never delete); record each
  build's sha in the build ledger with its label.
- Bisect points use only SAFE variants (variant A). Variant-B packages
  (delete listener skipped) are diagnostic-only and never a bisect point or
  upload candidate.
- EVERY gate prints how many files/items it checked and FAILS (non-zero exit)
  when that count is zero. A gate that scans nothing is a failed gate, not a pass.
- Gates run before compile: fs_throw_gate.py, no_detour_gate.py. After
  packaging: verify_build.sh --manifest for the exact PRX.
- Build jobs capped at 4 parallel compiles; one PRX build at a time.
- SPEED MODE (user, 2026-10-12, replaces earlier agent/review rules): work
  directly in the main conversation (diagnose, patch, build, deploy). Workflows
  only for broad genuinely-parallel jobs, when a direct attempt is stuck, or for
  background prep that never blocks a build/launch; under 5 agents, no reviews
  of reviews. Small changes/scripts: one quick self-check. Runtime code that
  ships to the PS5: ONE independent reviewer agent. Never make a build or launch
  wait on a background workflow. Lua-only changes (zzdiag, self-test, config):
  fast Lua deploy, no rebuild. Only one agent ever touches the console.

## 5. Evidence and claims

- Evidence goes in the durable evidence tree (outputs/2026-10-08-l25-prep/
  evidence/), never /tmp.
- Console logs are NUL-interleaved: decode with harvest/decode_session_log.py,
  do not plain-grep.
- Label every claim "console-proven" (citing this session's log lines) or
  "Mac-only". Never reuse an earlier session's evidence for a new verdict.

## 6. Acceptance and order (user decision, 2026-10-12)

1. Townfall: finish the fix builds, then launch with ALL mods including the
   minimap. Done after 2 clean launches: all mods working, no stutter,
   self-test once per launch.
2. Stellar Blade blind test: NO Stellar Blade-specific code, hand-written values
   or tuning. Only the generic steps any new game needs: readable executable
   copy (decrypt-off rules), automatic profile generator, the same runtime +
   self-test + one Lua mod per type. Failures are fixed at engine/version level.
3. Then capabilities 1-12, starting with paks (#7, #8, #12). Mac-side work on
   1-12 may run in parallel only if it does not slow Townfall.
- Three passes across restarts only for kernel work (Phase B).

## 7. Reports

After each launch: one markdown report for the Claude reviewer (under 30 MB,
paste-ready): build sha + snapshot, what changed, per-mod verdict with this
session's log lines, self-test PASS/FAIL/SLOW table with timings, open issues,
next step. Short and plain: the user is not a programmer.

## Where things are

- Runtime source: outputs/2026-10-08-l26-runtime/typescan-r4/candidate/
- Runbooks, reports, harvest tools, evidence, staged patches:
  outputs/2026-10-08-l25-prep/ (round5-staging/ holds staged patches)
- Known-good PRX: 1e49188b (outputs/2026-10-08-l26-package-A/), snapshot 40831653.
- Outage rules: outputs/2026-10-06-recovery-fakesign/POSTMORTEM.md
