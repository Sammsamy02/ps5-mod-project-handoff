# PROBE-RUN-CARD — T2 armed xwp probe (row 0, `0x7878000` X→RX) · route M-A step 1

Worker: worker-probe-prep · 2026-10-05 · preparation/documentation only. This card
assembles and cross-checks already-staged material; it stages nothing, contacts
nothing, launches nothing, and claims no outcome. Every console step is executed by
the CONTROLLER; the launch is USER-owned (manual, never automated). Every console
constant below stays [TARGET] until the Step-0 RETR and post-STOR readbacks prove it.

Basis (read and verified on disk today):
- worker-walkguard-gate REPORT.md §3 (route M-A, T2 record chain, fold-flip law)
- worker-igw-row0-rebuild STAGING-PROPOSAL.md §1–§4 (ini Option A, staging order, verdict shapes) + REPORT.md + BINARY-DELTA.md
- worker-game-session-ridealongs STAGE-PLAN-COMBINED.md (standing console ritual, SHT-strip readback law, ploader quirk)
- worker-final-ini-layout FINAL-INI-LAYOUT.md + verify_final_ini_layout.py (re-run by me on the Option A bytes: `FINAL-INI-OK`, rc=0)
- worker-launch10-walkguard-prep REPORT.md §4 (WST-5 gate, measured flip object `f5627427…`)
- worker-l11-marshal-integration RUNBOOK.md §5.1 (DIFF-1 ladder trap; r3-ladder unit in progress — see §7)
- worker-payload-selftest dispatch_selftest.py + SELFTEST-REPORT.md (the OTHER boot route — excluded below)
- worker-launch10-evidence-upgrade evidence.sh (standing evidence fetch tool, FTP `ps5-host-not-configured.invalid:2121`, pinned baselines)

---

## 0. ROUTE DETERMINATION — this is a TOWNFALL GAME BOOT, not a payload boot (verified)

The task asked to verify which. Answer: **the T2 probe requires the ploader.ini game-injection
route — the user manually launches Townfall (title `PPSA33286`); the armed probe PRX is
injected into the game process at boot. It is NOT a standalone `pldmgr /loadpayload`
dispatch.** Reasons, all from the staged design:

1. Row 0's fail-closed gate re-scans the CURRENT process VMMAP and refuses unless the fresh
   scan covers `[0x7878000, +0x1000)` with `prot == 0x4` — the tail page of the GAME IMAGE's
   exec span `[0x400000,0x787c000)` (walkguard REPORT §1.6; probe src `exec_write_probe.c:177-188`).
   In a separate-process payload (the `pldmgr` route, HTTP 8084, used by worker-payload-selftest)
   that address is not the game image ⇒ `XWP_REFUSE_NOT_COVERED (outcome=5)`, zero attempts,
   launch wasted for the verdict — exactly the ABSENT-class negative without touching memory.
2. The Option A ini is keyed `[PPSA33286]`; ploader injects plugins into that title's pid
   (ploader main.c L776-820, sequential file order).
3. worker-payload-selftest's own evidence class statement: its route is "NEVER game-process
   evidence" — the inverse of what T2 must prove (kernel syscall-74 behavior on the game's
   own exec pages in the game process).

So the "payload" in the workflow sense is only the upload/readback shape (STOR fself → RETR
SHT-strip readback → boot → evidence readback). The boot itself is a Townfall launch.

---

## 1. PRE-FLIGHT GATES (controller; all stop-on-mismatch)

| # | Gate | Pass condition |
|---|---|---|
| G-1 | Launch-10 window fully closed; its frozen byte-equality pre/post state recorded and UNDISTURBED (STAGING-PROPOSAL §2.1). If launch-10 flow has not terminated: DO NOT STAGE. | controller-confirmed |
| G-2 | Standing ritual (STAGE-PLAN-COMBINED header): Townfall fully closed on console, FTP up (`ps5-host-not-configured.invalid:2121`), ploader armed. | controller-confirmed |
| G-3 | Fresh distinct session TAG (timestamp form `<TAG>`); used on `.bak` and all local capture dirs. Never reuse a failed window's TAG. | assigned |
| G-4 | Local artifact hashes re-verified BEFORE any upload (table §3). | all equal |

