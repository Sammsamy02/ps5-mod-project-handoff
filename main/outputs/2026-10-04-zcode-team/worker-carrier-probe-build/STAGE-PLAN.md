# STAGE-PLAN — modbridge_carrier (cc1) as the 9th ini entry

Worker: worker-carrier-probe-build, 2026-10-04. Proposal-only staging plan.
NO console contact was made; every console step requires the standing ritual
(game fully closed, Townfall closed on console, FTP up, ploader armed, USER
launches manually — never automated). The probe spec is
`worker-r3-carrier-prep/CARRIER-CANDIDATES.md` §5-6 (cc1, fc1 → qdrv1 → cc1
lineage). This plan EXTENDS `worker-final-ini-layout/FINAL-INI-LAYOUT.md`
(the definitive 8-entry return-session law); that worker's files were NOT
edited. The carrier line is APPENDED at the NEXT FREE index after the
current tail (`modbridge_ingame_facts.prx = 0`), never editing any existing
line (CANDIDATES §6.4) — a NEW 9-entry layout with NEW pinned bytes under a
NEW worker/ID, exactly the "mode change is a NEW layout" discipline the
final-ini law requires for any byte change.

## 0. Frozen artifact (verify hashes before ANY upload; refuse on mismatch)

| Module | Console path | Bytes | sha256 |
|---|---|---|---|
| cc1 (this worker) | `/data/plugins/modbridge_carrier.prx` | 26845 | `d83410aa9069a3039bb4079892b707373ad5d993992b0ed0b55abb1f57730e52` |

Build identity: `cc1-0864d65c1baf`, variant `cc1-carrier-census-ridealong`.
Determinism double-build run1==run2 byte-identical (final PRX, inner ELF,
combined object, import contract — `build-receipt.json` `.determinism`).
Local checks PASSED this session:
`check_carrier_artifact.py` on determinism/run1 AND run2
(LOCAL_PACKAGE_CHECKS_PASSED; init/fini 1/1 == frozen lineage; SDK
0x11000000; 10-row decoded import EXACT equality; per-phdr segment
equality; module-name patch evidence `modbridge_carrier` with 0 residual
`modbridge_ingame_facts`; forbidden-symbol gate dynsym + combined object)
and `tests/run_host_checks.py` (24/24 scenarios, strict-JSONL oracle —
pass paths main/negative_clean/wide_only/budget plus EVERY refusal class of
carrier_driver.h incl. the four CANDIDATES §6.3 census codes; see
`receipts/host-test-receipt.txt`). Frozen lineage PRX
`modbridge_runtime_aligned_entry.prx` re-hashed == `3b998f09…` at build
time; vendored byte-identical files re-verified vs the fc1pool1 build (itself
verified vs qdrv1/igf5).

Distinct identity: module `modbridge_carrier` (17 bytes; the Packager's
hardcoded `modbridge_ingame_facts` was length-preserving-patched BEFORE
finalize/make_fself). Imports: TEN verbatim frozen-191-contract rows —
the igf 7 + nanosleep (fc1pool1 precedent) + pthread_create/pthread_detach
(qdrv1/observer one-thread precedent; pthread_self dropped — cc1 publishes
no thread-identity fold). NO mutex import of any kind; ZERO raw `syscall`
instructions (build-gated == 0); no mprotect; no hooks; no engine writes.

## 1. ploader.ini — 9-entry layout (was 8), 8 registered — the cap holds

Final-ini law count verified this session against the canonical
(`worker-final-ini-layout/ploader.ini.canonical`, FINAL-INI-OK, 8 entries /
7 registered) and against the NEW candidate bytes: with the carrier line
appended the layout is **9 entries total, 8 registered** — xwp, fc1,
composition, qdrv1 (qh_driver), observer, aligned_entry, path_test,
carrier — plus the parser-inert igf `= 0` line: **8 of MAX_PLUGINS=10,
headroom 2** (receipt `receipts/final-ini-count-check.txt`; pinned parser
law: modes 0/1/2, 0 = skipped consuming NO slot, registration order ==
file order, NO dedupe, `MAX_PLUGINS = 10`).

