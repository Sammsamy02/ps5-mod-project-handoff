# FIDELITY-REPORT — runtime-expectation doc chain vs FINAL runtime pieces

worker-ue4ss-docs-fidelity · 2026-10-05 · read-only verification; no console contact, no
nested subagents, nothing executed, no file outside this worker directory written.

All paths relative to `/WORKSPACE/.codex/worktrees/translation-core/what` (`W:`).

## 0. The FINAL artifacts this pass verifies against

| Piece | File | Identity |
|---|---|---|
| Merged initializer | `W:outputs/2026-10-04-zcode-team/worker-initializer-integration/staging/UnrealInitializer.cpp` | **2044 lines** (the docs' "MERGED" was 2006); byte-identical copy in `worker-launch10-prebuild/staging/initializer-launch10/` |
| Bridge | `W:.../worker-module-start-wiring/runtime_start_bridge_r13.cpp` | 734 lines, sha256 `10385e22…`; r12 byte-frozen at `feed5254…`; r13 header: five review diffs, "otherwise byte-identical in behavior to r12"; comp6 driver `BRIDGE_DEFAULT` moved to r13 |
| Entry | `W:.../worker-module-start-wiring/runtime_module_entry_r4.c` | r4 freshness prefix + best-effort failed status |
| Seed tables | `staging/SeedTable.h` vs `staging/SeedTable.launch10.h` | 3-slot launch-9: `kGMalloc{false,…}`, `kRequiredSetComplete=false`. 4-slot armed: `kGMalloc{true,true,0x930add8,…}` (runtime 0x970add8), `kRequiredSetComplete=true`. The launch-10 prebuild stages the 4-slot content **as** `SeedTable.h` |
| Settings | `W:.../worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini` | 1448 B, sha256 `d8faa4eca2ed2f22…` |

Key cross-artifact facts established by read-back:

- Shape A **shipped**: the arming gate is the compile-time `kRequiredSetComplete` check
  (initializer `:897-909`, throw at `:908`). There is no Shape-B required-set narrowing in
  the code; instead the 4-slot TABLE satisfies Shape A (gate passes, all four slots present).
- **IGW gating is OFF**: `kExecWritabilityVerdictGating = false` (`:620-637`,
  "DESIGNED, NOT ARMED… nothing reads any verdict file").
- Slot-3 consumer exists: `SeedArming::consume_required_slot_seeds` binds GMalloc at `:781`
  (Phase 2b `:763-783`); armed log line "slots 0/2/3/4" at `:927`.
- Sequencing is recorded in the derivation JSON (`worker-seed-table-derivation/SEED-TABLE.json`
  `:106/:112`): the gmalloc constant was "deliberately HELD OUT of the launch-9 build per the
  Shape-A sequencing decision: launch 9 = 3-slot table + loud [Seeds] stop + IGW verdicts;
  the armed 4-slot table … enters at launch 10 per LAUNCH10-MATRIX".

## 1. Per-doc verdicts

### 1.1 LAUNCH9-PREDICTION.md — verdict: CURRENT (logic verified), line-drift + r12→r13 caveat

Every semantic claim checked matches the final artifacts:

| Doc claim | Final artifact check |
|---|---|
| `[Seeds] Required-slot seed table incomplete: present […] missing [gmalloc] …` (MERGED `:876`) | Exact string present at `:908`; the full string now also carries the tail "Arming requires all four REQUIRED slots (SHAPE A safe default, LAUNCH9-PREDICTION): … Startup blocked; nothing bound, nothing exec-side touched." — the doc's ellipsized quote is a correct prefix |
| `UnrealInitializer::PostInitialize: GMalloc is uninitialized.` (`:396-398`) | Byte-exact at `:396-398` — unchanged |
| `[Seeds] REFUTED: …` (`:635-641`) | Present; throw now at `:662` ("Terminal for this launch; derive afresh with new evidence, never replay.") |
| Settings v3 `[EngineVersionOverride] MajorVersion=5 MinorVersion=6` `:24-26`; `[Hooks] HookProcessInternal=0 HookProcessLocalScriptFunction=0` `:34-35`; 1448 B sha `d8fa4ec…` | All exact (ini lines 24-26 / 34-35; sha256 `d8faa4eca2ed2f22…` recomputed) |
| Entry r4 `[t=<ms>ms pid=<pid>]` `:190-202`; failed status `:224-247`; `mfa_activate_failed` `:296` | Prefix appends at entry `:198-202` (inside cited span); `write_status_failed` at `:228` (off by 4); `:296` exact |
| Bridge r12: `/app0` `:149`, anchor diagnostics `:360-375`, opt-ins `:608-611`, cfg_gamedir/workdir `:638-639`, lease `:462-493`, MRS_ERR_CONTEXT `:683-692`, MRS_ERR_CONVERSION `:694-697`, StartActivatedRuntimeWithAck `:649-661` | All verified EXACT against r12 (`anchor_cov/match/mm/c_either` at `:360-375`; `allow_vm_scan_base_as_module_image_base` `:606`, `allow_configured_roots…` `:611`; `:638-639`; `:658`; `:684/:689`; `:695/:700`). Caveat: the FINAL bridge is r13 — same behavior (r13 header: anchors, walk bounds, ladder semantics, configured-roots attach unchanged), lines shifted ≈ +20-32 (cfg_gamedir/workdir now `:670-671`; CONTEXT now `:716/:721`; CONVERSION now `:727/:732`), plus the NEW `bridge_lease_reg_spans` diagnostic at `:521` |
| I10 "ProcessInternal block SKIPPED … settings v3 → scaffold `:1240-1241` → gate `:1876`" | CONFIRMED: gate `if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)` now at `:1914`, with the retained-pipeline honoring comment at `:1902-1913` naming both ini keys. **No invalidation of the "Zydis block gated off" claim — the v3 key-off is exactly what makes FLAG-3 a no-op.** |
| Seed constants: GUA 0x97ba110, pool 0x9765ec0, ctor base+0x23d590=0x63d590, SCO 0x890aa0, image sha `3b6cfad4…` | All match the generated tables (linked 0x93ba110 / 0x9365ec0 / 0x23d590 / 0x490aa0; runtime = linked + 0x400000; kImageSha256 identical in both tables) |
| MERGED `:862-877` arming gate; `:719-832` consume; `:913-922` ps_scan skip; `:1385-1397` verifier skip; `:1659-1667` LoadMap; `:1749-1764` tick callbacks; `:1876` A-PI; "2006 lines" | LINE DRIFT (file now 2044 lines): gate `:897-909` (throw `:908`); consume def `:741` (SCO bind `:853`); armed block `:910-931` (log `:927`); Lua-override gates `:1181`/`:1276`; verifier skip `:1425-1432`; LoadMap walk `:1690-1697` (OPTIONAL_GET `:1693`); Tick `:1701`; tick-callback registrations `:1787`/`:1798` (+ persistent `:1804`); "Native game-thread initializer callback registration failed" throw ≈`:1791` (string present); A-PI gate `:1914`; ProcessEvent walk `:1854+`; required-objects log `:1886` / TypeChecker `:1898` |
| Generator completeness rule `gen_seed_table.py:93/:126` | Stale: `kRequiredSetComplete` is now emitted at generator `:214` (rule inside the slot-normalization path) |

Invalidation flag (scoped, and the doc itself anticipated it): §5's closing conditional —
"the controller's final seed-arming rule (all-4 ⇒ #2 first; {0,2,4} ⇒ #3/#4/#1 in that
order)" — is now SETTLED by the artifacts: Shape A shipped and launch 9 runs the 3-slot
table, so for launch 9 the prediction is candidate #2 (the `:908` loud refusal), exactly as
the doc's I3 Shape-A branch describes. The doc's candidates #1 (PostInitialize GMalloc
throw) and #2 are both UNREACHABLE under the launch-10 armed 4-slot table (gate passes;
GMalloc binds from the seed at `:781` before PostInitialize) — those candidates remain valid
only as launch-9/3-slot outcomes. "Engine-ready is not the expected outcome of launch 9"
stands.

