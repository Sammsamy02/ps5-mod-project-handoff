# REPORT.md — worker-itemspawner-dryrun (re-dispatch)

2026-10-04. Re-dispatch with the correct absolute workspace after a rate-limit death;
task completed within scope: static analysis only, mod never executed, no network, no
console contact, no nested subagents. Written only under
`outputs/2026-10-04-zcode-team/worker-itemspawner-dryrun/`. ~18 tool calls used (limit 30).

## Inputs verified

- Retained intake `.superpowers/native-observations/item-spawner-real-mod-intake-1/`:
  `intake.json` cross-checked; `main.lua` retained as zip member `member-00000000.bin`
  (NOT a loose file) — 746 lines and sha256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`
  re-verified this session against the pin; `enabled.txt` member `member-00000001.bin`
  0 B / `e3b0c442…855`. Analyzed in place; nothing extracted or modified.
- `worker-modreqs/RUNTIME-REQUIREMENTS.md`, `worker-activation-bundle/BUNDLE-SPEC.md`,
  `ZCODE-FORK-HANDOFF.md` — read in full; cites reused (`IS:<line>`, A/B/C/D/E requirement
  IDs, ladder rungs).

## Deliverables

1. **TWO-PHASE-CHECKLIST.md** — load-time vs first-keybind with line cites. Key findings:
   load-time engine contact is only 6× `RegisterKeyBind` (IS:694–729, hard dependency),
   pcall-guarded `RegisterLoadMapPreHook` (IS:731, soft), and one `print` (IS:746 via 6).
   `require("UEHelpers")`, `FindFirstOf`, `StaticFindObject`, `StaticConstructObject`,
   `FName`/`FText`, `LoadAsset`, and `ExecuteInGameThread` are all LAZY (first keybind).
   Non-F7 callbacks early-return before `ExecuteInGameThread` while the menu is closed.
   Settings the path depends on: `DefaultExecuteInGameThreadMethod=EngineTick` (A9),
   `DefaultFNameToStringMethod=Scan` (B8); the Lua reads no settings itself.
2. **FIRST-RUN-CONFIG.md** — the minimal BUNDLE-SPEC §2 bundle (6 keys) with the exact
   minimal settings.ini, plus the harmlessness argument. States the hard gate honestly:
   keys 5+6 (MemberVariableLayout.ini / VTableLayout.ini) have no validated Townfall
   bytes anywhere retained — `setup_unreal()` throws before any Lua, so a first run that
   reaches Lua waits on worker-2; the placeholder file-load path is not a mod run.
3. **BLOCKERS.md** — register B0–B8 mapped to ladder rungs; ordered what-breaks-first:
   (0) layout bytes today, (1) load phase should complete once B0 closes, (2) first F7
   dies at IS:695 on nil `ExecuteInGameThread` (baseline-observed, mod logs nothing),
   (3) without B2 the mod fails gracefully with only a `Scan failed:` line, (4) then
   `UI creation failed:` expected (UMG D1–D5 + FName/FText gaps), (5) first spawn-path
   break predicted at `LineTraceSingle` out-param marshalling (B5, IS:245–264) before
   LoadAsset/spawn can be judged, (6) standing A8 caveat: physical F7 delivery on PS5
   is unproven, making "nothing logged" ambiguous.
4. **OBSERVABLES.md** — scene-free console-side oracles. Phase-1 pass = exactly one line
   `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
   (byte-exact from IS:746) in the ACTIVE retained log path, with a full absence list;
   fail discrimination (no records vs records-without-mod-line). Phase-2-no-scene = the
   `Scan failed: Item database not ready. Load a save first.` line (or the nil-global
   tail that discriminates a missing `FindFirstOf`), then designed silence; sub-case
   signals if a PlayerController exists. Paths evidenced from retained checkpoints:
   `/data/townfall_runtime_aligned_entry.log` (current lineage + v3 same-path contract),
   `/data/townfall_runtime_buffered_lua.log` (checkpoint
   `completed_buffered_lua_console_probe`), historical `/data/townfall_mod.log`.

## Honest limits

- Which log path the supplier-composed runtime routes Lua `print()` into is a composition
  fact not resolvable statically; OBSERVABLES.md is path-agnostic and flags it as an
  open check, not a claim.
- All "expected" statements are predictions from pinned source + retained evidence;
  none is a performed run. No acceptance is claimed; BLOCKERS.md B0 stands: no honest
  first run exists until worker-2 layout data lands.
- 19 `print`/log behavior details (e.g., silent `buildUI` return at IS:493–497, pcall
  swallowing require failures so "no require error" is NOT evidence UEHelpers resolved)
  are documented where relevant rather than in this summary.
