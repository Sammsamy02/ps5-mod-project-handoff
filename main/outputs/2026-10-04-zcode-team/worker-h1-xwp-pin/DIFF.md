# DIFF — worker-h1-xwp-pin (2026-10-05)

H-1 fix: `console_batch.py` `XWP_ARMED` re-pinned from the OLD launch-9 build
(`worker-writability-probe-build`, `igd1-6ebab07f2111`, fself `48949316…`) to the
ROW-0 REBUILD (`worker-igw-row0-rebuild`, `igd1-e030a09eb4d1`, fself `e1541af6…`),
constants verified against the artifacts with the tool's own law BEFORE pinning,
plus the seq-1 build_id post-stage check and the wrong-build/mismatch/correct
test coverage. The full machine-readable change is **`diff.patch`** in this dir —
a real unified diff against the reconstructed pre-edit snapshot (see §5).

Files touched (exactly three, all inside `worker-console-batch-runner/`):

| file | +lines | −lines | nature |
|---|---|---|---|
| `console_batch.py` | 175 | 49 | pin constants + flow text + seq-1 evidence check + selftest |
| `tests/fake_console_drive.py` | 76 | 6 | fixture grammar + 2 new H-1 scenarios + green identity check |
| `USER-SESSION-GUIDE.md` | 10 | 4 | doc-only: stale pin constants brought to the new pin |

## 1. The pin (console_batch.py `XWP_ARMED`)

```diff
-XWP_ARMED = {
-    "prx": f"{T}/worker-writability-probe-build/modbridge_exec_write_probe_armed.prx",
-    "sha": "48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e",
-    "bare": f"{T}/worker-writability-probe-build/determinism/run-armed-1/bare-sdk/…",
-    "bare_sha": "cfed5e3d7e62100649fe1221cd4eaaf7554df421f08792264ae5238acb6d0830",
-    "readback_body_sha": "daea2ff4a6533fa9006efeeb1b24913a586b72c0f68826f563b9479bf6ef561b",
-    "build_id": "igd1-6ebab07f2111",
+XWP_ARMED = {
+    "prx": f"{T}/worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx",
+    "sha": "e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df",
+    "bare": f"{T}/worker-igw-row0-rebuild/determinism/run-armed-1/bare-sdk/…",
+    "bare_sha": "cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9",
+    "readback_body_sha": "e3f13d1994a1b4d27de05e31885d5a5cf9ea91daea3c57c2c229afd3696c295e",
+    "build_id": "igd1-e030a09eb4d1",
```

Unchanged keys: `bytes` 20013, `bare_bytes` 69464, `readback_bytes` 67832,
`target_prx`, `module_line`, `log` — same console filename, same evidence sink.
Provenance comment rewritten: supersedes launch-9 per worker-probe-pairing-prep
H-1, verified with THIS tool's law before pinning.