New pinned bytes (whole-file target after the ensure op; ASCII, LF,
trailing newline, 440 bytes, sha256
`fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507`):

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
/data/plugins/modbridge_carrier.prx = 2
```

HONEST VERIFIER NOTE: `worker-final-ini-layout/verify_final_ini_layout.py`
pins the 8-entry canonical and will print `FINAL-INI-FAIL` (UNKNOWN line +
order) on the 9-entry bytes — that is EXPECTED and CORRECT behavior for the
layout owner's tool; it re-confirmed this session on both files. Adopting
the 9-entry law is a NEW pinned-bytes decision for
worker-final-ini-layout / the controller; this worker ships the constants
and does NOT edit that worker's files. Until adoption, the count-law gate
for the carrier staging is `receipts/final-ini-count-check.txt` (registered
== exactly 8, order above, no duplicates) plus the STAGE-PLAN-COMBINED §4
diff law (the edit itself must still be a single appended line).

Why appended AFTER the igf `= 0` line (next free index, CANDIDATES §6.4):
cc1 is data-side-only and load-order independent (no symbol resolution
against any other module; qdrv1 §1 precedent); it must NOT displace any
existing line (never edit the composition line), and appending keeps every
existing module's startup ordering byte-identical. `= 2` (late) is the
single-mode law; the probe handles an early window honestly (map/anchor/
count/pool-uninitialized are TRANSIENT stages riding the bounded 30 x 4 s
window; terminal refusals are recorded, never retried). module_start ALWAYS
returns 0 except the sink-open failure (-22); a refused census never makes
ploader flag this plugin FAIL.

## 2. Step-0 preflight additions (on top of STAGE-PLAN-COMBINED sec 2)

1. RETR `/data/plugins/ploader.ini` pre-image; FAIL if a
   `modbridge_carrier` line is ALREADY present (duplicate hazard — ploader
   does not dedupe) or if any of the 8 expected existing lines is
   missing/mode-changed.
2. RETR stat of `/data/modbridge-carrier-facts.jsonl` IF PRESENT (owned
   evidence from an earlier window; opened O_APPEND|O_CREAT so old lines
   persist harmlessly — record size+sha baseline, never delete).
3. RETR stat of `/data/plugins/UE4SS.log` — the baseline for this launch's
   single `CC1:` line.
4. On-console backup STOR `ploader.ini.<TAG>.bak` (as in the combined plan).

## 3. PRX upload + readback (SHT-strip law)

Same channel law as STAGE-PLAN-COMBINED sec 3 (readback = segment-covered
prefix of the inner bare-sdk ELF; exact-length + body-hash + Ehdr-exception).

| constant | value |
|---|---|
| inner (bare-sdk) sha256 | `e72366c497d8ffa4a76f3f2f01bb898a0d41c2434813429be1408dc2c15a7f71` (73088 B) |
| expected readback bytes (max p_offset+p_filesz) | **71528** |
| readback body sha256 (bytes 64..end) | `1d17a27db4cea8182673360aef06ac2e47dd0eb81993b2226a181b3fd6d6e868` |

```sh
curl -s -T modbridge_carrier.prx "$FTPB/data/plugins/modbridge_carrier.prx"
curl -s "$FTPB/data/plugins/modbridge_carrier.prx" -o /tmp/<TAG>/carrier.readback
# then the STAGE-PLAN-COMBINED sec 3 snippet with
#   $BARE_SDK = worker-carrier-probe-build/determinism/run1/bare-sdk/modbridge_carrier.prx
```

Expected `READBACK-OK`, size exactly 71528. Any FAIL -> STOP, re-STOR once
from the verified local fself, re-verify; second failure aborts the staging.

## 4. Ini edit + verify

Content-anchored ensure (final-ini law §3.3 shape, ONE new op):

```
ensure("/data/plugins/modbridge_carrier.prx = 2"):
    duplicates > 0 -> STOP (reconcile)
    else -> insert immediately AFTER the line
            "/data/plugins/modbridge_ingame_facts.prx = 0"