### 1.2 ENGINE-READY-PATH.md — verdict: STALE (superseded; its reused parts stand)

Self-declared superseded by LAUNCH9-PREDICTION; verified faithful to its OWN artifact
(`worker-query-s2-arm/staging/UnrealInitializer.cpp`: GMalloc string at `:389` exact,
"PS scan timed out" at `:630` exact) but divergent from the final chain:

- Initializer unit replaced (1627-line query-s2-arm overlay → 2044-line merged SeedArming
  unit). Corrected line map (final): GMalloc `:396-398` (was UI:389); "PS scan timed out"
  `:978` (was UI:630); AOB aggregate `:989`; "FName constructor verification callback
  registration failed" `:1330` (was UI0:907); TypeChecker `:1898` (was UI:1494);
  ExecuteUbergraph/ProcessInternal refusal ≈`:1922` (was UI:1503); required-objects empty
  throw after log `:1886` (was UI:1482-1485).
- Behavior invalidated by the final pieces: ps_scan loop + AOB scans are SKIPPED when seeds
  are armed (`:927` log; gates `:1181`/`:1276`); VerifyFNameConstructor is skipped when
  seed-bound (`:1425-1432`); ProcessInternal is settings-gated (`:1914`; v3 keys 0 ⇒ block
  never runs); the PostInitialize GMalloc gate is no longer the guaranteed terminal (slot-3
  seed binds first under the armed table); Shape-A refusal and seed refutation classes did
  not exist when this doc was written. Bridge/entry are r9/r2-era here (final r13/r4).
