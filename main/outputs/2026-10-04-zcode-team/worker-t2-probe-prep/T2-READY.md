# T2-READY — complete preparation package for the T2 armed xwp console probe

Worker: worker-t2-probe-prep · 2026-10-05 · preparation only. ZERO console contact,
zero launches, zero game claims, zero shared-lineage edits. Everything below was
written only under `outputs/2026-10-04-zcode-team/worker-t2-probe-prep/`. Every
console step is executed by the CONTROLLER; the game launch is USER-owned (manual,
never automated). Every console constant stays [TARGET] until the Step-0 RETR
readbacks prove it.

Assembled from (all re-read/verified on disk today):
- `worker-igw-row0-rebuild/` — armed probe + `check_xwp_artifact.py` + `import-contract.json`
- `worker-probe-prep/PROBE-RUN-CARD.md` — the standing run card (route determination,
  ritual, record grammar, decision table); this file CROSS-CHECKS it and adds the 934 amendment
- `worker-ini-order/ploader.ini.retr-20261005-preimage` — the 1216aab6 pre-image bytes
- `worker-launch11-dry-compose/staging/asmhelper-guard/ASMHelper.guarded.cpp` — the staged fold unit
- `worker-fold1-preverify/receipts/` + `worker-launch10-walkguard-prep/REPORT.md` — fold measurements
- `worker-md-hedge/MD-HEDGE-PROPOSAL.md` §2.3 — the 934 cross-cutting flag (basis of §C below)

---

## A. ARTIFACT STATUS — all three pieces verified

### A.1 The armed xwp probe — READY

Local: `outputs/2026-10-04-zcode-team/worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx`

| check | expected | measured by this worker | verdict |
|---|---|---|---|
| fself sha256 | `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df` | same (shasum -a 256) | PASS |
| fself size | 20013 B | 20013 B | PASS |
| bare-sdk inner sha256 | `cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9` (69464 B) | checker `inner_elf_sha256` | PASS |
| build id | `igd1-e030a09eb4d1` | checker receipt | PASS |
| checker full gate run | all PASS | re-run today → receipt saved at `worker-t2-probe-prep/artifact-check-receipt.json` | PASS |

Checker gate detail (from the saved receipt): wrapper magic OK, 6 segment copies
verified, init/fini arrays 1/1, **exactly 8 strong NID-encoded imports / 0 weak**,
`syscall_instructions_combined: 0`, `syscall_byte_pair_in_text: false`,
`armed_typed_reference_ok: true`, `mprotect_route: "sceKernelMprotect_import"`.

8-import contract (`determinism/run-armed-1/import-contract.json`): 7 verbatim
console-proven rows (`__error`, `sceKernelOpen`, `sceKernelWrite`, `sceKernelClose`,
`sysctl`, `getpid`, `clock_gettime`) PLUS exactly ONE derived row —
**`sceKernelMprotect` (libkernel, GLOBAL/FUNC)** with provenance
`"derived:SharpProspero-StubCatalog (NOT console-proven; LOUD FLAG)"`. The same
contract pins `raw_syscall_route: "FORBIDDEN (igf5 crash class; inverted gate)"` and
`packager_rejects_weak_imports: true`. Expected console RETR readback of the STOR'd
file: 67832 B, body sha (bytes 64..end) `e3f13d1994a1b4d27de05e31885d5a5cf9ea91daea3c57c2c229afd3696c295e`
(run card §3a — supersedes the older `daea2ff4…` constant).

### A.2 The ini variant — BUILT AND STAGED

Local staged file: `outputs/2026-10-04-zcode-team/worker-t2-probe-prep/ploader.ini.xwp-enabled`

