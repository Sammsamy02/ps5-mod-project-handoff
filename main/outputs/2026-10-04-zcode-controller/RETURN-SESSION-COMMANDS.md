# RETURN-SESSION-COMMANDS — the single cheat-sheet (controller; rev 2026-10-05c — launch-10-build refresh; supersedes rev 2026-10-05a. Note: 2026-10-05b is the `LAUNCH-DAY-10.md` rev; no rev-b of THIS file was published.)

**Console state UNCHANGED** (last controller-verified reads, 2026-10-05; re-verify at open —
hashes in these docs are last-VERIFIED reads, not promises for today): `/data/plugins/ploader.ini`
= SHORTEST supported config — ONLY supplier_composition (=2) and path_test (=2) enabled;
aligned_entry and ALL probes DISABLED (=0), incl. xwp; sha
`1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`. Settings v3 verified `d8faa4ec`
(1448 B) at `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`; loadlist `4d98ecd7` intact;
mod tree verified (main.lua 21415 B, UEHelpers 9962 B). Manager **8084** (never 9223); FTP
`ftp://ps5-host-not-configured.invalid:2121` anonymous.

HISTORY (do not re-derive): launch-9 RAN 2026-10-05 (pid 273) — no crash, game reached its menu, NO
mods loaded (Item Spawner popup correctly absent); composition-6 refused at the PLH lease read, the
aligned entry refused TERMINAL at the facts adapter, the entry's self-contained fallback harness
FULL-PASSED (incl. a live embedded Lua VM). Full verdict: `launch9-evidence/LAUNCH9-RESULT.md`.
The 9-entry canonical `fd9c7393…` (440 B) and the 3-plugin intermediate `32d0c0d9…` are superseded.

Tool: `python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py <flow>` —
rev `b982f794` (adds the files-first-config-last staging order, the `stage-launch10` single-PRX
packet and `restore-ini`; host suites: selftest **462/0** + fake-drive **143/0**, exit 0/0/0 —
`worker-launch10-staging-packet/SELFTEST-RECEIPT.txt`). HISTORY: rev `5ca8d1c7` (ninth-slot
adoption; selftest 405/0) is superseded. Flows: plan dll8 dll7 e2e stage-ridealongs stage-observer
stage-qdrv1 stage-settings **stage-launch10** **restore-ini** rollback-ridealongs game-evidence all
selftest; flags `--dry-run`, `--assume-yes`, `--dll {7,8}`, `plan --only csv`, `--run-root`,
`--continue-on-failure`; stage-launch10 adds `--fself <path> --expect-sha <sha256>`. The delivered
wrapper `worker-launch10-staging-packet/run_stage_launch10.sh` NOW EXISTS (supersedes the
LAUNCH-DAY-10 rev-b note that said it was not yet delivered). Archive each flow via
`worker-session-archive-scaffold/session_run.sh`.

## Launch-10 build state (the critical path; `launch10-build/`)

EDITION.json pins edition **launch10-final-b** — every unit sha re-verified on disk this refresh:

1. geometryfix core+capacity: `final-units/live_module_facts.geometryfix.cpp` `440a5786…`
   (intersect-extent redirect + capture capacity/static-buffer hunk).
2. initializer (v3): `final-units/UnrealInitializer.cpp` `c3a1c9eb…` (pool-bound v2 + validators +
   read-path guards + armed SeedTable 4/4) — **v4 fold PENDING** (see the gate below).
3. sink: `worker-diagnostic-sink/staging/diagnostic.c` `f5e96c41…` (BOOTSTRAP_UNITS substitution).
4. staged header: `launch10-build/staging/live_module_facts.hpp` `67170b21…` — anchor cap
   kMaxAnchorMappings 8→**16** AND capture kMaxVmMappings 1024→**16,384** (overrides the
   driver-pinned default `f114f876` via `--stage-header`; the stage-header receipt must show BOTH pins).
