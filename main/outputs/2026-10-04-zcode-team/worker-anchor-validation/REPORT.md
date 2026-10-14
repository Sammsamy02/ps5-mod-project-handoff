# REPORT — worker-anchor-validation (2026-10-04)

## Status: DELIVERED AND FROZEN (local build + host verification; NO console run)

- **Build ID `igf4-4891869337b4`**
- **PRX sha256 `4aa624cc4cc045c6c137b07b2283f458ad2e41a576273444d0fc75e1e9762756`
  (26077 B)** — `modbridge_ingame_facts.prx` (top level) ==
  `determinism/run1/` == `determinism/run2/` byte-identical
- Inner ELF (decryption-oracle target): 85536 B,
  sha256 `bd591ed3159ff1c571b85ebba0118e5f65912b5780297ccafd41ebe640b51914`
- Contract sha256 `42b2fdc3ea85bd62ffbc069629f5997b8c0ec922382f46560ca05bee1fbedf96`
  (7 rows byte-identical to the frozen 191-import contract)
- Gates at freeze: HOST-CHECKS-PASSED; LOCAL_PACKAGE_CHECKS_PASSED; frozen
  lineage hash verified; forbidden-symbol gate passed; expected-bytes
  provenance chain verified at build time; cross-worker predicate object
  hash MATCH.

## Controller corrections applied

