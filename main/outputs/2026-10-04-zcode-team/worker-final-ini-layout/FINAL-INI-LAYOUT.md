# FINAL-INI-LAYOUT — THE definitive return-session ploader.ini

> **SUPERSEDED-IN-PLACE (NINTH SLOT, 2026-10-04):** `ploader.ini.canonical`,
> `verify_final_ini_layout.py` and `simulate_ploader_ini_final.py` in THIS
> directory now pin the **9-entry / 8-registered** law of
> `worker-carrier-probe-build/STAGE-PLAN.md` (cc1 carrier appended after the
> igf `= 0` line; canonical 440 B, sha `fd9c7393…`), adopted by
> `worker-ninth-slot-ini-adoption` into the batch tool. The 8-entry law
> described below (400 B, `bbc34c16…`, 7 registered) is the historical record:
> preserved byte-exact at `worker-ninth-slot-ini-adoption/law/*.asfound` and in
> this worker's delivered receipts. The ordering/ensure/bootstrap law of this
> document is UNCHANGED by the ninth slot (one more content-anchored entry).

Worker: worker-final-ini-layout, 2026-10-04. Proposal-only layout data + the
ordering contract. NO console contact was made; every console step still
requires the standing ritual (game fully closed, Townfall closed on console,
FTP up, ploader armed, USER launches manually — never automated). This document
owns the definitive 8-entry layout; it supersedes, FOR THE RETURN SESSION:

- the 6-entry reference shape in `worker-game-session-ridealongs/
  STAGE-PLAN-COMBINED.md` §1 (its parser law §0/§3/§4 remains the law; its
  SC-0..SC-8 step skeleton remains the staging skeleton),
- the append-at-end position of `modbridge_fc1pool.prx` in
  `worker-fc1-validator-probe/STAGE-PLAN.md` §2 (fc1 moves to entry 2),
- the "registered count exactly 5" gate quoted in
  `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` step 3
  (it becomes exactly 7).

The `worker-observer-staging-extension` console_batch flows are NOT edited by
this worker; the constants to adopt are briefed in
`CONSOLE-BATCH-CONSTANTS-BRIEF.md` next to this file.

---

## 1. The canonical bytes

Exact target for `/data/plugins/ploader.ini` (delivered verbatim as
`ploader.ini.canonical`, 400 bytes,
sha256 `bbc34c16621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1`,
ASCII, LF endings, trailing newline present):

```ini
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 2
/data/plugins/modbridge_fc1pool.prx = 2
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 2
/data/plugins/modbridge_runtime_aligned_observer.prx = 2
/data/plugins/modbridge_runtime_aligned_entry.prx = 2
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
```

7 active entries (mode 2) register; the mode-0 line is skipped at parse and
consumes no slot: **7 of MAX_PLUGINS=10, headroom 3** (8/10 even if igf were
ever flipped to 2 — which it must not be, see §4.2).

## 2. Per-entry order rationale

Pinned parser facts (pinned source
`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`,
as established by the ridealongs plan): registration order == file order ==
injection order (sequential per-pid loop L776-820); `parse_mode` accepts only
0/1/2 (L605); mode 0 is SKIPPED at parse — never registered, never
PT_ATTACHed, no FAIL notify, no timeout wait (L672); NO path dedupe (a
duplicated line registers and injects TWICE); mode 2 = late (stable 1000 ms /
min_mod 16 / settle 600 ms, per-plugin wait cap 25 s L471-476/L779); whole
list must finish inside `INJECTOR_TIMEOUT_S` = 120 s (L735).

