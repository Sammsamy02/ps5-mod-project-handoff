# KNOWLEDGE-BASE — the accelerator doc for the NEXT session (worker-knowledge-synthesis, 2026-10-05)

**Read this SECOND, after `README-RETURN.md` (workspace root).** That file is the return checklist;
this file is the compressed set of lessons that each cost real debugging time, so the next session
(or a new engineer) never re-pays them. It covers the zcode-team arc from takeover 2026-10-04 to the
full-surface mobilization 2026-10-05 17:38 UTC.

**Sourcing rule:** synthesis only, zero new claims. Every line carries its pointer.

Citation shorthand:
- `L<key>` = ledger entry `zcode_<key>` in `.superpowers/native-observations/transport-experiments-progress.json`
  (the 335 `zcode_*` keys, in append order, ARE the session history; originals are never edited —
  supersede markers only, per `Lreturn_session_log.supersede_policy`).
- `W/…` = `outputs/2026-10-04-zcode-team/…`; `C/…` = `outputs/2026-10-04-zcode-controller/…`.
- Companion docs (durable TARGET facts, not lessons): `W/worker-knowledge-consolidation/KNOWLEDGE-BASE.md`
  ("READ THIS FIRST" for target facts), `W/worker-project-state-index/{CURRENT-STATE-DIGEST,INDEX}.md`.

---

## 1. THE ARITHMETIC TRAPS (each one cost a real debugging cycle)

### 1.1 The +0x400000 slide vs the 0x4000000 slip — the extent war
- **The law:** runtime address = file/link-time p_vaddr **+ 0x400000** (uniform slide, ALL PT_LOADs).
  Image extent = **[0x400000, 0xc0602c8)** because `kImageExtentVaddr = 0xbc602c8` (FILE vaddrs) and
  `0x400000 + 0xbc602c8 = 0xc0602c8`. **`0xfc602c8` is NEVER right** — it carries a 0x3c00000
  (=60 MiB) bias, i.e. someone added 0x400000**0** (one zero too many) instead of 0x400000.
  Source: `Lextent_arithmetic_correction` (user-corrected, 2026-10-05T14:40), `Lgeometry_reference_and_l10_build`,
  `W/worker-geometry-reference/GEOMETRY.md` §3; `README-RETURN.md` §5 "Extent law".
- **The insidious part:** a LATER ledger entry (`Ll10_poolfix_complete_verifier_hardened`) wrongly
  called 0xc0602c8 "a one-digit arithmetic slip" and restated the runtime end as 0xfc602c8 — the PROSE
  was the error; **the live code was always right** (`extent_end = kCandidateBase + kImageExtentVaddr`
  at final-units bridge :679/:790/:982; SeedValidation.hpp :79-85). The correction flag on the oracle's
  prediction `[0x400000,0xc0602c8)` was retracted — no code change was needed or made.
  Source: `Lextent_arithmetic_correction` (status: "PRIOR_ENTRY_PROSE_WRONG_CODE_ALWAYS_RIGHT").
- **Lesson:** when prose and code disagree about a constant, trust the code's expression and
  recompute from pinned inputs — then fix the prose. A wrong correction note is as expensive as the
  original slip.

### 1.2 The LOAD0 +0x4000 file-offset trap
- **The law:** `file_offset = vaddr + 0x4000` for LOAD0/LOAD1 (LOAD0 has `p_offset=0x4000`,
  `p_vaddr=0` — so file offset is NOT vaddr there). Verified prologues (Tick 0x2fac8c0, SCO 0x490aa0,
  FName ctor 0x23d590) were read at vaddr+0x4000.
  Source: `Lworker_expected_bytes` (offset_formula field); `W/worker-geometry-reference/GEOMETRY.md` §1-2.