5. consolidated+repaired bridge: `final-units/runtime_start_bridge_r14_diag_spanpub.cpp`
   `663e7ce9…` (r14+diag+spanpub merged; repaired: orphan call removed, 4-arg wrapper call,
   SeedValidation.hpp staged, missing brace restored; standalone -Werror PASS).
   Plus **walkguard edit 5** (hardcoded in the driver as `--replace-runtime-unit
   asmhelper-asmhelper`): `worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp`
   `694af016…`, shipped **fold-0** — the guard WRAPS every walk read but the RX-fold define is
   ABSENT, so it refuses every walk text read LOUDLY (`[WalkRxGuard] REFUSED …`) with ZERO syscalls;
   composed guard object `81dd832b…`. Plus the **layout freshen**: luamod/cppmod/settings recompiled
   from UNCHANGED attested sources against current headers (`freshen-deps.txt`, 1321-line dep list
   seeding the replace receipt) — zero new log/refusal strings by construction (ORACLE-SUPPLEMENT S4).

`launch10_one_pass.py` (rev `a93b4b34…`) is the hardened receipt-tied runner: unit-sha verification
refuses EDITION drift; `--full` = verify-only → receipt regen (initializer coverage merged) → two
passes (`prx-candidate-l10-1`, `-2`) → ODR + finalize + post-link accessor verification → gate v2 →
determinism twin (same `coherent-tick-file-witness-runtime.o` sha). The accessor/final-packaging
check is receipt-tied (package-profile + build-receipt + the driver-written `artifacts-receipt.json`;
every claim hash-enforced — filename-only acceptance and guessed ELF selection are REMOVED).

Wrapper status (honest): the stage-aware delta-reconciliation wrapper was validated by
`worker-wrapper-validation` → **NO-GO** against driver `ee154e7d…` (one defect: the pre-proof
compile lacked the `LLVM_CONFIG` env; fail-closed direction proven 10/10 harness cases). Both
specified fixes (env line + attempted/accepted decline record) ARE applied in the current driver
`f4103572…`. The post-fix GO procedure (re-run `harness.py` with `expect_superseded=True`) is NOT
yet recorded — the GO is banked in the driver code, not in a re-validation receipt.

On-disk build status: **the full pass has NOT completed.** `prx-candidate-l10-1`'s last attempt
ended with `GAPS.json` (lineage `undefined_delta_added_matches_checkpoint_claim`) and an accessor
verification FAIL-MISSING-PRX (09:26, before the runner hardening); there is no `l10-2`, no gate v2
GO ×2, no determinism proof. The build has not landed — LAUNCH-DAY-10 gate §1 is not green; do not
stage. (HISTORY: the earlier `TEAM/.superpowers` stray-root shadow is GONE from disk;
`worker-stray-root-audit` + `worker-stray-root-hardening` delivered `find_root_v2.py`; the frozen
`package_prx.py` `e6fbe6c5…` itself remains unmodified.)

## The gate — the full pass fires when worker-fname-validator-v4 folds

`worker-fname-validator-v4` RE-corrected the FName-pool admission (host fixture suite **69/0**,
`receipts/PASS-pool-bounds-v4-fixtures.json`): CurrentBlock is a **0-based open block** (engine
predicate `id>>16 <= CurrentBlock`); **image-resident blocks are legitimate** (static-BSS allocator —
v4 INVERTS the v3 no-image rule; image-readable spans publish IN FULL, exec/prot-none never publish;
storage separation kept); entry-0 **'None' anchor** added (`check_none_anchor`); the outside-image
refusal clause is DROPPED. Staged units: `staging/UnrealInitializer.cpp` `4a2b9400…` +
`staging/SeedValidation.hpp` `b84f8585…` (SeedTable.h unchanged). Fold mechanics per
`worker-fname-validator-v4/DIFF.md`: whole-file replacement of the two final-units files PLUS the
REQUIRED one-line bridge rename `publish_nonimage_readable_spans(` → `publish_readable_spans(`
(~line 730; the alias currently delegates byte-for-byte, so it compiles either way; completion
marker = zero references). Folding changes unit shas → **refresh EDITION.json first**, then run
`launch10_one_pass.py --full`. Labeling law: the MUST-PASS image-resident fixture stays
**candidate/synthetic** — `worker-blocks0-provenance/PROVENANCE.md` verdict: NO CAPTURE EXISTS;
`[0x970c000,0x9a34000)` is a VMMAP mapping-class observation, not a captured Blocks[0] value.