```

Edit from `.pre`, diff must show EXACTLY the one appended line (+
possible leading NL); STOR + RETR diff byte-exact; parse-law gate:
registered == exactly 8 in the order of §1, igf must NOT appear as
registered. Launch expectation: exactly **8** `OK (<ms>): <filename>`
ploader notification lines, zero FAIL lines.

## 5. Interaction audit (delta against the combined plan's table)

| Path | Writers | Readers | Verdict |
|---|---|---|---|
| `/data/modbridge-carrier-facts.jsonl` | **cc1 ONLY** (own `sceKernelOpen/Write/Close` trio, `O_APPEND\|O_CREAT\|O_NOFOLLOW` 0600, one `sceKernelWrite` per <1 KiB line; v3 separator-correct writer) | host-side review after capture | NEW file, single writer by construction; NOT qdrv1's file; no other module touches this name |
| `/data/plugins/UE4SS.log` | UE4SS; path_test (2 lines); composition (failure paths only); fc1pool (ONE `FC1POOL:` line); **cc1 (exactly ONE `CC1:` line per launch, 0644 append-only <= 224 B)** | observer (read-only stat+tail) | path_test/fc1 precedent class (bounded append-only writers vs read-only tail reader); one more bounded writer of the same class |
| game memory | cc1 READS ONLY: current-map-gated plain reads in the readable spans containing GUA 0x97ba110 and the FNamePool derived from base+0x9365ec0 (header, bootstrap ids, Blocks[], entries) | — | no writes, no mprotect, no hooks, 0 `syscall` instructions (build-gated); read-read overlap with qdrv1/observer/fc1pool is inert |

Timing: ONE detached thread, 30 polls x 4 s (~2 min bounded window, qdrv1
cadence), then the thread closes the channel and exits. No mutex imports.
Rides any row R1-R9 including command-free rows (R6/R7/R8) since it stages
without recomposition, and runs in EVERY launch window including `T-A`
(CANDIDATES §6.1). Pre-existing ploader quirk (fail mid-list retries the
whole list) unchanged; read every notification line; any FAIL -> STOP.

## 6. What the probe does in-game

1. seq1 `entry` + seq2 `startup` (module_start; startup carries
   `polls:30, poll_s:4`).
2. seq3 `mapscan` context record (`trusted`, `pool_probe:"0x9365ec0"`,
   `current_block:0` pre-admission, gua_rt).
3. Per poll (30 x 4 s): fresh sysctl KERN_PROC_VMMAP scan, then the qdrv1
   GUA header-window admission (rcodes 1-7; layout-A predicates incl. the
   corrected GC invariant). Poll counters ride every `qh_counter` line:
   `refused_bindings_absent` (pre-admission polls, the wiring-lag analog),
   `refused_layout_gate_closed` (admitted but NumElements <= 10000 — the
   qdrv1 harness threshold).
4. Gate OPEN (admitted AND count > 10000): base := GUA_rt - 0x93ba110
   (expected 0x400000; divergent base still validated, flagged), pool :=
   base + 0x9365ec0 (= 0x9765ec0 when base matches). fc1's VALIDATED
   control chain: CurrentBlock >= 1 and <= 8192, Blocks[0] nonzero/
   canonical/covered, bootstrap ids at pool+0x14040/+0x14044 distinct,
   "None" / "ByteProperty" ASCII at entry+2 (wide bit0 = 0).
5. THE CENSUS (once, never re-rated): walk every block [0, CurrentBlock)
   entry-to-entry using the fc1/CANDIDATES header decode (bIsWide bit0,
   Len bits15:6; stride = 2+Len ASCII / 2+2*Len wide, in 2-byte offset
   units), budget-bounded at 1,048,576 entries, progress marker every
   4096, matching ONLY the fixed 53-name table (3 carrier candidates + 19
   walk class-strings + 27 ctor-family decoder controls + "None" /
   "ByteProperty" positive controls + "PostRender" /
   "ZZZNotARealFunctionQQ" negative controls — every name image-confirmed
   with a linked-label vaddr in carrier_driver.h). Each match emits ONE
   `census_match` line (name, table_index, kind, id, block, offset,
   block_ptr, entry_addr, bytes16). Wide entries are skipped and counted.
6. `census_result` verdict record: `CENSUS:VALIDATED` with
   `controls:{None_id_zero, ByteProperty_ascii_ok}`,
   `negative_hits:{PostRender, ZZZNotARealFunctionQQ}`, `matched_count`,
   `wide_entries_skipped`, `census_entries_scanned`,
   `census_progress_markers`, `census_truncated`, `budget_entries`; or
   `CENSUS:REFUSED` with a `reason_code`. Then the ONE `CC1:` UE4SS.log
   line and the `summary` record; remaining polls continue as `terminal`
   counters until the window closes (t+~2 min).

cc1 does NOT deliver FUNC_Native, invocation rate, or thread identity
(CANDIDATES §6.1) — those stay with the R3D §4.2 in-runtime calibration.

## 7. Expected evidence at session end

| File | Producer | Success signature |
|---|---|---|
| `/data/modbridge-carrier-facts.jsonl` | cc1 (NEW lines, `build_id":"cc1-0864d65c1baf`) | PASS: seq1 `entry`, seq2 `startup` (polls 30, poll_s 4), seq3 `mapscan`, 30 `qh_counter` lines (gate opens once admission + count > 10000), `census_match` lines for every matched name, `census_result` verdict `CENSUS:VALIDATED` (controls both 1, `PostRender` negative hit 0, `census_truncated` 0, `matched_count` >= 3 expected: ReceiveTick, Tick, BlueprintUpdateAnimation — live truth decides), `summary` (write_failures 0, polls_run 30). Every line strict JSON < 1 KiB, envelope `marker=modbridge.ingamefacts, schema=1`. |
| `/data/plugins/UE4SS.log` | (+ cc1 line) | exactly ONE new line this launch: `CC1: build=cc1-0864d65c1baf verdict=CENSUS:VALIDATED reason=none entries=<n> matches=<n>` |

### Expected record shapes

**PASS** (`census_result`): `verdict:"CENSUS:VALIDATED"`,
`reason_code:"none"`, `controls:{None_id_zero:1, ByteProperty_ascii_ok:1}`,
`negative_hits:{PostRender:0, ZZZNotARealFunctionQQ:0}`,
`matched_count:<n>`, `wide_entries_skipped:<n>`,
`census_entries_scanned:<n>`, `census_progress_markers:<n//4096>`,
`census_truncated:0`, `budget_entries:1048576`, plus
`base_matches_observed_bias:1`, `base_derived:"0x0000000000400000"`,
`pool_runtime:"0x00000009765ec0"`, `current_block:>=1`, `id_none:0`,
`id_byteproperty:<u32>`. Matched names ride as separate `census_match`
lines (split from census_result exactly like fc1's fc1pool_entries split,
to keep every line < 1 KiB). A `census_truncated:1` VALIDATED verdict is a
BOUNDED census: `budget_entries` + `census_entries_scanned` carry the real
coverage — never read it as full-pool coverage.

