# STAGE-PLAN — modbridge_fc1pool as another =2 ridealong for the comp6 session

Worker: worker-fc1-validator-probe, 2026-10-04. Proposal-only staging plan.
NO console contact was made; every console step requires the standing ritual
(game fully closed, Townfall closed on console, FTP up, ploader armed, USER
launches manually — never automated). This plan EXTENDS
`worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md` (the qdrv1+observer
combined staging, 6-entry layout); that worker's files were NOT edited. The
fc1pool line is appended AFTER the existing six, making 7 entries / 6
registered plugins — still inside every parser limit (`MAX_PLUGINS = 10`,
path <= 255, line 1024; pinned `ploader-cleanup-candidate-1/main.c` L26-29).

## 0. Frozen artifact (verify hashes before ANY upload; refuse on mismatch)

| Module | Console path | Bytes | sha256 |
|---|---|---|---|
| fc1pool1 (this worker) | `/data/plugins/modbridge_fc1pool.prx` | 22733 | `d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96` |

Build identity: `fc1pool1-78508a2e2940`, variant `fc1pool1-fname-pool-validator`.
Determinism double-build run1==run2 byte-identical (final PRX, inner ELF,
combined object, import contract — `build-receipt.json` `.determinism`).
Local checks PASSED this session:
`check_fc1pool_artifact.py` on determinism/run1 AND run2
(LOCAL_PACKAGE_CHECKS_PASSED, init/fini 1/1 == frozen lineage, SDK
0x11000000, 8-row decoded import exact equality, per-phdr segment equality,
module-name patch evidence, forbidden-symbol gate) and
`tests/run_host_checks.py` (8/8 scenarios, strict-JSON oracle; see
`receipts/host-test-receipt.txt`). Frozen lineage PRX
`modbridge_runtime_aligned_entry.prx` re-hashed == `3b998f09…` at build time.

Distinct identity: module `modbridge_fc1pool` (17 bytes; the Packager's
hardcoded `modbridge_ingame_facts` was length-preserving-patched to it
BEFORE finalize/make_fself; bare ELF carries >= 1 `modbridge_fc1pool`
string and 0 residual `modbridge_ingame_facts` — asserted by the checker).
Nothing is replaced: the PRX stages under its OWN name alongside the
composition (`modbridge_supplier_composition`), aligned-entry, path_test,
qdrv1, and observer.

## 1. ploader.ini — 7-entry layout (was 6)

Parser facts inherited from STAGE-PLAN-COMBINED sec 1 (pinned main.c):
only modes 0/1/2 valid; **0 = skipped at parse, consumes NO slot**;
2 = late (stable 1000 ms / min_mod 16 / settle 600 ms, per-plugin wait cap
25 s); registration order == file order; NO path dedupe — the edit must be
append-only and count-verified.

Reference shape (line ORDER of existing lines must follow the ACTUAL Step-0
RETR pre-image — the exact console bytes were never recorded host-side; this
is STAGE-PLAN-COMBINED sec 1 plus the ONE new append):

```ini
[PPSA33286]
/data/plugins/modbridge_runtime_aligned_entry.prx = 2
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 2
/data/plugins/modbridge_runtime_aligned_observer.prx = 2
/data/plugins/modbridge_fc1pool.prx = 2
```

7 entries, 6 registered (ingame_facts stays `= 0`, parser-inert) — 6/10,
2 slots free. The fc1pool line is APPENDED `= 2`, LAST, so every existing
module's startup ordering is unchanged and the one-shot validator runs last.
Registration-count check after the edit: registered must be exactly 6,
all mode 2 except ingame_facts (must NOT appear as registered).

Why `= 2` (late) is correct for THIS probe: the chain needs the engine's
FName pool initialized. The probe still handles an early load honestly:
`CurrentBlock==0` is classified TRANSIENT and retried inside a bounded grace
(8 polls x 1 s nanosleep, `pool_uninitialized_after_grace` at exhaustion) —
far inside the 25 s per-plugin wait cap. module_start ALWAYS returns 0
except the sink-open failure (-22); a refused validation never makes ploader
flag this plugin FAIL.