| property | value |
|---|---|
| source pre-image | `worker-ini-order/ploader.ini.retr-20261005-preimage` (== `worker-boot-protocol/probe/console-ploader.ini`, == the l10 `.pre` copies), sha256 `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`, 440 B |
| edit | ONE byte: byte offset 65 (cmp position 66), `0x30`→`0x32` — line 2 `/data/plugins/modbridge_exec_write_probe_armed.prx = 0` → `= 2` |
| variant sha256 | **`acc6b4f5ab7ba163e9229d672101467146b0690737197fdcc1efaedd43bb9495`** |
| variant size | 440 B (unchanged), LF endings, trailing newline |
| enabled entries after edit | 3: `exec_write_probe_armed` (2), `supplier_composition` (2), `path_test` (2) — everything else `= 0` |

Verbatim variant bytes:

```ini
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 2
/data/plugins/modbridge_fc1pool.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 0
/data/plugins/modbridge_runtime_aligned_observer.prx = 0
/data/plugins/modbridge_runtime_aligned_entry.prx = 0
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_carrier.prx = 0
```

Honest note for the controller: `worker-probe-prep/PROBE-RUN-CARD.md` §4 recommends
the Option A 9-entry ini (`fd9c7393…`, 8 enabled) for the T2 window. The minimal
`acc6b4f5…` variant requested by this task differs from Option A ONLY in the six
ridealong lines (kept `= 0`), which shrinks the §3c presence-guard obligation from 8
files to 3 and the expected ploader notification shape from 8 OK to 3 OK. The probe
stays at ini position 2 either way, so its records still land first (~first 2–3 s of
injection). The variant choice is controller-owned; BOTH byte shapes are staged and
hash-pinned, and the T2 verdict logic is identical under both.