## 2. STEP-0 — ini pre-image + backup (before ANY change)

1. RETR `/data/plugins/ploader.ini` → `/tmp/<TAG>/ploader.ini.pre`; record sha256, byte
   count, line count, last byte.
   - **Expected pre-image: `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`**
     (the launch-10-frozen 2-plugin shape: `supplier_composition` + `path_test` enabled —
     STAGING-PROPOSAL §1 Option B note). **Any divergence = STOP + reconcile, never blind
     overwrite.**
2. RETR stat (size + mtime) of `/data/plugins/UE4SS.log` and byte size + sha of
   `/data/modbridge-exec-write-probe.jsonl` IF PRESENT (launch-10 pinned baseline:
   4362 B, `20cff601…` — evidence.sh BASE_XWP/PIN_XWP_SHA; the file is append-only across
   boots, so T2's records are everything past the pre-launch cut).
3. Console `.bak` FIRST: STOR the untouched pre-image to
   `/data/plugins/ploader.ini.<TAG>.bak`; RETR + byte-verify readback == `.pre`.

## 3. FILES FIRST — uploads + readbacks (SHT-strip law; upload → readback → verify, in this order)

**3a. THE PROBE (the only semantically new bytes).** STOR BINARY to the FINAL name — never
a re-STOR of launch-9 bytes; the file NAME is the same, the artifact is distinct:

```
local : outputs/2026-10-04-zcode-team/worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx
remote: /data/plugins/modbridge_exec_write_probe_armed.prx
```

| constant | value (all re-verified on disk by this worker) |
|---|---|
| build id | `igd1-e030a09eb4d1` |
| fself sha256 | `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df` (20013 B) |
| bare-sdk sha256 | `cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9` (69464 B) |
| **expected RETR readback length** | **67832 B** (= max(p_offset+p_filesz) of the bare-sdk ELF; recomputed by me from `determinism/run-armed-1/bare-sdk/modbridge_exec_write_probe.prx`) |
| **expected readback body sha256 (bytes 64..end)** | **`e3f13d1994a1b4d27de05e31885d5a5cf9ea91daea3c57c2c229afd3696c295e`** (recomputed by me; NOTE this supersedes the FINAL-INI-LAYOUT §4 xwp row `daea2ff4…`, which pins the OLD launch-9 probe — same 67832 length, different body. Stale-constant trap.) |
| header-exception law | readback = segment-covered prefix of the bare inner ELF; only `e_shoff` (40..47) and `e_shentsize/e_shnum/e_shstrndx` (58..63) may differ; embedded `e_shnum` == 0. Verify with the exact STAGE-PLAN-COMBINED §3 snippet against the `cc544d6e…` bare ELF. |
| re-STOR budget | any FAIL → ONE re-STOR from the verified local fself, re-verify; second failure → abort staging (console keeps prior state) + reconcile |
| artifact readiness | `check_xwp_artifact.py determinism/run-armed-1 armed` → all gates PASS (fself law, 6 segment copies, init/fini 1/1, 8 strong NID-encoded imports incl. the single typed `sceKernelMprotect`, zero weak, ZERO raw `syscall` instructions, no `0F 05` pair, no forbidden identities, provenance strings present). Top-level fself == run1 == run2 (relink-identical). |

**3b. Option A ridealongs (presence guard obligation).** The `fd9c7393…` ini carries 8
ENABLED (`= 2`) entries; ALL 8 files must be present+non-empty on console BEFORE the ini
publish. For each: RETR-stat; if absent or sha-divergent from its owner's pin, upload from
the owner's staged artifact + SHT-strip readback verify. Known constants:

| module (ini position) | fself sha256 (bytes) | readback bytes | readback body sha (64..end) | owner/doc |
|---|---|---|---|---|
| `modbridge_fc1pool.prx` (2) | `d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96` (22733) | 67848 | `7e2dce27e415bea80c3100d74e6f58c9a2cd8697bc87e03b67226fba09646ed3` | FINAL-INI-LAYOUT §4 |
| `modbridge_qh_driver.prx` (4) | `52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4` (26157) | 87112 | `71406d3ccc7c1da0688b68ff35fd35c4a7ef9241b4d02ce427504cd392002cf5` | STAGE-PLAN-COMBINED §0/§3 |
| `modbridge_runtime_aligned_observer.prx` (5) | `1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30` (30152151) | 30143976 | `3f89a5b66bba614926f7e7b1273c1d2aadeb505b2797dcd7389b12cebeee6622` | STAGE-PLAN-COMBINED §0/§3 |
| `modbridge_supplier_composition.prx` (3) | controller-pinned (currently staged under launch-10's window; "stay/refresh" is the controller's call — the T2 verdict does not depend on it; the probe's records land BEFORE the composition's `module_start` by ini order) | — | — | controller manifest |
| `modbridge_runtime_aligned_entry.prx` (6), `modbridge_path_test.prx` (7), `modbridge_carrier.prx` (9) | their own owners' constants (launch-9 had `aligned_entry` f6530c79 loaded; `fc1pool`/`qh_driver`/`observer`/`carrier` were 0B/absent then — under Option A they must exist THIS time or the presence guard refuses) | — | — | owners' stage plans |
| `modbridge_ingame_facts.prx` (8) | `= 0` — SKIPPED at parse; file need not be present; no slot consumed | — | — | FINAL-INI-LAYOUT §2 |

**3c. Presence guard.** Over EVERY enabled entry of the pending ini: file exists, non-empty,
sha per §3a/§3b. Any miss → upload+verify that file first; never publish an ini that enables
a missing file (the launch-9-incident law).

## 4. CONFIG LAST — publish the Option A ini

Verbatim bytes to STOR to `/data/plugins/ploader.ini` (440 B,
sha256 `fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507`,
LF endings, trailing newline; local file:
`outputs/2026-10-04-zcode-controller/launch9-evidence/ploader-ini-9entry-restore.txt`;
I re-ran `verify_final_ini_layout.py` on it today: `FINAL-INI-OK … 8 active entries in
definitive order (carrier last, after igf=0); igf=0 skipped; 2/10 slot headroom`, rc=0).
This is the SAME byte config launch-9 ran (proven shape; a preserved canonical, not a new
invention). Do NOT stage the `bbc34c16…` 400-B `ploader.ini.canonical` variant or the
Option B 162-B minimal ini — neither is law-ready for this window (STAGING-PROPOSAL §1).

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

STOR → fresh RETR → `diff` byte-exact against the local 440 B. On ANY failure: ONE bounded
restore of the `1216aab6…` `.bak` (the standing single-restore law), verify, stop.

## 5. USER LAUNCH — what the user does, and for how long

1. Controller prompt: "arm ploader" → user confirms ploader armed (standing ritual).
2. Controller prompt: "launch Townfall manually" → **the USER starts the game by hand from
   the console UI. Never automated, never remote-triggered.**
3. Expected phases / durations (design-pinned mechanics, not outcome predictions):
   - ploader injection, sequential file order, mode-2 spacing (~1.6 s stable+settle each,
     per-plugin wait cap 25 s): 8 enabled entries ≈ **13–20 s** of injection window. The xwp
     probe is FIRST, so its records land within the first ~2–3 s of injection.
   - The probe is one-shot at `module_start`, records flushed synchronously via
     `sceKernelWrite` BEFORE returning, no threads, no unload, fail-open (always returns 0).
     Its `entry_guard` makes any ploader list-retry a no-op (re-entry answers 0) — the
     seq1–seq19 set below is single-shot even under the ploader retry quirk
     (STAGE-PLAN-COMBINED §5: a mid-list FAIL retries the whole list).
   - **The T2 verdict evidence is complete ≈ 15–40 s after launch**; the full boot +
     ridealong windows (observer ~60 s, qdrv1 ~2 min) settle within ~3 min. Budget the
     user window ≈ **5 minutes** launch-to-collection; staging before it is the usual
     upload/readback time (observer alone is 30 MB if it needs refresh).
4. Post-launch notification surface: read every ploader notification line. Expected: **8 OK
   lines (`OK (<ms>): <filename>`), zero FAIL lines** (igf=0 is skipped silently). Any FAIL
   → STOP, triage per STAGE-PLAN-COMBINED §5/§7; do not relaunch the identical layout.

## 6. EVIDENCE TO PULL AFTERWARD (names, tags, and the rc record chain)

Fetch via the standing tool (`worker-launch10-evidence-upgrade/evidence.sh`, FTP base
`ftp://ps5-host-not-configured.invalid:2121`) or plain RETR into `/tmp/<TAG>/`:

| # | Console file | Role | What proves the verdict |
|---|---|---|---|
| 1 | `/data/modbridge-exec-write-probe.jsonl` | PRIMARY (the probe's own synchronous records) | the seq3–seq8 row-0 chain below; durable tags: `"marker":"modbridge.execwriteprobe"`, `"schema":1`, `"build_id":"igd1-e030a09eb4d1"` |
| 2 | `/data/plugins/UE4SS.log` | best-effort MIRROR of every record (same JSONL lines) + ridealong lines | mirror consistency; `PATH_TEST:`/`ACTIVATION_TEST:` freshness lines for THIS launch |
| 3 | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | composition ridealong state (context only) | session/pid stamp proves a fresh window |
| 4 | `/data/modbridge-supplier-diag.jsonl` | OPTIONAL sdiag ridealong (composition sink; absence is normal) | per-pid session split per evidence.sh G2; NOT a T2 verdict input |
| 5 | ploader notification text (from launch) | injection surface | the 8 OK / 0 FAIL shape |

Isolation law: keep the pre-launch byte cuts (Step-0 §2.2) and tag the fetched set with
`<TAG>` + build id `igd1-e030a09eb4d1`. Proposed durable ledger tags for this window
(STAGE-PLAN-COMBINED §8 style): `XWP2-STOR`, `XWP2-READBACK`, `XWP2-PRESENCE`,
`INI9-PRE`, `INI9-BAK`, `INI9-STOR`, `INI9-READBACK`, `T2-LAUNCH`, `T2-JSONL`, `T2-MIRROR`.

**The T2 rc record chain (the evidence the walkguard REPORT §3.1 specifies) — expected
grammar, 19 records total** (shapes pinned by `exec_write_probe.c` + the launch-9 captured
jsonl `igd1-6ebab07f2111`; top-level `rc`/`errno` are the syscall results):

| seq | exp | pinned fields (success-path values) |
|---|---|---|
| 1 | `entry` | `phase=module_start`, `armed=1`, `table_rows=6`, build `igd1-e030a09eb4d1` |
| 2 | `startup` | `attempt_budget=8`, channels open, `mirror_open_rc=0` |
| 3 | `row` (row 0) | `"confirmed":1`, `addr 0x7878000`, `from_prot 0x4`, `to_prot 0x5`, `page_count 1` (verbatim from the table) |
| 4 | `vmmap_persist` | **`span_prot=0x4`** over the covering span (pre-state proof), `attempt_addr=0x7878000`, `attempt_len=0x1000`, `walk_trusted=1` |
| 5 | `attempt` | **top-level `rc` = THE number**; `mprotect_route="sceKernelMprotect_import"`; `requested_prot=0x5`; `outcome=8` (XWP_ATTEMPT_OK) iff rc==0 |
| 6 | `rw_test` | `read_done=1`, `write_tested=0` (to_prot 0x5 has no W bit), `first_byte=<observed>` |
| 7 | `restore` | ALWAYS attempted back to 0x4; success: top-level `rc=0`, `outcome=10` (XWP_RESTORE_OK), `restored_prot=0x4` |
| 8 | `post_restore` | fresh VMMAP re-proof: `restored_matches_from_prot=1`, `post_observed_prot=0x4` |
| 9–18 | rows 1–5 | each: `row` (`"confirmed":0`) + `refusal` `outcome=2` (XWP_SKIP_NOT_CONFIRMED), `no_memory_touched=1`, zero attempts |
| 19 | `summary` | `rows_seen=6`, `rows_attempted=1`, `rows_refused=0`, `rows_skipped=5`, `attempts_used=1`, `attempt_budget=8`, `budget_exhausted=0`, `mirror_failures=0`, `armed=1`, `mprotect_route="sceKernelMprotect_import"` |

**Verdict law: `rc==0` at seq5 ⇒ `W_WORKS`.** Honest caveat carried from the walkguard
REPORT §3.1: T2's page is the exec span's tail page past the file-content end
(design-pinned crash safety; extent end `0x7879b1c` per the row note) — same kernel VMMAP
record class (prot=0x4, span `[0x400000,0x787c000)`) as the code pages, a class-exact,
byte-remote proxy. Runtime residual refusals on real code pages remain possible and are
fail-closed-loud by the guard itself (ASM:156-161), never a crash.

## 7. DECISION TABLE — every T2 outcome → exact next controller action

| # | Observed chain | Verdict | Exact next controller action |
|---|---|---|---|
| 1 | seq5 `attempt` `rc==0`, `outcome=8`; seq7 `restore` `outcome=10`; seq8 `post_restore restored_matches_from_prot=1` | **`W_WORKS`** | **Fold flip + recompose** (walkguard REPORT §3.2): in the staged `ASMHelper.guarded.cpp`, change `#define RC_ASM_WALK_RX_GUARD 0` → `1` (one token; keep the `__FILE_NAME__` basename pin), admit via `--file`, declare expected object `f5627427131f53ace920cfa178e4cb5711c02fde1b47eca5d1a45e1a4957a3df` in the manifest, recompose through the SAME edition/driver (launch11-combined). Initializer `e7f349af…` untouched. NOTE: `f5627427…` was measured but NEVER linked — the recompose run must re-measure/verify the linked object against it. AND: the recomposed LAUNCH must wait for the §5.1 ladder re-order (row 8 below). |
| 2 | seq5 `rc==0` but seq7 `restore` `outcome=9`/`rc!=0`, OR seq8 `restored_matches_from_prot=0` | **`W_WORKS`-attempt / restore-UNRELIABLE** (not a clean verdict; new evidence class) | **STOP — do not flip, do not recompose.** The walkguard restores per read and permanently latch-refuses on ANY restore failure (ASM:118-122, :180-188) — this outcome demonstrates the restore half is unreliable on this kernel, so fold-1 would hard-refuse every walk read after the first restore anomaly, and the probe may have left a page non-X for the session. Reconcile first; any retry must be a NEW-id probe design (never replay identical bytes); the M-D hedge evaluation (row 3) applies in the interim. [Derived by this worker; STAGING-PROPOSAL §3 has no explicit row for this case — flagged honestly rather than silently folded into row 1.] |
| 3 | seq5 `rc!=0` (negative rc + errno recorded), `outcome=7` (XWP_ATTEMPT_MPROTECT_FAIL); restore still re-proves 0x4 | **`W_FAILS_R_WORKS`** (kernel-enforced exec pages via the import route; R-half confirmed, W-half not) | **Fold stays 0 (WST-5 not satisfied — the X→RX rc==0 does not exist). M-D hedge becomes the pre-built alternative:** vtable-swap/data-side dispatch design (SWAP §5 R1/R2) whose gate is D-1 (mprotect R→RW on the RELRO page `[0x82bc000,0x953c000)` prot=1) — a DIFFERENT syscall-74 page class, its own unproven fact. Rows 4/5 of the armed table are `confirmed=0` and stay that way: **a D-1 verdict needs its own design-designated confirmation, NEW build id, NEVER this `e1541af6…` build**, per LAUNCH10-MATRIX R6 (controller+user approve a NEW falsifiable design first). D-1 refusal ⇒ heap tier (R3 marshal) + polling only; ProcessEvent parity not claimable. Walk reads stay refused-loud (fold-0, `guard_off_no_rx_verdict`, zero syscalls). |
| 4a | seq4 `vmmap_persist` shows `span_prot != 0x4` or the scan does not cover `0x7878000` → `refusal` `outcome=6`/`5`, `attempts_used=0` | **Refusal (map drift)** — reconcile-only | **STOP.** The launch's map differs from the design pin. Reconcile against the current launch's real VMMAP (bridge spanpub evidence); any re-pinned probe is a NEW id + rebuild. Never re-rate unchanged evidence. No memory touched (fail-closed worked). |
| 4b | No xwp records this session (jsonl did not grow past the Step-0 cut; no `igd1-e030a09eb4d1` marker anywhere) | **`ABSENT`** — probe did not load | **Fold-0 stays; reconcile the staging, not the kernel.** Check in order: ploader notification lines (FAIL?); the Step-0 pre-image sha (was the ini actually the `fd9c7393…` bytes?); `XWP2-READBACK` receipt (was the NEW fself actually resident under the final name?); presence-guard log. The launch is wasted for the verdict; no memory touched. Fix staging, NEW `<TAG>`, full re-stage (never replay a failed activation without fresh preflight). |
| 4c | FATAL shape (a seq missing = death at the preceding action) or game-side crash attributable to the probe window | **`FATAL`** | Fold-0 stays; reconcile before ANY further exec-page work (STAGING-PROPOSAL §4 row 3; walkguard-prep lines 13-16/112/122). Preserve raw logs; no replay. |

**WST-5 reconciliation note (so nobody mis-folds):** walkguard-prep's gate wording
("admissible exactly when the IGW D2 seq5 records `rx_result.mprotect_rc == 0` —
W_WORKS/W_FAILS_R_WORKS rows") uses the ORIGINAL launch-10 EW-probe matrix semantics, where
`W_FAILS_R_WORKS` meant "seq5 (X→RX) rc==0, seq10 (RWX/write) rc!=0" — i.e. the X→RX
transition itself SUCCEEDED in both fold-admissible rows (LAUNCH10-MATRIX D2 line). The T2
probe's single row-0 attempt IS the X→RX transition, so the law reduces exactly to: **fold
iff the seq5 `attempt` `rc == 0`.** T2 `rc!=0` (row 3 above) does NOT satisfy WST-5; the
STAGING-PROPOSAL §3 gloss of that shape as "W_FAILS_R_WORKS" names the residual route
(R-works ⇒ D-1 design remains meaningful), not a fold permission. The guard's own runtime
fail-closure (transition rc!=0 → loud refusal, NO read) remains the mis-fold backstop
either way.

**Ladder-trap dependency (walkguard REPORT §4 + RUNBOOK §5.1) — blocks the row-1 LAUNCH,
not the flip itself:** M-A produces the walk-permitted world in which the vtable fallback
assigns Tick, so `is_engine_tick_hook_available()` goes true and
`ensure_engine_tick_hooked()` throws `EngineTick callback registration failed` at the first
default-method ExecuteInGameThread — the marshal sits armed and unused. The
marshal-first-when-armed ladder re-order (ExecuteInGameThread + all three WithDelay
ladders, distinct-ID revision) is being authored in parallel by worker-r3-ladder
(dir observed in progress: staging/`LuaMod.cpp` + objects exist, no REPORT yet — NOT
terminal as of this card). **Controller must not launch the fold-1 recomposition until that
revision is terminal and staged; T2 rc==0 authorizes the flip, not the launch.** Unchanged
by M-A: GATE-2 (fold-1 permits walk READS only; PLH detour disasm+write stays compiled
out/rerouted), GATE-5 (LuaMod's eager LoadMap registration needs its own scoped guard,
unowned), GATE-3 (pad ownership). Restores-on-launch remain loud-only.

## 8. ARTIFACT READINESS VERDICT (this worker's check, host-side only)

- Staged fself present and hash-exact: **YES** (`e1541af6…`, 20013 B; == determinism run1
  and run2; bare `cc544d6e…` 69464 B; checker all-PASS). Upload shape matches the standing
  payload workflow (STOR → RETR SHT-strip readback 67832 B / body `e3f13d19…` → boot via
  the ploader game-injection route → evidence readback). **READY.**
- Option A ini bytes present and law-verified: **YES** (`fd9c7393…`, 440 B; FINAL-INI-OK
  rc=0 re-run by me). **READY.**
- Fold-flip pre-measurement on file: **YES** (walkguard-prep probe 2, object `f5627427…`,
  argc=148, 1 syscall instruction, export surface unchanged; never-linked caveat above).
- Open items (not blockers for T2 itself): ridealong constants for composition/
  aligned_entry/path_test/carrier must come from their owners at stage time; r3-ladder
  re-order not yet terminal (blocks only the post-W_WORKS launch); console constants stay
  [TARGET] until Step-0.

## 9. Honest bounds

Derived entirely from staged sources and already-captured evidence; zero console contact;
zero engine/provider edits; nothing staged, flipped, or launched by this worker. Line
numbers are as read today and re-anchor on any controller edit. The record grammar is the
machinery's pinned shape (host-tested with mocked mprotect + the captured launch-9 jsonl),
not a console measurement. No launch outcome is predicted; `rc` is THE open number.