- The RE worker's own first pass hit exactly this assumption and retracted it — recorded as a
  standing tooling caution. Source: `Lre_fname_findings_validator_v4` ("+0x4000 LOAD0
  file-offset==vaddr trap (affected their own first pass, retracted)").
- Corollary trap: section headers are UNREADABLE (`e_shoff` ≈1.87 GB > file size; `e_shnum=58` is
  stale) — **parse phdrs only, never sections**. Source: `W/worker-geometry-reference/GEOMETRY.md` §1;
  `W/worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md` §1.

### 1.3 Hex-digit-width errors are a CLASS, not a one-off
Observed instances of the same failure mode (dropping/adding one hex digit or mis-transcribing):
1. The "LOAD7 does not fit the slide" claim — a hex-digit arithmetic error
   (`0x913c000+0x400000=0x953c000`; observed RW span [0x970c000,0x9a34000) = LOAD7 BSS page-rounded).
   Source: `Lgeometry_reference_and_l10_build`; the same digit-width error was in
   `Lworker_vaddr_semantics`'s "single global bias disproven" verdict (later reversed).
2. Seed summary said LOAD3 memsz=`0x12800000` — the bytes say `0x1280000` (8x too big).
   Source: `W/worker-geometry-reference/GEOMETRY.md` §2 "Seed-fact corrections".
3. CP2 tick constant is `0x33ac8c0`, NOT `0x37ac8c0`. Source: `README-RETURN.md` §5.
4. Pad error code recorded as `0x80A40004` in the first ledger entry, corrected to
   `0x80920004` (= -2137915388 decimal; `pad_sdl_ps5.h:21`). Source: `Lreal_timing_ccomp_closed_pad_correction`
   vs the earlier `Ll10_live_runs_geometry_verified`.
- **The habit that ends the class:** check EVERY hand-written hex literal against
  `python3 -c "print(hex(int('bc602c8',16) + int('400000',16)))"`-style recomputation — the
  0xfc602c8 error itself propagated "from misreading python's own correct hex() output"
  (`Lextent_arithmetic_correction` root_cause), so also prefer *generated* numbers over transcribed
  ones: `GEOMETRY.md` states every number in it is computed by `derive_geometry.py` from pinned
  inputs, "nothing is hand-transcribed". Adopt that stance for any derived constant.

---

## 2. THE TOOLING LAWS (learned live, each backed by an incident)

### 2.1 The ftpsrv readback law — never byte-compare a console readback, never trust a path
- The console's ftpsrv does NOT return the file you uploaded byte-for-byte. A correct readback shows:
  all PT_LOAD **segment bodies byte-identical** (segment-coverage compare over `p_offset+p_filesz`),
  differences confined to ELF-header **SHT fields** (an allowed byte set, `e_shnum` zeroed) and a
  **stripped tail** (len == local max(p_offset+p_filesz); observed tails 2016B, and the recurring
  ~1560B inner-ELF shrink class on v2/v3 stagings). Source:
  `Ll10_staged_ready_to_launch` ("readback verified by the SEGMENT-COVERAGE law: 5/5 PT_LOAD segments,
  30,774,619 covered bytes IDENTICAL; differences confined to ELF-header SHT fields + stripped tail"),
  `Lreturn_session_log` phase 0 + ridealong phase ("14 tail-32 diffs are the SAME documented ftpsrv
  SHT-strip artifact class … NOT corruption; upload not repeated; gate not relaxed"),
  `W/worker-launch10-staging-packet/REPORT.md` check `l10.4` (the codified law incl. SHT_HDR_ALLOWED),
  `Lv3_final` staging note (1560B reduction, same class).
- The matching anti-patterns: (a) byte-comparing the whole file → false "corruption" alarms;
  (b) accepting a filename/path match as proof of content → closed in the verifier rewrite
  ("filename/path-only acceptance and glob-guessed ELF selection REMOVED; file must RESOLVE INSIDE
  THE CANDIDATE and match recorded sha+size"). Source: `Lartifact_verification_complete`.
- Also an ftpsrv fact: MKD is refused and STOR-into-missing-dir returns **rc 9 silently** — the E2E
  installer looked "dispatched OK" but had no intake dir. Source: `Lreturn_e2e_blocked`.

### 2.2 Objects embed the source BASENAME — comparators must preserve it
- Compiled object bytes contain the source file's basename (e.g. `ASMHelper.guarded.cpp`), so a
  semantically identical flip compiled from a RENAMED probe file produces a different object sha.
  The renamed-probe comparator `f5627427…` is **not composition-admissible**; the admissible
  basename-preserving flip is `6604ce2d…`. Source: `Lfold1_ready_comparator_corrected`;
  `W/worker-fold1-preverify/FOLD1-READY.md` "Correction".
- Rule: when a comparator mismatch appears, diff the compile *invocation* (argv, source basename)
  before doubting the content.

### 2.3 Minified/generated driver + script edit discipline — single-line, tagged, pinned edits
- The fixed composition edition was consolidated as **4 single-line tagged edits** (final `b78fb031`)
  rather than free-form rewriting; the walkguard fold is a **one-token in-file flip**
  (`#define RC_ASM_WALK_RX_GUARD 0`→`1`) that preserves the basename and the 148-token argv.
  Sources: `Ll10_final_build_progress`, `Lfold1_ready_comparator_corrected`.
- Frozen drivers are never hand-edited: per-unit overrides go through pinned mechanisms —
  `--stage-header live_module_facts.hpp=<path>=<sha>` (sha-pinned header override that overrode the
  driver default pin f114f876), `--replace-runtime-unit` + mandatory `--replace-receipt` with the
  unit's recorded 190-arg argv replayed verbatim (only source/-o/-MF redirected; fresh run dir
  required — the script refuses an existing one).
  Sources: `Ll10_cap_fold`, `W/worker-initializer-integration/REPLACE-ARGV.md`.
- The one-line env fix class: a wrapper GO/NO-GO failed on a single missing
  `env_extra={'LLVM_CONFIG': …}` on one `b.run` call — "the exact class the frozen pipeline guards
  everywhere else". Every subprocess invocation must carry the full frozen toolchain env.
  Source: `Lwrapper_validation_go`.

### 2.4 args-scope / cwd-reset classes
- **cwd-reset:** a controller prompt that said "workspace root" without the absolute path sent 10
  workers to search the WRONG tree (the fork session's CWD was the unrelated studio project) — all
  honestly reported BLOCKED. Standing rule: every dispatch carries
  `/WORKSPACE/.codex/worktrees/translation-core/what` verbatim in the header; workers use
  absolute paths. Source: `Lflash_capacity_trial_1` (root_cause_blocked).
- **args-scope (frozen argv):** identity is proven by replaying the exact recorded argv (e.g. the
  148-token asmhelper argv, the 190-arg initializer argv). A frozen-argv compile is
  path-independent: the same control object sha reproduces from different worker dirs (854a476b
  byte-matched across 5 reproductions). Any argv drift must be same-source-freshen-class and is
  recorded, not silently accepted. Sources: `Ll11_enqueue_fix_delivered` ("control 854a476b
  BYTE-MATCHES the consumer object = frozen-argv path-independence"), `Ll11_merge_m0_delivered`
  (5th reproduction), `Ll10_final_build_progress` (argv-drift same-source-freshen).

### 2.5 Root resolution / shadowing — the stray-root incident
- A vendor run created a stray `outputs/2026-10-04-zcode-team/.superpowers/` directory which SHADOWED
  the frozen `package_prx.py` root resolution (first bare `.superpowers` marker wins), producing two
  fail-closed wrong-root receipts (zero wrong-root approvals — the fail-closed design held).
  Fix: quarantine (rename, bytes preserved) + `find_root_v2.py` (strong marker + corroboration +
  loud ambiguity failure, 9/9 host tests). Source: `Lstray_root_incident_record`; `README-RETURN.md` §2.
- Related stale-pin hazard: `console_batch.py XWP_ARMED` pinned an OLD build id under the same
  filename — a batch flow would silently stage wrong bytes and self-verify PASS. Any staged-pin
  flow needs a seq-1 build_id check + explicit re-staging + end-of-window restore.
  Source: `Lprobe_pairing_prepped` (H-1 hazard, controller-must-fix).

---

## 3. THE RUNTIME FACTS (the launch-10 four-run arc and its accounts)

### 3.1 The four-run arc — each run died at the next gate, zero crashes throughout
| run | pid | gate hit (in order) | classifier label | fix class |
|---|---|---|---|---|
| 1 | 317 | PS5Pad.ini **ASCII gate** — two UTF-8 em-dashes | FAILED_E2_START | data fix, no rebuild |
| 2 | 341 | **size gate** at MFA activate — staged file (2958) shorter than loadlist-declared (2960) | FAILED_B7_MFA | loadlist data fix |
| 3 | 355 | **pad open** at NativePadInput phase=6 → `0x80920004 PS5_PAD_ERROR_ALREADY_OPENED` | MINIMAL_ACCEPT_E2_REFUSAL | code fix (pad-defer), in flight |
| 4 | 370 | identical pad phase-6 refusal (clean repro) | — | confirms stability |

- Sources: `Ll10_live_runs_geometry_verified`, `Lrun4_repro_l11plan_r2`,
  `W/worker-real-timing/TIMING.md` §0 (per-pid gate table), `Ll10_verdict_confirmed`
  (classifier labels; append-only contract verified — every run's first 7591B sha-matches baseline).
- **The arc's headline:** run3 is where the GEOMETRY FIX WAS VERIFIED LIVE —
  `lmf_extent_end=0x0c0602c8` byte-exact vs prediction, `lmf_anchor_count=7`, `lmf_intervals_found=1`
  (coalesced); conversion PASSED the launch-9 killer stage (derive_readable_ranges zero-intervals
  failure). Oracle verdict: MINIMAL-ACCEPT achieved (load-capable; spawn NOT expected — see 3.5).
- **NO-CRASH invariant held in all four runs** — the candidate's fail-closed design is empirically
  stable (4th consecutive no-crash run). Source: `Lrun4_repro_l11plan_r2`.
- Data-gate hygiene: the default package-v3 tree still carried the em-dash pad ini + 2960-row
  loadlist AFTER the live fixes — restaging from defaults would have re-killed a launch. It was
  healed, and `data_preflight.py` now re-implements the runtime's OWN checks 1:1 (33/33 selftest,
  reproduces both incidents, `--fix` byte-identical to the manual fixes). Run it before any restaging.
  Source: `Ldata_preflight_delivered_defaults_healed`.

### 3.2 C_comp = 0.8 s — on the EARLY-REFUSAL path only
- Real per-pid t_ms deltas: `module_start_entered → return` = 813ms (r1) / 803ms (r3) / 718ms (r2,
  early abort) — **37x under the 30s pt_call cap**. ~85% of it is the flat 235-ctor block
  (679-683ms); VMMAP walks 11-12ms, lease 8ms, anchors 16ms, lmf conversion 7ms.
  Projection: ~6-11s ploader serial wall; composition ≈1.1% of the measured serial chain.
  Source: `Lreal_timing_ccomp_closed_pad_correction`; `W/worker-real-timing/TIMING.md` §1-3.
- **Scope discipline (user directive):** 0.8s is the EARLY-REFUSAL path only; full-startup cost and
  the ~71s inter-plugin gap are separately accounted (3.3); run-5 baselines captured for the next
  NECESSARY run. Source: `Luser_directives_scoping` (directive 2).

### 3.3 The ~71s inter-plugin gap = a TICK BUDGET, not a timeout, not a failure path
- Measured 70.7-71.8s (composition refusal → path_test module_start) in ALL four boots.
- **Primary hypothesis REFUTED at source level:** a failed module_start takes NO different path —
  the injected shellcode INT3s on return either way, cleanup is byte-identical, no retry, no
  timeout, failed plugins don't break the loop (pinned ploader `main.c`, sha `2b521cd7…`).
- **Actual account:** `path_test` RE-PAYS the per-plugin readiness wait (main.c:784, charged per
  plugin), and `wait_stable`'s "25s" is a TICK budget (500 × 50ms, NO clock check, main.c:518-559)
  each tick paying ~264 kernel backdoor calls (~0.3ms fitted K) ⇒ ≈64.6s + attach + cleanup =
  65.5-72.2s band. The ±1.12s boot-to-boot determinism is the FIXED-TICK-CAP signature.
- Fixes (PROPOSAL-ONLY — ploader is a console system component, user decision): PATCH 02
  hoist-readiness (gap → 0.8-7.9s) + PATCH 05 wallclock-deadline (caps burn ≈25.3s). The obvious
  ini-order mitigation was REJECTED: it masks the metric, saves ~0s, and breaks the PATH_TEST
  terminator guard. The internal split stays inference until a ploader-03 steplog exists.
  Source: `L70s_gap_solved`; `W/worker-ploader-70s/{GAP-ANALYSIS.md,PROPOSAL.md,PATCHES/}`.

### 3.4 The pad ownership question (the live blocker at session close)
- Fact: the game holds the EXCLUSIVE pad handle at the earliest injection moment — scePad open fails
  `0x80920004 PS5_PAD_ERROR_ALREADY_OPENED` (NOT a service-down error; the retry window is
  "until the handle is free", event-driven, not "until service up"). The runtime's phased init
  THREW on it (NativePadInput.cpp:22) — redesign = log-not-throw + event-driven retry on first
  read/poll + analyze non-exclusive/peek scePad open modes. Source:
  `Lreal_timing_ccomp_closed_pad_correction`, `Ll10_live_runs_geometry_verified` (root_cause/fix).
- The OWNERSHIP question is still open and load-bearing: does OUR OWN port hold a second handle
  (duplicate-initialization hypothesis — trace every scePadOpen in the composed units), and is there
  a coexisting input path (SDK handle enumeration / peek / non-exclusive modes) with distinct
  owned-vs-borrowed cleanup? Source: `Luser_directives_scoping` (directive 1).
- **Two DISTINCT outcomes, never conflate:** pad **A** = startup continues (deferral only, the
  pad_input_deferred tag) vs pad **B** = usable controller input alongside the game (the menu needs
  B). Deferral alone delivers only A. Source: `Luser_priority_menu` (3), `Ll10b_runbook_delivered`.

### 3.5 The pump structural fact — marshal-only, scoped
- Under the CURRENT composition and ownership policy, the FuncPtr-marshal is the ONLY armed pump
  path: the LuaMod LoadMap arm does not hit the folded guard site and is not an unguarded raw path —
  every PLH text touch is owned-memory-mediated and soft-refuses non-owned extents with zero
  dereference/syscalls; live console game text is never registry-backed, so the PLH detour layer
  CANNOT arm against it in any kernel world; the "W-works / initializer-owns-pump" oracle branch is
  UNREACHABLE. Arms fail softly (~19/boot), boot continues, Lua loads. The L11 decision tree
  collapses to marshal-always. Sources: `Ll11_loadmap_verdict_marshal_only`;
  scope pinned to current composition by `Luser_directives_scoping` (directive 4) and
  `C/launch10-build/SPAWN-CAPABILITY.md` (candidate is LOAD-CAPABLE, NOT SPAWN-CAPABLE — first-OPTIONS
  silence after a successful load is EXPECTED, walkguard loud refusal, no crash).

### 3.6 The geometry predicted-pair, 3x-confirmed
- Prediction (from the cap-fold analysis): `anchor_count=7 / spans=6 / reg=6 / intervals_found=1`
  (coalesced — adjacent records; NOT 5-7; the hard gate is >0), extent [0x400000,0xc0602c8).
  Source: `Ll10_cap_fold`.
- Confirmed on THREE conversion-passing epochs (pids 317, 355, 370 — run4 also passed conversion,
  an honest extension of the earlier two-epoch framing). Sources: `Ll10_verdict_confirmed` ("the pair
  confirmed TWICE"), `Lepoch_view_delivered` ("confirmed on THREE conversion-passing epochs").
- Reading rule for next time: the on-console sdiag **accumulates across boots** (run1 file ==
  run3's first 548 lines; 2139 records over 4 epochs by close); `runtime-status.txt` is
  LATEST-STATE-ONLY and is never raced; one monotonic clock across sdiag t_ms / UE4SS [t=] /
  PATH_TEST ns (validated 3/3). `tools/epoch_view.py` recovers per-epoch rows;
  `PROCEDURE.md` = the RUN-N reading order. Sources: `Lreal_timing_ccomp_closed_pad_correction`,
  `Luser_directives_scoping` (3), `Lepoch_view_delivered`.

---

## 4. THE PROCESS PATTERNS THAT WORKED (and one race class to never rebuild)

### 4.1 Evidence-ladder discipline — no promises ahead of demonstrated stages
- Progress is judged ONLY on fresh same-run evidence up the ladder: runtime starts → Item Spawner
  loads → game-thread work runs → controller input ARRIVES → menu opens. Startup evidence is pulled
  BEFORE asking the user to test OPTIONS; ask 2 only when stages 1-3 (+ input-route-if-shipped) are
  demonstrated; the menu is never promised early. Verdict vocabulary pinned per run
  (BEST/GOOD/MINIMAL-ACCEPT/FAIL + refusal_class). Source: `Luser_priority_menu`,
  `Ll10b_runbook_delivered`, `Ll10_verdict_confirmed`.
- Corollary that paid off twice: distinct MINIMAL-ACCEPT/GOOD definitions let run3 count as real
  progress (conversion pass + intervals>0 + diag events) without claiming GOOD (pad gate precedes
  the initializer seeds). Source: `Ll10_live_runs_geometry_verified`.

### 4.2 Distinct-outcome labeling — pad A/B, candidate/synthetic
- Pad outcomes A (deferral/startup-continues) and B (usable input) are labeled distinctly in runbook
  and roster so a delivered A is never misread as B (4.1/3.4). Source: `Luser_priority_menu`,
  `Ll10b_runbook_delivered`.
- Provenance labeling law: an image-resident pool fixture is **candidate/synthetic** until a real
  captured Blocks[0] exists — and none does (9-corpus search: NO pinned observation ever captured an
  actual Blocks[0] pointer; the [0x970c000,0x9a34000) figure is a MAPPING-class record that prose
  had promoted to a pointer claim). Every literal Blocks[0] hex in-tree is synthetic. Validators
  must carry the label, and mapping-class evidence must not be cited as pointer-class.
  Source: `Lblocks0_provenance_verdict`; `README-RETURN.md` §5.

### 4.3 Comparator chains with byte-matched controls
- The L11 chain is the model: consumer `854a476b` → enqueue-fix `8593884e` → +loadmap-r2 `3eeba02f`
  → merged-object comparator **`813b249d`** (7,039,032B), with the control byte-matching the
  consumer object at every step (frozen-argv path-independence, 5th reproduction at merge).
  Merge itself proven order-independent (both diffs reproduce byte-identically in either order;
  reverse-patch round-trips to base). Source: `Ll11_marshal_consumer_delivered`,
  `Ll11_enqueue_fix_delivered`, `Ll11_loadmap_verdict_marshal_only`, `Ll11_merge_m0_delivered`.
- Controls are re-measured, never assumed: fold-0 rebuilt under the CURRENT header tree reproduced
  the shipped object `81dd832b` BYTE-IDENTICALLY (that's what made fold-1's comparator correction
  credible); the D1 determinism proof is a twin-composition byte-identical compare (`5c82c8f2…`).
  Comparator corrections are recorded, not hidden (f5627427 renamed-probe, 2.2).
  Sources: `Lfold1_ready_comparator_corrected`, `Ll10_build_complete`.
- Verifier hardening in the same spirit: receipt-EXPLICIT fields only, `_tied_file()` refuses paths
  outside the candidate, negative cases proven to FAIL (no false PASS). Sources:
  `Lartifact_verification_complete`, `Ll10_poolfix_complete_verifier_hardened`.

### 4.4 The fold discipline — worker-authoring dirs vs controller final-units
- Workers author + fixture + compile-gate inside their OWN `W/worker-*/` dirs (staging/), and the
  controller folds reviewed units into `C/launch10-build/final-units/` with receipt chains. Folding
  may deliberately retire aliases (the v4 AL1 alias re-pointed to the primary name at fold —
  intended, recorded). Identity at fold is enforced by pinned shas at every step (EDITION.json pins,
  --stage-header =<sha>, merged-fixture receipt). Sources: `Ll10_final_build_progress`,
  `Lprestaging_items_closed`, `README-RETURN.md` §3 step 3 (the fixed fold order: fold v4 → refresh
  EDITION.json pins → `launch10_one_pass.py --full` → gate GO x2 + determinism + witness → only then
  `console_batch.py stage-launch10`).
- Compositions stay fail-closed: a missing staged header (SeedValidation.hpp absent from the overlay)
  was the real packaging-compile blocker — found by repairing, not by loosening.
  Source: `Ll10_poolfix_complete_verifier_hardened` (controller repairs).

### 4.5 The two-snapshot race class (why launch-8 failed, and the structural fix)
- **The class:** registering spans from one sysctl snapshot (T1, fresh pre-capture) while deriving
  leases from ANOTHER (T2, capture-frozen) — text exec-only at T1 but readable at T2 → false
  "unregistered bytes / includes unregistered bytes" refusals (launch-8's code=2 at 0x400000;
  launch-9's code=1 zero-intervals was its geometry sibling). **The fix pattern:** move registration
  INSIDE the scan wrapper so both consumers read the SAME frozen capture — race eliminated by
  construction — plus a pre-lease coverage gate with named-gap refusal (70/0 fixtures incl. the
  verbatim launch-8 throw reproduced OLD / defused NEW). Source: `Ll10_bridge_consolidation`;
  attribution correction `Llaunch9_attribution_correction` (the launch-9 "continuous hole" was false,
  stitched cross-vintage).
- Same-class process rule: one attempt, one clock, per-pid writers — never stitch evidence across
  epochs/boots and treat it as one run (the false-gap incident was exactly a cross-vintage stitch).

### 4.6 Small process laws that repeatedly saved the run
- Re-dispatch exactly-once; on ambiguity (divergent hash, FAIL line, JOB FAILED): STOP + preserve +
  reconcile, never replay (`README-RETURN.md` §5; `Lreturn_session_log` "upload NOT repeated,
  dispatch exactly once").
- Consolidation with supersede markers, never deletion (`Lreturn_session_log.supersede_policy`;
  originals preserved intact).
- Concurrency honesty: spawn acceptance != sustained completion (30 accepted → ≥5 hard 1302s;
  sustained ≈15); paced admission + refill-on-completion. Source: `Lflash_capacity_trial_1`.
- Console-safety law earned twice the hard way: no double-dispatch at 5s spacing (ARGV probe crash,
  2026-10-04) and no raw inline-syscall shims in game-process probes (igf5 console+game crash);
  ≥30s gaps for determinism tests; manager is 8084, never 9223. Sources: `Lconsole_unreachable`,
  `Lconsole_crash_2`, `README-RETURN.md` §3.

---

## 5. STATE POINTERS — where everything lives for the return session

Read `README-RETURN.md` first; it was disk-reverified (29/29) at the launch-10-build refresh. The
pointers below are the ones THIS synthesis leaned on, plus the session-close roster.

**Authoritative state**
- Ledger: `.superpowers/native-observations/transport-experiments-progress.json` (335 `zcode_*` keys;
  last entry `zcode_full_surface_mobilization`, 2026-10-05T17:38Z; pre-takeover backup:
  `transport-experiments-progress.json.bak-zcode-2026-10-04`).
- Return checklist + session-earned rules + restore paths: `README-RETURN.md` (§3 first commands,
  §5 do-NOT rules, §6 KB/evidence/L11/restore).
- Cheat sheet: `C/RETURN-SESSION-COMMANDS.md`; launch-10 runbook: `C/LAUNCH-DAY-10.md` (gate §1);
  launch-DAY-10B user-window script: `W/worker-l10b-window-prep/LAUNCH-DAY-10B.md` (two-ask ladder,
  pad A/B); L11 plan: `W/worker-l11-plan/LAUNCH11-PLAN.md`; matrices:
  `W/worker-launch10-matrix-prep/LAUNCH10-MATRIX.md`, `W/worker-launch11-integration-matrix/…`.

**Build + candidate (as of session close)**
- Driver/edition: `C/launch10-build/launch10_one_pass.py` + `EDITION.json` (launch10-final-b);
  spawn-capability record: `C/launch10-build/SPAWN-CAPABILITY.md`; final-units + receipts under the
  same dir; staged launch-10 candidate PRX sha `60756ef2…` (30,837,767B; ini untouched `1216aab6…`).
  Source: `Ll10_staged_ready_to_launch`, `Ll10_build_complete`.
- L11 merged unit (all inputs ready EXCEPT pad): `W/worker-l11-merge/` (base 04c9d916 + enqueue
  3b19068a + loadmap-r2 d2ad2818 → merged source dc687602; comparator 813b249d).
  Source: `Ll11_merge_m0_delivered`.
- Oracle (merged W1-W7 + capacity): `W/worker-launch10-acceptance-criteria/ORACLE.md` +
  `W/worker-oracle-final-sync/ORACLE-SUPPLEMENT.md`; classifier `evidence.sh` (post-gap-fix
  `7b03ca95`, host 94/0): `W/worker-evidence-gap-fix-2/`; epoch reader:
  `W/worker-durable-events/tools/epoch_view.py` (+PROCEDURE.md); data preflight:
  `W/worker-data-preflight/tools/data_preflight.py`; launch-10 evidence (runs 1-4):
  `C/launch10-evidence/` + `C/launch10-prep/` baselines; results: `W/worker-verdict-first-real/LAUNCH10-RESULTS.md`.
- Timing/gap worker reports: `W/worker-real-timing/{TIMING.md,CLAIM.json}`;
  `W/worker-ploader-70s/{GAP-ANALYSIS.md,PROPOSAL.md,PATCHES/}`.
- Geometry: `W/worker-geometry-reference/{GEOMETRY.md,GEOMETRY.json,parse_phdrs.py,derive_geometry.py}`;
  pinned image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  (sha `3b6cfad4…`, 194,241,424B). Ghidra 12.1.4 at `/WORKSPACE/Projects/re-tools`.

**Workers in flight at session close (roster of 9, distinct ownership; `Lfull_surface_mobilization`)**
1. pad-defer — CRITICAL PATH: both pad outcomes + the ownership probe (3.4); also holds the BLOCKING
   admission slot for the L11 compose (`Lrun4_repro_l11plan_r2`).
2. combined-editions — pre-built compose machinery for the next console candidate = COMBINED
   (pad fix + merged L11 luamod `813b249d` + launch-10-final units); loop v2 composes+stages it on
   pad delivery (`Luser_priority_menu`).
3. menu-input-map — controller→menu dependency chain + the outcome-B verdict.
4. re-spawn-chain — the spawn-path RE + PS5 readiness per step (the rung after the menu).
5. l3-profile — game-build profile-layer artifact + generator from verified facts.
6. l2-adapter — engine-adapter contract manifest + completeness dashboard + second-game reuse.
7. knowledge-synthesis — this document.
8. bss-budget — launch-12 lever guardrail (16,384→32,768 capture = ~36.3MiB BSS; budget check
   REQUIRED before any bump; `Lcapture22k_fixture_delivered`).
9. REA trial — the agent-RE workflow verdict.

**Known live hazards at close (do not rediscover)**
- `console_batch.py XWP_ARMED` stale build-id pin (H-1) — must be fixed before ANY probe window
  (`Lprobe_pairing_prepped`).
- The 70s ploader gap is UNDERSTOOD but ploader patches are PROPOSAL-ONLY (user decision)
  (`L70s_gap_solved`).
- E2E installer intake-dir blocker parked, never confirmed complete (`Lreturn_e2e_blocked`,
  `README-RETURN.md` §3.4).
- `0xfc602c8` and `0x80A40004` and `0x37ac8c0` are known-wrong figures that still circulate in older
  prose — never reuse (§1 above; `README-RETURN.md` §5).