| # | Line (mode) | Role | Why HERE |
|---|---|---|---|
| 1 | `modbridge_exec_write_probe_armed.prx = 2` | IGW exec-writability probe (igd1-6ebab07f2111, fself sha `48949316…`, 20013 B). One-shot SYNCHRONOUS `module_start`: full mprotect ladder, per-attempt records flushed via synchronous `sceKernelWrite` BEFORE the next syscall, always returns 0, no threads, no unload; writes `/data/modbridge-exec-write-probe.jsonl` (+ UE4SS mirror) | FIRST so its verdict records EXIST before the composition's `module_start` begins. The composition's folded initializer is designed to consume the verdict for same-launch gating (`UnrealInitializer.cpp` `SeedArming`, `kExecWritabilityVerdictGating`, currently `false` = "DESIGNED, NOT ARMED" until the ploader.ini ordering + verdict schema are pinned — this document pins the ordering half). First position gives maximum margin: nothing else can delay it. |
| 2 | `modbridge_fc1pool.prx = 2` | fc1 validator probe (fc1pool1-78508a2e2940, fself sha `d2431bf8…`, 22733 B). One-shot synchronous `module_start`, 8 x 1 s grace inside 25 s cap, NO threads; writes `/data/modbridge-fc1pool-facts.jsonl` + exactly one `FC1POOL:` UE4SS line | Second, still BEFORE composition: same-launch evidence-before-ladder. If the composition's ladder later dies the title, fc1's verdict is already flushed — evidence survives a mid-ladder death. Also the earliest-possible pool state is measured (pre any composition-side FName activity). |
| 3 | `modbridge_supplier_composition.prx = 2` | composition-5 + folded initializer + runtime ladder; the ONLY consumer-side module | Third: starts only after BOTH probe verdicts are on disk. This is the ordering the initializer's future verdict gating assumes; it is also the historical anchor the qdrv1 stager inserts after. |
| 4 | `modbridge_qh_driver.prx = 2` | qdrv1 record-sink ridealong (sha `52c32216…`); one detached thread, ~2 min window, admission-gated on the GUA header | Immediately after composition so its bounded window overlaps the LIVE runtime, not the pre-ladder boot. Unchanged relative position from STAGE-PLAN-COMBINED (anchor = composition). |
| 5 | `modbridge_runtime_aligned_observer.prx = 2` | observer ridealong (sha `1afa0997…`); one thread, 30 x 2 s (~60 s) window, stat+tail reader | After composition for the same window-overlap reason; after qdrv1 so both ridealongs' windows start in deterministic order (observer's UE4SS.log growth baseline includes qdrv1-free state). Unchanged relative position (anchor = qdrv1). |
| 6 | `modbridge_runtime_aligned_entry.prx = 2` | frozen standalone aligned-entry module; diagnostic producer on `/data/townfall_runtime_aligned_entry.log` (a channel the composition ALSO writes) | Moved AFTER the ridealongs: the standalone module's constructor lines then postdate the modern stack's startup, so channel interleaving is deterministic (composition ladder first, standalone diagnostic last). Also removes any constructor-order coupling between the standalone module and the two probes. |
| 7 | `modbridge_path_test.prx = 2` | freshness oracle: at `module_start` appends exactly `PATH_TEST:` + `ACTIVATION_TEST:` to `/data/plugins/UE4SS.log`, returns 0, no threads | LAST active — a TERMINATOR marker: its two lines can only appear after every earlier `module_start` completed, so "fresh PATH_TEST lines for this launch" proves full-list traversal (7/7). Doubles as observer growth-event cross-check (unchanged role). |
| 8 | `modbridge_ingame_facts.prx = 0` | igf-era one-shot probe, INERT | Mode 0 = skipped at parse: never loaded, NOT a second C2 writer on qdrv1's `/data/modbridge-ingame-facts.jsonl` (single-writer discipline preserved), consumes NO slot. Kept byte-identical for audit provenance; its .prx may stay on disk unloaded. Position is NOT load-bearing; last keeps the active block contiguous 1-7. |

Mode note: `= 2` for all seven active entries is the established single-mode
law; ORDER comes from file position only (mode 1 would order identically —
simulator variant `probes_mode1` proves registration order is unchanged).
Do NOT introduce mode-1 lines casually: the byte-exact verifier pins `= 2`;
a mode change is a NEW layout requiring re-pinned bytes and a NEW worker/ID.

Timing budget: 7 sequential mode-2 injections ≈ 7 x (load + ~1.6 s
stable+settle) ≈ 12-18 s total, far inside the 120 s injector timeout; fc1's
worst case (~8 s grace) is inside the 25 s per-plugin cap; xwp is a bounded
8-attempt one-shot under the same cap.

## 3. ORDERING CONTRACT

### 3.1 The one mechanism

Injection order == file order. Mode values time-gate each injection but never
reorder. Therefore: THE FILE ORDER IS THE CONTRACT. Anything that changes line
order changes launch behavior. The byte-exact verifier
(`verify_final_ini_layout.py`, §4) enforces the contract; the pinned parser
does NOT (it happily parses degraded orders — that is exactly why the
verifier exists).

### 3.2 What breaks if reordered