Presence-guard obligation for the minimal variant (BEFORE publishing the ini):
`modbridge_exec_write_probe_armed.prx` (upload this window, §B.2),
`modbridge_supplier_composition.prx` + `modbridge_path_test.prx` (launch-10-frozen
pair — RETR-stat presence + sha vs owner pins; refresh only if divergent, from their
owners' staged artifacts, with SHT-strip readback).

### A.3 Ini flip readiness — CONFIRMED

The pre-image line 2 is exactly `/data/plugins/modbridge_exec_write_probe_armed.prx = 0`
(unique match in the file) — the `=0` → `=2` flip is a well-defined single-byte edit,
already applied in the staged variant. On console the flip is done by REPLACING the
file via STOR of the 440-B variant (never an in-place console edit), with the Step-0
pre-image + `.bak` ritual of §B.3.

---

## B. T2 CONSOLE WINDOW — step-by-step (controller executes, user launches)

### B.1 Pre-flight gates (all stop-on-mismatch)

1. G-1/G-2/G-3 of the run card: launch-10 window fully closed with its frozen
   pre/post state undisturbed; Townfall fully closed; FTP up (`ps5-host-not-configured.invalid:2121`);
   ploader armed; fresh distinct session `<TAG>`.
2. Re-hash the two local artifacts NOW: probe fself `e1541af6…` (20013 B), ini
   variant `acc6b4f5…` (440 B) [or Option A `fd9c7393…` if the controller picks it].
   Any mismatch → STOP, re-stage locally, never upload divergent bytes.

### B.2 Files first (SHT-strip law: upload → readback → verify, in this order)

1. STOR BINARY the probe to the FINAL name
   `/data/plugins/modbridge_exec_write_probe_armed.prx` from the verified local fself.
2. RETR readback; expect 67832 B, body sha `e3f13d19…`; verify the header-exception
   law against the `cc544d6e…` bare ELF (only `e_shoff` + section-table fields may
   differ; embedded `e_shnum == 0`). FAIL → ONE re-STOR, re-verify; second FAIL →
   abort staging + reconcile.
3. Presence-guard the enabled set (3 files for the minimal variant; 8 for Option A):
   every enabled entry exists, non-empty, sha per pins. Any miss → upload+verify that
   file FIRST. Never publish an ini that enables a missing file.

### B.3 Config last (ini publish)

1. STEP-0 pre-image: RETR `/data/plugins/ploader.ini` → record sha/size/lines/last byte.
   **Expected `1216aab6…` (440 B). Any divergence = STOP + reconcile, never blind overwrite.**
2. Console `.bak` FIRST: STOR the untouched pre-image to
   `/data/plugins/ploader.ini.<TAG>.bak`; RETR + byte-verify == pre-image.
3. STOR the variant (`acc6b4f5…`, or Option A `fd9c7393…`) → fresh RETR → `diff`
   byte-exact vs local. ANY failure → ONE bounded restore of the `1216aab6…` `.bak`,
   verify, stop.

### B.4 User launch (USER-owned, by hand)

1. Controller: "arm ploader" → user confirms.
2. Controller: "launch Townfall manually" → USER starts the game from the console UI.
   Never automated, never remote-triggered.
3. Read every ploader notification line. Expected for the minimal variant: **3 OK
   lines (`OK (<ms>): <filename>`), 0 FAIL** (probe first; `= 0` entries are skipped
   silently). For Option A: 8 OK / 0 FAIL. Any FAIL → STOP; triage per
   STAGE-PLAN-COMBINED §5/§7; do not relaunch the identical layout.
4. The probe is one-shot at `module_start`, synchronous flush via `sceKernelWrite`,
   fail-open, re-entry-guarded — T2 evidence is complete ≈ 15–40 s after launch.
   Budget the user window ≈ 5 minutes launch-to-collection.

### B.5 Evidence to pull

| # | console file | role |
|---|---|---|
| 1 | `/data/modbridge-exec-write-probe.jsonl` | PRIMARY — the 19-record chain; everything past the Step-0 byte cut |
| 2 | `/data/plugins/UE4SS.log` | best-effort mirror of every record + ridealong freshness lines |
| 3 | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | composition context: session/pid stamp proves a fresh window |
| 4 | ploader notification text | the OK/FAIL shape |

Tag the fetched set `<TAG>` + build `igd1-e030a09eb4d1`. Ledger tags per run card §6:
`XWP2-STOR`, `XWP2-READBACK`, `XWP2-PRESENCE`, `INI-PRE`, `INI-BAK`, `INI-STOR`,
`INI-READBACK`, `T2-LAUNCH`, `T2-JSONL`, `T2-MIRROR`.

### B.6 The rc record chain (expected grammar — 19 records)

seq1 `entry` (`armed=1`, `table_rows=6`, build `igd1-e030a09eb4d1`) · seq2 `startup`
(`attempt_budget=8`, `mirror_open_rc=0`) · **seq3 `row` row0 `confirmed:1`,
`addr 0x7878000`, `from_prot 0x4`, `to_prot 0x5`** · seq4 `vmmap_persist`
(`span_prot=0x4` over the covering span — pre-state proof) · **seq5 `attempt` —
top-level `rc` = THE number; `mprotect_route="sceKernelMprotect_import"`; `outcome=8`
iff rc==0** · seq6 `rw_test` (`read_done=1`, `write_tested=0`) · seq7 `restore`
(ALWAYS back to 0x4; success `rc=0`, `outcome=10`) · seq8 `post_restore`
(`restored_matches_from_prot=1`, `post_observed_prot=0x4`) · seq9–18 rows 1–5 each
`row confirmed:0` + `refusal outcome=2`, zero attempts · seq19 `summary`
(`rows_attempted=1`, `attempts_used=1`, `budget_exhausted=0`, `mirror_failures=0`).
A missing seq = death at the preceding action (FATAL shape).

---

## C. THE MPROTECT NID FINDING (task-3 answer) — the flip is UNSAFE as measured

**Verdict: the walkguard fold-1 guard uses a RAW INLINE SYSCALL, not an NID-routed
mprotect. Under the session-934 law (any raw `syscall` instruction executed in-game is
process-fatal), the measured fold-flip object must NOT be booted even after a T2
`rc==0`. The guard code must be revised to route through the typed `sceKernelMprotect`
import BEFORE the flip is safe.**

Evidence (all read on disk today):

1. Staged source `worker-launch11-dry-compose/staging/asmhelper-guard/ASMHelper.guarded.cpp`
   (sha `694af016…`; identical copies in `worker-walk-site-transitions/` and
   `worker-launch10-walkguard-prep/staging/asmhelper-guard/`), lines 202–214:
   under `#if RC_ASM_WALK_RX_GUARD`, `sys_mprotect` is
   `__asm__ volatile("syscall\n\t" …)` with `"a"((uint64_t)RC_ASM_WALK_SYS_MPROTECT)`
   and `RC_ASM_WALK_SYS_MPROTECT` = **74** (line 61). The comment (lines 198–201) says
   it follows the `igcar_sys_mprotect` pattern "inline x86-64 syscall, NO import
   identity" — written against the OLD inverted gate, before the 934 record. At
   fold-0 the block is preprocessor-dead (zero syscall text — probe 0 `81dd832b…`, 0
   syscalls); at fold-1 it is LIVE.
2. Measurements agree: `worker-launch10-walkguard-prep/REPORT.md` probes — fold-1 via
   `-D`: `6604ce2d…`, **1 syscall**; fold-1 via the one-token flip:
   `f5627427131f53ace920cfa178e4cb5711c02fde1b47eca5d1a45e1a4957a3df`, **1 syscall**,
   argc=148 (composition-admissible), export surface unchanged. Independent
   re-measurement `worker-fold1-preverify/receipts/evidence/preverify-measured.json`:
   fold1 `6604ce2d…` `syscalls: 1` (fold0: 0). The fold1 receipt source
   (`worker-fold1-preverify/receipts/fold1/ASMHelper.guarded.cpp`, sha `93a1197…`)
   diffs against the staged unit in EXACTLY the one token: line 58
   `#define RC_ASM_WALK_RX_GUARD 1` vs `0` — so the measured object is the staged
   unit's fold-1 form, byte-for-byte lineage. `worker-walkguard-gate/REPORT.md` M-A
   row itself documents "Composed guard: inline syscall 74, zero imports (fold-1
   measured)". (Byte-scan caveat: a raw `0F 05` grep over the .o finds more pairs in
   non-instruction data; the compile-measured instruction count of 1 is the authority.)