- Still authoritative: §0 legend, P1–P6 loader-thread facts, §5 directory derivations and
  §6 event-loop ordering — the parts LAUNCH9-PREDICTION explicitly reuses
  ("OWNER/BACKEND/PROG per its §0 legend").

### 1.3 LAUNCH10-MATRIX.md — verdict: CURRENT as a decision router; STALE in cited details

Substantively intact (its rows are keyed on launch-9 evidence classes, which the sequencing
decision preserves), with these fixes:

- D1 `T-A` cites "MERGED `:876`" → `:908`.
- §3.1 `--bridge-sha256 <r12-sha-from-REPORT>` → the driver default is now r13
  (`build_runtime_composition6.py:81` BRIDGE_DEFAULT; `:604` sha default "r13 measurement";
  r13 sha `10385e22…`; r12 frozen `feed5254…`). The same r12 wording sits in its cited
  source, `worker-comp6-driver/ONE-PASS-CHECKLIST.md:21/:63/:77/:85/:129`.
- U-SWAP-5 initializer cites: `:1876`→`:1914`; `:1385-1397`→`:1425-1432`; tick registration
  `:1749-1764`→`:1787-1804`; SCO `:480`→`:479-481`. Its SUBSTANCE still holds: in the final
  file the EngineTick registrations (`:1787/:1798/:1804`) and the SCO searcher-pool hook
  (`:479-481`) remain unconditional; the compile-time gates cover the scan arms and the
  A-PI block, not the detours.
