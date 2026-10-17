# STAGING CARD — ploader.ini single-variable ORDER test (path_test FIRST)

worker-ini-order, 2026-10-05. **PREPARATION ONLY — this worker performed ZERO console
writes.** All console evidence below is read-only FTP RETR. The STOR sequence is for
the CONTROLLER, after user approval, game CLOSED, per ROLLBACK-RUNBOOK §4.2–4.4 and
TEST-MATRIX.md "Ini freeze cycle" (unfreeze→modify→refreeze→re-pin).

Hypothesis under test (single variable = ini ORDER only): if the diagnostic loader's
rc points at an import/dependency class, moving path_test (small, trivial imports)
before modbridge_supplier_composition makes composition load second — by the time
path_test has loaded and settled, provider modules are resident. Outcome 1 confirms
the residency race; outcome 2 weakens it and strengthens a size/class cause.

---

## 0. Verified console state (read-only, this session)

- Fresh RETR `ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini` at 2026-10-05 ~20:55
  local: **440 bytes, sha256 `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`**
  — matches the pin exactly. Saved as `ploader.ini.retr-20261005-preimage` in this dir.
- LF-only line endings, trailing newline, 10 lines (header + 9 plugin lines).
- Corroborating byte-identical copies (same sha, all 440 B):
  - `worker-staging-next/runs/20261005-200730-stage-c/20261005-200730-stage-launch10/l10-ploader.ini.pre`
    (freeze copy from the last GO stage-launch10 run, 20:07:30)
  - `/tmp/ppsa33286-preimages-next/ploader.ini.pre` (stage-next phase A/B pre-image, 20:05)
  - `/tmp/minimal2-ploader.ini` (older host copy)
- **NOT the pin (older 9-entry config, sha `fd9c7393…`)**: `/tmp/actual-ploader-ini.txt`,
  `/tmp/launch-ini-check.txt`, `/tmp/ploader.ini.cc1candidate`,
  `worker-final-ini-layout/ploader.ini.canonical`. Never roll back to these.

**Authoritative local pre-image for this task:** `worker-ini-order/ploader.ini.retr-20261005-preimage`
(freshest RETR; byte-equal to all corroborating copies above, so any of them is an
acceptable rollback source — verify sha before use, per the verify-hashes law).

Current line order (the pin, 1216aab6…):

```
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 0
/data/plugins/modbridge_fc1pool.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2     <- line 4: injected FIRST today
/data/plugins/modbridge_qh_driver.prx = 0
/data/plugins/modbridge_runtime_aligned_observer.prx = 0
/data/plugins/modbridge_runtime_aligned_entry.prx = 0
/data/plugins/modbridge_path_test.prx = 2                <- line 8: injected SECOND today
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_carrier.prx = 0
```

## 1. The variant (exact bytes, prepared)

File: **`ploader.ini.variant-pathtest-first`** (this dir)
- **sha256 `3cbb8df441f4fe5e9b2c295d3307032b8f171f448a9e7056a75b20f1112a1ad0`**, 440 bytes.
- Sole change: the `modbridge_path_test.prx = 2` line moved from line 8 to line 4,
  immediately BEFORE `modbridge_supplier_composition.prx = 2`. Every other line
  byte-exact and in the same order (all `=0` entries untouched, header untouched,
  LF endings, trailing newline preserved).