## Phase 0 — console back (game CLOSED)

1. Reachability: curl http://ps5-host-not-configured.invalid:8084/list_payloads  (expect JSON)
2. Pre-image gate: fetch ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini — expect sha `1216aab6…`
   (2-plugin SHORTEST config). HISTORY: the session-eve expectation `8d331b3f…` (247 B), the
   3-plugin `32d0c0d9…` and the 9-entry `fd9c7393…` are all superseded. STOP on drift.
3. comp6 `76ee6af3…` (30,773,943 B) is the currently staged fself (ran launch-9). **HOLD on any
   staging**: the next fself is the launch-10 rebuild, staged ONLY via the `stage-launch10` flow
   after the build lands (gate §1 green: one-pass `--full` complete, gate GO ×2, determinism,
   witness PASS, both stage-header pins). The ini is NEVER written by that flow.

## Phase 1 — game-closed batch (HISTORY + current law)

`python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py all`
(2026-10-05 history: dll8/ALLFAM8 ran — ledger PASS; e2e was dispatched with a missing-intake-dir
blocker recorded in the ledger — never confirmed complete; fallback attribution `--dll 7` —
RECONCILED.) The launch-9 9-entry staging (stage-ridealongs: 8 registered, cc1 last, cc1
`d83410aa…` 26845 B → readback 71528 B `131ff1c2…`, bootstrap whole-file STOR law, settings v3
lockstep pair) is KEPT FOR THE RECORD; the launch-10 law is: stage-launch10 writes exactly ONE
console path (the composition PRX) — pre-image + console `.bak` FIRST, FSELF STORed EXACTLY ONCE,
SHT-strip-tolerant content law vs the LOCAL file, ini re-verified byte-equal `1216aab6…` after,
fresh baselines, GO/NO-GO; on ANY failed verification the OLD PRX is restored ONCE byte-exact,
never a second attempt. Archive: one run dir per flow (verdict --from tool output).

## Phase 2 — game session (launch-10 instruction)

Read `LAUNCH-DAY-10.md` §2 verbatim one-liner to the user; standing rule: OPTIONS only AFTER
mods-started evidence exists (`[Lua] [TownfallItemSpawner] Loaded …` in UE4SS.log; the tool's ge.5
gate). Post-popup evidence layer (OPTIONS→F7 per PS5Pad.ini; the FIRST press after a map load is
the free drain probe — ≥1 NEW `[TownfallItemSpawner]` line MUST appear; press #2+ is mute by
design; 16-payload failure taxonomy across the 5 spawn stages):
`worker-post-popup-evidence/POST-POPUP.md` + `FAILURE-TAXONOMY.md` (integrated in LAUNCH-DAY-10 §4b/§5).
SPAWN-CAPABILITY (`launch10-build/SPAWN-CAPABILITY.md`, source-established, recorded per user
directive 2026-10-05): this candidate is **LOAD-CAPABLE, NOT SPAWN-CAPABLE** — the armed build's
pump-arm path needs an exec-only text read; at fold-0 the walkguard refuses it LOUDLY (NO-CRASH
holds, the mod loads and logs) so `Menu created.` never prints and **first-OPTIONS-press silence
after a successful load is EXPECTED for this candidate, not a drain stall**. Visible spawn is NOT a
launch-10 verdict input (oracle G4). Item Spawner activation has never been observed in-game.
controller: console_batch.py game-evidence (UE4SS.log + runtime-status.txt remain the always-available
durable verification; CP2 tick constant is `0x33ac8c0` — NOT `0x37ac8c0`); any FAIL line → STOP +
rollback-ridealongs.

## Phase 3 — launch-10 plan (read LAUNCH-DAY-10.md rev b end-to-end; it owns gate/evidence/verdict/restore)

