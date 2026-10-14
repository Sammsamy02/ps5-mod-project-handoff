# ZCODE FORK HANDOFF V2 — 2026-10-04 ~22:00Z (supersedes ZCODE-FORK-HANDOFF.md)

You are the new controller for the PS5 mod-manager project (Townfall / PPSA33286,
UE4SS engine integration). Authoritative workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`.

This V2 **completely replaces** `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`.
That file's picture — "v3 PROBE IN FLIGHT at fork", "single-load-bias OPEN",
"LOAD7 mapping not yet found", "fileio-prep ready to apply", "mutex host validation
re-running" — is all OBSOLETE. Every one of those items resolved hours before this
rewrite (see proven facts below). Use the old handoff only as history; act on this file.

Authoritative state sources (read-only):
- Checkpoint: `.superpowers/native-observations/transport-experiments-progress.json` —
  452 `zcode_*` entries with hashes and evidence; latest entries run 21:26Z–21:48Z.
- One-page track summary: `outputs/2026-10-04-zcode-team/worker-checkpoint-reconcile/CURRENT-STATE.md`
  (21:30Z snapshot) + its `RECONCILIATION.md`.
- Controller files: `outputs/2026-10-04-zcode-controller/` (CONTROLLER.md, CONSOLE-LOG.md,
  `console/` receipts: `ingame-v3-full-results.jsonl`, `anchor-validation-results.jsonl`,
  `igf6-results.jsonl`, `mutex-console-result.jsonl`, each with sha256 receipt).
- Worker deliverables: `outputs/2026-10-04-zcode-team/<slug>/` (each has CLAIM.json + REPORT.md).
- This document was produced by `outputs/2026-10-04-zcode-team/worker-handoff-update/`
  from those records; the original handoff file was left untouched (read-only replacement).

## Standing authorization (user-granted, unchanged)
- Console `ps5-host-not-configured.invalid` (pldmgr HTTP 8084, anonymous FTP 2121): upload/run project tests,
  collect logs, reversible mod ops; game-file changes need backups + restoration path.
  Never repeat an already-passed diagnostic without changed implementation or a specific
  open question.
- Seat: max 12 open requests total INCLUDING the controller. Workers run GLM-5.3-Flash with
  deep budgets (100–150 calls). Refill completed slots; no nested subagents; honest
  host-vs-target evidence always.
- Installation HOLD still honored: `game_file_mutations_permitted_now=false`; Townfall
  launches only on explicit user authorization for that specific launch.

## 0. One-paragraph state

Single-load-bias mapping is CONFIRMED with a full trusted in-game segment map; GUA
(GUObjectArray) is live-validated at runtime `0x97ba110` (71,959 objects, read+write);
engine identity and build axes are RESOLVED; the complete game-bound runtime module
(composition-4: supplier + base-scan + query harness + S2 arm + ack) is COMPOSED and
static-clean (17/17); the C2 alternative correspondence check is SPEC-COMPLETE and needs
exactly one probe launch; the three Windows-runtime DLL families (InitOnce, Mutex, FileIO)
all PASSED on the console target; the 9-file master staging package for the first real
runtime launch is assembled and rehearsed READY TO EXECUTE with zero corrections.
Promotion is imminent: staged launch + C2 evaluation are the remaining critical-path steps.
Nothing is promoted yet, and no staged launch has ever been performed (section 6).

## 1. Critical path state

Acceptance ladder (user directive): genuine game-thread callback -> valid object query ->
harmless reflected access -> unchanged Item Spawner activation -> visible spawn via real
input. Rungs 1–2 evidence spec complete; promotion framework delivered
(`worker-acceptance-evidence`, `worker-promotion-decision`) with C2 as the named blocker.

**C2 alternative — DELIVERED, evaluation IN FLIGHT (needs one probe launch).**
`worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md` (+ `candidate-ranges.json`),
checkpoint `zcode_worker_c2_alt_correspondence` 21:37:54Z. Five immutable file-backed
candidates from the retained game image, at precomputed single-bias runtime addresses:

| id | candidate | bytes | runtime addr | segment |
|----|-----------|-------|--------------|---------|
| A | GNU build-ID note | 36 | 0xC05F250 | aux file-backed |
| B | L"UE5" string | 8 | 0x7C26782 | rodata (verified accessor) |
| C | UTF-16 "BuildIdOverride" | 32 | 0x79CFFDA | rodata |
| D | "Current Changelist\0" | 19 | 0x7F8845D | rodata |
| E | AES S-box | 64 | 0x81D2A40 | rodata |

- Relocation check: all 1,663,596 r_offsets target only RW/RELRO segments — zero in
  rodata/aux; candidates are truly immutable.
- PASS: 5/5 matches (bounded form: A + B + >=2 of C/D/E) with zero mismatches AND
  base == 0x400000. FAIL: ANY mismatch (contradicting evidence — promotion STOPS) or
  base != 0x400000.
- Expected bytes are NOT embedded in the probe; offline comparison only (no
  self-fulfillment). Evaluation requires one new probe launch: the existing GUA data is
  BSS, not file-backed, so the probe must read the 5 candidates.
- Scope limit: this establishes image-level correspondence only. Tick/SCO remain
  explicitly UNRESOLVED regardless of outcome.

**Composition-4 — DELIVERED (the complete game-bound module).**
Checkpoint `zcode_supplier_composition_4` 21:39:34Z (`worker-composition-4-fix`):
- Runtime object `coherent-tick-file-witness-runtime.o`, 117,779,552 B, sha256 `45dd9901…`;
  368 units (353 retained + 15 fresh incl. ack runtime_owner/ue4ss_backend + harness
  initializer + supplier).
- PRX `outputs/2026-10-04-zcode-team/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx`:
  30,778,567 B, sha256 prefix `de372302…` (full hash in its `build-receipt.json`),
  17/17 static checks, gaps `[]`.
- Symbols DEFINED in the object: `QueryHarness::Global`, `ArmLayoutGate`,
  `DisarmLayoutGate`, `OnEngineTickDispatch`; ack side `RuntimeAckSink::Open`,
  4-arg `PrepareActivatedUE4SSRuntime`, `StartActivatedRuntimeWithAck`.
- Delta vs composition-3 PRX: EMPTY import delta (still only `[sceKernelGetModuleInfo]`
  added to base; clock_gettime/getpid already present). Composition-3
  (`zcode_supplier_composition_3`, 20/20 checks, PRX `b8bfae77…` 30,756,791 B) remains
  the staging placeholder but is superseded for launch purposes by composition-4.

**Promotion path (both delivered pieces converge):** run the staged launch with the
composition-4 PRX (Tier B evidence expected — ack sink + UE4SS.log + Lua lines), run the
C2 candidate reads (one probe launch), evaluate per the pass/fail criteria above. If C2
passes and staging behaves, the promotion decision lands and rung-3 (reflected access)
work unblocks. Order and evidence rules in section 4.

## 2. Proven facts (do not re-derive; each has records + sha256 receipts)

**Mapping — single-bias CONFIRMED** (`zcode_v3_MAPPING_BREAKTHROUGH` 19:15:31Z, from
1,035 v3 records, build `igf3-de0c0cb644d3`, copy `console/ingame-v3-full-results.jsonl`):
- Load base = **0x400000**; all segments at base + p_vaddr. This was the open arbitration
  question in the old handoff — CLOSED.
- VMMAP quality: 21,721 total records, 16,384 retained, 5,337 truncated, all at 144 B
  stride (arbitration prediction P1 confirmed), trust 1; spans R 16,160 / W 16,012 / X 73.
- LOAD0 EXEC [0x400000, 0x787c000) — byte-exact memsz match to vaddr-semantics
  (0x7479b1c). **Tick runtime = 0x33ac8c0**, **StaticConstructObject runtime = 0x890aa0**,
  both in-span (mapping-confirmed only; see limits).
- LOAD7 start PREDICTED 0x953c000 -> observed first RW span starts EXACTLY at 0x953c000
  (LOAD7 correspondence PROVEN by exact span-start match). The old "gap"
  [0x787c000, 0x8018c000) was v2 stride corruption hiding data segments; v3's clean walk
  found 8 mapped spans. Segment table: [0x400000,0x787c000) X LOAD0 exec ·
  [0x787c000,0x82bc000) R ro1 (~10.2 MB) · [0x82bc000,0x953c000) R ro2 (~18.5 MB) ·
  [0x953c000,0x9568000) RW L7 start · [0x9568000,0x96dc000) R inter-segment ·
  [0x96dc000,0x970c000) RW 2 · [0x970c000,0x9a34000) RW BSS/heap containing GUA.
- FTP lesson born here: v3 ran on BOTH earlier launches; the FTP file-caching artifact
  delayed visibility until the module's write flushed the file. A missing/stale FTP
  result file is NOT proof a module did not run (section 3).

**GUA (S2) LIVE-VALIDATED** (`zcode_anchor_validation_RESULTS` 19:59:58Z, from
igf4-4891869337b4 records, copy `console/anchor-validation-results.jsonl`):
- Address **0x97ba110**, RW span [0x970c000, 0x9a34000). 96-byte read SUCCESS + same-value
  writeback SUCCESS.
- Decoded: ObjFirstGCIndex 35477, ObjLastNonGCIndex 35476, MaxObjectsNotConsideredByGC
  35477, OpenForDisregardForGC 0, Objects** 0x13800220a0, PreAllocatedObjects NULL,
  MaxElements 2,162,688, **NumElements 71,959**, MaxChunks 33, NumChunks 2. All structural
  sanity checks pass — a real FUObjectArray, layout A.
- The batch1 predicate's GC-order refutation was a CHECKER BUG (it tested
  ObjFirstGCIndex <= ObjLastNonGCIndex; the UE invariant is ObjFirstGCIndex ==
  ObjLastNonGCIndex + 1). Fixed and re-validated (`worker-predicate-gc-fix`). Original
  and corrected results remain distinguishable in the record.

**Identity + axes RESOLVED** (`worker-layout-identity`, `worker-axes-detector`):
engine **5.6.1-0+++UE5**, build_id **04bb4330a7d71dcefe1cc2c4d19769d5**, SHIPPING
(margin 60), STATS OFF (margin 12), target prospero-x86_64-sysv, PointerSize 8 —
from the retained image + 244-marker cost model, not guesses.

**Staged layout pair COMPLETE and stageable** (`worker-emit-final-pair`):
`emission-final/MemberVariableLayout.ini` (588 B, `f291d552…`) + `VTableLayout.ini`
(186 B, `9c20f7c9…`); ParsePair-verified, 17 members, all identity real, label
LIVE-EVIDENCE-EMITTED; negative controls passed (stripped-BuildId refused exit 3, null
identity refused exit 2). FUObjectArray 10 members UEP_TotalSize 0xB8; TUObjectArray
7 members 0x20. Scope: rung 1→2 ONLY; code-anchor confirmation PENDING and declared
non-blocking by its task.

**Three DLL families TARGET-PROVEN on the console (Windows-runtime track):**
- Mutex (`zcode_mutex_console_target` 21:27:30Z): build `6f5bf85a0f314df6…`, artifact
  sha256 `fc4b09af…`, 259,568 B, staged at
  `/data/pldmgr/payloads/MUTEX_DLL_TEST/MUTEX_DLL_TEST.elf`. Phases 1/2/3/5, imports
  51/10, mutex enabled:1, results [86, 93], cleanup pending:0 retired:1, first_failure
  all zeros. Host controls had already PASSed after 6 builds (`zcode_worker_mutex`).
- FileIO (`zcode_fileio_console_target` 21:33:47Z): build `664f83a5e8374059…` — **first
  attempt FAILED stage 41 because `/data/modbridge` was absent; FTP MKD `/data/modbridge`
  fixed it; retry PASSED.** Final: imports 61/10, fileio enabled:1 AND mutex enabled:1,
  results [86, 93], same clean cleanup. This entry's own words: "THIRD FAMILY TARGET-PROVEN
  (InitOnce + Mutex + File I/O all passed on console)."
- InitOnce + sync runtime had already passed host+target (`zcode_worker_4_integration`).
- FileIO diffs were rebased on the mutex tree and APPLIED (`worker-fileio-apply` delivered
  host+SDK 21:29:10Z; `worker-fileio-rebuild` delivered 21:32:54Z with corrected report
  path).

## 3. Launch ritual (updated with today's lessons)

Core sequence (unchanged in shape, expanded with what the day taught):
1. **Game fully CLOSED.** Verify no suspended Townfall application.
2. **Pre-flight (per worker-console-safety + master staging package):** pre-image RETR +
   hash-log of anything the pass will overwrite (back up on console only after recording
   the pre-image hash); rename (never delete) stale result JSONL; write the
   expected-evidence table BEFORE dispatch; ensure result sinks are `/data/...` — NEVER
   `/tmp` (FTP refuses /tmp reads; INITONCE c49108fe lost its results this way).
3. **Ensure `/data/modbridge` exists** (and any namespace dirs the payload expects).
   Lesson: the fileio target failed stage 41 purely because `/data/modbridge` was absent;
   `FTP MKD /data/modbridge` fixed it. The staging package also writes under
   `/data/modbridge/roots/PPSA33286` — mkdir those paths first (manifest §B Step 1).
4. **Stage + verify before any dispatch.** After any STOR/upload: wait 15–30s, then
   readback on a FRESH FTP connection; ELF readbacks must start `7f 45 4c 46` (ftpsrv
   auto-decrypts FSELF on RETR; local FSELF starts `4f 15 3d 1d`); verify sha256 against
   THIS attempt's receipt only.
5. **Arm ploader** (user clicks ploader.elf in Payload Manager, or
   `GET /loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`), then **RE-SEND the
   mode-2 entry for THIS launch**: a mode-2 module line in `/data/plugins/ploader.ini`
   is per-launch — a module sent for launch N is NOT present at launch N+1 without the
   arm + entry re-send.
6. **User launches Townfall.** Records appear ~1 MINUTE AFTER LAUNCH at injection
   completion (ploader shows "injecting 2 plugins", then OK/FAIL ~1 min in). Wait ~2 min,
   then collect evidence via bounded FTP RETR. ONE launch per staged build.
7. **Freshness oracle:** result-file mtime (+ the pre-launch stale-file rename) is the
   PRIMARY signal; RETR readback is secondary. ploader OK / dispatch HTTP 200 is NEVER
   "the module ran" — the record stream is the only truth (R-PLD-01).

**FTP caching lesson (from v3):** an FTP readback that shows the old file or nothing is
NOT evidence the module did not run — v3's results were invisible to FTP until the
diagnostic module's own write flushed the file, then 1,035 records appeared. So: after an
apparently empty result, re-check mtime after subsequent writes before concluding failure,
prefer unique per-attempt destination names for high-stakes payloads, and treat
size-identical-to-previous-generation readbacks as staleness warnings. Never re-pin a
local receipt to match a divergent readback (R-E2E-FR3 / R-FTP-02).

**ploader re-arm after crashes:** after ANY crash/timeout/refused-port event — STOP all
console contact immediately; no ploader re-dispatch, no retry, no ping loops while state
is unreconciled. Hand power/network to the user and wait for explicit confirmation.
First re-contact is READ-ONLY in order: `GET /list_payloads` -> FTP check of the expected
result file (existence/size/mtime) -> `/manage:check` + readback sha of the staged
payload. If the load path itself is in doubt, re-stage a known-good diagnostic (igd1
pattern: loads and writes after crashes) — that proves the PATH, never the crashed
candidate's innocence. Only after manager responsiveness + reconciled results + a
recorded crash entry + fresh authorization may a DIFFERENT reviewed candidate dispatch.

## 4. Integration queue (controller-owned, in order)

1. **Staged launch — the master staging package is READY TO EXECUTE.**
   `worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md` (9-file manifest, every
   hash recomputed on disk) + `worker-final-staging-rehearsal/REHEARSAL-RESULT.md`
   (21:47:43Z, **READY TO EXECUTE, 0 hash mismatches, 0 corrections**; its section 6 is
   the verbatim-ready command list). Files: settings ini, layout pair (files 4+5, the
   critical prerequisite), PS5Pad.ini, Item Spawner main.lua + enabled.txt, UEHelpers.lua,
   PRX (second-to-last), corrected 446-B loadlist LAST (sha `49ab7a3a…`; the staged-package
   copy lacking layout rows is REFUSED; `worker-loadlist-final`'s delivery is
   byte-identical to the manifest set). **Gate A0 comes FIRST, exactly once**: an FTP STOR
   write-probe to `/data/modbridge/intake/.writeprobe`; no roundtrip -> STOP (no evidenced
   write route has ever existed; G1, host-unverifiable).
   **Composition-4 contingency is now LIVE** (manifest §C): the composition-4 PRX exists
   with 17/17 checks and gaps `[]` — hash-verify it and substitute it as file 1 (record
   BOTH hashes: placeholder `b8bfae77…`, composition-4 `de372302…`; same destination
   filename, mode stays `2`). With composition-4 staged, Tier B evidence (fresh
   `$R/runtime-status.txt` ack `activated`->`engine-ready`->`mods-started`, fresh
   `/data/plugins/UE4SS.log`, `[TownfallItemSpawner]` Lua lines) is EXPECTED; with the
   composition-3 placeholder only Tier A (`townfall_supplier_composition.log`) is
   possible. Any staged-wrong outcome runs UNMODDED (mfa all-or-nothing) — that is
   evidence, not a crash to retry. Triage: `worker-post-promotion-runtime/FAILURE-TRIAGE.md`.
2. **C2 evaluation:** one probe launch reading candidates A–E per
   `worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md`; offline comparison against the
   retained image; apply the pass/fail criteria of section 1 exactly. Coordinate with the
   user for the launch (installation HOLD).
3. **On promotion:** apply the query-harness + S2-arm one-shot integration patch (40/40,
   `worker-query-harness*`, `worker-query-s2-arm`); rung-1→2 through the harness gate
   (`ArmLayoutGate`/`OnEngineTickDispatch` are IN composition-4). Then rung-3 reads: the
   111/111 read sequence is prepared (`worker-batch2-live-prep`) and the chunk probe is
   host-clean (20/20, 156 assertions, ASan+UBSan clean, 21:46:07Z
   `worker-rung3-chunk-probe`) — but NO rung-3 read has ever executed in-game.
4. **Acceptance ladder rungs 3→5:** harmless reflected access -> unchanged Item Spawner
   -> visible spawn via real input. Evidence spec in `worker-acceptance-evidence`; each
   rung needs its own records; none exist yet.
5. **DLL track continuation:** three families target-proven; remaining work is
   third-party DLL retention/acquisition per the controller RUNBOOK
   (`worker-dll-mod-acquire`, 21:28:55Z; retention survey found none retained anywhere);
   check `worker-mutex-sdk-retry` for the SDK-target closure that fed the passing
   console build. Non-blocking for the game path.
6. **Installer track:** console E2E — rehearsal 61/61 GO, host-side gap closure
   delivered, console dry-run plan + staging manifests ready; download ELF
   staged-deferred pending a signed-in owner session (`worker-e2e-console-dry`,
   `worker-e2e-console-staging`, `worker-console-staging-bundle`). F1 closed (RDWR
   O_RDWR, native download ELF linked 123 imports / 0 missing); F-R1 zip->mfb 75/75
   console-linked; F-R2 87/87; uninstall D1 fixed 186/186.
7. **App track:** worker-8 redo delivered 61/61 real-binary tests; the records/UI audit
   carries a provenance caveat (applied to an export copy) — re-verify `app.js`/
   `index.html` against the authoritative worktree before trusting it.
8. **Container track:** utoc 144-byte header spec complete; pak index SETTLED — saved-pak
   authoritative count 4699 (byte-exact); 4544 belongs to the original on-disc pak;
   never cross-apply.
9. **CRT startup:** implement delivered 9/9 host PS5 cross-compile clean (21:48:18Z,
   `worker-crt-startup-implement`; design was adopted earlier with census filled) —
   review and fold into the next recomposition.
10. **Housekeeping:** risk register V2 live (26 rows updated, 4 new, 15 refill,
    `worker-risk-update` 21:39:40Z). Composition-3-era controller debts ("pinned-values
    patch for ack bootstrap files", "module_start RuntimeAckConfig supplier") appear
    superseded by composition-4's included ack units — verify before spending anything
    on them. Old queue items from the V1 handoff are closed: v3 re-dispatch (delivered),
    fileio-prep (superseded by apply/rebuild), composition-2/3 (superseded by 4),
    mutex host validation (passed + console-proven).

## 5. Console safety rules (standing — from worker-console-safety CONSOLE-SAFETY-PROCEDURE.md)

Every rule below was earned by a recorded crash or failure. The full procedure with
citations lives in the worker's file; these are the binding highlights:

**Standing gates:** installation HOLD (no game-file writes / autoload changes / Townfall
launch without explicit user authorization for that launch). Every dispatch needs explicit
authorization naming the exact reviewed candidate (build-id + sha256). **One console
actor: the controller** — workers NEVER touch the console. Evidence sinks are `/data/...`
on console and owned `outputs/...` on host, never `/tmp`.

**Pre-dispatch checklist (ALL before any `/loadpayload`):** (1) `GET /list_payloads` 200
with readable catalog — a refused port after a dispatch is a crash signature, never
"try again in a minute"; (2) dispatch authorized for that exact artifact; (3) local
sha256 == build receipt; hash before replacing owned console files, preserve the old
build; (4) upload verified per R-E2E-FR3/R-FTP-01 (wait 15–30s, fresh connection,
bounded RETR, `7f 45 4c 46`, size+sha equal to THIS attempt); (5) payload risk scan —
zero raw `syscall` (0F 05) sites, imports limited to the receipted set, no unreviewed
novel mechanism; (6) novel operations LAST, after the write-through identity records, so
a fault localizes by absence; (7) result path sane (`/data/...`), expected filename
recorded, stale result JSONL renamed; (8) console state clean, previous run reconciled.

**Dispatch discipline:** ONE dispatch per authorization — never double-dispatch (crash 1:
dispatch #2 five seconds after #1 took the whole console down). Minimum 30s between any
two dispatches, including determinism pairs (determinism is proven at build time instead,
igf6 pattern). Never dispatch when a previous result is uncollected/unreconciled, the
console crashed without a read-only verification pass since, the payload carries an
unreviewed mechanism, or the only reason is "the first attempt probably didn't take".
**A dispatch timeout is a failure signal: NO retry** — go to crash recovery.

**Monitoring:** expected-evidence table written BEFORE dispatch, including "records stop
at seq N". First check is freshness (mtime), then content. Do not poll RETR in a tight
loop. Zero records + instability = STOP (write-through sink writes seq 1–3 first). Partial
records = localize by absence, capture the result once (single bounded RETR + local copy +
sha256), do NOT re-dispatch to "confirm". Collect once, receipt forever.

**Known-prohibited (condensed from the 12-row table):** double-dispatch <30s; inline
syscalls / `sceKernelMprotect`-class shims in ploader modules (igf5 leading suspect,
never confirmed); exec-span byte reads from a ploader module, broad investigation of the
igf6 mechanism, or repeated text-read attempts (user directive 21:17:12Z — any future
exec-page probe needs a NEW falsifiable design + user coordination); ARGV_ECHO.elf
re-dispatch until reconciled; dispatch without review; game-file writes outside
authorization; `/tmp` sinks; retry-on-timeout / blind replay; deleting or overwriting
console-side evidence (rename stale JSONL); ploader re-dispatch while post-crash state is
unreconciled; treating ploader OK as "the module ran"; re-pinning receipts to match
divergent readbacks.

**Crash recovery:** STOP all contact -> record preceding actions while fresh -> user
controls power/network -> read-only re-contact in the order of section 3 -> assess from
records only -> verify the load path with a known-good if in doubt -> reconcile + record
cause hypothesis (or honest "unlocalized") -> only then a DIFFERENT reviewed candidate.

Day total for calibration: 2 console crashes + 1 game crash + 2 non-crash failures, all
attributed or honestly marked unlocalized; every prohibition above traces to one of them.

## 6. Honest limits — what is NOT proven (do not upgrade these)

- **Tick (0x33ac8c0) / SCO (0x890aa0) are UNRESOLVED.** Confirmed only by MAPPING (in the
  correct exec span at predicted addresses). Byte-read is BLOCKED: the igf4 safety gate
  refused exec-only pages, and igf6's interruption occurred after mapscan and BEFORE the
  first exec-span read. The "PS5 exec-only pages are hardware-enforced" assertion is the
  LEADING HYPOTHESIS, not a proven fact (user amendment 21:17:12Z: mechanism NOT
  localized). User directive: no broad investigation, no repeated text-read attempts.
- **The C2 check does not substitute Tick/SCO.** It establishes image-level
  correspondence only; Tick/SCO stay explicitly unresolved whatever it returns.
- **Rung-3 reads are PENDING.** The 111/111 sequence is prepared and the chunk probe is
  host-verified, but zero reflected-access reads have executed in-game. Rungs 3–5 of the
  acceptance ladder have NO runtime evidence at all.
- **No staged launch has ever been performed.** No workspace record evidences an FTP STOR
  write to the console (PREREQS G3) — Gate A0 decides this first, with a hard STOP. The
  master package is rehearsed host-side only (worker-final-staging-rehearsal, no network,
  no console, no Lua executed). "READY TO EXECUTE" is a host-side verdict.
- **Nothing is promoted.** GUA is STRONG_LIVE_CANDIDATE_EVIDENCE per the promotion
  discipline (read/write verified, sanity passing) — promotion requires the C2
  correspondence evaluation to pass, and any single mismatch stops promotion.
- **Composition-4 is static-clean, not runtime-proven.** 17/17 static checks and symbol
  presence; it has never loaded on a console. Delta honesty: its import delta over
  composition-3 is EMPTY; the new capability is in defined symbols, not new imports.
- **igf5's cause is unlocalized** (inline-syscall shim is the leading hypothesis, never
  confirmed; igd1 control proved only the load path). Crash 2 attribution carries 3
  suspects. The ARGV_ECHO residual (R-ARGV-01/R-G1-02) remains unreconciled.
- **v3 is incomplete as a probe** (stopped after VMMAP pages; cause unknown) — but the
  VMMAP alone carried the mapping analysis. Do not treat v3's silence as protocol failure.
- **Layout pair scope is rung 1→2 only**; code-anchor confirmation inside the pair task
  is declared PENDING/non-blocking by its own task definition.
- **Gameplay remains a prototype.** No visible spawn, no Item Spawner change, no
  full-game acceptance claim exists or may be derived from any record above.

## Honesty rules in force (unchanged)

Candidate != validated. No guessed offsets/widths (width table gates). FName construction
has its own materialized candidates (runtime lead 0x63d590, 9 call sites, FC-1 runtime
probe delivered) and remains its own gap. Predicate hits never auto-establish
identity/base/layout. Unperformed runs are never reported as passed. Raw provenance is
preserved; corrections are recorded as distinguishable amendments, never silent edits.
Only promote what completed checks actually establish.
