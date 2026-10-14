# CAPTURE-CAPACITY.md — the >16,384 capture class (worker-capture-22k-fixture)

Status: staged proposal evidence, 2026-10-05. Host pure-function proof only; no
target run, no console contact. Header under test: the FOLDED v4
`final-units/SeedValidation.hpp`, staged byte-identical
(sha256 `f09ec9b3…a5de`); originals untouched. Suite: 34 passed, 0 failed
(exit 0), deterministic rerun — see `receipts/PASS-capture22k-fixtures.json`.

## 1. Real-run basis (worker-verdict-first-real, G7/G9)

| fact | value | source |
|---|---|---|
| run1 walk total | `bridge_vmentry_entries=22761` | classifier-outputs/run1-summary.txt |
| run1 stop reason | `bridge_vm_walk_stop=0` (clean parse — the cut is the SNAPSHOT capacity, not a malformed tail) | same |
| snapshot capacity | 16,384 records (`kBridgeVmmapEntries`, static bss) | runtime_start_bridge_r14_diag_spanpub.cpp:240 |
| run3 walk total | `bridge_vmentry_entries=10028` (complete; the production norm) | classifier-outputs/run3-summary.txt |
| publication window | `spanpub_cap=1` — saturated at 16 on BOTH boots | G9, REPORT.md |
| envelope diag | `bridge_spanpub_env_low/high` — the envelope as a 64-bit split | bridge :739-740 |

Two DIFFERENT counters exist and must never be conflated:
`bridge_vmentry_entries` counts EVERY record parsed from the sysctl payload
(variable-size `kve_structsize` walk); the snapshot stored for consumers holds
only the FIRST 16,384 of them (the address-ordered prefix). A >capacity boot is
therefore a **prefix capture**, exactly the shape the v4 envelope logic was
designed for — and until this fixture, the only exercised shapes were the
complete-capture fixture H (10,019 records) and the synthetic 3,405-cut H5.

## 2. The pipeline law (why a truncated capture is a prefix)

```
sysctl KERN_PROC_VMMAP
  -> g_bridge_vmmap[16384] raw kinfo_vmentry (~18.1 MiB static bss, bridge:240)
  -> bridge_vmmap_walk (variable-size records; stop 0=clean / 1=malformed / 2=tail)
     stores records while entries < consumer capacity  -> PREFIX in mappings[]
  -> diagnostics: bridge_vmentry_entries = ALL parsed; bridge_vm_walk_stop
  -> bridge_spanpub_publish(mappings, count): copies bounded = min(count, 16384)
  -> rc_seed::publish_readable_spans(prefix)  -> top-16 table + envelope_end
  -> initializer: capture_envelope = max(bridge_capture_envelope, max published end)
  -> rc_seed::check_pool_shape(..., capture_envelope)
```

Kernel VMMAP enumeration is address-ordered and records do not overlap, so a
truncated capture is a PREFIX:

- `envelope_end = max record end over the WHOLE captured prefix` = the cut.
- **omitted <=> at/above the envelope.** Every omitted record starts at or
  above the cut (fixture E4 asserts this on the first omitted record).
- A coverage miss BELOW the envelope is a genuine unmapped/unreadable verdict
  ("not covered … capture complete below the envelope").
- A miss AT/ABOVE the envelope must NEVER read as 'unmapped' — it refuses
  "snapshot truncated: cannot prove heap coverage at or past the captured
  envelope (omitted mapping, not proven unmapped)", bounds_source
  `capture-truncated`.

## 3. The fixture (host-test/test_capture22k_host.cpp)

One synthetic 22,761-record walk, address-ascending, bridge-parity truncation
(the filter receives ONLY the first 16,384 records):

| records | content |
|---|---|
| 0–7 | pinned image low map + vintage giant + JIT (v3/v4 suite numbers) |
| 8000 | below-cut heap reservation `[0x432300000,0x452300000)` 512 MiB R-- |
| 9000…15000 | 16 medium 4 MiB readable spans (all below the cut) |
| 17000 | above-cut spanA `[0x6cf000000,0x6df000000)` 256 MiB |
| 19000 | above-cut spanB `[0x76ba00000,0x773a00000)` 128 MiB |
| 22600 | above-cut spanC `[0x870c00000,0x890c00000)` 512 MiB — true heap top |
| between | 32 KiB sub-floor filler (never publishes) |

