# ZCODE FORK HANDOFF V3 — 2026-10-04 ~22:45Z (supersedes ZCODE-FORK-HANDOFF-V2.md)

You are the new controller for the PS5 mod-manager project (Townfall / PPSA33286,
UE4SS engine integration). Authoritative workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`.

This V3 **updates** `outputs/2026-10-04-zcode-team/worker-handoff-update/ZCODE-FORK-HANDOFF-V2.md`.
V2's state sections (its §0, §1, §2, §4, §6) are superseded by this file; V2's
**launch ritual (§3), console safety rules (§5), and honesty rules remain IN
FORCE unchanged** — they were earned by recorded crashes and are not repeated in
full here. The old controller handoff (`outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`)
remains history only.

What changed since V2, in one line each:
- **Promotion is SIGNED** — the C2 alternative probe ran and PASSED (4/4 readable
  candidates matched, base 0x400000 confirmed); the rung 1→2 layout facts
  (FUObjectArray + TUObjectArray) are promoted.
- **The first real runtime launch happened — and startup FAILED.** Composition-4
  plus all activation files were staged, readback-verified, ploader-armed, and
  user-launched. The loader accepted the module; the module produced **zero
  output**: no UE4SS.log, no runtime-status.txt, no diagnostic log.
- **The 7 additional imports are the PRIME SUSPECT** (198 in composition-4 vs
  191 in the aligned-entry that loads every time) — hypothesis, not a finding.
- **The startup checkpoint diagnostic is IN FLIGHT** (`worker-startup-checkpoint`).
- DLL families: **7 complete, 2 in design**. Product roadmap: **5 milestones**
  delivered. Gameplay remains a prototype.

Authoritative state sources (read-only):
- Checkpoint: `.superpowers/native-observations/transport-experiments-progress.json`
  — the decisive new entries are `zcode_c2_alternative_RESULT` and
  `zcode_PROMOTION_SIGNED` (both 22:19:59Z), `zcode_UE4SS_STAGING_COMPLETE`
  (22:26:30Z), and `zcode_composition_startup_diagnosis` (22:40:38Z, with its
  `import_analysis` sub-entry 22:41:55Z).
- Console evidence copy: `outputs/2026-10-04-zcode-controller/console/c2-alternative-results.jsonl`
  (sha256 `e5683c15f3e5f9a352a766e4f69548f5a4db851b9570b0684f655d0ba1ff5a71`,
  build `igf7-3dd949879a0a`, 299 records).
- Worker deliverables: `outputs/2026-10-04-zcode-team/<slug>/` (each has
  CLAIM.json + REPORT.md; in-flight workers may have CLAIM.json only).
- This document was produced by `outputs/2026-10-04-zcode-team/worker-handoff-v3/`
  from those records. No existing file was modified.

## Standing authorization (user-granted, unchanged from V2)
- Console `ps5-host-not-configured.invalid` (pldmgr HTTP 8084, anonymous FTP 2121): upload/run project
  tests, collect logs, reversible mod ops; game-file changes need backups +
  restoration path. Never repeat an already-passed diagnostic without a changed
  implementation or a specific open question.
- Seat: max 12 open requests total INCLUDING the controller. Workers run
  GLM-5.3-Flash with deep budgets. Refill completed slots; no nested subagents;
  honest host-vs-target evidence always.
- **Installation HOLD still honored: `game_file_mutations_permitted_now=false`;
  Townfall launches only on explicit user authorization for that specific launch.**
- One console actor: the controller. Workers NEVER touch the console.

## 0. One-paragraph state

The promotion gate is closed in the good direction: the C2 alternative
correspondence probe (igf7) passed with zero mismatches and base 0x400000
confirmed by four independent data windows, and the rung 1→2 layout pair was
SIGNED into promoted status (22:19:59Z). Everything for the first real runtime
was then staged and verified on console (22:26:30Z): the composition-4 PRX, 8
activation files, the armed ploader entry. The user-authorized launch ran —
ploader accepted the module ("OK") — and then **nothing happened**: no
UE4SS.log, no runtime-status.txt, no diagnostic record; there is no evidence
module_start was ever reached. The controller's diagnosis (22:40:38Z) exonerates
staging (ELF magic + Gate A1 pass on readback) and names the **7-import delta**
over the proven 191-import aligned-entry as PRIME SUSPECT — the SDK libraries
declare those symbols on disk, but the runtime loader may fail to resolve them
at module load. `worker-startup-checkpoint` is building the falsification probe
(a composition-4-import-surface checkpoint writer) right now. **Runtime startup
is the current critical path and is UNRESOLVED.** Tick/SCO remain explicitly
unresolved. Gameplay remains a prototype.

## 1. Decisive events since V2 (order + receipts)

| UTC (2026-10-04) | Checkpoint / worker | What happened |
|---|---|---|
| 22:13:02Z | `zcode_worker_pad_hardware_probe` | Standalone pad probe delivered frozen (build `padp-79947e016dfe`); **requires ISOLATION from the composed runtime** (pad-lease race). |
| 22:19:59Z | `zcode_c2_alternative_RESULT` | C2 alternative PASS — promotion gate open (details §2). |
| 22:19:59Z | `zcode_PROMOTION_SIGNED` | **Promotion SIGNED** — `LAYOUT_FACTS_PROMOTED_RUNG_1_2_SCOPE`, C1–C5 all met. |
| 22:21:16Z | `zcode_worker_combined_igf7` | Combined C2 + rung-3 probe delivered frozen (build `igf7-695a17bd2c6f`, sha prefix `c0d5dbfb`, 41645 B, determinism byte-equal, 8/8 host tests). For a LATER rung-3 launch; distinct from the C2 probe that already ran. |
| 22:26:30Z | `zcode_UE4SS_STAGING_COMPLETE` | All files staged + readback-verified, ploader armed (details §4). |
| (user launch) | — | Authorized Townfall launch: loader accepted the module; **zero output**. |
| 22:40:38Z | `zcode_composition_startup_diagnosis` | `NO_OUTPUT_STARTUP_STAGE_UNRESOLVED` — staging exonerated, gap localized, import analysis attached (details §5). |
| in flight | `worker-startup-checkpoint` | Startup checkpoint diagnostic building (details §6). |

Also delivered in the same window (status unchanged from V2 otherwise):
`worker-msvcp140-design`, `worker-multi-mod` (184 checks, 0 failures),
`worker-seh64-design`, `worker-module-enum`, `worker-virtual-memory`,
`worker-web-ui-connect` (5 canonical patches; frontend patches are fallbacks —
prefer the sibling outline if it lands).

## 2. Promotion — SIGNED (rung 1→2 scope ONLY)

**C2 alternative — PASSED** (`zcode_c2_alternative_RESULT`, actor
zcode-controller, from igf7-3dd949879a0a in-game records, 299 records; offline
comparison only — no self-fulfillment):

- **Base check:** derived **0x400000**, matches expected; exec span
  `[0x400000, 0x787c000)`, exec extent matches.
- **Candidates (immutable, file-backed, precomputed single-bias addresses):**
  - B — `L"UE5"` string, 8 B: **MATCH**
  - C — UTF-16 "BuildIdOverride", 32 B: **MATCH**
  - D — "Current Changelist\0", 19 B: **MATCH**
  - E — AES S-box, 64 B: **MATCH**
  - A — GNU build-ID note @0xC05F250: **honest REFUSED** (not in a readable
    span) — does not count against, per the spec.
- **Verdict:** required A+B+≥2 of C/D/E with zero mismatches and base
  0x400000; actual B+C+D+E (4 matches, exceeds threshold), zero mismatches,
  base confirmed → **PASS**. Distinctive immutable game-image data confirmed at
  precomputed addresses in the live process, with readable mappings verified in
  the same invocation.

**Signing** (`zcode_PROMOTION_SIGNED`, actor zcode-root-controller,
`LAYOUT_FACTS_PROMOTED_RUNG_1_2_SCOPE`):
- C1 GUA: PASS (layout A validated, corrected predicate, 71,959 objects,
  read+write at 0x97ba110).
- C2 correspondence: PASS (4/4 readable candidates, base 0x400000, zero mismatches).
- C3 axes: PASS (SHIPPING margin 60, STATS OFF margin 12, from game evidence).
- C4 identity: PASS (`5.6.1-0+++UE5` / `04bb4330a7d71dcefe1cc2c4d19769d5`).
- C5 pair: DONE (staged pair emitted with real identity, ParsePair-verified).

**Scope and limits:** promotion extends ONLY to the FUObjectArray +
TUObjectArray subset (rung 1→2). Higher rungs need their own reads and their
own decisions (`worker-promotion-decision/PROMOTION-DECISION.md` §2–3 ladder).
**Tick/SCO are EXPLICITLY UNRESOLVED — not promoted, not substituted by C2.**
C2 establishes image-level correspondence only.

## 3. Proven facts (unchanged from V2 — do not re-derive)

All of V2 §2 stands: single-bias mapping CONFIRMED (base 0x400000, LOAD7 span
start proven, Tick 0x33ac8c0 / SCO 0x890aa0 mapping-confirmed only), GUA
live-validated at 0x97ba110 (71,959 objects, read+writeback), identity/axes
RESOLVED, staged layout pair emitted with real identity, three Windows-runtime
DLL families (InitOnce, Mutex, FileIO) TARGET-PROVEN on console. The FTP
caching lesson and the ploader re-arm protocol stand unchanged.

## 4. The staged launch — what is on the console RIGHT NOW

**Staging** (`zcode_UE4SS_STAGING_COMPLETE` 22:26:30Z, actor
zcode-root-controller, `ALL_FILES_STAGED_VERIFIED_PLOADER_ARMED`):

| item | console destination | sha256 prefix | bytes |
|---|---|---|---|
| PRX (composition-4) | `/data/plugins/modbridge_supplier_composition.prx` | `de372302` | 30,778,567 |
| UE4SS-settings.ini | `$R/ue4ss/UE4SS-settings.ini` | `9f3fa127` | 779 |
| MemberVariableLayout.ini (promoted pair) | `$R/ue4ss/MemberVariableLayout.ini` | `f291d552` | 588 |
| VTableLayout.ini (promoted pair) | `$R/ue4ss/VTableLayout.ini` | `9c20f7c9` | 186 |
| PS5Pad.ini | `$R/ue4ss/PS5Pad.ini` | `1d919fbf` | 2,960 |
| main.lua (Item Spawner) | `$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | `f586043a` | 21,415 |
| enabled.txt | `$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | `e3b0c442` | 0 |
| UEHelpers.lua | `$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | `5a8f6609` | 9,962 |
| loadlist.txt (corrected, 7 rows, 35,890 B total) | `$R/loadlist.txt` | `49ab7a3a` | 446 |

