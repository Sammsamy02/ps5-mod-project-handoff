# REPORT — worker-launch9-prediction

2026-10-04 · read-only synthesis · no console contact, no nested subagents, no writes
outside `outputs/2026-10-04-zcode-team/worker-launch9-prediction/`. Deliverable order
honored: CLAIM.json first, LAUNCH9-PREDICTION.md, this REPORT last.

## 1. What was done

1. Read the superseded baseline (`worker-engine-ready-walkthrough/ENGINE-READY-PATH.md`,
   composition-5/r9 lineage) and reused its still-valid loader-thread P1–P6 facts and its
   §6 event-loop ordering correction (mods-started precedes main.lua) where the comp6
   manifest does not touch OWNER/BACKEND/PROG files.
2. Read the final piece set end to end and re-verified load-bearing line numbers on disk:
   - Bridge r12 (`worker-module-start-wiring/runtime_start_bridge_r12.cpp`): window gate
     `:340-389`, configured roots `:149/:608-611/:636-639`, page-quantized lease `:462-493`,
     Stage 2/3 error mapping `:641-697`.
   - Entry r4: freshness prefixes `:190-202`, failed-status writer `:224-247`, stage stamps
     `:296/:322-327`.
   - comp6 scaffold (`2026-10-04-zcode-controller/comp6-integration/`):
     `Platform-NativeProgramContext.hpp:92-135` (configured-roots Validate fork),
     `UE4SSProgram.cpp:427-433` (banner), `:767-791` (setup_paths), `:1011-1033` (version
     override), `:1240-1242` (hook config copies), `:1259-1275` (DoEarlyScan);
     `change-manifest.json` (piece set + fold_ins status "IN FLIGHT").
   - Merged initializer (`worker-initializer-integration/staging/UnrealInitializer.cpp`,
     2006 lines): seed consumption `:719-832`, arming gate `:862-898`, scan-arm skip
     `:913-953`, DoScan gates `:1143-1151/:1234-1245`, FName-verify skip `:1385-1397`,
     tick vtable route `:1668-1767`, ProcessInternal honoring `:1863-1937`, PostInitialize
     GMalloc gate `:396-398`, SCO callback `:480`; plus the shipped intermediate
     `SeedTable.h` (`kRequiredSetComplete=false`) and the generator
     (`gen_seed_table.py:93,126` — completeness = all 4 slots).
   - Final seed table (`worker-seed-table-derivation/SEED-TABLE.json`): slots 0/2/4 seeded
     with derivation evidence, slot 3 GMalloc honestly UNSEEDED (`:55-70`),
     required_mask_effect `:102` (GMalloc consumer skipped → PostInitialize throws).
   - Settings v3: `package-v3/ue4ss/UE4SS-settings.ini` — recomputed on disk: 1448 B,
     sha256 `d8faa4ec…` matching SETTINGS-V3-DECISION §4; `[EngineVersionOverride]` at
     `:24-26`, `HookProcessInternal=0`/`HookProcessLocalScriptFunction=0` at `:34-35`.
   - The exec-page mechanics: `UnrealVirtualBaseVC.hpp:115/:133` (macro tails),
     `ASMHelper.cpp:57-69` (`resolve_function_address_from_potential_jmp` — no
     `__PROSPERO__` conditional anywhere in the file, grep-verified), FLAG-1/FLAG-2/FLAG-3
     definitions in `worker-all-signatures-inventory/SIGNATURES-INVENTORY.md:206-236`.
   - Ridealongs: `worker-game-session-ridealongs/REPORT.md` (observer+qdrv1, non-interference),
     `worker-exec-writability/REPORT.md` §3 (IGW-1 rides the comp6 launch as an appended
     `= 2` ploader.ini line; emits {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL}), fc1 validator as
     the slot-2 validator (SEED-TABLE.json `:53`).
3. Answered the central question (see LAUNCH9-PREDICTION §1): the piece set removes all
   pre-init and scan-class exec reads, but two unconditional exec-page dependencies remain
   inside init — the vtable-walk jmp-probe READ (LoadMap/Tick/ProcessEvent) and the
   EngineTick detour WRITE — plus one non-exec structural blocker: GMalloc (required slot 3)
   is unseeded and PostInitialize throws even if exec pages are fully R/W.
4. Wrote the ordered hop table (E1–E6 loader thread, I1–I13 init thread, L1–L2 event loop)
   with pass condition + satisfying piece per hop, the F7 contingency chain, and the ranked
   failure-candidate list with exact throw strings.

## 2. Key findings

- **The stop location hinges on one unresolved in-flight decision**: the seed-arming rule.
  The staged arming code + generator make the final {0,2,4} table a LOUD incomplete-table
  throw at ScanGame entry (Shape A); the described piece set ("seeds for slots 0/2/4,
  FName ctor binding") implies Shape B (required set narrowed to {0,2,4}). Both are
  documented with exact signatures; the stop FAMILY (before engine-ready) is identical.
- **GMalloc is the piece set's one unremediated REQUIRED slot** — not an exec-page question
  at all. SEED-TABLE.json's own required_mask_effect predicts the PostInitialize throw.
- **The "tick vtable route = no code reads" framing is corrected by FLAG-2**: the macro
  chain unconditionally decodes the first instruction at the resolved text address
  (ASMHelper.cpp has no Prospero variant). This read sits at hop I6 (LoadMap) — BEFORE the
  tick hop — so it is the first exec-page touch of the whole init under Shape B.
- The D2 fix removes the scan-class exec reads exactly as designed (text_base=0 absence
  spelling + protection filter), and settings v3 removes the ProcessInternal Zydis reads;
  neither touches the vtable-walk probe or the detour writes.

## 3. Honest limits

- The staged-initializer merge is IN FLIGHT: all MERGED citations are against the merge
  owner's staging copy (`worker-initializer-integration/staging/UnrealInitializer.cpp`);
  the shipped copy may differ (the manifest's initializer fold-in has no REPORT yet, and
  `worker-fname-ctor-audit` gained FNAME-VERDICT.md after the index froze — read here and
  reflected via SEED-TABLE cross-checks, not read in full this session).
- Prediction, not evidence: no claim here has console backing; the D2-only dry build
  (35ba028e, gate GO 8/8) proves the pipeline, not Resolution-B behavior
  (worker-comp6-driver REPORT §4).
- The exec-only/unreadable premise rests on the trusted igf6/r8 records; the IGW probe
  exists precisely because this premise is the leading hypothesis, not a proven fact. The
  prediction states both branches.
- Tool budget: ~40 tool calls, inside the 30–50 bound. Line numbers were read back, not
  carried from other workers' documents, for every load-bearing cite.

## 4. Deliverables

| File | Content |
|---|---|
| `CLAIM.json` | scope + central answer + most-likely outcome (written first) |
| `LAUNCH9-PREDICTION.md` | piece-set table, central-question answer, ordered hop table (E/I/L), F7 chain, ranked failure candidates, single most-likely outcome |
| `REPORT.md` | this file (written last) |
