# DESIGN — worker-record-sink-addon (qdrv1, 2026-10-04)

Minimal late-load PRX add-on that drives the query-harness record path from
OUTSIDE the composition, so `/data/modbridge-ingame-facts.jsonl` can carry the
rung-1/2 QueryResultRecord evidence once composition-5 r6 reaches engine-ready.
Writes confined to this worker dir; no console contact; no nested subagents.

## 1. Route decision (task question 1)

**The igf-lineage route suffices; the add-on is a self-contained driver PRX.**
Findings that decide it:

1. The composed PRX stages `native-query-harness`
   (`worker-composition-5-rebuild/build_runtime_composition5.py:157`) but
   NOTHING in the composed source calls `OnEngineTickDispatch` or
   `FormatRecordJsonLine` — tree-wide grep finds host tests only;
   `worker-query-s2-arm/integration-patch/WIRING.md:120-124` leaves the driver
   to the controller. So the harness inside the composition is inert, and the
   add-on must provide BOTH the dispatch and the sink.
2. A separate PRX has the same in-process privileges the composition has:
   the igf probe family (worker-anchor-validation igf4 → ingame-probe v2/v3 →
   igf6) READ live engine memory from their own module and WROTE this exact
   JSONL through their own `sceKernelOpen/Write/Close` trio. The live GUA
   facts were validated from outside the composition: layout A (FUObjectArray
   outer) at runtime `0x97ba110`, NumElements=71959, first_gc=35477 >
   last_non_gc=35476, Objects**=0x13800220a0, MaxChunks=33, NumChunks=2
   (worker-staged-pair-live/CLAIM.json, worker-batch2-live-prep
   provenance).
3. The harness is seam-injectable by design (`NativeQueryHarness.hpp:155-173`,
   `InstallEngineBindings` + `ArmLayoutGate`): the add-on supplies its own
   four probes; the RC::Unreal default bindings live inside the composed
   runtime and cannot be linked here.
4. What a separate PRX can NOT do: execute on the engine's OS game thread.
   That needs a code detour (mprotect/raw syscalls — the igf5 crash correlate,
   forbidden by the frozen recipe), or an in-composition persistent tick
   callback (WIRING.md route, already decided for a later composition rev).
   **LOUD NON-CLAIM:** the add-on dispatches from its OWN worker thread;
   `game_thread_id_key` is the driver-thread identity fold. Rung-1/2 verdicts
   from this add-on evidence the RECORD PATH (seqlock record →
   FormatRecordJsonLine → IgSink JSONL, counters advancing, refusals counted,
   real engine count observations) — the in-game-thread claim stays with the
   composition route.

## 2. Architecture

```
module_entry.c (lineage entry, labeled)          qh_driver.cpp (NEW)
  module_start:                                     worker thread, 30 polls x 4 s:
    sink open on C2                                    1. fresh ig_mapscan_run()
    seq1 entry marker (FIRST, lineage law)             2. GUA admission (below)
    bounded ctor walk (<=64)                           3. wiring schedule
    seq2 startup record                                4. Global().OnEngineTickDispatch()
    qh_driver_start -> 1 pthread (detached,            5. emit counter envelope
    bounded, self-terminating)                            + once: the pinned
    module_stop: lineage fini walk, no sink access          FormatRecordJsonLine LINE
```

Record shapes on C2 (`/data/modbridge-ingame-facts.jsonl`, append-only, one
`sceKernelWrite` per line, all < 1 KB):

- **ig envelope counter line per poll** (vendored `record_sink.c` v3, envelope
  `marker=modbridge.ingamefacts, schema=1, build_id=qdrv1-*, seq=N`), fields:
  `poll`, `phase` (bindings_absent|gate_closed|armed|terminal), `map_trusted`,
  `admitted`, `admission_rcode`, raw GUA header fields (`gua_num_elements`,
  `gua_max_elements`, `gua_max_chunks`, `gua_num_chunks`, `gua_first_gc`,
  `gua_last_non_gc`) for OFFLINE corrected-predicate replay, and the harness
  counters with names rung_check.py reads inside `.fields`:
  `ticks_dispatched`, `ticks_accepted`, `refused_bindings_absent`,
  `refused_layout_gate_closed`, `refused_identity_unknown`,
  `refused_wrong_thread`, `staged_resets`, `qh_record_emitted`.
- **the qh record**: the EXACT `FormatRecordJsonLine` output written as a
  STANDALONE line (NOT enveloped) — rung_check.py classifies rung evidence by
  top-level `"record":"native_query_harness"`; enveloping would hide that key.
  Emitted exactly once, when `TrySnapshot` first hands out the terminal
  record. This is the one intentional deviation from the envelope discipline,
  mandated by the pinned checker shape.