Prefix result (mirrors the real boots' saturated window): 22 qualifying spans,
top-16 kept, `cap_saturated=1`; below-cut heap survives the size contest.

### (a) below-cut blocks0 ADMITS

`blocks0 = 0x432320000` → ok, bounds_source `published-readable-spans`;
span boundaries lo-inclusive / hi−8 ok; END-TO-END shape + entry-0 'None'
anchor + entry bounds all pass (A1–A3). Coverage proof holds below the cut.

### (b) above-cut blocks0 refuses TRUNCATED, never 'unmapped'

| probe | blocks0 | verdict |
|---|---|---|
| spanA | 0x6cf020000 | "snapshot truncated …", capture-truncated (T1) |
| spanB | 0x76ba20000 | "snapshot truncated …", capture-truncated (T2) |
| spanC | 0x870c20000 | "snapshot truncated …", capture-truncated (T3) |
| == envelope | 0x6a39e8000 | truncation (at/above the cut, T4) |
| envelope−8 | 0x6a39e7ff8 | honest unmapped class — below the cut the capture is a complete prefix (T5) |

None of the above-cut probes ever produces the "not covered / unmapped" string.

### (c) envelope math on the real numbers (22,761 / 16,384 / 6,377)

- `envelope_end = 0x6a39e8000` = explicit max end over the captured prefix =
  last captured record's end (E1/E2) — **the cut, NOT the true heap top**.
- True walk end `0x89c008000` > envelope (E5); the cut sits strictly below
  every above-cut span (E3).
- First omitted record (index 16,384) starts at `0x6a3b00000` ≥ envelope (E4).
- Published table: no above-cut span present, every published span ends at or
  below the envelope (P7/P8) — the table reflects ONLY the first 16,384
  records.

### (d) complete-capture contrast = the capacity lever

Publishing all 22,761 records moves the envelope to the true walk end and the
SAME three above-cut blocks0 values ADMIT (C1/C2). The refusal is a
capture-coverage property of the 16,384 cut, not a property of the addresses.
(Complete capture still saturates the top-16 window: 25 qualifying, 16 kept,
`cap_saturated=1` — cap saturation is orthogonal to truncation, C3.)

## 4. Expected launch behavior on a >16,384 boot

Diagnostics (in order, single writer): `bridge_vmentry_entries=<N>total`,
`bridge_vm_walk_stop=0`, then `bridge_spanpub_count=16`,
`bridge_spanpub_total=<qualifying within the prefix>`, `bridge_spanpub_cap=1`,
`bridge_spanpub_env_low/env_high` = the cut as a 64-bit split
(low = envelope & 0xffffffff, high = envelope >> 32; G9 parity).

Pool admission:
- `Blocks[0]` below the cut and inside a published span → normal admission
  (the `[Seeds]` pool-validated path). This is the only admission case; being
  below the cut is necessary but not sufficient — the span must also survive
  the top-16 size window (documented v4 C5 cap limit; a below-cut blocks0 in a
  dropped span refuses "not covered", honestly but falsely — saturation is
  counted and diagnosed via `bridge_spanpub_total/cap`).
- `Blocks[0]` at/above the cut → `check_pool_shape` returns
  `snapshot truncated: cannot prove heap coverage at or past the captured
  envelope (omitted mapping, not proven unmapped)` on every 50 ms poll until
  the validation budget expires, then the terminal refusal throws:

```
[Seeds] REFUTED: FName pool at 0x<pool>: snapshot truncated: cannot prove heap
coverage at or past the captured envelope (omitted mapping, not proven
unmapped) (observed CurrentBlock <N>, Blocks[0] 0x<blocks0>; published
readable spans <N>). Terminal for this launch; derive afresh with new
evidence, never replay.
```

(Literal assembled from the folded unit: `seed_refused` wrapper +
`shape.failure` + observed-words suffix — final-units/UnrealInitializer.cpp
seed_refused :687 and the pool wait loop :940-960.) Module start fails; the
game continues; no frozen-envelope fallback ever fires. run3's 10,028 ≤ 16,384
is the production norm: complete capture, envelope past everything the walk
reached.

Caller-contract pin (D1): the header TRUSTS the envelope argument. A prefix
table with an inflated envelope would misread truncation as unmapped. The
shipped call site cannot inflate it: the initializer takes
max(bridge_capture_envelope, max published end) and the bridge reports the
whole-snapshot max record end.

## 5. For launch-12 (capacity lever + stack-vs-bss law)

If the truncated refusal fires live: it is honest-by-design — never replay it,
never loosen the validator to admit past the cut. The known lever is capacity:

1. First verify the pool worker's REQUIRED core hunk is actually folded in the
   launch-12 unit (worker-fname-heap-runtime-bounds DIFF.md §3,
   proposal-only at staging time): `kMaxVmMappings = 1024 → 16384`
   (live_module_facts.hpp:85) and `OsVmMapping mappings[kMaxVmMappings];` →
   `static OsVmMapping mappings[...]` (live_module_facts.cpp:203).
2. If 16,384 still truncates (the run1 shape: 22,761 total), the next lever is
   16,384 → 32,768 across all three capacity arrays.
3. **Stack-vs-bss law** (pool worker core hunk, verbatim intent: "stack would
   be unsafe on a game-hijacked thread"): capacity arrays MUST be
   process-lifetime static bss, never stack:
   - lmf snapshot side: 32,768 × 24 B `OsVmMapping` = **768 KiB BSS**
     (16,384 × 24 B = 384 KiB today);
   - bridge spanpub copy: `g_spanpub_records` 32,768 × 24 B
     `VmSpanRecord` = 768 KiB BSS (384 KiB today);
   - bridge raw buffer: `g_bridge_vmmap` 32,768 × 1,160 B `kinfo_vmentry`
     ≈ **36.3 MiB BSS** (≈ 18.1 MiB today, the r4 "~19 MiB" comment).
   BSS is zero-fill (no file-size cost) but residents when touched — check the
   PRX memory budget before folding.
4. Any bump is a NEW bounded revision with distinct IDs under the iteration
   law: new evidence, no resubmission of the identical refused launch, no
   re-rating of unchanged captures.

## 6. Honest limits

- Host pure-function proof only: no target run, no console, no dereference of
  Blocks[0]; the anchor read, entry-chain reads and behavioral probe remain
  target-side.
- The walk is SYNTHETIC. Real pinned facts: 22,761 / 16,384 / walk_stop=0 /
  10,028 / spanpub_cap=1. The address layout past the pinned image spans is
  plausible-not-captured; NO real Blocks[0] pointer value exists (labeling
  law, worker-blocks0-provenance). The suite asserts RELATIONS, not a
  predicted real map.
- 2 initial FAILs were fixture-side arithmetic (the 128 KiB tail record is
  exactly one block capacity and qualifies: totals 22/25, not 21/24). The
  validator classified correctly in both runs; fixed and rerun 34/34.
- Fixture numbers like `0x6a39e8000` are the synthetic cut; a real boot's cut
  differs. The durable facts are the relations: envelope = captured max end,
  omitted ⇔ at/above envelope, above-cut ⇒ truncation class.