(`$R` = `/data/modbridge/roots/PPSA33286`; full hashes and local sources in
`worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md` §A — the manifest is 9
files: the composition-4 PRX + the 8 activation files above — plus ONE in-place
ploader.ini edit, which is not an upload.)
- **ploader state:** `modbridge_supplier_composition.prx = 2` (mode 2, late);
  `ingame_facts` disabled (mode 0); **`aligned_entry` retained** — the proven
  191-import aligned-entry module line is still in the ini, available as the
  working control. The mode-2 module line is PER-LAUNCH: re-send the entry for
  any new launch (V2 §3 step 5).
- Readbacks: ALL VERIFIED (activation files by sha256; PRX by ELF magic — see §5).

**Outcome of the launch:** ploader showed OK (module accepted, positive module
id). Then **zero output**: no `/data/plugins/UE4SS.log`, no runtime-status.txt,
no diagnostic log, no game crash reported in the diagnosis entry. **No evidence
module_start was ever reached.** This is the current blocker.

## 5. Startup failure — diagnosis of record + import analysis (PRIME SUSPECT)

`zcode_composition_startup_diagnosis` (actor zcode-controller,
`NO_OUTPUT_STARTUP_STAGE_UNRESOLVED`):
- **PRX upload NOT corrupted:** ELF magic confirmed on readback, fself wrapper
  overhead normal, Gate A1 pass.