Evidence chain READY (host-side; nothing advanced — only the console launch is evidence):
ORACLE.md **83 predictions** (`worker-launch10-acceptance-criteria/ORACLE.md`) +
`worker-oracle-final-sync/ORACLE-SUPPLEMENT.md` (final-build deltas: walkguard rows **W1–W7**,
spanpub tags S2.2, capacity S3, freshen S4; predicted pair **anchor_count=7 / intervals_found=1**
with lease spans 6 / reg 6 — intervals is the COALESCED count; 6 is NOT expected; the hard gate is >0).
`worker-launch10-evidence-upgrade/evidence.sh` `76559126…` — fixture suite **94/0**; verdict ladder
BEST / GOOD / MINIMAL-ACCEPT / FAIL (+ S-30S and NO-CRASH specials); verdict rehearsal
(`worker-verdict-rehearsal/REPORT.md`): all five synthetic sets A–E discriminated 5/5.
Verdicts come ONLY from collected evidence; nothing here is an outcome promise.

Launch-11 assets READY (proposal-level; nothing staged, no launch planned without the user):
- luamod variant-B admission: `worker-luamod-l11-dryrun/VERDICT.md` **GO** — compose executes
  end-to-end, byte-identical double-pass; it deliberately STOPPED before packaging, so packaging
  remains UNPROVEN and is the controller's first exclusive step; `--program-source` REQUIRED (L11D-F1).
- marshal-consumer linkage: `worker-l11-marshal-integration/` delivered the DIFF-1 drain-consumer
  (consumer object `854a476b…`, full relocation chain, host-proven); integration into a launch-11
  candidate build is IN FLIGHT (RUNBOOK.md owns the remaining steps). This is the PRIMARY
  spawn path (zero exec-page writes) per SPAWN-CAPABILITY.md.
- IGW row-0 probe REBUILT (`worker-igw-row0-rebuild/REPORT.md`): ARMED `igd1-e030a09eb4d1`
  (fself `e1541af6…` 20013 B; skeleton `igd1-5f1a25c33203`); binary delta bounded at exactly
  ONE semantic byte (row-0 `confirmed` u64 LSB 0→1 at 0xc0c0) + build-id churn; rows 1–5 stay
  `confirmed: 0`. SECONDARY route; pairing with a probe-loading ini is a user-coordinated staging.
- PC acquisition pack: `inputs/pc-townfall/` (placeholder, awaiting the user's file drop) +
  `worker-pc-acquisition-pack/ACQUIRE.md` + `verify_intake.py`.

## Rollbacks (only on controller reconciliation; never blind)
- ini: current console state = SHORTEST 2-plugin config `1216aab6…` (`32d0c0d9…`, `8d331b3f…`,
  `fd9c7393…` are superseded history; the 9-entry canonical stays preserved at
  `launch9-evidence/ploader-ini-9entry-restore.txt`); the tool's rollback-ridealongs does two
  single-char 2->0 flips + .bak (rollback-runbook §4.3 / worker-final-ini-layout law); `restore-ini`
  restores byte-exact from a saved pre-publish copy with the same pre-publish guard.
- settings: current verified v3 (1448 B `d8faa4ec…`) on console; rollback = recorded v2 pair
  (settings 834 B `9570ead3…` + loadlist) in the stage-settings freeze inventory.
- comp6 slot: launch-9 ran the staged comp6 fself `76ee6af3…` (30,773,943 B); r11 backup chain
  `0911af0c` — re-upload the r11 fself from worker-composition-5-rebuild/prx-candidate-composition5-1/
  (RESTORE-PATH §A.5). stage-launch10 additionally secures a console `.bak` + local pre-image of the
  OLD PRX before any swap.
- Extent law (arithmetic-corrected; applies to every derivation and readback): runtime extent =
  **[0x400000, 0xc0602c8)**; `kImageExtentVaddr` = **0xbc602c8** for FILE vaddrs (uniform slide =
  file + 0x400000). `0xfc602c8` is a known-wrong 60 MiB-biased figure (the launch-8 clamp;
  0x400000+0xbc602c8 double-adds the base) — **never reuse it** (ORACLE.md:114, bridge-r12/r13 reviews).
- After any restore: re-verify settings v3 `d8faa4ec` + mod tree (main.lua 21415 B, UEHelpers 9962 B) unchanged.