## 2. Step-0 preflight additions (on top of STAGE-PLAN-COMBINED sec 2)

1. RETR `/data/plugins/ploader.ini` pre-image; FAIL if a
   `modbridge_fc1pool` line is ALREADY present (duplicate hazard — ploader
   does not dedupe) or if any of the 6 expected existing lines is missing.
2. RETR stat of `/data/modbridge-fc1pool-facts.jsonl` IF PRESENT (owned
   evidence from an earlier window; the file is opened O_APPEND|O_CREAT so
   old lines persist harmlessly — record size+sha baseline, never delete).
3. RETR stat of `/data/plugins/UE4SS.log` (already in the combined plan) —
   the baseline for this launch's single `FC1POOL:` line.
4. On-console backup STOR `ploader.ini.<TAG>.bak` (as in the combined plan).

## 3. PRX upload + readback (SHT-strip law)

Same channel law as STAGE-PLAN-COMBINED sec 3 (readback = segment-covered
prefix of the inner bare-sdk ELF; exact-length + body-hash + Ehdr-exception).

| constant | value |
|---|---|
| inner (bare-sdk) sha256 | `54f8335e6ed476a64e5bc0d65741c83808e52f79b55631a0803a410bd2b3f8a2` (69408 B) |
| expected readback bytes (max p_offset+p_filesz) | **67848** |
| readback body sha256 (bytes 64..end) | `7e2dce27e415bea80c3100d74e6f58c9a2cd8697bc87e03b67226fba09646ed3` |

```sh
curl -s -T modbridge_fc1pool.prx "$FTPB/data/plugins/modbridge_fc1pool.prx"
curl -s "$FTPB/data/plugins/modbridge_fc1pool.prx" -o /tmp/<TAG>/fc1pool.readback
# then the STAGE-PLAN-COMBINED sec 3 snippet with
#   $BARE_SDK = worker-fc1-validator-probe/determinism/run1/bare-sdk/modbridge_fc1pool.prx
```

Expected `READBACK-OK`, size exactly 67848. Any FAIL -> STOP, re-STOR once
from the verified local fself, re-verify; second failure aborts the staging.

## 4. Ini edit + verify

Identical to STAGE-PLAN-COMBINED sec 4 (append-only from `.pre`, diff must
show EXACTLY the one appended line (+possible leading NL), simulate parse:
registered == 6, STOR + RETR diff byte-exact) with the line
`/data/plugins/modbridge_fc1pool.prx = 2`.

## 5. Interaction audit (delta against the combined plan's table)

| Path | Writers | Readers | Verdict |
|---|---|---|---|
| `/data/modbridge-fc1pool-facts.jsonl` | **fc1pool1 ONLY** (own `sceKernelOpen/Write/Close` trio, `O_APPEND\|O_CREAT\|O_NOFOLLOW` 0600, one `sceKernelWrite` per <1 KiB line; v3 separator-correct writer) | host-side review after capture | NEW file, single writer by construction; no other module touches this name |
| `/data/plugins/UE4SS.log` | UE4SS; path_test (2 lines); composition (failure paths only); **fc1pool1 (exactly ONE `FC1POOL:` line per launch, `O_WRONLY\|O_CREAT\|O_APPEND\|O_NOFOLLOW` 0644, single bounded append <= 256 B)** | observer (read-only stat+tail) | path_test precedent class (append-only writers vs read-only tail reader); one more bounded writer of the same class; partial-line races land in the documented observer truncation path |
| game memory | fc1pool1 READS ONLY: current-map-gated plain reads in the readable spans containing GUA 0x97ba110 and pool 0x9765ec0 | — | no writes, no mprotect, 0 `syscall` instructions (build-gated); read-read overlap with qdrv1/observer is inert |