| Reorder | Consequence (mechanism) | Severity |
|---|---|---|
| probe (#1) AFTER composition (#3) | The probe still runs and still writes `/data/modbridge-exec-write-probe.jsonl` — but the composition's initializer already passed its init read. When `kExecWritabilityVerdictGating` is armed, verdict absent/unreadable at init → the initializer's compiled SAFE DEFAULT fires (SHAPE A: no seed-bound walk, no detour, loud refusal/throw only). **Acceptable degraded** — it is precisely the compiled default today, so no corruption, but the same-launch gating contract silently dies and the launch is wasted for gating evidence. | Degraded (acceptable but forbidden — verifier REJECTS) |
| fc1 (#2) AFTER composition (#3) | fc1 evidence lands later; a mid-ladder title death (the ladder has fatal-attempt classes) kills the launch BEFORE `module_start` → zero fc1 evidence for that launch. Any future same-launch consumption likewise sees nothing. | Degraded evidence risk (verifier REJECTS) |
| composition (#3) AFTER qdrv1/observer (#4/#5) | Ridealong windows start pre-ladder: observer's ~60 s window and qdrv1's ~2 min admission window burn on boot state → honest negatives (`stat_failed`, `admission_rcode != 0`). Windows close; a retry needs a NEW launch (never re-rate unchanged evidence). Not a crash — pure evidence loss. | Evidence loss (verifier REJECTS) |
| path_test (#7) not last | Its 2 oracle lines no longer prove full-list traversal — freshness oracle weakens to "path_test ran". No corruption. | Weakened oracle (verifier REJECTS) |
| igf flipped to 0 → 2 | Registers an 8th plugin AND a SECOND writer on qdrv1's `/data/modbridge-ingame-facts.jsonl` — violates single-writer discipline (simulator `igf2_counterfactual`). | MUST NEVER (verifier REJECTS) |
| ANY duplicated line | ploader does NOT dedupe: registers and PT_ATTACHes the module TWICE (simulator `duplicate_hazard` re-confirmed). One-shot probes answer re-entry 0, but double injection of composition/observer-class modules is an undefined-interaction hazard and burns slots. | HAZARD (verifier REJECTS) |
| Mode flip on any pinned line (e.g. composition → 1) | Changes that module's timing semantics vs every documented interaction audit; staged-pair law says existing-line modes are never rewritten (STOP + reconcile). | STOP (verifier REJECTS) |
| Unknown extra line | The parser REGISTERS any well-formed path (simulator `unknown_extra_line`) — slot burn + a module nothing audited. Only the verifier catches it. | HAZARD (verifier REJECTS) |

### 3.3 Idempotent append rules — byte-for-byte convergence regardless of staging order

Pure tail-appends in different staging orders produce different files, so the
staging contract is CONTENT-ANCHORED ENSURE, not tail-append:

```
ensure(path):
    line        = "<path> = <pinned mode>"      # never invent text
    duplicates  = count of existing lines for <path>
    duplicates > 1        -> STOP (reconcile; no dedupe law)
    duplicates == 1       -> no-op (idempotent)
    duplicates == 0       -> ensure(<canonical predecessor of path>)
                             insert <line> immediately AFTER the predecessor line
```

Canonical predecessor chain (= §1 order; header is a PRECONDITION proven by
the Step-0 RETR, never appended):

```
[PPSA33286]
  └─ modbridge_exec_write_probe_armed.prx = 2
       └─ modbridge_fc1pool.prx = 2
            └─ modbridge_supplier_composition.prx = 2
                 └─ modbridge_qh_driver.prx = 2
                      └─ modbridge_runtime_aligned_observer.prx = 2
                           └─ modbridge_runtime_aligned_entry.prx = 2
                                └─ modbridge_path_test.prx = 2
                                     └─ modbridge_ingame_facts.prx = 0
```

Why this converges: every insertion position is determined by CONTENT
anchors, never by file tail or operation history; each line has exactly one
canonical predecessor, so the fixed point is unique. PROVEN in the simulator:
ALL 8! = 40320 permutations of the eight ensure ops from the bare header
converge byte-for-byte to `ploader.ini.canonical`
(`simulate_ploader_ini_final.py`, variant `staging_permutations`), as do
repeated applications (`staging_subsets_and_repeats`).

Per-stager obligations (exact lines each stager appends; any execution order):

| Staging step | Owns (appends/ensures) | Anchor |
|---|---|---|
| xwp stager (NEW) | `…/modbridge_exec_write_probe_armed.prx = 2` | header |
| fc1 stager (NEW) | `…/modbridge_fc1pool.prx = 2` | xwp line |
| composition | NEVER appended — PRE-EXISTING, validated (mode 2, exactly once) | — |
| qdrv1 stager (`stage-qdrv1`, unchanged) | `…/modbridge_qh_driver.prx = 2` | composition line |
| observer stager (`stage-observer`, unchanged) | `…/modbridge_runtime_aligned_observer.prx = 2` | qdrv1 line |
| aligned-entry owner | PRE-EXISTING normally; ensured-if-absent (anchor = observer) | observer line |
| path_test owner | PRE-EXISTING normally; ensured-if-absent (anchor = aligned_entry) | aligned_entry line |
| igf | PRE-EXISTING `= 0` normally; ensured-if-absent last (anchor = path_test) | path_test line |

Note the existing console_batch anchors (qdrv1 → composition, observer →
qdrv1) are ALREADY canonical-position-correct; only the two probe inserts and
the aligned_entry/path_test relocation are new.

BOOTSTRAP (once, reconciliation-gated — the ONLY sanctioned non-append op):
two pre-existing lines (aligned_entry, path_test) move from their historical
pre-composition position to entries 6-7, which NO sequence of appends can
achieve. Therefore the return session starts: Step-0 RETR pre-image →
`sha256` + `.bak` STOR (`ploader.ini.<TAG>.bak`) → ONE host-built whole-file
STOR of the canonical 8 lines (all line texts are pinned above, so the
rebuilt file is byte-exact canonical) → RETR readback diff byte-exact. If the
Step-0 pre-image already matches canonical relative order for pre-existing
lines, skip the rebuild and run ensure ops only. Any pre-image anomaly
(missing expected line, wrong mode, unknown line, duplicate) = STOP +
reconcile per rollback-runbook sec 4.3 — never a blind rewrite, never a
second silent attempt; distinct TAG per attempt.

Sequence per session (order of STAGING STEPS may vary; the file outcome may
not): BOOTSTRAP → ensure ops in any order (idempotent) → byte-exact verify →
STOR → RETR diff byte-exact → launch. After launch expect exactly **7**
`OK (<ms>): <filename>` ploader notification lines and zero FAIL lines (the
old SC-5b gate's 5 becomes 7).

### 3.4 Final-layout verifier (exact snippet)

```sh
# after any staging step, and MANDATORY before the launch-gating STOR:
python3 worker-final-ini-layout/verify_final_ini_layout.py <ini-file>
#   FINAL-INI-OK  = byte-exact canonical (sha256 bbc34c16…), parser law re-checked
#   FINAL-INI-FAIL + structured diagnosis (missing/unknown/duplicate/mode/order)
# full built-in proof:
python3 worker-final-ini-layout/verify_final_ini_layout.py --selftest   # 9/9
python3 worker-final-ini-layout/simulate_ploader_ini_final.py           # 13 variants
```

## 4. Stager upload law for the two NEW modules (SHT-strip channel)

Same law as STAGE-PLAN-COMBINED §3 (readback = segment-covered prefix of the
bare-sdk ELF; verify exact-length + body-hash + Ehdr-exception with the §3
snippet). Constants recomputed THIS SESSION from the verified bare-sdk ELFs
(run1 == run2 both probes; fself hashes re-verified on disk):

| Module | console path | fself sha256 (bytes) | bare-sdk sha256 (bytes) | readback bytes | readback body sha256 (64..end) |
|---|---|---|---|---|---|
| xwp-armed | `/data/plugins/modbridge_exec_write_probe_armed.prx` | `48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e` (20013) | `cfed5e3d7e62100649fe1221cd4eaaf7554df421f08792264ae5238acb6d0830` (69464) | **67832** | `daea2ff4a6533fa9006efeeb1b24913a586b72c0f68826f563b9479bf6ef561b` |
| fc1pool | `/data/plugins/modbridge_fc1pool.prx` | `d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96` (22733) | `54f8335e6ed476a64e5bc0d65741c83808e52f79b55631a0803a410bd2b3f8a2` (69408) | **67848** | `7e2dce27e415bea80c3100d74e6f58c9a2cd8697bc87e03b67226fba09646ed3` |

Upload to FINAL names (nothing replaced), RETR readback, run the §3 snippet;
any FAIL → one re-STOR from the verified local fself, then abort-and-reconcile.
qdrv1/observer constants are unchanged from STAGE-PLAN-COMBINED §0/§3.

## 5. Receipts

- `receipts/simulate_final.run2.txt` — simulator PASS, 13 variants (exit 0),
  including the 40320-permutation byte-exact convergence proof and the
  duplicate/mode-3/unknown-line parser-law demonstrations. (run1 preserved:
  it caught my own wrong expectation on `unknown_extra_line` — the parser
  DOES register unknown lines; fixed expectation, doc §3.2.)
- `receipts/verify_selftest.txt` — verifier selftest 9/9 (canonical ACCEPT,
  8 mutation classes REJECT).
- `receipts/verify_canonical.txt` — `FINAL-INI-OK` on `ploader.ini.canonical`.
- Original ridealongs simulator left untouched; extension delivered as a NEW
  file embedding its parser law verbatim.

## 6. Honesty boundaries

Host-side layout design and parser-law simulation only. No console contact;
no on-console byte is claimed — every console constant stays [TARGET] until
the Step-0 RETR and the post-STOR RETR diff. The probe-before-composition
requirement is a parser-source fact (sequential injection) plus the
initializer's documented consumption design (`kExecWritabilityVerdictGating`
currently `false`): today the ordering is prospective (no behavioral
difference), and arming the constant remains a separate, explicitly-gated
decision with the verdict jsonl schema pinned by its own worker. The 12-18 s
timing figures are host-side estimates from pinned parser constants, not
measurements.