- **Gap localized:** between loader acceptance and module entry execution.
- **Staging claim reconciled honestly:** activation files fully verified with
  sha256 readbacks; the PRX was verified for ELF magic only — full inner-ELF
  hash equality through the ftpsrv decrypt channel is IMPOSSIBLE by
  construction (documented size law: readback = bare-sdk − 1560 bytes; see
  `worker-v3-startup-diag/REPORT.md` §2). The gap is documented, not hidden.
- **Last observed:** ploader accepted the module (OK). No evidence module_start
  ran.

**Import analysis** (`import_analysis` sub-entry, 22:41:55Z):
- Aligned-entry (loads successfully every time): **191 imports**.
- Composition-4: **197 strong + 1 weak = 198 total** (evidence: composition-4
  `lineage-verification.json` — `composed_undefined_baseline` current_count 198,
  1 added; `composed_undefined_measured` 197 strong + 1 weak).
- **Delta = 7 additional imports:** `sceKernelGetModuleInfo`;
  `sceKernelDlsym` (from extended status); `sceKernelLoadStartModule`;
  `sceKernelStopUnloadModule`; `qsort`; `clock_gettime`; `getpid` (the last two
  are likely already in the frozen contract — verification pending).
- **Hypothesis (PRIME SUSPECT, UNCONFIRMED):** the SDK libraries DECLARE these
  symbols (nm-verified on disk) but the runtime loader may fail to RESOLVE them
  at module load time. The 191-import module loads every time; the 198-import
  module produces no output. This is falsifiable — the diagnostic in §6 tests
  exactly this.
