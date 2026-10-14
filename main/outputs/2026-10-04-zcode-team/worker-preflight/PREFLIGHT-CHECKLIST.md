# PREFLIGHT-CHECKLIST — console staging pre-flight (local verification)

Worker: worker-preflight, 2026-10-04. Source of truth: `outputs/2026-10-04-zcode-team/worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md` (§A manifest). Method: `stat -f%z` + `shasum -a 256` recomputed on the actual local files this pass; content files read directly. No network, no console contact — this verifies what SHOULD be staged, not what IS staged.

`$T` = `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team`

---

## 1. The 9 staged files — existence, sha256, byte size — **PASS (9/9)**

All 9 local sources exist; every recomputed sha256 and byte size equals the §A manifest cell exactly.

| # | file (relative to $T) | size B | sha256 match | verdict |
|---|---|---|---|---|
| 1 | `worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx` | 30778567 | `de372302…9aa0f` MATCH | PASS |
| 2 | `worker-console-staging-bundle/loadlist.txt` | 446 | `49ab7a3a…1e3bf` MATCH | PASS |
| 3 | `worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini` | 779 | `9f3fa127…0f7f` MATCH | PASS |
| 4 | `worker-emit-final-pair/emission-final/MemberVariableLayout.ini` | 588 | `f291d552…3f32` MATCH | PASS |
| 5 | `worker-emit-final-pair/emission-final/VTableLayout.ini` | 186 | `9c20f7c9…ad86` MATCH | PASS |
| 6 | `worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini` | 2960 | `1d919fbf…ca1e` MATCH | PASS |
| 7 | `worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a…9658` MATCH | PASS |
| 8 | `worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c442…b855` MATCH (empty-file sha) | PASS |
| 9 | `worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609…15a7` MATCH | PASS |

PRX extra checks (this worker, direct read): first 4 bytes = `4f 15 3d 1d` (FSELF outer magic, matches §A/Gate A1 premise); candidate dir receipt `verification-evidence/lineage-verification.json` = 33 checks, 0 fail (recomputed by parsing the JSON).

## 2. ploader.ini edit — **PASS as specified in V2 §C; DISCREPANCY vs task briefing**