- entry / startup / mapscan / summary lines: lineage discipline (entry marker
  first, seq 1..3, summary at thread end incl. `wiring_refusals`,
  `write_failures`, `driver_key` hex).

## 3. Harness vendoring (labeled changes only)

`src/NativeQueryHarness.hpp` is BYTE-IDENTICAL to worker-query-harness.
`src/NativeQueryHarness.cpp` differs only in the wiring-guard plumbing
(diff pinned at `receipts/nqh-addon-variant.diff`; all record/seqlock/refusal/
formatter logic untouched):

1. The build compiles it with `-DNATIVE_QUERY_HARNESS_HOST_TEST` ON TARGET —
   in this build the macro's only effect is excluding the RC::Unreal default
   bindings; host tests compile the same TU the target links. The add-on wires
   through `InstallEngineBindings`.
2. The seven `throw std::logic_error` sites (wiring guards only, conditions
   and order unchanged) become `++g_nqh_wiring_refusals; return;` — this PRX
   is freestanding (no unwinder). The counter is never expected nonzero; it is
   reported in the summary line as a LOUD wiring-bug signal.
3. `std::this_thread::yield()` → bounded `nqh_pause_spin()` (same retry bound).
4. `std::memcpy` (32-byte gate_source copy) → `ig_memcpy`.
   `src/qh_libc.c` defines plain `memcpy`/`memset` shims over the lineage
   helpers so any compiler-emitted copy call resolves INSIDE the module
   (import surface unchanged).

## 4. Engine window + admission (the four bindings)

Per poll, BEFORE the single dispatch: fresh `ig_mapscan_run()` (controller
correction #1 discipline: a CURRENT sysctl KERN_PROC_VMMAP scan backs every
foreign dereference), then the GUA header window admission over a 96-byte
stack copy read by direct dereference (the igf6-proven read class —
span-checked first; NO mprotect, NO raw syscall; the recipe asserts 0 `syscall`
instructions):

- rcode 1/2: address not covered by a current readable span → refuse;
- rcode 3: layout-B pointer view (@+0x00 as u64) is CANONICAL → the two
  retained 5.x layout hypotheses are ambiguous in this window → refuse
  (with live layout-A data the u64 concatenates two small int32s and fails
  the user-half rule (p>>47)==0, so exactly one interpretation survives; note
  the conservative consequence: while ObjLastNonGCIndex < 0x8000 the header
  window cannot exclude TUObjectArray-direct and the driver refuses);
- rcode 4/5: Objects**/PreAllocated not canonical-or-null / both null;
- rcode 6: counts insane — `!(0 < NumElements <= MaxElements &&
  1 <= NumChunks <= MaxChunks <= 512 && MaxElements <= MaxChunks*65536)`
  (constants cited from worker-batch2-live-prep provenance, originally
  worker-predicate-gc-fix townfall_layout_probes.cpp:19-31);
- rcode 7: corrected GC invariant —
  `first_gc > last_non_gc && first_gc >= 0 && last_non_gc >= -1 &&
  last_non_gc <= MaxElements` (the corrected form; the old inverted form
  refuted healthy arrays: live first=35477, last=35476).