Timing: one-shot synchronous module_start — anchor+pool chain completes in
one pass when the engine pool is up (sub-second typical), bounded worst case
8 s grace (8 x 1 s nanosleep) < 25 s ploader per-plugin cap. NO thread is
created (qdrv1's pthread rows are dropped from the import surface).
Pre-existing ploader quirk (fail mid-list retries the whole list) is
unchanged and mitigated exactly as in the combined plan (read every
notification line; any FAIL -> STOP).

## 6. What the probe does in-game (the validated chain)

Chain per `worker-fname-ctor-audit/FNAME-VERDICT.md` sec 5, executed as
plain READS via the igf6-proven span-gated class (fresh per-invocation
sysctl KERN_PROC_VMMAP scan backs EVERY dereference; controller correction
#1/#6 discipline):

1. **Anchor admission** (base proof): the pinned GUA window (runtime
   0x97ba110) must be covered by a readable span of the CURRENT map, then
   the qdrv1 96-byte header admission (rcodes 1-7, layout-A predicates incl.
   corrected GC invariant). base := 0x97ba110 - 0x93ba110 (expected 0x400000;
   a divergent base is reported, still validated, never guessed through).
2. **Pool header** (runtime pool = base + 0x9365ec0 = 0x9765ec0 when base
   matches): map-gated read of [pool, pool+0x20): CurrentBlock u32 @+0x8
   (>= 1 required; > 8192 = insane), CurrentByteOffset @+0xC (recorded),
   Blocks[0] u64 @+0x10 (nonzero + user-half canonical + covered by a
   readable span). Init guard byte @ pool-0x20 recorded (evidence only).
3. **Bootstrap ids**: map-gated read of [pool+0x14040, +0x14048):
   id_none (expected 0 — recorded as `id_none_is_zero_expected`),
   id_byteproperty; must be distinct.
4. **Entry resolution** per id: block = id>>16 (< CurrentBlock), off =
   id&0xFFFF; Blocks[block] pointer (map-gated 8-byte read, nonzero +
   canonical); entry = blockptr + (off<<1); map-gated 40-byte read; header
   u16 bit0 (bIsWide) must be 0; ASCII compare at entry+2 == "None\0" /
   "ByteProperty\0" exactly.
5. **Verdict**: `fc1pool_validated=1` iff all 11 checks pass. The
   `id_byteproperty` value is emitted as
   `byteprop_id_for_find_crosscheck` — the data-side reference the
   initializer's later behavioral `FNAME_Find("ByteProperty")` probe must
   equal (FNAME-VERDICT sec 5 option 1). The exec-side ctor checks
   (FC1-A/B) and the behavioral call remain with the initializer; this
   probe independently proves the POOL the initializer depends on.

Fail-open: hard refusals stop immediately (no retry, no replay); only
transient stages (map untrusted / anchor admission / CurrentBlock==0) may
wait inside the 8-poll grace; every refusal is a recorded terminal verdict
with a `reason_code`; module_start returns 0 regardless.

## 7. Expected evidence at session end (delta rows for the combined plan sec 6)

| File | Producer | Success signature |
|---|---|---|
| `/data/modbridge-fc1pool-facts.jsonl` | fc1pool1 (NEW lines, `build_id":"fc1pool1-78508a2e2940`) | PASS: seq1 `entry`, seq2 `startup`, seq3 `poll` (anchor_rcode 0, base_matches_observed_bias 1), seq4 `fc1pool_result` verdict `validated` checks 11/11, seq5 `fc1pool_entries` (both ascii_ok 1), seq6 `summary` (write_failures 0). Every line strict JSON < 1 KiB, envelope `marker=modbridge.ingamefacts, schema=1`. |
| `/data/plugins/UE4SS.log` | (+ fc1pool1 line) | exactly ONE new line this launch: `FC1POOL: build=fc1pool1-78508a2e2940 verdict=validated reason=none base=0x…400000 pool=0x…9765ec0 cb=<n> none=0 byteprop=<n> checks=11/11` |

### Expected record shapes

**PASS** (`fc1pool_result` fields): `verdict:"validated"`,
`reason_code:"none"`, `fc1pool_validated:1`, `map_trusted:1`,
`anchor_admitted:1`, `anchor_rcode:0`, `base_derived:"0x0000000000400000"`,
`base_matches_observed_bias:1`, `pool_runtime:"0x00000009765ec0"`,
`guard_byte_read:1` (guard_byte value recorded), `current_block:>=1`,
`blocks0_canonical:1`, `id_none:0`, `id_none_is_zero_expected:1`,
`id_byteproperty:<u32 nonzero>`,
`byteprop_id_for_find_crosscheck:<same u32>`, `checks_passed:11`,
`checks_total:11`, `ue4ss_log_line_written:1`. Companion
`fc1pool_entries`: `pool_window_prot:3` (RW span observed for the eboot),
`none_ascii_ok:1`, `byteproperty_ascii_ok:1`, plus raw
`bytes16_none`/`bytes16_byteproperty` hex and entry/blockptr addresses for
offline re-derivation.

**Each refusal** (`verdict:"refused"` — hard, terminal at first poll,
`polls_run` recorded):
- `pool_window_uncovered` / `pool_window_not_readable` — pool address not
  in a current readable span (base moved or layout wrong) → `blocks0:*`
  fields zero, anchor_admitted 1, DO NOT relaunch identical layout;
- `current_block_insane` — CurrentBlock > 8192 (header window misread —
  label wrong);
- `blocks0_invalid` / `blocks0_uncovered` — Blocks[0] zero/non-canonical/
  unmapped (pool label wrong or base moved);
- `handles_window_uncovered`, `bootstrap_ids_degenerate` (ids equal),
  `handle_block_index_oob` (id>>16 >= CurrentBlock);
- `block_pointer_invalid` / `block_pointer_window_uncovered`,
  `entry_window_uncovered`, `entry_wide_flag_set` (header bit0=1),
  `none_ascii_mismatch` / `byteproperty_ascii_mismatch` (string bytes differ
  — bytes16 hex carries the actual observed bytes).

**Grace refusals** (`verdict:"refused_after_grace"`, `polls_run:8`):
- `map_untrusted` — sysctl map never trustworthy (transient environment);
- `anchor_not_admitted` — GUA rcodes 1-7 persisted (recorded per envelope);
- `pool_uninitialized_after_grace` — CurrentBlock stayed 0 (pool init later
  than the window; a NEW launch window with the engine fully booted is the
  remedy, never a re-rating of unchanged evidence).

All shapes: `{"marker":"modbridge.ingamefacts","schema":1,
"build_id":"fc1pool1-…","seq":N,"exp":"…","rc":0,"errno":0,"fields":{…}}`.
The `modbridge.ingamefacts` marker is the v3 writer's lineage envelope brand
(byte-identical vendored writer); the fc1pool channel is disambiguated by
its OWN path and the `fc1pool1-` build_id prefix.

## 8. Rollback

1. Game closed, ploader NOT armed: flip the fc1pool line to `= 0` (one
   character edit) or restore `ploader.ini.<TAG>.bak`; RETR + byte-verify.
2. Optionally delete `/data/plugins/modbridge_fc1pool.prx` if discarding;
   the fc1pool JSONL and the UE4SS.log line are owned evidence — keep.
3. Never replay a failed activation without a fresh preflight and a distinct
   TAG; a refused verdict is terminal evidence — a revision needs NEW ids.

## 9. Honesty boundaries

Staging preparation only; no console contact, no in-game claims. The PRX is
statically verified (deterministic double-build + artifact checks) and its
read/record logic host-tested (8 scenarios against a synthetic arena) — the
raw byte-dereference class and the import surface are the console-proven
igf6/qdrv1 lineage, but THIS module has never executed on target. A
`validated` verdict evidences the POOL DATA at base+0x9365ec0 for that
launch only; it does NOT validate the FName ctor code candidate (FC1-A/B
stay with the initializer route) nor any FindFirstOf behavior.
