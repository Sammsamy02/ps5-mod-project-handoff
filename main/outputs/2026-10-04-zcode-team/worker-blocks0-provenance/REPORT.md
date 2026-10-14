# REPORT — worker-blocks0-provenance (2026-10-05)

## Question (user directive)

Did ANY pinned observation ever CAPTURE the actual FName-pool Blocks[0] POINTER
VALUE, and from which invocation? The figure [0x970c000,0x9a34000) is a VMMAP
region observation (mapping class), NOT a captured pointer value — the pool
validator's image-resident fixture may only be labeled 'observed' if a real
captured value exists; otherwise it stays 'candidate/synthetic'.

## Verdict

**NO CAPTURE EXISTS — the 'candidate/synthetic' label stands.**

- No invocation (launch-1…9, igf1…igf7 probes, livefacts, carrier/fc1pool
  builds, host tests) ever captured a Blocks[0] value. The only code that could
  (initializer Phase-3 pool chain at UI:827/868; fc1pool driver `ig_kv_hex
  "blocks0"`) never reached its read on target:
  - launch-9 (2026-10-05, pid 273, entry build f6530c79…): refused upstream at
    the entry geometry gate; **zero** `[Seeds]` lines of any kind in the full
    2,343,756-byte aligned-entry log and all three sibling sinks (verified by
    byte-level grep).
  - fc1pool (build fc1pool1-78508a2e2940): built, **not injected** (LAUNCH9
    scoreboard "NO (0B), produced no lines").
  - igf6 FC-1 module (igf6-fc1prep-204a6d60a4ed): built, **never executed**
    (igf6 records stop at entry/startup/mapscan; SEED-TABLE slot 2:
    "none yet in-game").
  - launch-10 (next candidate): **not run** (HOLD per README-RETURN.md).
- Every literal Blocks[0] hex value in the tree is synthetic:
  0x80234ae0 / 0x0 (verdict-rehearsal fixtures — self-declared "synthetic
  launch-10 evidence … no console contact"), 0x300020000
  (fname-heap-runtime-bounds host positive-test A8), and cc1-host jsonl rows
  (carrier probe host tests).

## Origin of [0x970c000,0x9a34000) — confirmed mapping class

The range appears in the in-game facts records as a **kernel VMMAP entry**
(`start=0x970c000 end=0x9a34000 prot=3`): igf3 `v1_vmmap_page` seq 4 +
`v1_vmmap_raw` seq 1031 (raw kinfo_vmentry rec 6), as igf4 `gua_read`
containing-span metadata (`span_start/span_end` around the real 96-byte GUA
read at 0x97ba110), and as igf7 `mapspans` s5. It is the module image's
LOAD7 BSS-tail mapping — the region that **contains the pool struct**
(0x9765ec0), not a recorded value of pool+0x10. No jsonl anywhere contains a
read of 0x9765ec0/0x9765ed0.

Consequently the sentence in worker-re-fname-pool (FNAME-POOL-RE.md:30/168,
CLAIM.json:118/126) — "the launch-9 runtime observation that placed Blocks[0]
inside [0x970c000,0x9a34000)" — is a **conflation** of the mapping-class
observation with a pointer-value claim. That worker's own UNRESOLVED section
already required "one fresh runtime read of the qword at runtime 0x9765ed0";
none has happened. Static RE (FNamePool::Init → FMemory::Malloc(0x20000,2) →
GMalloc binned) actually predicts OS-heap placement, i.e. outside the image —
one more reason the region figure must not be treated as a captured pointer.

## Labeling law applied

The pool validator's image-resident fixture (and any SeedTable/validator
predicate citing [0x970c000,0x9a34000) as a Blocks[0] home) remains
**candidate/synthetic**. Relabeling to 'observed' requires a future archived
in-game record: the `[Seeds] FName pool validated … Blocks[0] 0x<hex>` line
(ORACLE F14) or a `/data/modbridge-fc1pool-facts.jsonl` `blocks0` kv, with
build id + session provenance. Note the armed validator's own heap window is
the separate, also-mapping-derived vintage span [0x8018c000,0x100118000)
(SEED-TABLE ST:13-14, "identity and ABI unverified") — same evidence class,
same law.

## Deliverables

- `CLAIM.json` — verdict + H1–H9 per-hit provenance (values, files, byte
  offsets, invocations, region membership).
- `PROVENANCE.md` — full search coverage table (9 corpora, markers, results),
  mapping-class record table, relabeling condition.

## Method note

Read-only: byte-level greps (incl. case-insensitive) over all jsonl/log mirrors
under `outputs/`, `.superpowers/native-observations/**`, `work/`, `docs/`;
record-level JSON parsing of the probe results to enumerate every
build/exp pair actually captured (igf3 vmmap-only; igf4 GUA/SCO/Tick anchors;
igf5/igf6 entry+startup+mapscan only); source inspection of the only two
capture paths; receipts cross-check (LAUNCH9-RESULT.md, RUN-RECEIPT.md,
SEED-TABLE.json, fc1pool REPORT.md, README-RETURN.md). No console contact, no
writes outside this worker directory.