- Byte-diff summary vs the pin: identical bytes 0–130 and 354–439; differing span
  bytes 131–353 (177 bytes, the two moved lines' region); `cmp` first difference at
  char 132 = line 4. Same multiset of lines (sorted-lines equal), same length.
- Resulting injection order: path_test FIRST, composition SECOND.

## 2. Why ini order = injection order (baseline main.c citation)

Pinned baseline source `worker-staging-next/ploader-build/main.c` (the session-built
payload, sha 4c339235…):

- `read_ini` (L710–742): parses `/data/plugins/ploader.ini` line-by-line with `fgets`
  in FILE ORDER; mode-0/-1 lines are skipped at parse (`if (mode <= 0) continue;`,
  L731); every valid `path = mode` line calls `register_plugin` (L738).
- `register_plugin` (L701–707): appends via `int idx = g_plugins[slot].path_count++;`
  — registration order equals ini line order. The `=0` lines never enter `paths[]`.
- `run_plugins` (L797–890): for the matched title, the injection loop is
  `for (int p = 0; p < entry->path_count; p++)` (L835) — plugins inject SERIALLY in
  array order = ini order. Each iteration: `wait_for_needs` (mode-2: stable 1000 ms,
  min_mod 16, settle 600 ms, 25 s budget) → `pt_attach` → `inject_prx` (L843–866),
  timed by `now_ms()` and reported as `OK (%ldms)` / `FAIL (%ldms, …)` (L875–877).
- The `proven_stable_pid` memo (L522–530, L555–577): the FIRST plugin pays the full
  `wait_stable` burn; the SECOND gets a cheap 4-tick re-check. Under the variant,
  path_test pays the full wait, then composition is injected right after via the
  cached re-check — exactly the "path_test loaded and settled first" window.

The diagnostic-lineage build that will actually run
(`worker-loader-instrument/build-diagnostic/main.c`, sha of source
7f352dcd…) keeps the identical law: `register_plugin` `path_count++` (L642–645),
`mode <= 0` skip (L672), same injection loop (L776). Its only diagnostic deltas are
the FAIL line format `FAIL (%ldms, rc=0x%08x)` (L814) and restore_creds error
handling — parsing/ordering is unchanged. So ini order = processing order in BOTH.

## 3. Reverse control (no work needed)

Composition-first = the CURRENT order = the pinned file itself (`1216aab6…`).
It is the live baseline and every existing observation of the ~85 ms FAIL was
captured under it. No artifact needed; the pin bytes in §0 are the control.

## 4. Staging sequence (controller, after user approval; game CLOSED)

Mirror of TEST-MATRIX.md L210–223 / ROLLBACK-RUNBOOK §4.2–4.4, adapted for a line
MOVE instead of a mode flip. One bounded pass; fresh FTP connection per transfer;
no DELE; never retry on ambiguity — reconcile instead.

1. **Unfreeze (pre-image)**: RETR `/data/plugins/ploader.ini` → `ploader.ini.pre`;
   sha MUST equal `1216aab61236…` (the pin). Any drift = STOP, reconcile, do not stage.
2. **Rollback artifact**: STOR the pre-image to
   `/data/plugins/ploader.ini.$TAG.bak` (exactly once; RETR-verify sha).
3. **Modify (local, on the pre-image copy)**: use the prepared
   `ploader.ini.variant-pathtest-first` from this dir — do NOT hand-edit. Its sha
   must verify `3cbb8df441f4…` and `diff` vs the pre-image must show exactly the two
   moved lines (nothing else), per the byte-exact law.
4. **Refreeze (upload)**: ONE STOR of the variant bytes to
   `/data/plugins/ploader.ini`; then a FRESH-connection RETR must be byte-identical
   (sha `3cbb8df441f4…`, 440 B — plain-text file, full identity expected; the
   SHT-strip tail-artifact class does not apply to a 440 B ini).
5. **Re-pin (host, same cycle)** — where the old pin is encoded:
   - `worker-console-batch-runner/console_batch.py` **L494**
     `L10["ini_sha256"] = "1216aab6…"` (functional check value; aliased at L508 to
     `L10_INI_EXPECTED_SHA`, used by l10.1 `ini_freeze_fn` at L3900 and l10.5).
     → change to `3cbb8df441f4fe5e9b2c295d3307032b8f171f448a9e7056a75b20f1112a1ad0`.
   - Same file, descriptive strings that name the pin: l10.1 title/gates
     (L3909–3914) and l10.5 title/gates (L4033–4037) say `1216aab6…`; the selftest
     ASSERTS the literal `1216aab6` inside l10.1/l10.5 gates+titles (L5196–5200) and
     inside the stage-launch10 dry-run event stream (anchor list L5212). → update
     these to the new sha in the same edit, then run `console_batch.py selftest`
     and require green. Until this re-pin lands, a later `stage-launch10` phase C
     will loudly REFUSE at l10.1 with zero console writes (safe refusal, not drift).
   - `worker-staging-next/run_stage_next.sh`: **no functional change required** — its
     ini freeze is dynamic (RETR sha at start L155–157, require byte-equal at end
     L212–214, `post` re-check L291–298; it never writes the ini and hardcodes no
     sha; STAGING-PLAN-NEXT.md rows 9/11 state the law generically). It freezes
     whatever ini is present when the pass starts.
   - Record the new pin (`3cbb8df4…`, 440 B) + this card in the run record; the old
     pin remains valid ONLY for the restore direction (step 6).
6. **Then** proceed with whatever else the approved plan stages (PRX/phase C runs
   their own pre-image/.bak/readback laws), and the USER launches (tools never launch).

## 5. Rollback (any failure, ambiguity, or user stop)

ONE bounded restore, no blind retries:
- STOR the saved pre-image bytes (sha `1216aab6…`, §0 authoritative copy) back to
  `/data/plugins/ploader.ini` (the console `.bak` from step 2 is the second artifact).
- Fresh-connection RETR must hash `1216aab61236…` / 440 B; record the restore
  receipt. If the readback diverges: STOP, reconcile, never replay.
- Host re-pin reverts to `1216aab6…` (reverse of §4 step 5) + selftest green.

## 6. Expected observations (one launch; diagnostic loader notifies per plugin)

Per-plugin lines arrive in ini order (`path_test` first, then `composition`), each
with its own ms value; the diagnostic build prints rc on FAIL
(`worker-loader-instrument/build-diagnostic/main.c` L814; decode table
STAGE-CARD.md L93–107). Baseline signature: composition
`FAIL (85ms, rc=0x80020064)` — `0x80020064` = ESTART, kernel attempted the start and
it failed/aborted (accepted far enough to start; NOT a format/stamp rejection).

1. **Composition loads after path_test** (path_test `OK`, then composition `OK` or
   at least no immediate ESTART FAIL, plus composition's own downstream evidence):
   residency-race hypothesis CONFIRMED — order/second-slot is the variable.
2. **Composition still fails immediately**: `FAIL (~85ms, rc=0x80020064)` again ⇒
   residency-race WEAKENED; size/class cause STRENGTHENED (path_test resident first
   did not change the outcome).
3. **Different ms value** (same or different rc): NOTE IT verbatim. Larger ms at
   rc=0x80020064 suggests it progressed further into start; a different rc decodes
   per the STAGE-CARD table (0x800200cb/0x800200d1/0x800200d3 are UNRETAINED ⇒
   UNKNOWN — capture the popup, extend the decode from a pinned public source, do
   not act on guessed semantics).
4. Confounder guard: if **path_test itself** does not come back `OK` first, the
   ordering test is confounded — record honestly, treat the run as a new
   observation, do not re-rate or replay.

No completion/parity claims are made from this card; it stages a measurement.

## 7. Artifacts in this dir

- `STAGING-CARD-INI-ORDER.md` — this card.
- `ploader.ini.retr-20261005-preimage` — fresh RETR of the live ini (the pin, 1216aab6…, 440 B).
- `ploader.ini.variant-pathtest-first` — the exact staging bytes (3cbb8df4…, 440 B).
