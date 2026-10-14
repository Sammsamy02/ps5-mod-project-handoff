# REPORT — worker-h1-xwp-pin (2026-10-05)

Status: **DELIVERED** — H-1 closed at the tool level. `console_batch.py` now
pins the xwp-armed slot to the ROW-0 REBUILD, the constants were proven against
the artifacts with the tool's own law before pinning, and the wrong-build /
build-id-mismatch / correct-staged behaviors are all enforced AND tested.
LOCAL ONLY: zero console contact, zero launches, zero ini writes; the original
`worker-writability-probe-build/` and all launch-10 artifacts untouched.

## Task (from worker-probe-pairing-prep/REPORT.md H-1)

`console_batch.py` pinned `XWP_ARMED` to the OLD launch-9 build
(`igd1-6ebab07f2111`, fself `48949316…`). A probe-window batch flow staging
"xwp" as-is would upload the OLD bytes under the SAME console filename and
self-verify them PASS — reproducing launch-9's all-skip evidence instead of the
row-0 verdict. Fix: pin update under NEW ids + seq-1 build_id post-stage check,
with tests.

## 1. Constants verified BEFORE pinning (tool's own law, not the brief)

| constant | pinned value | verification |
|---|---|---|
| fself sha/bytes | `e1541af6…` / 20013 B | shasum recompute on `worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx` — exact |
| bare-sdk sha/bytes | `cc544d6e…` / 69464 B | shasum recompute on `determinism/run-armed-1/bare-sdk/…` — exact |
| readback length | 67832 B | `sht_prefix_end(bare)` == 67832 (console_batch.py snippet semantics), BOTH determinism runs |
| readback body sha | `e3f13d19…` | `sha256(bare[64:67832])` == pin, BOTH runs (run1==run2) |
| build_id | `igd1-e030a09eb4d1` | embedded verbatim in fself wrapper AND bare ELF; recorded in `build-receipt-armed.json` |
| structure | — | row0's frozen `check_xwp_artifact.py determinism/run-armed-1 armed` rc=0 (8 strong imports, 0 syscalls, mprotect typed once) |

## 2. Pin updated (console_batch.py)

`XWP_ARMED` → row-0 rebuild paths/hashes/`igd1-e030a09eb4d1`, with provenance
comment (supersedes launch-9 per H-1; verification method recorded). Keys that
must NOT move didn't: 20013 B, 69464 B, 67832 B, `target_prx`, `module_line`,
`log` — same console filename, same evidence sink. Flow text updated where the
old constants were printed (stage-ridealongs notes, ride.0 preflight, ride.1
§3 cross-check gate — now explicitly "a launch-9 readback daea2ff4… REFUSES
here", ride.6 upload gate — "NEVER the launch-9 48949316… bytes under this
console name"), ge.3b title, game-evidence receipt, selftest anchors.

## 3. Seq-1 build_id post-stage check (the runbook requirement)

`probe_evidence_verdict` now treats the seq-1 `entry` record as the staged
build identity: wrong value → loud **WRONG BUILD FAIL** naming the observed id
(re-stage, never re-rate); fresh records with no seq-1 entry → honest **WARN**
"staged build identity UNVERIFIED"; the observed identity lands in
`probe-verdicts.json` as `seq1_entry_build_id` — the on-console build identity
the runbook must record. The pre-existing foreign-build_id FAIL and all
fc1/cc1 behavior are unchanged.

## 4. Tests (all three mission cases covered, driving the REAL engine)

- **wrong-build refused**: selftest N feeds a launch-9-law readback (old bare
  ELF prefix, e_shnum zeroed) to `sht_strip_readback_law` → FAIL; fake-drive
  `order-law-oldbuild` serves the stale bytes through the fake ftpsrv channel →
  ride.6 FAILS, guard+publish SKIPPED, zero ini STORs, old ini byte-intact,
  exactly STOR + ONE re-STOR then abort.
- **build-id mismatch refused**: selftest K + fake-drive `ge-wrongbuild` feed a
  seq-1 entry carrying `igd1-6ebab07f2111` to the REAL `ge.3b` step → FAIL,
  refusal names the stale id, receipt records it.
- **correct staged**: fake-drive happy paths stage the row-0 bytes —
  `xwp-readback-verdict.json`: `READBACK-OK len=67832 body-sha=e3f13d1994a1`;
  green game-evidence sink carries seq-1 entry `igd1-e030a09eb4d1` → PASS with
  the identity receipt. `XWP_STAGES` fixture corrected to the pinned
  entry/startup grammar (build ids always derived from `cb.XWP_ARMED`).

## 5. Suite evidence (full capture in SELFTEST-RECEIPT.txt)

| suite | pre-edit baseline | post-edit final |
|---|---|---|
| `python3 console_batch.py selftest` | 462 ok, 0 failed | **474 ok, 0 failed** (exit 0) |
| `python3 tests/fake_console_drive.py` | 0 failed checks | **153 ok, 0 failed** (exit 0) |

Net +12 selftest checks, +11 fake-drive checks — all xwp-scoped. Every
pre-existing check passes with unchanged expectations (the only modified
expectations are the xwp fixture grammar and the xwp verdict message string).

## 6. Scope discipline

Exactly three files changed, all in `worker-console-batch-runner/`:
`console_batch.py`, `tests/fake_console_drive.py`, `USER-SESSION-GUIDE.md`
(doc-only). Behavior-bearing edits are only the `XWP_ARMED` constants and the
xwp section of the evidence classifier. `diff.patch` (sha `b9a43176c4f929e7…`)
is a real unified diff against the reconstructed pre-edit snapshots (git HEAD
predates the return session; reverse-apply of the exact recorded edits, each
asserted count==1, snapshots py_compile clean).

## Honest limits

- Host-side only: whether `igd1-e030a09eb4d1` actually executes on target is a
  future probe-window fact; the seq-1 entry record will prove it then.
- `native_executed=false`, `in_game_results=false`; sceKernelMprotect remains
  NOT console-proven (row0 CLAIM limits carried, not widened).
- The classifier's new WARN/FAIL branches classify evidence; they never
  certify writability (existing law kept verbatim).
- No other worker's dirs modified; consumed `worker-igw-row0-rebuild/` and
  `worker-writability-probe-build/` read-only.

## Deliverables (this dir)

| file | what |
|---|---|
| `CLAIM.json` | machine-readable claim (marker `h1.xwp.pin/v1`) |
| `DIFF.md` | human diff guide + scope argument |
| `diff.patch` | full unified diff vs pre-edit snapshots |
| `SELFTEST-RECEIPT.txt` | full captured output of both suites (post-edit, exit 0) |
| `REPORT.md` | this report |
