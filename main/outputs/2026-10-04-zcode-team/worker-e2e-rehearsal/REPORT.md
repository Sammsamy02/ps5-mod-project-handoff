# REPORT.md — worker-e2e-rehearsal: full staged E2E rehearsal on host

Origin: worker-e2e-rehearsal, 2026-10-04 (ZCode team, PS5 mod-manager).
Scope: rebuild the newest wiring from vendored copies inside THIS directory
and run the complete staged flow end-to-end on host (download -> prepare ->
install -> activation-records -> uninstall -> clean-state verify), capturing
the e2e-status.jsonl transcript. Host-side only; no network; no console; no
original file modified; all outputs under
`outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/`.

## 1. What ran (commands, in order)

```
# vendoring (copies only, into this dir)
cp ../worker-e2e-final/compose_e2e.py .
cp ../worker-e2e-final/owned/* owned/
cp ../worker-e2e-final/test/smoke_host.sh ../worker-e2e-final/test/smoke_e2e.sh test/

# build (patch chain -> adapted inner script -> both flavors)
MODBRIDGE_ROOT=<worktree root> python3 compose_e2e.py --flavor host     # OK
MODBRIDGE_ROOT=<worktree root> python3 compose_e2e.py --flavor console  # OK

# regression on MY rebuilt binary
sh test/smoke_host.sh   # PASS=26 FAIL=0
sh test/smoke_e2e.sh    # PASS=31 FAIL=0

# the staged rehearsal (driver committed in this dir)
sh run_rehearsal.sh     # PASS=61 FAIL=0 INFO=3, exit 0
```

`run_rehearsal.sh` executes, per E2E-RUNBOOK-V2 + CONSOLE-E2E-PLAN:
S0 baseline -> S1 `download --job` (file:// stub transport, REAL sink +
records; corpus = saved `minimap-v3.1.zip`, sha cross-checked against
worker-6's zip-roundtrip pinned value) -> S2a `prepare-archive --mod` (real
libarchive intake, durable records transition, idempotency) -> S2a probe
`install --mods <archive-mod>` (expected refusal) -> S2b `make-bundle` +
`register-mod --bundle` + `prepare` -> S3 `install` (transactional) -> S4/5
`simulate-runtime` activated/mods-started (ack-file format proof only;
running derives ONLY from the runtime status file) -> S6 `install --remove`
(one mod, then all) -> S7 clean-state verification + transcript snapshot.

## 2. Evidence (all paths in this directory)

| Artifact | Meaning |
|---|---|
| `rehearsal/summary.log` | 61 PASS / 0 FAIL / 3 INFO, per-stage |
| `rehearsal/console.log` | full command transcript (every run + exit code + tool output) |
| `rehearsal/e2e-status.jsonl` | the bounded stage-event transcript: 44 lines = 21 begins, 20 ends (0 failed), 3 status events (stage-1 ok w/ sha, two stage-2 ok w/ manifest), 1 unmatched begin = the expected S2a refusal probe |
| `rehearsal/records.txt` | final durable state: 3 mods all `prepared`, game generation 3, load-list sha |
| `rehearsal/download-sidecar.json` | content-addressed download record (sha, bytes, no URL) |
| `composition/receipts/smoke-host.log`, `smoke-e2e.log` | 26/26 and 31/31 on the rebuild |
| `composition/receipts/*`, `e2e-receipts/*` | regenerated provenance (vendor manifest, patch chains) |

Key transcript lines (from `rehearsal/e2e-status.jsonl`):

```
{"event":"status","stage":"stage-1-download","outcome":"ok","detail":"cf3c353b…f377"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}
```

Provenance cross-check: rebuilt `composition/build/host/modinst_composed`
sha256 `c1a594c33ce74005a572c5ad7b9cf37731b6261e6252b3ee5c77a823e1db048e`
== worker-e2e-final's pinned value (bit-identical rebuild); regenerated
`composition_e2e_inner.py` sha `606a8a98…` == pinned. Vendored inputs
(`compose_e2e.py`, `modinst_download.c/h`, `mb_curl_stub_host.c`) match the
pinned `e2e-receipts/final-provenance.json` shas.

## 3. Findings

- **F-R1 (known limitation, now empirically confirmed on the flow):** a
  downloaded real zip (minimap corpus) downloads and quarantines fine, but
  `install --mods <archive-mod>` refuses: exit 2,
  `error archive_unreadable bundle for mod '?' rejected: not a modforge
  bundle`. There is no quarantine->mfb bridge, so the "download -> prepare
  -> install" end-goal for a REAL archive needs supplier-provided `.mfb`
  bundles (2b path, proven end-to-end here) or a new archive->bundle adapter.
  Not fixed (new component work, not a small patch). Console run must choose
  its path; see READINESS §4.
- **F-R2 (new, minor, transcript honesty):** `install`'s refusal path exits
  inside `cmd_install` (e.g. bad bundle) BEFORE the dispatch emits
  `e2e_status_end`, leaving an unterminated `begin` line in
  e2e-status.jsonl (21 begins vs 20 ends in my transcript; the unmatched one
  is exactly the S2a refusal probe). Same class as worker-e2e-final's F2
  (prepare-archive), unfixed there for install: a real fix means routing
  cmd_install's internal `exit(EX_*)` calls through the status end path —
  not a small change, so recorded, not patched. Readers already treat a
  missing end line as "command died" (RUNBOOK), so semantics stay honest.
  Also noted: `install` emits no terminal `status` event (download and
  prepare-archive do); harmless, but a console watcher should pair
  begin/end, not expect an install status event.
- **F-R3 (provenance observation):** my rebuilt console payload ELF is
  664400 bytes with 116 imports, 0 missing — same facts as e2e-final's
  receipt — but its sha256 (`fb59d0c9413e64498576e718c630de93b45bc201fd557a
  8a843dc0bdbf209678`) differs from the pinned `3860cd56…` (the console link
  is not bit-reproducible; the host binary is). Recorded so the console run
  accepts either sha after re-verifying size/imports, or re-pins.
- **Fixed in MY vendored copy (driver bug, not a product defect):** my first
  rehearsal pass invoked `make-bundle` with the `--state/--roots` prefix;
  `make-bundle` must be `argv[1]` (it bypasses the store's flag loop —
  worker-7 `modinst.c` main). Symptom: usage dump, exit 2, cascade of
  downstream failures. Fixed in `run_rehearsal.sh` (runs 2 and 3 clean);
  first-pass log not retained — the committed artifacts are from the final
  clean run only.

## 4. Result

Full staged flow END-TO-END on host with the newest wiring: **complete,
exit 0, 61/61 stage checks pass** (plus 26/26 + 31/31 regression suites on
the same rebuild). Readiness verdict, F1 console impact, precise console-only
steps and the go/no-go recommendation: see `READINESS.md`.

Bounds respected: ≤40 tool calls, no nested subagents, no network, no
console access, existing files untouched (vendored copies only; the only
writes outside this dir were the suites' ephemeral `$TMPDIR` trees).
