# RUNBOOK-V2-ADDENDUM — new evidence sections for the post-loading-regression candidate

Worker: worker-runbook-v2 · 2026-10-06 · proposal/documentation only. Extends
`worker-launch-readiness/NEXT-LAUNCH-RUNBOOK.md` (§2/§6/§7 unchanged and still
in force) with the next candidate's NEW durable evidence vocabulary. Machine
implementation: `analyzer_v2.py` (this dir; fork of worker-evidence-pull's
analyze.py v1.0.0 — its originals untouched). No console contact, no launch
authorization, no game claims; every observable below is PENDING-live-proof
until the same run (same pid) shows it.

New vocabulary sources (read-only):
- pool_copyin_probe_fresh + pool_copyin_probe_rc/errno/sink:
  `worker-copyin-unit/ADMISSION-NOTE.md` (KernelWriteCopyin = ReadableCoverageSource 2;
  probe fires ONLY on a Mapped range verdict; admission law = full transfer + DOUBLE
  sink-size verification; refusal/EFAULT does NOT imply unmapped).
- pool_range_verdict_fresh 0/1/2/3: pool v5 (existing runbook §2 C1).
- game_eboot_sha256_w0..w3 + game_content_version_packed + runtime-status `gamedata`
  row: worker-game-version design. Pinned manifests (sha256 recomputed 2026-10-05
  from the console bins in worker-game-version/):
  decrypted eboot `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
  toplevel eboot `afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e`.
  Pinned content version `02.003.000` (param.json contentVersion; pfs-version.dat).
- insession_<tag>: `worker-insession-diag/staging/insession_cmd_core.h` grammar v1
  (`{"cmd":"diag","tag":"...","repeat":N}` → one durable sdiag row
  `insession_<tag>` with value = repeat, 1..1024). STAGED, NOT COMPOSED — this
  section is a placeholder until that unit lands in a composed build.

---

## 1. Reading order (supersedes the bare §2 order for THIS candidate)

```
GI  GAME-IDENTITY gate      (analyzer_v2 §GAME-IDENTITY; BEFORE C0)
C0  boot frame              (runbook §2 C0 — unchanged)
C1  POOL verdict CASCADE    (range verdict → copyin probe → resolution note; §3 here)
C2  post-seeds chain        (only after C1 PROVEN — unchanged)
C3  the LOAD question       (only after C2 — unchanged; THE ONE QUESTION)
C4  input ARRIVES           (only after C3 PASS — unchanged)
C5  delivery + menu         (only after C4 arrival — unchanged)
INSESSION inventory         (read LAST; inventory, never a rung)
```

Rules:
- **GI is a gate, not a rung.** GI `PENDING` (identity tags absent = pre-candidate
  build) NEVER blocks the cascade — archived pre-candidate evidence classifies
  exactly as v1 did. GI `REFUSED` (any mismatch) is a HARD STOP: no rung claims
  that boot, all cached anchors suspect, re-pin derivation first (row 30).
- All claims still stop at the FIRST unmet rung (§2). The C1 subverdict is
  classification of the pool OUTCOME; it never upgrades a downstream rung.
- INSESSION rows are inventory. No rung claim, no menu claim, no spawn claim may
  cite an insession tag in v2.

## 2. GAME-IDENTITY section (GI)

| observable | law |
|---|---|
| `eboot sha w0..w3 vs pinned manifests` | reassemble w0..w3 as BIG-ENDIAN u32 words = digest bytes 0..15 → compare the first 32 hex chars against BOTH pinned manifests (decrypted `3b6cfad4…`, toplevel `afe0a2a5…`). Match = PROVEN (16-byte window; full-digest equality unproven by design). No match = REFUSED (IDENTITY DRIFT). Partial words = PENDING (never compared bit-wise). |
| `content version packed vs 02.003.000` | packed law (documented assumption, see Honesty note): `major<<24 \| minor<<16 \| patch<<8 \| 0` → 02.003.000 packs to 0x02030000; analyzer decodes `(v>>24).(v>>16&0xFF).(v>>8&0xFF)` and string-compares to `02.003.000`. Match = PROVEN; anything else = REFUSED with the decode shown (never guessed). |
| `runtime-status gamedata row` | a `gamedata` state row in runtime-status; LATEST-STATE-ONLY, applies to the session pid ALONE. Recognizable identity fields in its detail (pinned sha prefix / version string) are reported as a cross-check; the row's format is not pinned by a landed unit, so nothing is parsed into claims from it. |
| `GAME IDENTITY verdict` | REFUSED if any drift; PROVEN iff sha words AND packed version both match; else PENDING. |

**Honesty note:** the word-order (big-endian), the 16-byte compare window, and the
byte-per-field version packing are analyzer_v2's documented laws chosen from the
pinned manifests' bytes; if the composed emitter differs, the GI rows show the
raw values and the drift instead of silently matching. Fix the law with evidence,
never by loosening the comparison.

## 3. C1-POOL cascade (the fork, v2 shape)

Read in law order; each step is a durable observable:

1. **Range verdict** — `pool_range_verdict_fresh` 0/1/2/3 (meanings unchanged,
   runbook §7 rows 1-4). Probe is ONCE per boot.
2. **Copyin probe** — `pool_copyin_probe_fresh` 1 readable+read / 0 refused /
   2 unavailable / 3 invalid, plus raw captures `pool_copyin_probe_rc`,
   `pool_copyin_probe_errno`, `pool_copyin_probe_sink`. The SINK byte truth
   disambiguates, never the rc shape (convention law). Fires ONLY on range
   verdict 1 (Mapped) — anything else is an order violation.
3. **Resolution note text** — the `range-verdict resolution: …` suffix in the
   refusal line (or `not consulted (failure class not rescuable)` for the
   structural class).

Consistency laws (any violation ⇒ CAPTURE CORRUPT, do NOT rate C1, reconcile
before any revision):
- `pool_copyin_probe_fresh` present while the resolution note says the probe was
  not consulted → contradiction.
- copyin fired (tag present) while `pool_range_verdict_fresh != 1` → order
  violation (the probe must not fire on a kernel-refused range).
- copyin = 1 together with a REFUSAL → mutual-exclusion violation.
- pool VALIDATES while copyin verdict ∈ {0, 2, 3} → contradiction (the probe only
  fires after the span path already failed, so only 1 can coexist with admission).

### The C1 verdict-table cell (v2)

| subverdict | when | reading |
|---|---|---|
| `REFUSED-HONEST` | refusal + cascade consistent (copyin absent-consistent, 0, or 2) | the expected honest mapped-but-unreadable (or structural) refusal. Bank the durable probe evidence. Do NOT re-launch unchanged; a distinct-ID revision requires NEW derivation evidence. |
| `ADMITTED-VIA-COPYIN` | validated line + (copyin=1 or the success line names `kernel-write-copyin-read`) | pool admitted THROUGH the listing-gap owned-read probe (KernelWriteCopyin source). Span membership NOT claimed; downstream pool-entry reads ride the admitted bounds_source. Record that the v5 refusal expectation was falsified, with exact numbers. |
| `ADMITTED-VIA-SPAN` | validated line, no copyin admission markers | pool admitted via readable-span membership (the pre-copyin path). Same record-the-falsification duty. |
| `NOT-RATED` | copyin = 3 (invalid request) | instrumentation bug; the pool verdict is NOT RATED this boot (runbook row 4 analogue). |
| `UNRATABLE` | any consistency-law violation | capture corrupt — do NOT rate C1; reconcile before any revision. |
| `PENDING` | no pool verdict evidence in window | not reached or not emitted. |

## 4. INSESSION section (placeholder)

Lists every `insession_<tag>` sdiag record in the pid window: tag, count,
value-as-repeat, seq span. Grammar v1 bound: repeat ∈ 1..1024 — an out-of-range
value is flagged capture corrupt (loud, never executed semantics are the unit's
job, not the analyzer's). Presence means the in-session command consumer executed
in that pid; absence is the honest pre-composition state. Inventory only.

## 5. New failure-decision rows (append to runbook §7; numbering continues)

| # | observation | verdict / next action |
|---|---|---|
| 25 | Pool REFUSES; `pool_copyin_probe_fresh`=**2** (+rc/errno/sink raws) | copyin route UNAVAILABLE on this kernel (policy abort / sink-open fail / partial / vacuous sink / lseek masquerade) → **STOP the copyin route**; range-verdict route policy unchanged (rows 1-4); no re-launch on this route. |
| 26 | Pool REFUSES; `pool_copyin_probe_fresh`=**0** at the DM-class address (Blocks[0]) | owned-read REFUSAL: readable+read NOT proven. EFAULT does NOT imply unmapped (address-class caveat) → **DECISION-MEMO-BRANCH-ZERO applies** (`worker-blocks0-misread/DECISION-MEMO-BRANCH-ZERO.md`): A1 raw-qword dump + A2 page0 sub-verdict in a NEW distinct-ID revision only; never re-probe in-boot; never read blocks0 on the basis of any verdict. |
| 27 | Pool REFUSES; `pool_copyin_probe_fresh`=**3** | invalid probe request (len 0 or >16 MiB) → instrumentation bug; C1 NOT RATED this boot; fix the probe sizing before any re-launch. |
| 28 | copyin=1 while the pool REFUSES, OR any copyin tag where the note says the probe was not consulted | mutual-exclusion / never-fired violation → **CAPTURE CORRUPT, do NOT rate C1**; reconcile the linked build before any revision (identity first: GI section). |
| 29 | copyin tag present while `pool_range_verdict_fresh` ≠ 1 (or absent) | order violation (copyin fires only on Mapped) → capture corrupt; do NOT rate; verify the composed unit set (initializer hunk 3 ordering) before re-launch. |
| 30 | `game_eboot_sha256_w0..w3` or `game_content_version_packed` mismatch vs the pinned manifests (`3b6cfad4…`/`afe0a2a5…`, `02.003.000`) | **IDENTITY DRIFT — HARD STOP**: no rung claims this boot; a different binary than sha 3b6cfad4 reopens branch-zero memo R4/R5 assumptions; re-pin the derivation and the staging receipt before ANY re-launch; never "analyze around" a mismatch. |
| 31 | Pool VALIDATES with the line naming `kernel-write-copyin-read` | **ADMITTED-VIA-COPYIN**: capture the full seeds chain + raw probe captures + the resolution/admission line; span membership NOT claimed; record the falsified v5 expectation with exact span/envelope numbers; continue C2. |
| 32 | `insession_*` tag with value outside 1..1024, or a tag whose command was not sent this session | consumer/receipt mismatch → inventory flagged capture corrupt; reconcile the poll glue; no rung impact (inventory only). |

Pre-existing rows 1-24 keep their meanings; where a row cites only the range
verdict, the copyin cascade adds evidence but never downgrades it.

## 6. Completion-assessment mapping (what each rung may be called ONCE its tags are reachable)

| evidence tags/lines (same pid, fresh window) | constitutes | never constitutes |
|---|---|---|
| GI PROVEN (sha words + packed version match pinned) | boot-frame identity; cross-boot comparability of C1+ evidence for this boot | any rung pass by itself |
| C0 trio + `runtime_start_ok` + spanpub + armed line | runtime-start boot frame (unchanged §2 C0) | pool readability |
| `pool_copyin_probe_fresh`=1 + `pool_range_verdict_fresh`=1 + ordered seqs + `kernel-write-copyin-read` in the validated line | C1 PROVEN via copyin admission (KernelWriteCopyin bounds_source) | span membership; any downstream rung |
| validated line without copyin markers | C1 PROVEN via span | copyin-route evidence |
| `[R3] FuncPtr-marshal armed` + `marshal is the game-thread pump` | C3 game-thread-pump evidence (unchanged F-1 caveat for latency) | input arrival |
| `pad_available`>0 once, after last `pad_retry_failed`, none after | C4 INPUT ARRIVES (unchanged) | delivery |
| OPTIONS press ≥ arrival t_ms → `<N> spawnable pickups found. Pages=<M>` AND `Menu created.` | **MENU RUNG** (C5) — the first time both lines exist in the same pid, the menu is honestly reached | item SPAWNING (M6) — record verbatim, claim nothing, even if observed |
| menu lines with only `Scan failed: Item database not ready.` | drain ALIVE, no save loaded (row 19; menu mechanics intact) | menu rung |
| `insession_<tag>` receipts | in-session command consumer executed; repeatable in-session diagnostics (fills the "no host→game channel" gap ONCE COMPOSED) | boot-rung evidence; menu/spawn evidence; any §2 claim |

Completion statement law (unchanged in spirit): a rung is claimed only by its own
same-run observables; the menu is claimed only by its two exact lines; spawn is
never claimed by this runbook; PENDING includes not-reached; every REFUSED is a
successful evidence outcome if the game survived.

## 7. Dry-run receipts (this dir)

- Regression: `regression-pid700/{v1,v2}/results.json` — analyzer_v2 vs v1 on the
  archived pid-700 fullfix evidence; `compare_regression.py` proves REGRESSION-PROOF
  twice (fresh v1 AND the archived `worker-evidence-pull/dryrun-pid700/results.json`):
  26/26 v1 observables verbatim (+10 new v2 rows), 12 greps identical, rungs /
  cascade stop / failure hint / pool verdict identical. GI=PENDING, C1=REFUSED-HONEST,
  insession=0 — exactly v1's classification plus the new vocabulary rows.
- Synthetic: `fixtures/` (7 cases, SYNTHETIC bytes, generated by `make_fixtures.py`)
  checked by `check_fixtures.py` → ALL 7 FIXTURE EXPECTATIONS MET:
  fx01 REFUSED-HONEST + copyin=0 + branch-zero + GI PROVEN + insession;
  fx02 copyin=2 route stop; fx03 copyin=3 NOT-RATED;
  fx04 full ladder C0-C5 all PROVEN + ADMITTED-VIA-COPYIN + THE ONE QUESTION PASS;
  fx05 ADMITTED-VIA-SPAN + GI PENDING (does not block) + insession receipt;
  fx06 range=0/copyin=1 order violation → UNRATABLE (capture corrupt);
  fx07 wrong sha words + wrong version → GI REFUSED hard stop, INSESSION never
  annotated. Analyzer outputs land in `dryrun-fixtures/<case>/`.
