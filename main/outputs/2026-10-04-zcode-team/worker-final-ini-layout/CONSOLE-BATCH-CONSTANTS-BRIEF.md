# CONSOLE-BATCH-CONSTANTS-BRIEF — for worker-observer-staging-extension

> **SUPERSEDED (NINTH SLOT, 2026-10-04):** adopt
> `worker-carrier-probe-build/CONSOLE-BATCH-CONSTANTS-BRIEF.md` instead —
> 9 entries / 8 registered / EXACTLY 8 OK lines, canonical 440 B `fd9c7393…`,
> adopted into `console_batch.py` by `worker-ninth-slot-ini-adoption`
> (selftest 405 ok / 0 failed). This 8-entry brief (7 registered / EXACTLY 7 OK
> / 400 B `bbc34c16…`) is preserved as the historical record, unedited below.

From: worker-final-ini-layout, 2026-10-04. BRIEF ONLY — this worker did NOT
touch `worker-console-batch-runner/console_batch.py` or any extension file.
Adoption is the extension worker's decision, under its existing ownership
chain (its CLAIM.json shows status complete for the 6-entry law; adopting the
definitive 8-entry law is a NEW bounded revision with NEW ids).

## What changes vs the delivered 6-entry law

`console_batch.py` (as delivered, sha fd21d171…) pins, in `COMBINED_INI`:
`expected_existing` = 4 lines (aligned_entry 2, path_test 2, igf 0,
composition 2), qdrv1/observer appended, `registered_expected_combined = 5`,
`registered_expected_observer_only = 4`, end-state gate "header once; 4
expected lines pinned modes; qdrv1 + observer = 2" and post-launch 5-OK-line
gate. The definitive return-session layout (FINAL-INI-LAYOUT.md §1) makes
these stale: 8 entries, 7 registered, probes first, aligned_entry/path_test
relocated AFTER observer.

## Constants to adopt

1. New artifact dicts (same shape as the delivered OBS/QDRV1 dicts):

```python
XWP_ARMED = {
    "prx": f"{T}/worker-writability-probe-build/modbridge_exec_write_probe_armed.prx",
    "sha": "48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e",
    "bytes": 20013,
    "bare": f"{T}/worker-writability-probe-build/determinism/run-armed-1/bare-sdk/modbridge_exec_write_probe.prx",
    "bare_sha": "cfed5e3d7e62100649fe1221cd4eaaf7554df421f08792264ae5238acb6d0830",
    "bare_bytes": 69464,
    "readback_bytes": 67832,
    "readback_body_sha": "daea2ff4a6533fa9006efeeb1b24913a586b72c0f68826f563b9479bf6ef561b",
    "target_prx": "/data/plugins/modbridge_exec_write_probe_armed.prx",
    "module_line": "/data/plugins/modbridge_exec_write_probe_armed.prx",
    "log": "/data/modbridge-exec-write-probe.jsonl",   # + UE4SS mirror lines
}
FC1POOL = {
    "prx": f"{T}/worker-fc1-validator-probe/modbridge_fc1pool.prx",
    "sha": "d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96",
    "bytes": 22733,
    "bare": f"{T}/worker-fc1-validator-probe/determinism/run1/bare-sdk/modbridge_fc1pool.prx",
    "bare_sha": "54f8335e6ed476a64e5bc0d65741c83808e52f79b55631a0803a410bd2b3f8a2",
    "bare_bytes": 69408,
    "readback_bytes": 67848,
    "readback_body_sha": "7e2dce27e415bea80c3100d74e6f58c9a2cd8697bc87e03b67226fba09646ed3",
    "target_prx": "/data/plugins/modbridge_fc1pool.prx",
    "module_line": "/data/plugins/modbridge_fc1pool.prx",
    "log": "/data/modbridge-fc1pool-facts.jsonl",      # + one FC1POOL: UE4SS line
}
```
(readback constants host-recomputed this session from the bare-sdk ELFs under
the same SHT-strip law; run1 == run2 for both.)

2. Replace `COMBINED_INI` with `FINAL_INI`:

```python
FINAL_INI = {
    "ini": "/data/plugins/ploader.ini",
    "header": "[PPSA33286]",
    "expected_existing": {   # pinned modes; never rewritten, only validated
        "/data/plugins/modbridge_exec_write_probe_armed.prx": 2,
        "/data/plugins/modbridge_fc1pool.prx": 2,
        "/data/plugins/modbridge_supplier_composition.prx": 2,
        "/data/plugins/modbridge_qh_driver.prx": 2,
        "/data/plugins/modbridge_runtime_aligned_observer.prx": 2,
        "/data/plugins/modbridge_runtime_aligned_entry.prx": 2,
        "/data/plugins/modbridge_path_test.prx": 2,
        "/data/plugins/modbridge_ingame_facts.prx": 0,
    },
    "anchor_of": {   # content-anchored ensure (FINAL-INI-LAYOUT.md §3.3);
                     # NOT tail-append — converges byte-for-byte in any order
        "/data/plugins/modbridge_exec_write_probe_armed.prx": "[PPSA33286]",
        "/data/plugins/modbridge_fc1pool.prx": "/data/plugins/modbridge_exec_write_probe_armed.prx",
        "/data/plugins/modbridge_qh_driver.prx": "/data/plugins/modbridge_supplier_composition.prx",
        "/data/plugins/modbridge_runtime_aligned_observer.prx": "/data/plugins/modbridge_qh_driver.prx",
        "/data/plugins/modbridge_runtime_aligned_entry.prx": "/data/plugins/modbridge_runtime_aligned_observer.prx",
        "/data/plugins/modbridge_path_test.prx": "/data/plugins/modbridge_runtime_aligned_entry.prx",
        "/data/plugins/modbridge_ingame_facts.prx": "/data/plugins/modbridge_path_test.prx",
    },
    "registered_expected_final": 7,          # was 5 (mode-0 igf consumes no slot)
    "registered_expected_partial": "<n active lines present>",  # per-flow subset
    "canonical_sha256": "bbc34c16621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1",
    "canonical_bytes": 400,
    "canonical_template": f"{T}/worker-final-ini-layout/ploader.ini.canonical",
    "final_verifier": f"{T}/worker-final-ini-layout/verify_final_ini_layout.py",
}
```

3. Behavioral adoptions:
   - END-STATE GATE: byte-exact equality with the canonical template (sha
     above) via the final verifier, replacing the "5 registered" count gate;
     keep every existing STOP class (duplicate, missing expected, mode
     divergence, unknown line) — they are all still correct.
   - BOOTSTRAP RECONCILIATION (new, once): from the current console pre-image
     (aligned_entry/path_test historically BEFORE composition), appends alone
     cannot reach canonical; flow = Step-0 RETR + .bak STOR + ONE whole-file
     STOR of the canonical template + RETR byte-exact diff, then ensure-ops
     only. Gate it as its own verdict (e.g. `INI-BOOTSTRAPPED`), never a
     silent rewrite.
   - POST-LAUNCH GATE: expect exactly **7** `OK (<ms>): <filename>` lines and
     zero FAIL (was 5).
   - STAGING ORDER INDEPENDENCE: the new flow should tolerate stager steps
     running in any order (ensure ops are idempotent + order-commutative;
     proven over all 40320 permutations by
     `worker-final-ini-layout/simulate_ploader_ini_final.py`).
   - selftest/drive: add the two probes' readback-law sections (67832/67848 B
     bodies) and an order-permutation drive case (ensure ops in >=2 different
     orders reach the same bytes); keep all 271 prior checks green.

## Evidence-path additions for game-evidence flows

`/data/modbridge-exec-write-probe.jsonl` (xwp-armed verdicts; envelope
build_id `igd1-6ebab07f2111`) and `/data/modbridge-fc1pool-facts.jsonl`
(fc1pool1-78508a2e2940; success = `fc1pool_result` verdict `validated`
11/11 + single `FC1POOL:` UE4SS line). Success signatures per the probes'
own stage plans; new files, single-writer each.