**Refusals** (`verdict:"CENSUS:REFUSED"`, terminal, never re-rated —
FC §7 discipline). CANDIDATES §6.3 codes:
`pool_window_uncovered` / `none_ascii_mismatch` /
`current_block_insane` / `pool_uninitialized_after_grace`; plus the
fc1-chain codes (`pool_window_not_readable`, `blocks0_invalid`,
`blocks0_uncovered`, `handles_window_uncovered`,
`bootstrap_ids_degenerate`, `handle_block_index_oob`,
`block_pointer_invalid`, `block_pointer_window_uncovered` (defensive),
`entry_window_uncovered`, `entry_wide_flag_set`,
`byteproperty_ascii_mismatch`) and the census-walk codes
(`census_block_pointer_invalid`, `census_entry_window_uncovered`).
Grace-class (the bounded window itself was the grace):
`map_untrusted`, `anchor_not_admitted`, `census_gate_closed_after_grace`
(admission or the 10000-count gate never opened — launch-window timing;
the remedy is a NEW staged window, never a re-rating of unchanged
evidence).

All shapes:
`{"marker":"modbridge.ingamefacts","schema":1,"build_id":"cc1-…","seq":N,
"exp":"…","rc":0,"errno":0,"fields":{…}}`. The `modbridge.ingamefacts`
marker is the v3 writer's lineage envelope brand (byte-identical vendored
writer); the cc1 channel is disambiguated by its OWN path and the `cc1-`
build_id prefix.

## 8. Rollback

1. Game closed, ploader NOT armed: flip the carrier line to `= 0` (one
   character edit) or remove the appended line / restore
   `ploader.ini.<TAG>.bak`; RETR + byte-verify.
2. Optionally delete `/data/plugins/modbridge_carrier.prx` if discarding;
   the cc1 JSONL and the UE4SS.log line are owned evidence — keep.
3. Never replay a failed activation without a fresh preflight and a
   distinct TAG; a refused verdict is terminal evidence — a revision needs
   NEW ids.

## 9. Honesty boundaries

Staging preparation only; no console contact, no in-game claims. The PRX is
statically verified (deterministic double-build + artifact checks) and its
read/record/census logic host-tested (24 scenarios against a synthetic
arena, strict-JSONL oracle) — the raw byte-dereference class and the import
surface are the console-proven igf6/qdrv1/fc1pool1 lineage, but THIS module
has never executed on target. A `CENSUS:VALIDATED` verdict evidences WHICH
of the 53 fixed names are interned in the LIVE FName pool for that launch
only (ASCII-encoding entries; wide-encoded names are counted, not
decodable against an ASCII table — recorded as `wide_entries_skipped`).
It does NOT evidence FUNC_Native, invocation rate, game-thread identity,
or any UFunction-object walk (open per CANDIDATES §7.2). The census
resolves the NAME truth F-1 needs; the R3D §4.2 calibration remains the
only channel that can close F-1 either way.
