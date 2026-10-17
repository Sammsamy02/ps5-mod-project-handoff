# STAGE-PLAN — worker-combined-igf7 (combined C2-data + rung3-chunk probe, ONE game session)

Team 2026-10-04-zcode-team. This plan is PROPOSAL-ONLY for the controller:
nothing was staged, launched, or executed on target by this worker. Build and
host tests are complete; the launch decision and offline comparison are the
controller's.

## 1. What was built

| Artifact | Value |
|---|---|
| Module | `modbridge_ingame_facts` (same name/path/ini line as igf4/igf5/igf6) |
| Log path | `/data/modbridge-ingame-facts.jsonl` (append; per-launch marker `modbridge.ingamefacts/v4 build=igf7-695a17bd2c6f`) |
| Build ID | `igf7-695a17bd2c6f` |
| Deliverable | `modbridge_ingame_facts.prx` (41,645 bytes; sha256 `c0d5dbfbf5a69f806f2735439af65936186cead74b623f0297100d4409366458`) |
| Determinism | run1 == run2 byte-identical: final PRX, inner ELF, combined object, import contract (see `build-receipt.json`) |
| Imports | the SAME 7 verbatim frozen-contract rows (`__error`, `sceKernelOpen`, `sceKernelWrite`, `sceKernelClose`, `sysctl`, `getpid`, `clock_gettime`); zero new identities |
| Replaces | igf6-ad9bda1fc50a at `/data/plugins/modbridge_ingame_facts.prx` (staging = the usual ini-line swap, no other file changes) |

## 2. On-console record sequence (write-through, crash-localizing order)

1. `seq1` entry marker (FIRST write, before any check).
2. `seq2` startup (post init_array, ctors_run, channel state).
3. `seq3` `mapscan`: ONE bounded sysctl KERN_PROC_VMMAP collection + ONE
   `rc3_vmmap_walk_min` walk → the single shared span table. Fields: walk
   stats (total/retained/truncated/insane/ksz_144/stop_reason), trust gate,
   `derived_base` (lowest prot&4 span start; must be 0x400000), exec
   lo/hi/extent cross-checks, readable span count.
4. `seq4..` `mapspans`: the READABLE span list persisted paged
   (24 spans/page, `[start,end,prot]` with hex-quoted start/end). Closes the
   igf6 gap (8685 spans counted, none saved).
5. `seq..` `c2cand` ×5, order **B, C, D, E, A (A last)**: each gated FIRST on
   the same-invocation table — refusal record (`map_untrusted` |
   `exec_span_intersection` | `uncovered` | `straddle` |
   `covered_not_readable`) or raw `live_hex` + covering-span fields
   (start/end/prot). NO expected bytes, NO comparison on console.
6. `seq..` `rung3`: `rc3_probe_chunks` over the SAME table — GUA header
   (layout A) → Objects** @GUA+0x10 → chunk[0]/chunk[1] → first FUObjectItem
   of each (bounded 64-slot null-scan) → UObjectBase fields (NamePrivate raw)
   → check family (vptr/flags/index-bounds/round-trip/class/outer + needed-
   chunk rule) → `rc` + `cls` (0 Validated / 1 Inconclusive / 2 Refuted).
7. `seq..` `batch2`: `rc3b::run_ustruct_chain` — readable-only
   `tlp::MemSpan` publication + gated `rfl::probe_ustruct_chain` (UStruct
   chain + UClass CDO extension). `ran`/`gate`/`verdict`/codes/anchors.
8. `seq last` `summary`: base + exec geometry, walk stats, per-candidate
   gated/outcome flags, `exec_reads_attempted:0`, `game_memory_writes:0`,
   write-failure count, rung3 rc/cls, batch2 ran/gate/verdict.

## 3. Safety contract (inherited + igf6 lessons applied)

- Every foreign dereference is `igu_read()/igu_read_u64()` through
  `span->data` AFTER a covering READABLE-span proof on THIS invocation's
  walk. The gate refusing a read is the proven-safe path (igf4 survived);
  ungated exec reads are the proven-dead path (igf6 died at exactly that).
- Explicit `exec_span_intersection` refusal for every C2 candidate BEFORE any
  readability decision; candidates are data-only.
- ZERO raw `syscall` instructions (build gate asserted == 0; 0F 05 scan of
  the inner ELF .text clean); NO mprotect in any form (forbidden-symbol
  gate); no writes to game memory; no threads; module stays loaded but INERT
  after `module_start` returns 0.
- Expected bytes: absent from every input, the generated config, the combined
  object, and the inner ELF (LOUD build gate vs
  `worker-c2-probe/EXPECTED-VALUES-OFFLINE.json`).

## 4. Controller launch steps (user-coordinated, never automated)

1. Verify: sha256 of the staged PRX equals `c0d5dbfb…366458`; import contract
   `a4b31f2e…6f9b`; inner ELF `7610cb1e…d45c3` (see `build-receipt.json`).
2. Stage: replace `/data/plugins/modbridge_ingame_facts.prx` with the new PRX
   (same ini line; rotate/save the previous igf6 log if retention is wanted —
   the log appends, per-record `build_id` disambiguates).
3. Launch Townfall via ploader, let the probe run to quiesce (it finishes in
   `module_start`; no unload), then collect `/data/modbridge-ingame-facts.jsonl`.
4. Crash localization is by record absence exactly as before: entry → startup
   → mapscan → mapspans pages → B, C, D, E, A → rung3 → batch2 → summary.
   Interpretation matrix for the new fact classes:
   - died at first DATA read (a c2cand B record never arrives despite a
     readable covering span): NEW fact class (prot-1 deref fault) — do NOT
     reuse this design without re-arbitration (spec sec.7 residual risk).
   - `rung3` record absent after A: chunk-table deref fault class.
   - `batch2` absent after rung3: predicate fault class (would be new; the
     predicate only reads published-readable spans).
5. Offline comparison per `EXPECTED-VALUES-OFFLINE.json` (PASS matrix there)
   + rung3/batch2 verdict extraction. Tick/SCO remain UNRESOLVED regardless;
   C2(b) acceptance stays the controller's explicit signed act
   (PROMOTION-DECISION.md §2).

## 5. Honest limits

- Host-tested only (8/8 tests, 95 asserts, two identical runs, 89/89 valid
  JSON lines; no target fact is claimed by this worker).
- No saved evidence shows a successful deref of a prot-1 (read-only) or
  flags-0 aux span; if the kernel faults on prot-1 despite the map, that is a
  NEW fact class, localized by record absence (B first, A last).
- The batch2 predicate verdict on real bytes is unknown until this run; the
  bridge gate codes make every not-run reason machine-readable.
