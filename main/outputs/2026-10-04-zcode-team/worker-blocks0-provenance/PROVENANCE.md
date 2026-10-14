# PROVENANCE — was FName-pool Blocks[0] ever CAPTURED?

Worker: worker-blocks0-provenance · 2026-10-04/05 · LOCAL ONLY, read-only analysis.
Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`

## Verdict

**NO CAPTURE EXISTS.**

No pinned observation anywhere in the workspace records a Blocks[0] pointer value
read at runtime from the FName pool (qword at `0x9765ec0+0x10` = `0x9765ed0`).
The figure **[0x970c000,0x9a34000)** is a **VMMAP mapping-class observation** —
the kernel `kinfo_vmentry` record for the module image's RW/BSS-tail region that
*contains* the pool struct (`0x9765ec0`) and the GUA anchor (`0x97ba110`) —
captured by the igf3/igf4/igf7 in-game facts probes on 2026-10-04. It is a
property of the address-space map, not a loaded value.

Therefore, per the user's labeling law: **the pool validator's image-resident
fixture stays `candidate/synthetic`** until a real in-game invocation produces a
captured Blocks[0] u64.

## Where the figure actually comes from (mapping class)

| Record | Build / invocation | What was captured |
|---|---|---|
| `anchor-validation-results.jsonl:67` (byte ~82033) — `v1_vmmap_page` seq 4 | `igf3-de0c0cb644d3`, in-game 2026-10-04 | parsed VMMAP page: entry `start=0x970c000 end=0x9a34000 prot=3` |
| `anchor-validation-results.jsonl:1094` — `v1_vmmap_raw` seq 1031 chunk 3 rec 6 | `igf3-de0c0cb644d3` | raw 136-byte `kinfo_vmentry` header: `w[1]=0x970c000 w[2]=0x9a34000` |
| `anchor-validation-results.jsonl:1105` — `gua_read` seq 6 | `igf4-4891869337b4` | 96-byte data read at `rt_addr=0x97ba110`; `span_start=0x970c000 span_end=0x9a34000` is the **containing span** metadata. The real captured data read here was of the **GUObjectArray**, never the pool |
| `igf6-results.jsonl:1123` — `mapspans` s5 | `igf7-3dd949879a0a` | coalesced span string `0x970c000-0x9a34000:3` |
| mirrors: `igf6-results.jsonl:67/1094/1105`, `ingame-v3-full-results.jsonl:67/1094`, `c2-alternative-results.jsonl:67/1094/1105/1123` | same records, accumulating mirrors | identical mapping-class rows |

## Every literal "Blocks[0] 0x…" value in the tree is synthetic

| Value | Where | Why synthetic |
|---|---|---|
| `0x80234ae0` | worker-verdict-rehearsal `runs/{A,E}*` (line 47–53; byte offsets 3087/3289) and `fixtures/{A,E}/from-dir/UE4SS.log` (byte 8547) | RUN-RECEIPT.md header: *"synthetic launch-10 evidence … no console contact; no real verdict advanced"*; *"Predicted runtime values per task: … pool 0x9765ec0"* — a hand-picked value inside the vintage heap window, rendered to exercise the evidence classifier |
| `Blocks[0] 0x0`, `CurrentBlock 0` | rehearsal fixture set B (ORACLE F15 verbatim) | fixture wording "observed" = the **future** v2 refusal format (UI:846-847), not a past read |
| `0x300020000` | worker-fname-heap-runtime-bounds REPORT.md:47 (test A8) | declared "Required positive test … of a realistic walk" — 52/52 host fixture, span `[0x300000000,0x380000000)` is fabricated |
| `blocks0` kv rows | worker-carrier-probe-build `tests/build/tmp/*.jsonl` | host harness, `build_id:"cc1-host"`, `gua_rt:"0x110200000"` fabricated |
| `0x<hex>` placeholders | ORACLE.md F14/F15, CLAIM.json, source format strings (UnrealInitializer.cpp UI:827/846-850/869; SeedValidation.hpp) | patterns/templates, zero runtime instances |

## Why no invocation could have captured it

1. **The only producer of a captured Blocks[0] is the initializer's Phase-3 pool
   chain** (`read_pool_words` → shape check → entry reads → print at UI:827/868)
   or the standalone fc1pool probe (`ig_kv_hex "blocks0"`, driver.c:583).
2. **launch-9 (2026-10-05, pid 273)** — the initializer was never reached: entry
   geometry refusal (`no observed mapping interval intersects the module image
   extent with valid geometry`) was terminal. Verified: 0 occurrences of
   `Seeds`/`fname`/`CurrentBlock`/`Blocks[` in all four launch9-evidence files
   (aligned-entry-log-snapshot.log 2,343,756 B; UE4SS-log-final.txt;
   xwp-jsonl-final.jsonl; runtime-status-final.txt). LAUNCH9-RESULT.md: the
   Shape-A seeds stop "was never reached — both refusals happen upstream of the
   initializer".
3. **fc1pool** (`modbridge_fc1pool.prx`, build `fc1pool1-78508a2e2940`) — built,
   **not injected** in launch-9 ("fc1pool / qh_driver / observer / carrier — NO
   (0B), not injected; produced no lines").
4. **igf6 FC-1 module** (`igf6-fc1prep-204a6d60a4ed`) — built, "none yet in-game
   … BUILT but NOT executed" (SEED-TABLE.json slot 2). igf6's own records
   (`igf6-ad9bda1fc50a`) stop at entry/startup/mapscan.
5. **launch-10** — not run (README-RETURN.md: "next milestone is the launch-10
   rebuild", "HOLD on any staging"; only prep baselines + build candidates exist).
6. Static-RE workers (worker-re-fname-pool, worker-fname-ctor-audit) analyzed the
   decrypted eboot only and say so: "nothing has been read or called on target";
   "Needs one fresh runtime read of the qword at runtime 0x9765ed0".

## Search coverage table

| # | Corpus | Markers searched | Result |
|---|---|---|---|
| 1 | launch9-evidence: aligned-entry-log-snapshot.log, UE4SS-log-final.txt, xwp-jsonl-final.jsonl, runtime-status-final.txt | `Seeds`, `fname` (ci), `CurrentBlock`, `Blocks[`, `Blocks[0] 0x` | 0 matches in all four files — seed phase never ran |
| 2 | console jsonl (12 files): igf6-results, anchor-validation-results, ingame-v1/v2/v3, c2-alternative, livefacts-probe-result, allfam-dll-console-result, mutex-console-result, 6d295f56/1a6b37b7 console results | `blocks0`, `Blocks[0]`, `fname_pool`, `pool_blocks`, `current_block`, `0x9765e(cc0/d0)`, `none_id`, `byte_property`, `pool` | 0 matches anywhere (incl. case-insensitive `pool`: 0 in igf6/ingame-v3/anchor/c2) |
| 3 | `.superpowers/native-observations/**` (411 entries) | ingame-facts/livefacts mirrors, `9765ec0`, `Blocks[0]` | no mirrors of the facts jsonl present; no pool records |
| 4 | all UE4SS.log mirrors (20 files found via `find`) | `[Seeds] FName pool validated` | only synthetic fixture copies (worker-evidence-tools-final, worker-launch10-evidence-upgrade fixtures) + launch9 final (negative) |
| 5 | whole-tree grep for `Blocks[0] 0x` (outputs/, .superpowers/, work/, docs/) | literal captured-value pattern | only rehearsal fixtures + source format strings + ORACLE placeholders — no runtime artifact |
| 6 | whole-tree grep for `FName pool validated` | success-line pattern | source variants (8 staging trees), ORACLE/CLAIM/ACCEPTANCE docs, rehearsal fixtures — no fetched log |
| 7 | worker dirs: fc1-validator-probe, fname-runtime-probe, carrier-probe-build, re-fname-pool, fname-ctor-audit, fname-heap-runtime-bounds, fname-validator-v4, fname-fc1-materialize/prep, seed-table-derivation | build receipts, host tests, provenance | all builds host-only; explicit "NOT executed"/"no console contact" statements |
| 8 | launch-10 state: README-RETURN.md, LAUNCH-DAY-10.md, launch10-prep, launch10-build, ORACLE.md F14/F15 | run state + future capture formats | launch-10 not executed; F14/F15 are expected-value templates |
| 9 | SEED-TABLE.json + gen_seed_table.py + SeedTable.h | heap constants provenance | `kHeapLo/Hi = [0x8018c000,0x100118000)` = igf6-VMMAP-derived *heap-class* window (separate from the image-span figure), "identity and ABI unverified"; pool label 0x9365ec0 is operand-derived, "nothing read on target" |

**Not searched / out of scope:** the PS5 console itself (local-only mandate);
`.prx`/ELF binary interiors (embedding a capture would still require a fetched
record to be evidence — none exists); `outputs/ps5-mod-bridge/` engine sources
(read-path only, no capture artifacts); game save data.

## What a capture would look like (relabeling condition)

A fetched, archived record of either:
- UE4SS.log line `[Seeds] FName pool validated at 0x9765ec0 (CurrentBlock <n>,
  Blocks[0] 0x<hex>; 'None' id 0x<id>, 'ByteProperty' id 0x<id>)` (initializer
  UI:827/868, ORACLE F14), or
- `/data/modbridge-fc1pool-facts.jsonl` record with the `blocks0` kv
  (fc1pool driver, ORACLE D3 `byteprop_id_for_find_crosscheck` sibling),

with build id + session provenance. Until then: **candidate/synthetic**.