The full corrected-predicate chunk-extent chase (cfgL) is NOT performed
in-PRX (same honesty as igf6's minimal rebuild); the raw fields ride every
counter line for offline replay.

Bindings (all four consult ONLY this poll's admission snapshot):

| Binding | Implementation | Honesty note |
|---|---|---|
| `is_game_thread_initialized` | map trusted AND this poll admitted | "engine window proven", not UE4SS init |
| `is_in_game_thread_raw` | `pthread_self()==captured driver self` AND admitted | exactly one admitted thread: the driver |
| `game_thread_id_key` | 0 unless window proven; else `((pid<<32)\|(self&0xffffffff))\|1` | nonzero, stable for the session; NOT a UE4SS game-thread id |
| `read_num_elements` | admitted NumElements (layout A @ +0x24) | the sole engine read, harness law |

Key property: any admission failure makes the key 0, so the harness REFUSES
(`RefusedIdentityUnknown`) BEFORE the count read, discards any staged count
(`staged_resets`), and a terminal record can never be built from a failed or
torn read.

## 5. Wiring schedule (why it is honest)

- `InstallEngineBindings` happens on the first poll AFTER the count source
  first admits → poll 0 (and any pre-admission poll) exercises the harness's
  own `refused_bindings_absent` guard naturally.
- `ArmLayoutGate` only after 6 consecutive admitted polls with NumElements
  strictly > 10000 → the gate-closed refusals are natural, and the terminal
  pair can only be `Inconclusive` by a genuine post-arm count collapse, never
  by arming early. Gate facts: engine 5.6 / axes 0,0 (labeled add-on
  constants; the S2 scan detector is inside the composition), storage
  ChunkedArray (live NumChunks=2), source label `igu_layout_a_live_header`.
- After arm: poll N stages (accepted 1), poll N+1 publishes the terminal
  record (accepted 2) — `counts_recorded=2`, `refusals_between=0` by harness
  construction. Post-terminal polls continue counter lines (accepted frozen)
  so the file shows the whole arc. 30 polls x 4 s ≈ 2 min thread lifetime,
  then the thread closes the sink and exits (observer-lineage boundedness).

## 6. Build (frozen recipe, labeled deltas)

`build_qdrv_probe.py` reuses the igf6 pipeline unchanged (import_references-
first combine; Packager.dll used IN PLACE; finalize_sdk 0x11000000 stamp;
pinned make_fself; syscall gate == 0; forbidden-symbol gate; determinism
double-build). Deltas, all labeled:

- module `modbridge_qh_driver`, build_id prefix `qdrv1-` (own loader identity);
- 11 verbatim frozen-contract import rows: the igf 7 PLUS `nanosleep`,
  `pthread_create`, `pthread_detach`, `pthread_self` — FLAGGED additions,
  each byte-identical rows of the SAME frozen 191-row contract (pthread rows
  are the observer-lineage precedent). No derived identity, nothing else new;
- vendored identity is loud-verified per file (10 byte-identical vs igf5,
  hpp byte-identical vs worker-query-harness, 3 labeled files pinned, 3 new
  files recorded); `receipts/nqh-addon-variant.diff` is a deliverable;
- LABELED post-packaging transform: the used-in-place Packager.dll hardcodes
  `moduleName/exportLibraryName "modbridge_ingame_facts"`; the bare ELF gets
  a length-preserving string patch to `modbridge_qh_driver` (NUL-padded)
  BEFORE finalize/make_fself, so all downstream hashes and the fself wrapper
  cover the patched bytes, and both determinism runs prove it byte-stable
  (occurrences patched: 1 per run; final artifact carries 3
  `modbridge_qh_driver` strings, 0 `modbridge_ingame_facts`);
- the artifact checker derives the expected dynsym NIDs INDEPENDENTLY with
  the pinned SceNid algorithm (SharpProspero SceNid.cs: SHA1(name+suffix)
  first 8 bytes reversed, base64, 11 chars) — no circular oracle.

## 7. Host testing

`tests/run_host_tests.sh` compiles the REAL vendored sources (driver +
harness with the same target define + record_sink + ingame_libc) with host
shims and synthetic GUA fixtures, runs the full loop synchronously, and:

- main mode asserts: 30 polls, 29 sleeps, zero wiring refusals, exactly one
  qh record, ticks_accepted reaching 2, both guard refusals counted;
- refusal modes (bad_counts→6, bad_gc→7, ambiguous→3, uncovered→1) assert the
  driver never accepts a tick and never emits a qh record under a failed
  window;
- the emitted JSONL (REAL vendored output) is staged as rung_check fixtures
  (snapshot1 = history before the accepted==2 line, snap2 = full file) and
  `worker-rung-evidence-reader/rung_check.py --from-dir` verdicts:
  **RUNG1 PASS (all 5 conditions + driver line), RUNG2 PASS (terminal
  completed record), overall PASS.**

## 8. Non-claims and risks

- No console execution claimed; STAGE-PLAN-QDRV1.md only stages and predicts.
- The add-on does not run on the engine game thread (see 1.4). Rung-1/2 PASS
  from this channel is RECORD-PATH + real-GUA-count evidence; pair it with
  the composition route for in-game-thread claims.
- If the igf6 probe (modbridge_ingame_facts.prx) is staged alongside, both
  append to C2; each line is one small atomic-ish O_APPEND write and every
  line is self-contained JSON, but the controller may prefer to unstage the
  one-shot probe for the rung window.
- If the console loader rejects any of the 4 flagged verbatim rows (the v1
  correlate was DERIVED rows, so this is unlikely), the failure shape is the
  v2-documented silent load-reject; the fallback is a no-thread variant,
  which cannot pace the rung-1 window and is NOT built here.
- Monotonic risk: a genuine NumElements decrease between the two accepted
  polls publishes `monotonic="decreasing"` with status completed — rung-2
  then honestly FAILs; do not re-rate, take a fresh launch window.