3. The law: 934 record (`launch10-evidence/pid934-crashboot/EVIDENCE.md`) —
   `PPSA33286 crashed 0xa002030a SYSTEM_ILLEGAL_FUNCTION_CALL`, diagnosis: the raw
   `syscall` instruction itself is fatal inside the sandboxed game process; the mincore
   raw-syscall path was removed and replaced by NID-routed imports. MD-HEDGE §2.3
   draws the conclusion: even a T2 rc==0 does NOT make `f5627427…` bootable — the
   WST-5 fold law needs a 934 amendment.
4. The correct route exists as a working pattern — in the probe itself: the xwp
   probe calls mprotect via the single typed `sceKernelMprotect` import with ZERO raw
   syscall instructions and zero weak imports (A.1). The guard revision should do the
   same: replace the inline-asm `sys_mprotect` with a call through the same
   StubCatalog-derived NID row, as a DISTINCT-ID revision (new object sha, re-measured
   zero raw syscalls, same export surface), never a silent edit of `f5627427…`.
5. What T2 still decides (so the probe is NOT moot): (a) whether the catalog-derived
   NID actually RESOLVES — the probe's own load is the first falsifier (md-hedge
   U-MD-0 precondition 1; the sdknm libkernel export dump shows NO mprotect export, so
   the extension is evidence-negative in-tree); (b) whether the kernel PERMITS the
   X→RX transition on the exec page class (the seq5 `rc`). Both facts gate the revised
   guard exactly as they gate M-D.

---

## D. DECISION TREE — every T2 outcome → exact next action

