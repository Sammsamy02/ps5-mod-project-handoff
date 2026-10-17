# STAGING-PROPOSAL — launch-11 SECONDARY path: row-0-confirmed xwp probe (2026-10-05)

Worker: worker-igw-row0-rebuild · PROPOSAL ONLY. The controller executes every console
step; the launch stays USER-COORDINATED and must NEVER overlap launch-10's window.
No console contact was made; every console constant below is [TARGET] until the
Step-0 RETR and post-STOR readbacks prove it.

## 0. What this proposal stages

The rebuilt ARMED probe (this dir, `modbridge_exec_write_probe_armed.prx`):

| constant | value |
|---|---|
| build id | `igd1-e030a09eb4d1` |
| fself sha256 | `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df` (20013 B) |
| bare-sdk sha256 | `cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9` (69464 B) |
| SHT-strip readback | 67832 B, body sha (64..end) `e3f13d1994a1b4d27de05e31885d5a5cf9ea91daea3c57c2c229afd3696c295e` |
| table | 6 rows; ONLY row 0 (IGW-1 T2: `0x7878000`, 4→5, 1 page) `confirmed=1`; rows 1-5 `confirmed=0` |
| budget | 8 (T2 costs 1-2 of 8; rows 1-5 consume ZERO — they skip `outcome=2`) |

Delta vs the launch-9 probe is bounded at exactly ONE semantic byte (row 0 `confirmed`
field) plus build-id churn — see BINARY-DELTA.md. The probe does NOT replace the loaded
igf7 composition; it is a separate file; the ini line decides loading (the frozen
Packager still embeds internal moduleName `modbridge_ingame_facts` — known caveat,
unchanged from launch-9, which loaded and ran fine under it).

## 1. The probe-loading ini — two law-shaped options, ONE recommendation

**Config law (files-first-config-last, worker-staging-order):** upload + verify every
required file FIRST; publish the enabling configuration LAST; pre-publish presence
guard over every enabled entry; console `.bak` FIRST; byte-exact RETR readback; ONE
bounded restore on failure. The launch-9 incident (enabled lines with missing files)
is the law's reason to exist.

### Option A (RECOMMENDED — passes the layout law as it stands today): restore the preserved 9-entry canonical

- Bytes: `outputs/2026-10-04-zcode-controller/launch9-evidence/ploader-ini-9entry-restore.txt`
  — 440 B, sha256 `fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507`.
- Verified TODAY against the current layout owner's tool:
  `python3 worker-final-ini-layout/verify_final_ini_layout.py <file>` →
  `FINAL-INI-OK: byte-exact canonical 9-entry layout; 8 active entries in definitive
  order (carrier last, after igf=0); igf=0 skipped; 2/10 slot headroom.` (rc=0)
- Content (xwp-armed FIRST — exactly the ordering the FINAL-INI-LAYOUT §2 rationale
  pins: the probe's verdict records exist BEFORE the composition's `module_start`):

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

- Obligations it carries (honest cost): ALL 8 enabled (`= 2`) entries' files must be
  present+non-empty on console before the ini publish (the §3 guard enforces this —
  this is what makes it safe). That means this option stages the full launch-9
  ridealong set again (fc1pool, qh_driver, observer, carrier, aligned_entry,
  path_test, composition stay/refresh), not just the probe. It is the SAME byte
  config launch-9 ran (proven shape; `fd9c7393` is a preserved canonical, not a new
  invention), so it needs no new pinned-bytes decision. NOTE: this ini would stage
  the LAUNCH-9 probe build under that line unless the new PRX is uploaded first —
  the file NAME is the same, so files-first means: upload MY `e1541af6…` PRX to
  `/data/plugins/modbridge_exec_write_probe_armed.prx` (distinct new artifact, new
  id — never a re-STOR of launch-9 bytes), readback-verify, then publish.

### Option B (recorded alternative — NOT law-ready today): NEW minimal probe ini

A new distinct config adding ONLY the xwp line to the current 2-plugin shape
(current console `ploader.ini` = `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`,
supplier_composition + path_test enabled). TARGET bytes (content-anchored ensure:
xwp inserts immediately after the header per FINAL-INI-LAYOUT §3.3 — first entry,
before composition):

```ini
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 2
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_path_test.prx = 2
```

162 B, sha256 `0bfa59a633de706d20f9c4a42b9741ac8de4748d868a325fbe9d4754a51ee672`
(host-computed TARGET only). HONEST CAVEAT: `verify_final_ini_layout.py` pins
byte-exact canonical bytes and will REJECT these 162 B — correctly, for the law it
pins. Option B therefore REQUIRES the layout owner's NEW pinned-bytes decision + a
new worker/id before staging (never an in-place edit of `1216aab6…`, which launch-10
freezes byte-exact). Smaller blast radius (2 files to verify, not 8), but not free.

**Recommendation: Option A** — it is the proven, law-passing, preserved-bytes path;
Option B is viable only after the layout owner pins it.

## 2. Staging order (controller executes; user launches)

1. GATE: launch-10 window fully closed; its frozen byte-equality pre/post state
   recorded and undisturbed. If launch-10 has not terminated its flow: DO NOT stage.
2. Step-0 RETR pre-image of `/data/plugins/ploader.ini` + sha; console `.bak` STOR
   (distinct TAG; `1216aab6…` is the expected pre-image — any divergence = STOP +
   reconcile, never blind overwrite).
