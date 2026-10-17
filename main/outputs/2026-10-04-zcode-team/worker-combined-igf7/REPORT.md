# REPORT — worker-combined-igf7 (ONE combined probe: C2 alternative data reads + rung-3 chunk reads + rung-3 predicates)

CLAIM.json was written first (entry marker); this REPORT is the last artifact.
Everything written lives under
`outputs/2026-10-04-zcode-team/worker-combined-igf7/`. No console, no network,
no nested subagents; all sibling inputs consumed read-only (compiled IN PLACE,
shas recorded in the build receipt). ~45 tool calls used (bound: 60). Scope
honored: BUILD + HOST TEST + DETERMINISM only — no target fact is claimed.

## 0. Deliverables

| File | Purpose |
|---|---|
| `CLAIM.json` | ownership + bounds + probe contract (written first) |
| `src/combined_probe.cpp` | the combined probe body: ONE shared VMMAP walk + mapscan/mapspans records + 5 gated C2 candidate reads (B,C,D,E,A) + rung3 record + batch2 record + summary |
| `src/module_entry.c` | vendored igf-lineage entry contract (entry marker FIRST, startup, then `ig_run_combined_probe`); only the probe call + notes changed |
| `src/support_mem.c` | freestanding memcpy/memmove/memset/memcmp — keeps the import table at EXACTLY the 7 verbatim rows (the consumed C sources emit memcpy/memset references; a libc import for them would be a new, never-console-proven identity) |
| `tests/test_combined_host.cpp`, `tests/host_shims.c`, `tests/generated/ingame_config.h` | host suite (8 tests, 95 asserts) + POSIX shims for the 7-import ABI + host config |
| `tests/test-run.txt`, `tests/test-run2.txt`, `tests/json-validation.txt`, `tests/out-*.jsonl` | committed evidence: 8/8 PASS ×2 runs, 89/89 emitted lines parse as JSON |
| `build_igf7_combined_probe.py` | packaging pipeline reused EXACTLY from the frozen igf6 lineage (import_references-first combine, Packager.dll IN PLACE, finalize_sdk 0x11000000 stamp, pinned make_fself) + igf7 gates |
| `modbridge_ingame_facts.prx` | **igf7-695a17bd2c6f**, 41,645 bytes, sha256 `c0d5dbfbf5a69f806f2735439af65936186cead74b623f0297100d4409366458` |
| `build-receipt.json` | full build receipt: determinism matrix, consumed-input shas, gates, command log |
| `EXPECTED-VALUES-OFFLINE.json` | the OFFLINE C2 expected values (from worker-c2-alt-correspondence/worker-c2-probe, sha-pinned capture) + PASS matrix — NEVER embedded in the module |
| `STAGE-PLAN.md` | controller staging/launch/crash-localization/interpretation plan (proposal-only) |

## 1. The combined record sequence (ONE game session)

`seq1` entry → `seq2` startup → `seq3` mapscan → `seq4..` paged `mapspans`
(readable span list persisted; closes the igf6 8685-spans-unsaved gap) →
`c2cand` ×5 in order **B, C, D, E, A (A last)** → `rung3` → `batch2` →
`summary`. Every record carries the igf4/igf6 `build_id`; crash localization
is by record absence exactly as the lineage did.

## 2. One walk, not two (task requirement)

`seq3` performs ONE bounded sysctl KERN_PROC_VMMAP collection and ONE
`rc3_vmmap_walk_min` walk (v3 variable-kve_structsize stride, sane-record
retention, v3 trust gate, full-bounded mode — no early stop) producing THE
single `igu_span` table. That one table backs, in the same invocation:

- the persisted readable-span list (`mapspans` pages),
- `derived_base` = lowest prot&4 span start + exec lo/hi/extent cross-checks,
- C2 candidate gating (`igu_find_covering` + explicit exec-span intersection
  refusal) and the raw candidate reads,
- `rc3_probe_chunks` (rung-3 header → Objects** → chunk[0]/chunk[1] →
  FUObjectItem → UObjectBase with the full check family),
- `rc3b::run_ustruct_chain` readable-only `tlp::MemSpan` publication.

There is no second collection and no stale/recovered map anywhere in the
module. If the walk fails the trust gate, EVERY read refuses
(`RC3_ST_NO_MAP` / `map_untrusted`) — the honest needs-map failure mode.

## 3. Host-test evidence (task 3)