- Parallel design work: `worker-import-fallback` is designing a lazy/weak/
  dlsym/conditional import-resolution fallback for exactly these 7
  (CLAIM on disk; FALLBACK-DESIGN.md pending).

## 6. Diagnostic IN FLIGHT — worker-startup-checkpoint

Task: localize the failure between "ploader accepted" and "module entry
executed" for the composition-4 PRX. Planned steps (from its CLAIM.json):
1. Dump dynsym UND imports of composition-4 and the aligned-entry PRX from disk;
   diff → the new/risky import list (already partially reflected in §5).
2. **Diagnostic (a):** minimal 7-import checkpoint writer →
   `/data/plugins/UE4SS.log` (the frozen igd1 recipe from
   `worker-v3-startup-diag`).
3. **Diagnostic (b):** composition-4 EXACT import surface + checkpoint-only
   module_start.
4. Two-run determinism on both.
5. STAGE-PLAN.md (staging order + outcome matrix), REPORT.md last.

Status at this handoff: CLAIM.json + dynsym tooling on disk; diagnostics and
REPORT not yet delivered — **receive its delivery, then stage per its
STAGE-PLAN**. Decisive logic: (a) loads and writes → imports resolve, fault is
inside the composition-4 body → bisect content; (a) and (b) both fail → the
import surface is the blocker → apply §5's fallback design, recompose, static
checks, stage as a NEW build id; (b) loads where (a) fails is not expected but
would localize per-import.

Controller rules for the diagnostic launch: unchanged staging discipline (V2
§3, §5) — game closed, pre-image hash-log, stale result files renamed, sinks on
`/data/...` never `/tmp`, fresh-connection readback `7f 45 4c 46`, ONE launch
per staged build, user authorization per Townfall launch, never treat ploader
OK as "the module ran". If the checkpoint writer also produces nothing, recall
the FTP caching lesson (a missing readback is not yet proof of not-running) and
the crash protocol before any re-contact.

## 7. DLL families — 7 COMPLETE, 2 IN DESIGN

Complete (all host/native-object verified; **only the first three are
target-proven on console**):
1. **InitOnce** — target-proven (`zcode_worker_4_integration` + console).
2. **Mutex** — target-proven (`zcode_mutex_console_target` 21:27:30Z; results
   [86, 93], mutex enabled:1).
3. **FileIO** — target-proven (`zcode_fileio_console_target` 21:33:47Z;
   fileio AND mutex enabled:1 — "THIRD FAMILY TARGET-PROVEN").
4. **CRT startup** — implemented, 9/9 host PS5 cross-compile clean
   (`worker-crt-startup-implement` 21:48:18Z; design adopted with census).
5. **Static TLS** — implemented (`worker-static-tls`: TLS directory parsing,
   per-thread static TLS blocks, PROCESS/THREAD ATTACH/DETACH callbacks,
   `__tls_import`/`__tls_index` binder; census correction honored: only
   UE4SS.dll has a TLS directory — dwmapi.dll has none; no FLS, no TEB
   emulation claims).
6. **Module enumeration** — delivered (`worker-module-enum` 22:33:47Z: 14
   observed imports, 64-slot table, GetProcAddress name+ordinal,
   LoadLibrary/FreeLibrary refcounting, 14 ms_abi thunks; host + native objects
   passed, 0 undefined symbols).
7. **Virtual memory** — delivered (`worker-virtual-memory` 22:40:38Z:
   VirtualAlloc/Free/Protect/Query, 164 checks 0 failures ASan+UBSan, PS5
   cross-compile zero undefined, MBI layout identical).