3. FILES FIRST: STOR the new probe fself to the FINAL name
   `/data/plugins/modbridge_exec_write_probe_armed.prx` (BINARY; distinct file — the
   launch-9 bytes there are superseded history); fresh RETR readback; verify with the
   exact STAGE-PLAN-COMBINED §3 snippet against `e1541af6…`/`cc544d6e…`/67832/`e3f13d19…`.
   Any FAIL → ONE re-STOR from the verified local fself, re-verify; second failure →
   abort staging (console keeps its prior state) + reconcile.
   (Option A only: also upload/verify the other enabled ridealong files per their own
   owners' constants — presence guard will refuse otherwise.)
4. CONFIG LAST: run the pre-publish presence guard over every enabled entry of the
   new ini; only then STOR the ini (Option A: the `fd9c7393…` bytes verbatim) +
   byte-exact RETR diff. On any failure: bounded restore of the `1216aab6…` `.bak`
   (the standing single-restore law) and stop.
5. USER launches the game manually (never automated). No claims from this worker
   about what the launch will show beyond the pinned mechanics below.

## 3. Expected evidence (pinned mechanics, not predictions of outcomes)

The probe runs unconditionally at `module_start` (`module_entry.c:127`), one-shot,
records flushed synchronously to `/data/modbridge-exec-write-probe.jsonl`
(+ best-effort UE4SS.log mirror). Expected record grammar per the machinery and the
host-tested behavior:

- seq 1 `entry`: `armed=1, table_rows=6, build=igd1-e030a09eb4d1`.
- seq 2 `startup`: `attempt_budget=8`, channels open.
- Row 0 record chain (the ONLY row that can pass the confirmed gate):
  - `row`: `"confirmed":1, addr 0x7878000, from 0x4, to 0x5` (from the table, verbatim);
  - fresh VMMAP scan covers `[0x7878000, +0x1000)`; `vmmap_persist` with
    `span_prot=0x4` (pre-state proof);
  - `attempt`: `mprotect_route=sceKernelMprotect_import`, rc + errno — THE number;
  - `rw_test`: read test (`read_done=1`; `write_tested=0` — to_prot 0x5 has no W bit);
  - `restore`: ALWAYS attempted back to 0x4 (`outcome=10` on success);
  - `post_restore`: fresh VMMAP re-proof (`restored_matches_from_prot=1`).
- Rows 1-5: `row` records with `"confirmed":0` + `refusal` `outcome=2`
  (`XWP_SKIP_NOT_CONFIRMED`), `no_memory_touched=1`, zero attempts.
- `summary`: `rows_seen=6`; success path expects `rows_attempted=1`,
  `attempts_used=1`, `rows_skipped=5`, `armed=1`,
  `mprotect_route="sceKernelMprotect_import"`.

### Verdict shapes

- **T2 SUCCEEDS** (sceKernelMprotect flips X→RX on the text page): attempt `rc==0`,
  `outcome=8 (XWP_ATTEMPT_OK)`, rw_test read proves the page now READS as RX-mapped,
  restore + post_restore re-prove 0x4 → **`W_WORKS` verdict shape** (exec-writability
  via the imported mprotect route is REAL on this firmware/composition).
- **T2 REFUSED by the kernel** (mprotect rc<0): `outcome=7
  (XWP_ATTEMPT_MPROTECT_FAIL)` with errno recorded, restore still re-proves 0x4 →
  **`W_FAILS_R_WORKS`** (reads work, exec page protection is kernel-enforced against
  change; the R-half of the design is confirmed, the W-half is not).
- **Fail-closed refusals** (recorded honestly either way, ZERO risk): if the fresh
  VMMAP does not cover `0x7878000` or its prot ≠ 0x4 → `XWP_REFUSE_NOT_COVERED (5)` /
  `XWP_REFUSE_PROT_MISMATCH (6)`, `attempts_used=0` — the launch's map differs from
  the design pin; reconcile, never re-rate unchanged evidence.
- **ABSENT**: no xwp jsonl at all → the probe did not load (ini/file problem; the
  2-plugin ini SHA in the pre-image would prove which) — launch wasted for the
  verdict, no memory touched.

## 4. What each verdict unlocks (the decision table this rebuild exists for)

| verdict | unlocks | per |
|---|---|---|
| `W_WORKS` | walkguard ACTIVE fold becomes admissible: `RC_ASM_WALK_RX_GUARD` 0→1 single-define flip (pre-measured armed object `f5627427…`); exec-page risk answer = guard the walk site | worker-launch10-walkguard-prep REPORT (fold-1 admissible ONLY once the IGW verdict exists) |
| `W_FAILS_R_WORKS` | D-1-only route: exec fold stays 0; the vtable-swap design on the RELRO data page becomes the sole remaining mechanism — its own bounded decision (this table's rows 4/5 stay `confirmed=0`; a D-1 verdict would need its own design-designated confirmation, NEW id, never this build) | worker-launch10-walkguard-prep R3/R4 branch; IGW design D-1 |
| ABSENT / BOTH_FAIL / FATAL | fold-0 stays the only fold; no walkguard flip; reconcile before any further exec-page work | same REPORT lines 13-16, 112, 122 |

## 5. Honest limits

- Proposal-only: every console step above is the controller's; this worker made
  ZERO console contact and runs ZERO launches.
- `sceKernelMprotect` is STILL not console-proven (SharpProspero StubCatalog
  spelling; the loud provenance flag travels in the import contract). Option A's
  T2 chain is exactly the experiment that would prove or refuse it.
- All evidence claims here are host-side: determinism, gates, and binary diffs are
  proven; the jsonl shapes are the machinery's pinned grammar (host-tested with
  mocked mprotect), not console measurements.
- Timing/cost: T2 adds ~1.6 s stable+settle + ≤25 s per-plugin cap headroom
  (launch-9 already ran this module in position 1 under the same ini law);
  budget 8 with exactly 1-2 attempts consumed.