8/8 tests, 95 asserts, 0 failures — run twice, plus 89/89 emitted lines
validated with `json.loads` (the v3 writer guarantees non-truncated lines are
valid JSON; a bare-`0x`-number bug in my span-page emitter was found BY this
validation and fixed by quoting start/end). Fixtures mirror the live values
(GUA `0x97ba110`, Objects** `0x13800220a0`, 71959/33/2, first_gc 35477) with
SYNTHETIC chunk/object storage, and use the OFFLINE expected bytes ONLY as
read-plumbing oracles. Coverage:

- t01 happy: 5/5 candidates gated-and-read with byte-exact `live_hex`,
  base/exec-extent cross-checks, rung3 Validated (all checks PASS, both chunk
  views, round-trip), batch2 `ran=1 gate=OK`, summary clean, B→C→D→E→A order.
- t02 A covered-not-readable → refusal, others unaffected. t03 E inside the
  derived exec span → `exec_span_intersection` refusal BEFORE any read (the
  igf6 crash class is structurally unreachable). t04 non-canonical Objects**
  → rung3 Refuted + bridge `STEP_FAILED`, C2 reads independent. t05 insane
  structsize → walk stops (stop-never-guess), trust gate 0, global refusal,
  `RC3_ST_NO_MAP`, bridge refuses on the empty table. t06 C uncovered →
  `uncovered` refusal. t07 two-run determinism: byte-identical record
  streams. t08 shape: marker prefix, `}}` tail, strictly monotonic seq.
- Negative-result honesty: the bridge gate on an untrusted EMPTY table is
  `BRIDGE_GATE_ARG` (no spans to publish) — recorded, not silently mapped to
  MAP_UNTRUSTED.

## 4. Build gates (all LOUD, all passed)

- Zero raw `syscall` instructions in the combined object; no `0F 05` byte
  pair in the inner ELF `.text` (igf6-inverted gate).
- Forbidden-symbol gate (module-query/dl/mprotect families incl.
  `igcar_sys_mprotect`) — none present.
- Import contract = the SAME 7 verbatim frozen-contract rows, byte-equality
  re-verified after write; contract sha `a4b31f2e016ec50ac555c9f168fb52e29a9e9a3cb2ad979858a493383e9e6f9b`.
- Expected-bytes absence gate: none of the 5 candidate hex windows exist in
  any input source, the generated config, the combined object, or the inner
  ELF (no self-fulfilling probe; the comparison is offline controller work).
- Frozen lineage hash re-verified before build; every consumed input sha
  recorded (`build-receipt.json: consumed_inputs`).
- Two-run determinism: run1 == run2 on final PRX, inner ELF, combined
  object, and contract.

Predicate-provenance note (recorded in the receipt): the link uses the
worker-5 `townfall_layout_probes.cpp` — the EXACT set the 20/20 rung3 host
suite validated — NOT the igf6-pinned gc-fix cpp, because this probe never
replays the cfgH GUA-header predicate that pin was required for; the batch2
`rfl::probe_ustruct_chain` is unchanged.

## 5. What the controller still owes (nothing executed here)

- Stage + launch per `STAGE-PLAN.md` (user-coordinated; same ini line,
  same path — replaces igf6).
- Offline comparison per `EXPECTED-VALUES-OFFLINE.json` (PASS matrix included;
  base must equal 0x400000; every covering span `prot & 1` from the
  same-invocation mapspans records).
- Extract rung3 `rc`/`cls`/per-check states and the batch2 `gate`/`verdict` —
  on `cls==0` this is the first live UStruct/UClass CDO chain evidence
  (the batch3 PROMOTABLE-SET gate); on Inconclusive/Refuted every reason is
  machine-readable.
- C2(b) acceptance for Tick/SCO remains the controller's explicit signed act;
  this probe never touches Tick/SCO bytes and no >8 claim is made anywhere.

## 6. Honest limits

- Everything here is synthetic beyond the recorded live values; no target
  fact about the candidates, chunk table, chunks, items, or objects is
  established by this work.
- Residual risk (spec sec.7): no saved evidence shows a successful deref of a
  prot-1 or flags-0 span; if the kernel faults on prot-1 reads despite the
  map, the crash localizes by record absence (B first, A last) and is a NEW
  fact class — re-arbitrate before any design reuse.
- The `mapspans` persistence adds ~360 records for an igf6-shaped map
  (8685 readable spans, 24/page) — bounded and intentional; the log appends,
  so per-record `build_id` disambiguates runs.
- One scoped local process note: host-test iteration used only files under
  this worker directory; no unrelated process or file was touched.