- "Status at matrix time" columns are overtaken: the gmalloc candidate is derived with a
  V1/V2 validator and the armed 4-slot table is staged (U-SWAP-1 effectively landed;
  U-SWAP-6's validator exists in the derivation receipts). Checklist box 1's rule ("a
  `missing [gmalloc]` line is T-A (expected) or a Shape-B anomaly") remains correct for the
  launch-9 3-slot build; after the 4-slot table ships, that line can only mean stale-table
  staging drift.
- Consistency rule 1 (T-GM ⇒ W_WORKS-class) is UNCHANGED and consistent with the final code
  (PostInitialize is downstream of both exec dependencies); under the armed table T-GM
  becomes unreachable (seed binds first), which only narrows the row set.

### 1.4 MASTER-VERIFICATION-RUNBOOK.md (the CP table) — verdict: STALE-with-fix (3 items); error-code table CURRENT

INVALIDATED / STALE:

1. **CP2 tick constant (invalidated).** The runbook expects
   `GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)` = "linked `0x33ac8c0` +
   base `0x400000`" (§3 CP2, §6 row "Tick line ≠ `0x37ac8c0`", gap G3). The final
   byte-evidenced correction (SEED-TABLE.json slot-10 `correction_recorded`; initializer
   comment `:1746-1757`) reverses the convention: **runtime Tick = base + linked
   `0x2fac8c0` = `0x33ac8c0`**; "the once-circulated 0x37AC8C0 was a base-convention error:
   a runtime value mislabeled as linked" (linked 0x33ac8c0 decodes mid-instruction and
   contains ud2). Corrected expectation: vtable address ≈ **0x33ac8c0**. Additional CP2
   drift: under the armed seed path the scan arm is skipped, so the "both addresses equal"
   pass criterion can never print — the vtable fallback is the only tick route and the
   constant deliberately never enters the unit (comment `:1738-1757`).
2. **§4.3a restage heuristic (invalidated by settings v3).** "its ini sha `9f3fa127…30f7f`
   would diverge … check whether `UE4SS-settings.ini` line 12 was restaged" — settings v3
   (1448 B, sha `d8faa4ec…`) is an AUTHORIZED restage with different bytes; ini-sha/line
   divergence no longer proves an unauthorized restage. Re-pin the heuristic to the v3 sha.
3. **§1 L-line grammar "no timestamp" (stale).** Entry r4 prefixes every UE4SS.log failure
   line with `[t=<monotonic ms>ms pid=<pid>]` (entry `:184`, appends `:198-202`). The rest
   of the grammar (`modbridge module_start failed: <stage> code=<n> detail=<...>`) is
   unchanged.

CURRENT (verified, no fix):

- **Error-code table survives the r13 STOP_EXN renumber.** §1's "stage 1=CONVERSION
  2=CONTEXT 3=START 4=INTERNAL" matches the FINAL `runtime_start_bridge.h` exactly
  (`MRS_OK=0, MRS_ERR_CONVERSION=1, MRS_ERR_CONTEXT=2, MRS_ERR_START=3,
  MRS_ERR_INTERNAL=4`). The renumber moved only `MRS_ERR_STOP_EXN` 2→5 — a module_stop code
  this runbook never cites — and the header itself notes consumers test zero/nonzero. The
  §6 row `runtime_start_failed value=0x1/2/3` and the post-launch mapping
  (bridge 1/2/3/4 → CONVERSION/CONTEXT/START/INTERNAL) stay valid.
- **D1 diagnostic tags.** Entry r4 still emits `module_start_entered` (`:345`),
  `runtime_start_ok` (`:316`), `runtime_start_failed` (`:323/:324/:327`),
  `mfa_activate_failed` (`:296`) — the CP0/CP1 shapes hold.
- Status ladder grammar (`activated` → `engine-ready` → `mods-started`, terminal
  `stopped`/`failed`) unchanged.

## 2. Corrected-lines list (consolidated)

Final merged initializer (`worker-initializer-integration/staging/UnrealInitializer.cpp`,
2044 lines) vs the docs' MERGED citations:

| Item | Docs cite | Final |
|---|---|---|
| GMalloc PostInitialize throw | :396-398 | **:396-398 (unchanged)** |
| SCO searcher-pool hook | :480 | :479-481 |
| `[Seeds] REFUTED:` throw | :635-641 | :657-663 (throw :662) |
| `consume_required_slot_seeds` | :719-832 | def :741; gmalloc bind :781; SCO bind :853 |
| Arming gate (Shape A) | :862-877 | :897-909 |
| `[Seeds] Required-slot seed table incomplete` throw | :876 | **:908** |
| Armed block / ps_scan skip | :913-922 | :910-931 (log :927) |
| Lua-override scan gates | :1146-1151 / :1234-1245 | :1181 / :1272-1285 (`!bSeedsArmed` :1276) |
| Layout axes / publish / ArmLayoutGate | :1159-1203 | :1213 / :1215-1234 |
| FName detour verifier skip | :1385-1397 | :1425-1432 (def :1301) |
| LoadMap first vtable walk | :1659-1667 (:1662) | :1690-1697 (OPTIONAL_GET :1693) |
| Tick resolve + 0x33ac8c0 comment | :1668-1767 | :1701+; comment :1738-1757; fallback :1758-1778 |
| `RegisterEngineTickPreCallback` ×2 (+persistent) | :1749-1764 | :1787 / :1798 (+ :1804); throw ≈:1791 |
| ProcessEvent walk | :1814-1822 | :1854+ |
| Required-objects log + empty throw | :1848-1854 | :1886 (+ throw ≈:1891-1893) |
| TypeChecker throw | :1860 | :1898 |
| A-PI / ProcessInternal gate | :1876 | **:1914** (honoring comment :1902-1913) |
| "PS scan timed out" / AOB aggregate | (UI-era :630/:638-658) | :978 / :989 |
| "FName constructor verification callback registration failed" | (UI0:907) | :1330 |
| File length | 2006 lines | **2044 lines** |
| gen_seed_table.py completeness rule | :93/:126 | kRequiredSetComplete emitted :214 |

Bridge r12 → r13 (behavior identical per r13 header; r13 sha `10385e22…`):

| Item | r12 (docs cite) | r13 (final) |
|---|---|---|
| `kConfiguredGameDirectory="/app0"` | :149 | :~168 (same constant) |
| anchor diagnostics `anchor_cov/anchor_match/anchor_mm/anchor_c_either` | :360-375 | :380-395 |
| `bridge_exepath_source` | :405/:435/:444 | :426/:456/:465 |
| lease clamp + `bridge_lease_reg` | :462-493 (:466-468) | :484-522 (:488/:522) |
| `bridge_lease_reg_spans` (NEW) | — | **:521** (header note :8) |
| adapter opt-ins | :606-611 | :627-632 region |
| `bridge_cfg_gamedir`/`bridge_cfg_workdir` | :638-639 | :670-671 |
| MRS_ERR_CONTEXT returns | :684/:689 | :716/:721 |
| MRS_ERR_CONVERSION returns | :695/:700 | :727/:732 |
| `MRS_ERR_STOP_EXN` | value 2 (collision) | **value 5** (header :36-45; static_assert + zero-init + activation-first also new) |

Entry r4 (all verified): prefix `[t=` :198 / `ms pid=` :200 / `] modbridge module_start failed: ` :202;
`write_ue4ss_log_line` :184; `write_status_failed` :228; `mfa_activate_failed` :296;
`runtime_start_ok` :316; `runtime_start_failed` :323-327; `module_start_entered` :345.

Settings v3 (all verified): `[EngineVersionOverride]` :24, Major=5 :25, Minor=6 :26;
`HookProcessInternal=0` :34, `HookProcessLocalScriptFunction=0` :35; 1448 B;
sha256 `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038`.

## 3. Direct answers to the two named invalidation questions

1. **Does settings v3's key-off change the launch-9 prediction's "Zydis block gated off"
   claim?** No — it CONFIRMS it. The final initializer's gate at `:1914` tests exactly the
   two v3 keys (`bHookProcessInternal || bHookProcessLocalScriptFunction`), v3 sets both
   REQUIRED 0 (`:34-35` with the rationale comment), and the retained-pipeline honoring
   comment (`:1902-1913`) names the ini keys and the skip. The FLAG-3 Zydis block
   (disassembly of 164 code bytes) never runs under v3; the launch-9 doc's I10 is current.
2. **Does bridge r13's STOP_EXN renumber change any doc's error-code table?** No. The only
   error-code table in the chain (MASTER-VERIFICATION-RUNBOOK §1 grammar + §6 triage, plus
   the post-launch classifier's bridge 1/2/3/4 mapping) covers module_start codes 1-4,
   which the renumber does not touch; `MRS_ERR_STOP_EXN` (2→5) is a module_stop code cited
   by none of the four docs. The launch-9 doc's E-row codes (CONVERSION/CONTEXT) are
   unchanged in value; only its r12 line numbers shifted in r13.

## 4. Honesty bounds

- Static read-back of saved files only; no launch, no console, no execution.
- Line numbers are against the artifacts as they exist on disk now (initializer cpp mtime
  Oct 4 22:51; seed tables 23:51/23:58; prebuild 00:25); any further re-merge re-shifts them.
- Which table/composition ships a given launch remains the controller's recorded decision
  (SEED-TABLE.json :106-112); this report does not predict launches or outcomes.
- No quality, completion, or >8 claim of any kind is made or implied.
