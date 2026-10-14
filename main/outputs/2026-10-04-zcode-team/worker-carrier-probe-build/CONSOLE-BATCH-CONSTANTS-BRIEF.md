# CONSOLE-BATCH-CONSTANTS-BRIEF — for the batch tool (worker-console-batch-runner / worker-observer-staging-extension lineage)

From: worker-carrier-probe-build, 2026-10-04. BRIEF ONLY — this worker did
NOT touch `console_batch.py` or any extension file. Adoption is the tool
owner's decision, as a NEW bounded revision with NEW ids (the tool's
delivered laws pin the 6-entry and the definitive 8-entry layouts; the 9th
entry is a NEW layout requiring NEW pinned bytes — the same discipline
FINAL-INI-LAYOUT.md §3.3 established for the xwp/fc1 probes).

## What changes vs the definitive 8-entry law

FINAL-INI-LAYOUT.md pins 8 entries / 7 registered and post-launch gate
"exactly 7 OK lines". With the cc1 carrier probe appended at the NEXT FREE
index (after the igf `= 0` line, CARRIER-CANDIDATES §6.4; never editing any
existing line): **9 entries / 8 registered / post-launch exactly 8 OK
lines, 0 FAIL**. Count law re-verified this session against the pinned
parser facts (modes 0/1/2, 0 = skipped consuming NO slot, no dedupe,
`MAX_PLUGINS = 10`): 8/10, headroom 2
(`worker-carrier-probe-build/receipts/final-ini-count-check.txt`).

## Constants to adopt

1. New artifact dict (same shape as the delivered XWP/QDRV1/OBS dicts):

```python
CC1 = {
    "prx": f"{T}/worker-carrier-probe-build/modbridge_carrier.prx",
    "sha": "d83410aa9069a3039bb4079892b707373ad5d993992b0ed0b55abb1f57730e52",
    "bytes": 26845,
    "console_path": "/data/plugins/modbridge_carrier.prx",
    "bare": f"{T}/worker-carrier-probe-build/determinism/run1/bare-sdk/modbridge_carrier.prx",
    "bare_sha": "e72366c497d8ffa4a76f3f2f01bb898a0d41c2434813429be1408dc2c15a7f71",
    "bare_bytes": 73088,
    "readback_bytes": 71528,
    "readback_body_sha": "1d17a27db4cea8182673360aef06ac2e47dd0eb81993b2226a181b3fd6d6e868",
    "build_id": "cc1-0864d65c1baf",
    "ini_line": "/data/plugins/modbridge_carrier.prx = 2",
    "ini_predecessor": "/data/plugins/modbridge_ingame_facts.prx = 0",
    "log_path": "/data/modbridge-carrier-facts.jsonl",
    "ue4ss_line_prefix": "CC1: build=cc1-0864d65c1baf verdict=",
}
```

2. Ini law updates (9-entry layout; everything else byte-identical):

```python
CARRIER_INI_APPEND = "/data/plugins/modbridge_carrier.prx = 2"  # after the igf =0 line
NINE_ENTRY_WHOLE_FILE_SHA = "fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507"  # 440 bytes
registered_expected_with_carrier = 8      # xwp, fc1, composition, qdrv1, observer, aligned, path_test, carrier
post_launch_ok_lines_with_carrier = 8     # zero FAIL lines
```

3. Evidence gates for the cc1 channel (post-launch):

- `/data/modbridge-carrier-facts.jsonl` NEW lines with
  `"build_id":"cc1-0864d65c1baf"`, marker `modbridge.ingamefacts`, every
  line strict JSON < 1 KiB; PASS = seq1 `entry`, seq2 `startup` (polls 30,
  poll_s 4), seq3 `mapscan`, 30 `qh_counter` lines, `census_match` lines,
  `census_result` (`verdict:"CENSUS:VALIDATED"`, controls
  `None_id_zero:1` + `ByteProperty_ascii_ok:1`,
  `negative_hits.PostRender:0`), `summary` (`write_failures:0`).
- `/data/plugins/UE4SS.log`: exactly ONE new `CC1:` line per launch.
- Preflight: FAIL if a `modbridge_carrier` line already exists in the
  pre-image (no dedupe law); stat baselines for both evidence files;
  `ploader.ini.<TAG>.bak` backup as usual.
- `worker-final-ini-layout/verify_final_ini_layout.py` will print
  FINAL-INI-FAIL on the 9-entry bytes until its owner adopts the new law —
  EXPECTED, not a staging defect; use
  `worker-carrier-probe-build/receipts/final-ini-count-check.txt` +
  the single-appended-line diff law meanwhile.

Full staging ritual, record shapes, refusal vocabulary and rollback:
`worker-carrier-probe-build/STAGE-PLAN.md` (proposal-only; the standing
user-launch ritual applies to every console step).
