# REPORT.md — worker-configured-roots-design

**Verdict: DESIGN COMPLETE. Resolution B (explicit configured roots) is the
integrate-now recommendation; Resolution A has no live route on this kernel
(evidence-adjudicated in launch-8). Full diffs + composition mechanics
delivered; nothing was edited outside this worker directory.**

## 1. What was done (task-by-task)

1. **Consumer census (task 1)** — grepped all 371 composition unit sources
   (composition-4 run-3 receipt: 368 runtime units + bootstrap + fixed deps;
   list preserved as `unit-sources.txt`) plus the port include tree, native
   include/src tree, wiring dir, and contributing spike dirs. Result in
   `CONSUMER-USES.md`: "directories only" is CONFIRMED for the
   PROSPERO+mod_source startup path with two exact exceptions — the startup
   banner's `std::filesystem::file_size(m_game_path_and_exe_name)`
   (`UE4SSProgram.cpp:427`, throws on empty → startup failure) and two
   name-only metadata consumers (UEHeaderGenerator `:4172`, JMapGenerator
   `:2015`). Enforcement sites: adapter gate
   (`lmf_to_native_loader_facts.cpp:139-152`), `ValidateNativeProgramContext`
   (`Platform/NativeProgramContext.hpp:83-101`, in-game proof launch-6) and
   its SECOND call in the UE4SSProgram constructor (`:215`). Verified
   non-consumers: NativeModSource / PrepareActivatedUE4SSRuntime /
   RuntimeOwner / StartActivatedRuntimeWithAck — none touch the path.
2. **Resolution B (task 2)** — `RESOLUTION-DESIGN.md` §2: exact unified diffs
   for all seven units (adapter .cpp/.hpp, NativeLoaderContext.hpp/.cpp,
   port `Platform/NativeProgramContext.hpp`, port `UE4SSProgram.cpp`, bridge
   r12). New context fields `configured_game_directory` /
   `configured_working_directory`; default-refusal opt-in
   `allow_configured_roots_for_missing_executable_path`; "executable empty ⇒
   configured game directory required" enforced at the single
   `ValidateNativeProgramContext` choke point; PRX never substitutes the
   executable path; game-image identity (VmScan anchor facts) and
   `module_file_path` (our PRX) untouched and distinct. Threading path:
   entry constants already flow (`MODBRIDGE_MOD_ROOT` → bridge `mod_root`
   parameter, `runtime_module_entry_r3.c:65-68,281-282`); the bridge fills
   the facts between conversion and `MakeNativeProgramContext` — no
   downstream signature changes; one mechanics correction is documented
   inline (the opt-in must be set in the existing options block before the
   conversion call).
3. **Resolution A (task 3)** — §4: evidence table (sysctl ENOENT launch-7;
   exec-record kve_path zeros launch-7; r9 kve discovery scan ALL ZERO
   launch-8 — dead on this kernel; module-info domain mismatch; DT_DEBUG
   dead; dlopen refused by contract) + mount-layout enumeration with
   evidence vs speculative marking (`/app0` evidence-adjacent pending r10;
   `/mnt/app0`, `/system_ex/app/<id>`, procfs routes: zero mentions in the
   entire worktree — speculative, do not design against) + a three-part
   proof standard (in-process provenance, stat through the composition's own
   libc++, build-id identity binding to the retained image).
4. **Decision matrix (task 4)** — §5: four post-r10 branches (expected
   `stat_ownmod=1/stat_app0=0` → B; `stat_ownmod=0` → B plus a flagged
   single-gate review since it contradicts launch-8; `stat_app0=1` → B still
   lands, rung 3 simply serves a real path; future kve path → A-then-B
   composable). Recommendation: prepare B now, integrate on the next launch
   window via ONE composition-6 pass.

## 2. Key integration findings (beyond the task brief)

- **The retained `program` unit embeds the OLD validation.**
  `ValidateNativeProgramContext` is `inline` in the port header, so the
  hash-pinned `program` object carries its own compiled copy — header-only
  relaxation cannot work; `UE4SSProgram.cpp` (and the port header) MUST be
  recompiled under the new contract.
- **The current fixed script structurally refuses both edits**:
  `build_runtime_composition4_fixed.py:59` strictly re-checks every unit's
  original source hash (no attestation fallback on that loop) and `:51` pins
  the port tree via `validate_inputs()`. §3.2 specifies the minimal bounded
  fixed-script edition (attested retained-unit replacement, initially
  `{"program"}` + pre-seeded replace-receipt pins + an ODR sha-equality gate
  between the port and staged copies of `NativeProgramContext.hpp`).
- **ODR hazard identified**: `Platform/NativeProgramContext.hpp` exists as a
  port copy (for `program`) and a staged overlay copy (for bridge/factory
  TUs); today byte-identical (diff-verified) — the design keeps that
  invariant mechanically enforced.
- **launch-8 already proved the fix direction**: once a stat-able path was
  supplied (r9's now-forbidden PRX rung), conversion AND context validation
  passed — so `is_regular_file` works in-process for /data, the module-file
  gate stays intact under B, and the next blocking gates are Stage 2's lease
  (r11 page-quantized fix, untested in-game) and D2's text gates.
- **D2 deconfliction verified by reading its dir**: only `CLAIM.json` +
  `D2-DESIGN.md` exist; its ranked option (c) edits the SAME adapter file's
  `facts.mapped.*` block — hunks disjoint from mine; recommendation: merge
  both into one adapter revision + one composition-6 pass; the one script
  edition is owned by this change-set (D2 needs none).

## 3. Deliverables

- `CLAIM.json` (written first; updated at completion)
- `CONSUMER-USES.md` — task 1 census with file:line + functional weights
- `RESOLUTION-DESIGN.md` — §2 diffs (B), §3 integration mechanics, §4
  Resolution A evidence + proof standard, §5 decision matrix + D2
  coordination
- `unit-sources.txt` — the grepped 371-file unit list (provenance artifact)
- `REPORT.md` — this file (written last)

## 4. Honest scope

- Proposals only: no wiring, frozen script, receipt, staged-root, or port
  files were modified; writes confined to
  `outputs/2026-10-04-zcode-team/worker-configured-roots-design/`.
- No console contact, no nested subagents, no network. All runtime claims
  cite existing ledger entries (transport-ledger composition5-mirror:
  launch-1/2/7/8) or files read on disk.
- Diff line numbers are against the exact canonical files read today; hunks
  that depend on the in-flight wiring lineage note that r10/r11 (not the r9
  driver default) are the packaged base — candidate-2's overlay manifest
  confirms bridge r11 + entry r3.
- Nothing here claims startup success: the r10 stat diagnostic, the r11 lease
  fix, and D2's gates are separately evidenced steps after integration.