| # | Observed | Verdict | Next controller action |
|---|---|---|---|
| 1 | seq5 `rc==0`, `outcome=8`; seq7 `outcome=10`; seq8 `restored_matches_from_prot=1` | **W_WORKS** (page-class permission PROVEN; NID resolution PROVEN) | **Do NOT flip to `f5627427…` — 934 law (§C): it carries 1 raw syscall-74 instruction and is expected process-fatal at its first in-game transition.** Authorize a DISTINCT-ID NID-routed guard revision: replace the inline-asm `sys_mprotect` with the typed `sceKernelMprotect` import (same StubCatalog row as the probe), re-measure (zero syscall instructions, export surface unchanged, new object sha), amend the WST-5 fold law to "fold=1 admissible only with the NID-routed revision", then recompose. Launch additionally blocked until the r3-ladder marshal-first re-order is terminal (run card §7 ladder trap). |
| 2 | seq5 `rc==0` but seq7 `outcome=9`/`rc!=0` or seq8 `restored_matches_from_prot=0` | W_WORKS-attempt / restore-UNRELIABLE (new evidence class) | STOP — do not flip, do not revise-for-boot yet. The walkguard latches permanently on any restore anomaly; reconcile first. Any retry is a NEW-id probe design, never identical bytes. |
| 3 | seq5 `rc!=0` (negative rc + errno), `outcome=7`; restore re-proves 0x4 | **W_FAILS_R_WORKS** (NID resolved, call reachable, exec-page X→RX denied by kernel) | Fold stays 0 (WST-5 unsatisfied — no rc==0 exists). M-D hedge becomes the pre-built alternative, but D-1 is a DIFFERENT page class (RELRO prot=1 `[0x82bc000,0x953c000)`) and needs its own NEW-design designated confirmation with a NEW build id — never this `e1541af6…` build. D-1 refusal ⇒ heap tier (R3 marshal) + polling only. |
| 4 | seq4 `span_prot != 0x4` or scan doesn't cover `0x7878000` → refusal `outcome=5/6`, `attempts_used=0` | Refusal (map drift) | STOP, reconcile-only against the current launch's real VMMAP; any re-pinned probe is a NEW id + rebuild. No memory touched. |
| 5 | No xwp records this session (jsonl did not grow past the Step-0 cut) | **ABSENT** — probe did not load | Reconcile staging, not the kernel. **Load-failure sub-check (934/NID-critical): a ploader FAIL line on the probe PRX likely means the derived `sceKernelMprotect` NID was rejected at import validation — that falsifies the U-MD-0 extension route itself (md-hedge precondition 1): the composition can never import it either, and BOTH the revised-guard flip AND M-D collapse to the heap tier.** Distinguish from ini/pre-image/readback faults before concluding. Fix staging, NEW `<TAG>`, full re-stage; never replay a failed activation. |
| 6 | FATAL shape (missing seq) or game crash attributable to the probe window | **FATAL** | Fold-0 stays; preserve raw logs; reconcile before ANY further exec-page work; no replay (934 is the reconciled-only precedent). |

Standing WST-5 note (run card §7): T2's single row-0 attempt IS the X→RX transition,
so the admissibility law reduces to "fold iff seq5 `rc==0`" — now AMENDED by §C:
"fold iff seq5 `rc==0` AND the folded unit is the NID-routed revision (zero raw
syscall instructions)". The guard's runtime fail-closure (transition rc!=0 → loud
refusal, no read) remains the backstop either way.

---

## E. Honest bounds

Derived entirely from staged sources and already-captured evidence; zero console
contact; zero engine/provider edits; nothing uploaded, flipped, or launched by this
worker. The record grammar is the machinery's pinned host-tested shape, not a console
measurement. No launch outcome is predicted; `rc` is THE open number. The variant
choice (`acc6b4f5…` minimal vs `fd9c7393…` Option A) is controller-owned; both are
staged and hash-pinned. Console constants stay [TARGET] until Step-0 readbacks.