In design (assessment/design delivered, implementation pending):
8. **SEH64** (`worker-seh64-design`): minimal viable subset in 3 tiers (M0
   data plane, M1 SEH control flow, M2 control plane); ~70% host-testable,
   ~30% native-only; highest risk in personality trampolines.
9. **msvcp140** (`worker-msvcp140-design`): 282 import slots across 3 DLLs
   analyzed; essential ~125–150 (stream/locale core); no operator new/delete;
   recommendation: one bounded owner for the narrow-only 61-slot dwmapi set
   first.

## 8. Product roadmap — 5 milestones after Item Spawner

From `worker-product-roadmap/PRODUCT-ROADMAP.md` (planning deliverable; effort
ranges are judgments, not calendar promises):

| # | Milestone | Priority | Complexity | Key risks |
|---|---|---|---|---|
| M0 | Item Spawner acceptance ladder (genuine game-thread callback → valid object query → harmless reflected access → unchanged Item Spawner activation → visible spawn via real input) | current, IN FLIGHT — not claimed | — | R-COMP4-01 (now the startup failure), D1 staging, Gate A0, pad `hardware_runtime_validated:false` |
| M1 | Full E2E console run, zero developer intervention (owner-placed-archive route first; `installation_supported` flips to evidence-backed true) | (a) | M–L, 1–2 wk, 3–6 sessions | R-FTP-01/02, R-E2E-FR3, R-ACT-01 |
| M2 | Multi-mod support (N-mod install/toggle/uninstall, conflict classification, mod-list UI) | (b) | M–L, 2–3 wk, 2–4 sessions | classification honesty |
| M3 | Real Nexus download ON console (OAuth/PKCE + TLS first contact; removes the last computer-in-the-loop step) | (c) | L, 3–5 wk, 4–8 sessions | prod OAuth/TLS first contact, R-DL-01 |
| M4 | Additional games/engines (second title through the same evidence pipeline, zero title-switched code) | (d) | L–XL, 4–8+ wk | per-title evidence cost, R-CONTC-01 |
| M5 | App polish for daily use (async jobs/progress/cancel, error taxonomy, validation UX) | (e) | M, 2–4 wk (parallelizable) | honesty-surface erosion |

Sequencing M1 → M2 → M3 → M4 → M5 (M5 items pull forward opportunistically;
M3↔M2 swap possible if real downloads are needed sooner). Everything is defined
relative to M0 being accepted: one real mod provably installs, activates, and
visibly works through real input. `download_authorized` stays false until M3.

## 9. Integration queue (controller-owned, updated order)