**Verification that preceded the pin** (tool's own law, on-disk artifacts,
NOT the brief's claims): `shasum` recompute of fself + bare (exact match);
`sht_prefix_end(bare)` == 67832 and `sha256(bare[64:67832])` == `e3f13d19…`
(snippet semantics, recomputed for BOTH determinism runs, run1==run2); build-id
string `igd1-e030a09eb4d1` found embedded in BOTH the fself wrapper and the bare
ELF; row0's frozen `check_xwp_artifact.py determinism/run-armed-1 armed` rc=0.

## 2. Seq-1 build_id post-stage check (probe_evidence_verdict, ge.3b)

- Fresh xwp records are scanned for the seq-1 `entry` record (the staged build
  identity per EVIDENCE-CONTRACT seq 1).
- seq-1 entry present with `build_id != igd1-e030a09eb4d1` → **FAIL**: "WRONG
  BUILD — seq-1 entry record build_id <observed> != pinned igd1-e030a09eb4d1
  …re-stage the pinned row-0 rebuild, never re-rate this evidence". This is the
  H-1 refusal: the old bytes ran under the same console filename.
- Fresh records but NO seq-1 entry record → **WARN** "staged build identity
  UNVERIFIED this launch" (honest; recorded refusals stay terminal evidence).
- `info["xwp"]["seq1_entry_build_id"]` recorded into `probe-verdicts.json` —
  the runbook's "record the on-console build identity actually staged".
- The generic foreign-build_id FAIL branch is preserved for non-seq-1 records.
- ge.3b step title + game-evidence receipt strings updated to the new build id.

## 3. Flow text updates (same law, new constants)

`stage-ridealongs` fl.notes, ride.0 title/note, ride.1 gate (+ "a launch-9
readback daea2ff4… REFUSES here — H-1 wrong-build law"), ride.6 title/gates
("local sha == e1541af6… BEFORE upload (BUILD-ID law; NEVER the launch-9
48949316… bytes under this console name)"). Selftest A + game-evidence anchor
lists updated (`e1541af6`, `e3f13d19`, `igd1-e030a09eb4d1`; `48949316` retained
— it now appears in the refusal gate text).

## 4. Tests

- **selftest K** (classifier fixtures): xwp fixture corrected to the pinned
  grammar (seq-1 `entry`, seq-2 `startup`); +6 cases: green identity receipt,
  no-identity-when-no-fresh-records, growth-with-entry, WRONG BUILD on the old
  launch-9 id, generic foreign id on a non-seq-1 record, WARN-unverified.
- **selftest N** (NEW, 7 checks): pin == on-disk artifact (fself + bare, via
  `local_sha`); §3 constants recompute from the pinned bare; build-id embedded
  in both artifacts; **WRONG BUILD refused** — a launch-9-law readback (built
  from the OLD bare ELF, e_shnum zeroed) FAILS `sht_strip_readback_law` against
  the row-0 pin ("never self-verifies PASS"); **CORRECT staged** — the row-0
  readback PASSES its own law; pin/superseded-id inequality.
- **fake-console drive** (+11 checks, driving the REAL engine):
  - `order-law-oldbuild` scenario: fake console serves the LAUNCH-9 prefix for
    the xwp readback while the pin is row-0 → ride.6 FAILS, guard + publish
    SKIPPED, no ini STOR, console ini byte-identical, exactly initial STOR +
    ONE re-STOR then abort. This is the exact H-1 hazard, now refused.
  - `ge-wrongbuild` scenario: the real `ge.3b` step fn FAILS on a seq-1 entry
    carrying `igd1-6ebab07f2111`; refusal names the stale id; probe-verdicts.json
    records it. Plus `ge-noentry` WARN case and the green-path identity receipt.
  - `XWP_STAGES[1]` fixture corrected to entry/startup/attempt (build ids always
    derived from `cb.XWP_ARMED`, so the fixture tracks the pin).

## 5. Provenance of diff.patch

git HEAD predates the whole return session (the tool dir's evolution is
uncommitted), so the pre-edit baseline was reconstructed by reverse-applying the
exact recorded edit pairs (every pair asserted exactly one match; snapshots
`py_compile` clean). Pre-edit suite state on that baseline: selftest 462 ok /
0 failed, fake-drive 0 failed. `diff.patch` = `difflib.unified_diff` of the
reconstructed snapshots vs the delivered files.

| sha256 | file |
|---|---|
| `b982f794201a0d4d…` | pre-edit console_batch.py (snapshot) |
| `f2134f0a1b501112…` | pre-edit tests/fake_console_drive.py (snapshot) |
| `5b60980dae2c84c9…` | pre-edit USER-SESSION-GUIDE.md (snapshot) |
| `d1614c80aba15505…` | delivered console_batch.py |
| `355eea969f5f8b73…` | delivered tests/fake_console_drive.py |
| `386aed4e1c4c0439…` | delivered USER-SESSION-GUIDE.md |
| `b9a43176c4f929e7…` | diff.patch |

## 6. Behavior-change scope

Behavior-bearing edits are ONLY: (a) the `XWP_ARMED` constants — they flow into
stage-ridealongs/`all` preflight, §3 prefix cross-check, upload/readback law and
handoff text for the xwp slot; (b) the xwp section of `probe_evidence_verdict`
(ge.3b). The fc1/cc1/qdrv1/observer classifier sections, the ini laws, the
publish guard, restore-ini, launch-10, settings, dll7/dll8/e2e flows are
byte-untouched, and every one of their pre-existing fake-drive checks passes
with unchanged expectations (153 ok / 0 failed post-edit; selftest 474 ok /
0 failed).