- Verified correct against every authoritative record: V2 §C, `worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md` §3, `worker-prx-packaging/STAGING-CHECKLIST.md` §2, `worker-final-staging-rehearsal/REHEARSAL-RESULT.md` §3, `worker-handoff-v3/ZCODE-FORK-HANDOFF-V3.md` — all agree: under the existing `[PPSA33286]` header, ensure exactly `/data/plugins/modbridge_supplier_composition.prx = 2` (mode 2 = LATE, the only evidenced shape); change nothing else; no-op if present; pre-image backup + readback mandatory.
- **DISCREPANCY:** the task briefing's expected "3 entries: aligned_entry=2, ingame_facts=0, supplier_composition=2" appears in NO local record. History staged `modbridge_ingame_facts.prx = 2` (not 0) — composition ledgers: "appended /data/plugins/modbridge_ingame_facts.prx = 2 … ALL pre-existing entries preserved … incl. proven aligned_entry = 2". No record prescribes an ingame_facts `= 0` line or that exact 3-line section.
- **Console-state caveat (rehearsal's own caveat, still true):** console bytes of ploader.ini are not provable host-side. The newest console readback on file (`/.superpowers/native-observations/worker-channel-result-39046b2efa564f389a2c920d02c2aa4e/ploader.ini`, 2026-10-03 12:45) contains probe-era lines (`worker_channel = 2`, `import_diagnostics = 0`, `import_probe = 0`, `log_probe = 0`, `entry_A0 = 0`, `entry_A2 = 0`) and contains NEITHER the aligned-entry line NOR the supplier line. Controller must be ready to ADD the supplier line at Step 0/4, and must not assume "aligned-entry line intact" from the live pre-image.

## 3. Activation root structure vs mfa_activate — **PASS**

- `loadlist.txt` (file 2) read directly: header rows `modforge-loadlist 1` / `title PPSA33286` / `engine unreal` / `framework ue4ss-ps5` / `mod TownfallItemSpawner 1.0.0`, then exactly the 7 file rows with roles `config`/`lua_mod` — matches V2 §A verbatim.
- Byte-size truth: each row's size equals the actual staged file (779/588/186/2960/21415/0/9962); total preload = **35890 B** (recomputed), matching V2's corrected figure, under the 8 MiB/file and 64 MiB limits.
- Format: ASCII, TAB-separated (12/12 lines tab-bearing), LF endings, trailing newline (od-verified) — matches "printf … TAB separators, LF endings, trailing newline".
- Cross-checks: `worker-loadlist-final/final-package/loadlist.txt` sha256 = `49ab7a3a…1e3bf`, 446 B — byte-identical to file 2; `worker-final-staging-rehearsal/rehearsal-root/` contains exactly the 8-file activation set (loadlist + 7 files under `ue4ss/…`).
- Behavioral evidence: rehearsal T1–T6 PASS — layout keys found via `mfa_find` at 588/186; negative controls `MFA_E_TITLE` (rc −4), `MFA_E_SIZE` (rc −8), `MFA_E_NO_LOADLIST` (rc −2) — all-or-nothing byte-size refusal is real.
- Placement/roles match runtime: `$R=/data/modbridge/roots/PPSA33286` is the documented `mfa_config::root` exact string; roles `config`/`lua_mod` are mfa's default preload roles forwarded by `MakeActivatedUE4SSModSource` (RUNTIME-STAGING-PLAN §2).

## 4. Layout pair = PROMOTED facts — **PASS**

Both `MemberVariableLayout.ini` and `VTableLayout.ini` read directly; identical `[NativeLayout]` identity in both:
`EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`, `EngineMajor=5`, `EngineMinor=6` (engine 5.6.1), `NonShipping=0`, `Stats=0`, `Target=prospero-x86_64-sysv`, `PointerSize=8` — exactly the promoted facts. Member file additionally carries `FUObjectArray` (10 members, `UEP_TotalSize=0xB8`) + `TUObjectArray` (7 members, `UEP_TotalSize=0x20`) — matching §A and the rehearsal's parsepair cross-check (`ok:true`, `LIVE-EVIDENCE-EMITTED`, negative controls: stripped-BuildId refused exit 3, null identity refused exit 2).

## 5. Settings PS5 gate fields — **PASS**

`UE4SS-settings.ini` read directly: `[Debug] ConsoleEnabled=0`, `GuiConsoleEnabled=0`, `GuiConsoleVisible=0`, `RenderMode=ExternalThread`; `[CrashDump] EnableDumping=0`; `[General] bUseUObjectArrayCache=false`, `SecondsToScanBeforeGivingUp=120`, `DefaultExecuteInGameThreadMethod=EngineTick`, `DefaultFNameToStringMethod=Scan`. Header documents why no `[Overrides]` section (native activation refuses non-empty `ModsFolderPath`/`ControllingModsTxt`) — and indeed there is none. All PS5 gates present and zeroed.

## 6. Missing prerequisites — **1 CONDITIONAL (open on console), 1 PASS**

- **`/data/modbridge` existence on console: NOT PROVEN LOCALLY — CONDITIONAL.** Local records only reference it (installer verbs `--state /data/modbridge/state --roots /data/modbridge/roots`; Gate A0 probe path `/data/modbridge/intake/.writeprobe`). `worker-risk-update/RISK-REGISTER-V2.md` R-G3-01 "FTP STOR into console dirs unproven — ADVANCED"; `worker-installer-auto-setup/REPORT.md` notes `/data/modbridge` "must be created automatically (console E2E stage 41 failed…)"; V2's own honesty line: no evidenced FTP STOR write route. Gate A0 (write-probe, exactly once, first) is the designed decider — no local evidence contradicts staging, but nothing local proves the write route either.
- **Runtime can find the activation root: PASS.** `mfa_config::root` = `/data/modbridge/roots/PPSA33286` is pinned as the exact string in RUNTIME-STAGING-PLAN §2 (status file lives in that exact root string); rehearsal T5 proved `mfa_find` resolves both layout keys through the loadlist-root structure; §A documents the "Native activation requires both member and vtable layout files" gate is satisfied by files 4+5.

---

## Verdict

**GO, conditional on Gate A0.** All 9 staged artifacts are byte-perfect vs the manifest; loadlist, layout pair, and settings are content-correct; activation-root mechanics are rehearsal-proven. Two flags for the controller before launch:
1. ploader.ini: execute V2 §C as written (add/preserve the single supplier `= 2` line); do NOT act on the briefing's "3 entries / ingame_facts=0" shape — no record supports it; assume the live pre-image may lack BOTH the aligned and supplier lines (latest readback does).
2. `/data/modbridge` write route is unproven by design — Gate A0 roundtrip or STOP; use the installer route preferentially if reachable (same bytes/hashes).