1. **SAFETY BEFORE DEREFERENCE** — implemented in `src/mapscan.c` +
   `src/anchor_probe.cpp`. Every foreign read (Tick 0x33ac8c0, SCO 0x890aa0,
   GUA 0x97ba110) is gated by a CURRENT per-invocation sysctl
   KERN_PROC_VMMAP collection (bounded 64 KiB→4 MiB, v3 mode-1 lineage walk,
   trust-gated: >=8 sane records, <=20% insane, nothing dereferenced while
   untrusted). The v3 map is never assumed current. An unverifiable address
   yields a refusal record (`refusal:"map_untrusted"|
   "not_covered_current_map"|"covered_not_readable"`) and NEVER a
   dereference; the remaining checks still run (correction #4). Host tests
   verify the exact coverage formula over the checkpoint spans.
2. **CODE ANCHORS** — `tick_anchor`/`sco_anchor` records compare the 16
   gated live bytes against the retained-image expected bytes
   (`554889e54157415641554154534881ec` each, extracted at file offsets
   0x2fb08c0/0x494aa0 = profile vaddr + 0x4000, LOAD0 re-derived from the
   image ELF header this session; sha-pinned to the survey receipt). Byte
   equality is the correct code criterion; a mismatch records live_hex +
   per-byte diff_mask (relocation/thunking finding, not a crash).
3. **GUA PREDICATE IS STRUCTURAL** — the vendored batch1 predicate
   (worker-5, byte-identical source) runs on the LIVE 96 bytes under BOTH
   layout hypotheses (A FUObjectArray-outer / B TUObjectArray-direct), in
   two recorded feed configurations that are never collapsed:
   cfgH `gua_pred_header` (task-literal 96-byte span) and cfgL
   `gua_pred_live` (the full current readable span set — the configuration
   CONSUME.md itself names as the real module use). NO byte comparison with
   the executable for GUA: the `gua_read` record emits the bytes and words
   as evidence only.
4. **ONE INVOCATION** — seq 1..10 all under one `igf4-` build_id; each
   record flushed before the next gated dereference; a refusal never blocks
   later checks (same address space).
5. **READS AND WRITES AUTHORIZED** — one controlled write test: SAME-VALUE
   writeback of the 96 already-read GUA bytes via 12 eight-byte stores
   (value-preserving — no semantic change even under concurrent readers),
   then re-read+compare (`gua_writeback`: attempted/restored). Only
   attempted when the covering span is prot-writable; any other outcome is
   recorded, not forced.
6. **NO FULL MAP DUMP** — the sysctl buffer is walked internally only;
   records carry counts/trust flags and, per probed address, the single
   covering span's [start,end,prot]. There is no span-list record — the v3
   VMMAP dump (the last thing that ran before its stall) is gone by design.

Acknowledged as instructed: corrections incorporated before the build; the
probe is the correction-shaped design, not a post-hoc patch.

## What was built

Focused ploader PRX (same module name/path/log path, same 7-verbatim-import
packaging as the loaded-and-logged v2/v3 lineage), record sequence:

| seq | exp | content |
|-----|-----|---------|
| 1 | entry | entry marker, first, flushed (lineage contract) |
| 2 | startup | ctor count, channel state |
| 3 | mapscan | bounded current map: trust + counts, no dump |
| 4 | tick_anchor | gated 16 B at 0x33ac8c0 vs retained-image bytes (+code-shape) |
| 5 | sco_anchor | gated 16 B at 0x890aa0 vs retained-image bytes (+code-shape) |
| 6 | gua_read | gated 96 B at 0x97ba110: hex + w0..w3 layout-discerning words |
| 7 | gua_writeback | same-value 96 B writeback + re-read (writability proof) |
| 8 | gua_pred_header | predicate, cfgH feed, both hypotheses |
| 9 | gua_pred_live | predicate, cfgL feed, both hypotheses |
| 10 | summary | per-check pass/fail + promotion_gate_indicator (not a claim) |

Everything quiesces: channel closed, module loaded but inert, no unload.

## Expected-bytes extraction (retained image → artifact)

`expected_bytes_tick_sco.txt`: image sha256
`3b6cfad4…622fc` (== survey receipt capture), ELF-header-derived LOAD0
correspondence (p_offset 0x4000 / p_vaddr 0x0), Tick@0x2fb08c0 and
SCO@0x494aa0 → both `55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec`
(canonical frame setup). The hex is injected into the artifact via
generated/ingame_config.h with LOUD build-time re-verification — the PRX
provably derives from the retained-image evidence. Independent agreement:
worker-expected-bytes (32-byte extraction, same image) has byte-identical
first-16 windows, continuing into `sub rsp,0x88` (Tick) / `sub rsp,0x258`
(SCO).

## Cross-worker predicate consumption decision

worker-predicate-compile offered `predicate_bundle.o`. Consumed as a
CROSS-CHECK, not a link input (reasoning recorded in my coordinator reply):
the bundle IS the same worker-5 predicate; its C wrapper is single-span-only
while correction #3's live structural predicate needs the full span-set
feed (which CONSUME.md also states); source-level provenance, host
testability, and determinism stay in my pipeline. Result: my independently
compiled `townfall_layout_probes.o` is **byte-identical** to the bundle's
(sha256 `2acd3217…b9ca9`) — two workers, same source, same frozen flags,
same object bytes.

## Full outcome analysis — what each result means for promotion

Let M = mapping correspondence (single-load-bias, base 0x400000), C =
code anchors, G = GUA structural validation.

1. **tick_anchor match=1 AND sco_anchor match=1** → the EXEC-segment
   correspondence is CONFIRMED against the retained image: base+vaddr lands
   on the exact file bytes for two independent functions. This is the
   decisive anchor evidence the v3 stall prevented; combined with the
   already-proven GUA span arithmetic it promotes M from
   checkpoint-fact to launch-verified fact for that invocation. Either
   anchor ALONE matching still corroborates M (two functions = two
   independent hits); both matching removes the "one lucky alignment"
   objection.
2. **match=0 anywhere** → real finding, recorded with diff_mask: implies
   relocation, thunking, or a different image at 0x400000 this launch.
   Blocks promotion of M; the w0..w3 words and refusal fields localize
   which half of the model broke (exec vs RW).
3. **refusal records instead of bytes** → the safety gate fired: map
   untrusted (sysctl shape changed), address not covered (layout shifted),
   or covered-not-readable (prot model wrong). Each is a specific,
   actionable finding — and the module survives to say so.
4. **gua_pred_live Verified under exactly one layout** → the GUA layout is
   VALIDATED for the mapped snapshot (worker-5 discipline: elevates the
   candidate for that snapshot; lifetime/identity stay worker-1 facts).
   With anchor_confirmed, `promotion_gate_indicator=1` — the controller
   then promotes the layout facts per SEGMENT-CORRESPONDENCE + span-
   arbitration rules (the predicate never arbitrates alone).
5. **both layouts Validated (AMBIGUOUS)** → Inconclusive; the 96 bytes are
   real but self-consistent under both readings; w0..w3 decide by eye.
6. **cfgH Refuted GOA_R_PTR_OUTSIDE_READABLE + cfgL Validated** → the
   expected healthy shape for a heap-backed GUA: header-local feed cannot
   chase pointers; the live-span feed is the evidence. This pairing is a
   positive result, not a failure.
7. **cfgL Inconclusive GOA_E_CHUNK_EXTENT_UNVERIFIABLE** → needed chunk
   bytes fall in unreadable holes: evidence unavailable; re-arbitrate with
   the w0..w3 words (counts still discriminate sanity).
8. **cfgL Refuted (rcode recorded)** → hard contradiction (non-canonical
   pointer, insane counts, GC-order violation, null/duplicate/overlapping
   chunk, capacity mismatch): either 0x97ba110 is not the GUA this launch
   or the layout hypothesis set is wrong — blocks G promotion, does not
   touch M (code anchors are independent).
9. **gua_writeback restored=1** → the span is truly writable (BSS-like
   runtime data), strengthening "GUA is live runtime-populated structure"
   and proving the write path the future mod-manager needs. attempted=0
   with "covered_not_writable" would contradict the BSS model — a real
   finding.
10. **mapscan trusted=0** → every address refused; run reduces to
    "load+log works, current map unreadable" — still valuable, still no
    crash, and the v3 buffer facts (10017/10088×144) say this should not
    happen absent an OS-side change.

## Evidence summary (all local; nothing native executed)

- HOST-CHECKS-PASSED: writer exact (2 parse + 1 flagged-truncation per the
  lineage contract), predicate 13/13, vmmap walk 13/13, expected-bytes
  config match true.
- LOCAL_PACKAGE_CHECKS_PASSED: 7 NID-encoded dynsym imports, 0 weak;
  forbidden-symbol gate over the whole combined object; init/fini slots
  1/1; explicit DT_INIT/DT_FINI; SDK 0x11000000; fself segments equal inner
  ELF; bare sha pinned in fself info.
- Determinism: run1 == run2 byte-identical (fself, inner ELF, combined
  object, contract).
- Cross-worker predicate object hash: MATCH (see above).
- Frozen lineage hash re-verified before building (3b998f09…).
- Fix history this session (all local, none silently absorbed): sys/user.h
  include order (lineage order restored), module_entry missing generated
  config include, C++-linkage reference to the C record_sink header (now
  extern "C"-guarded in anchor_probe.cpp). Two earlier build dirs were
  removed after their LOUD failures (a78bdcda5dac, 10a9687c09ae,
  1fa7a32db58b); none produced a deliverable.

## Vendored-writer observations (recorded, NOT changed)

- `ig_obj_open` does not emit a leading member separator (v3 lineage
  quirk): a nested object after an existing member would emit invalid
  JSON. The igf4 records use only the separator-correct `ig_kv_*` helpers
  and raw-value arrays, so no igf4 record shape is affected; noted for any
  future consumer of `ig_obj_open`.
- Truncated records are exempt from the parse guarantee by design (the
  `__trunc":1` tail lands inside the cut value); igf4's largest record is
  ~0.9 KiB against the 2048-byte bound, so truncation is not expected
  in-game.

## Not done / not claimed

- No console staging, no launch, no in-game facts — STAGE-PLAN-ANCHOR.md is
  the controller's runbook. No layout promoted, no GUA validated: the run's
  outputs decide; candidate != validated per the standing honesty rules.
- I did not touch anything outside this output dir; all inputs (retained
  image, worker-5 predicate, v3 lineage sources, worker-predicate-compile
  bundle) were consumed read-only.