1. **Startup diagnosis — THE critical path.** Receive `worker-startup-checkpoint`
   (and `worker-import-fallback`'s design); stage the checkpoint diagnostic per
   its STAGE-PLAN; ONE launch; read the outcome matrix. **No composition-4
   relaunch without a changed implementation or a diagnostic result naming a
   fix** — never replay the identical failed candidate.
2. **If imports are confirmed as the blocker:** apply the fallback design (lazy/
   dlsym resolution of the 7) → recomposition → full static check class (17/17
   equivalent) → stage as a NEW reviewed candidate (new build id + sha256,
   explicit user authorization).
3. **Rung-3 reads** (only after startup is solved): the combined igf7 probe
   (`igf7-695a17bd2c6f`, C2 reads + Objects** → chunks → FUObjectItem →
   UObjectBase → UStruct chain) is delivered frozen for exactly this; rung 3→5
   per `worker-acceptance-evidence/EVIDENCE-SPEC.md`. No rung-3 read has ever
   executed in-game.
4. **DLL track:** SEH64 + msvcp140 design completion; the four host-complete
   families (CRT startup, static TLS, module enum, virtual memory) have NO
   console-target proof yet — target runs need their own authorized candidates.
   Pad hardware probe requires ISOLATION from the composed runtime
   (pad-lease race) — schedule it as a standalone launch, never alongside.
5. **Installer / app / container tracks:** unchanged from V2 §4 items 6–8
   (console E2E rehearsal 61/61 GO remains host-side; worker-8 records/UI audit
   provenance caveat stands; pak index 4699 vs 4544 never cross-apply).
6. **CRT startup implement** (9/9 clean) — review and fold into the next
   recomposition (carried from V2; now relevant to any post-diagnosis rebuild).
7. **Housekeeping:** risk register V2 live; the composition-4 startup failure
   should be appended to it with the diagnosis entry as its receipt.

## 10. Standing rules that still bind (condensed — full text: V2 §3, §5, worker-console-safety)

- **Installation HOLD**: no game-file writes, no autoload changes, no Townfall
  launch without explicit user authorization for that launch.
- **One console actor: the controller.** Workers never touch the console.
- Launch ritual: game fully CLOSED; pre-image RETR + hash-log before overwriting;
  rename stale result JSONL; write the expected-evidence table BEFORE dispatch;
  sinks `/data/...` never `/tmp`; `/data/modbridge` exists; stage → wait 15–30s →
  fresh-connection readback (`7f 45 4c 46`, this-attempt receipt); arm ploader
  and RE-SEND the mode-2 entry every launch; records appear ~1 minute after
  launch; freshness oracle = mtime; ONE launch per staged build.
- Dispatch discipline: one dispatch per authorization; ≥30s between dispatches;
  **a dispatch timeout is a failure signal — NO retry**; crash → STOP all
  contact, user controls power/network, read-only re-contact ladder.
- **ploader OK is NEVER "the module ran"** — today proved it twice (igf5's zero
  records, now composition-4's zero output). The record stream is the only truth.
- FTP caching: a missing/stale readback is NOT proof a module did not run.

## 11. Honest limits — what is NOT proven (do not upgrade these)

- **Runtime startup is UNRESOLVED.** Composition-4 has never executed
  module_start on the target. The 7-import delta is the PRIME SUSPECT — a
  hypothesis with a defined falsification test, not a finding. Nothing in the
  import analysis may be reported as confirmed.
- **Tick (0x33ac8c0) / SCO (0x890aa0) are UNRESOLVED** — explicitly not
  promoted and not substituted by C2. Mapping-confirmed only; exec-span byte
  reads are blocked (igf4 refused, igf6 interrupted); "PS5 exec-only pages are
  hardware-enforced" is the LEADING HYPOTHESIS, not a fact. User directive
  stands: no broad investigation, no repeated text-read attempts.
- **Promotion scope is rung 1→2 ONLY** (FUObjectArray + TUObjectArray). Rungs
  2-completion through 5 have ZERO in-game evidence; the combined igf7 probe is
  frozen, not run.
- **The Item Spawner has never activated in-game.** No visible spawn, no Lua
  line, no full-run acceptance exists. **Gameplay remains a prototype.**
- The staged PRX could only be verified on console for ELF magic — full
  inner-ELF hash verification is impossible through the ftpsrv channel by
  construction (documented size law); the staging claim was reconciled
  accordingly, with the gap documented.
- igf5's cause is unlocalized; the ARGV_ECHO residual (R-ARGV-01/R-G1-02)
  remains unreconciled; today's totals stand at 2 console crashes + 1 game
  crash + 2 non-crash failures + the composition-4 startup failure (cause
  under active diagnosis).
- `installation_supported` and `download_authorized` remain `false` until M1/M3
  console runs flip them with receipts.

## Honesty rules in force (unchanged, from V2)

Candidate != validated. No guessed offsets/widths (width table gates). Predicate
hits never auto-establish identity/base/layout. Unperformed runs are never
reported as passed. Raw provenance is preserved; corrections are recorded as
distinguishable amendments, never silent edits. Only promote what completed
checks actually establish.

---

### Provenance for this document

Produced by `worker-handoff-v3` from read-only sources: V2 handoff; checkpoint
entries `zcode_c2_alternative_RESULT`, `zcode_PROMOTION_SIGNED`,
`zcode_worker_combined_igf7`, `zcode_UE4SS_STAGING_COMPLETE`,
`zcode_composition_startup_diagnosis` (+ `import_analysis`), and the delivered
worker entries of 22:13Z–22:40Z in
`.superpowers/native-observations/transport-experiments-progress.json`;
`worker-promotion-decision/PROMOTION-DECISION.md`;
`worker-v3-startup-diag/REPORT.md`; `worker-startup-checkpoint/CLAIM.json`;
`worker-import-fallback/CLAIM.json`; `worker-staging-pkg-c4/CLAIM.json` +
`MASTER-STAGING-PACKAGE-V2.md`; `worker-product-roadmap/PRODUCT-ROADMAP.md`;
`worker-static-tls/CLAIM.json`. Full itemized hashes for the console inventory
live in `MASTER-STAGING-PACKAGE-V2.md` §A; this table shows sha256 prefixes for
scanability only.
