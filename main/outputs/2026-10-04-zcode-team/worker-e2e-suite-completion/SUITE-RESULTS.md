# SUITE-RESULTS.md — host suite re-runs on the build-r1 (config-entry) chain

Worker: worker-e2e-suite-completion, 2026-10-04. Host-only; no console
contact. This closes the NOT-RUN items (1) of
`worker-e2e-payload-build/PAYLOAD-DELIVERY.md` sec 4 (suite re-runs were
explicitly left to controller/staging work; the controller directed this
worker to finish them host-side).

## 0. What was tested (the binary + its chain)

The build-r1 console payload `modbridge-installer.payload.elf`
(`a21771f8…`, 681888 B) cannot run on the host; per tasking the suites run
against its HOST flavor, re-derived byte-exactly in this worker dir:

| artifact | sha256 | check |
|---|---|---|
| pristine 51-tuple core (copied from `worker-d1-chain-relink/composition/vendored/modinst_core.c`) | `45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d` | matches pinned pre-state |
| + `worker-e2e-config-entry/patch/modinst_core-config-entry.diff` (`4bab84ab…`, `patch -p1`) | `43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0` | BYTE-EXACT vs owned (cmp true) |
| host .o (D1 `host-build.json` compile argv verbatim, 3 pre-existing warnings, 0 new) | `51b881715d9a43f7c29886622f5d74becee5b38f02a93198f02549688c52ec40` | byte-exact vs pinned |
| **host binary under test** `build-host/modinst_composed_cfgentry` (D1 link argv: 12 D1 host .o + libarchive/liblzma/libz/libmbedcrypto) | **`4659c24c4a4d829dc13a59813e7b137bf4d0efeaf0bca45b21e064b589eb2aa1`** | **BYTE-EXACT vs `worker-e2e-config-entry/host-test/build/modinst_composed_cfgentry`** — the delivery's declared host proof transfer holds |

So every suite below drove exactly the source revision that the build-r1
payload ships (D1 merged 51-tuple chain + the 3 g1-cmd-entry tuples; chain
endpoint `43ea66c1…` independently re-verified in `promotion/`).

Harness adaptation (path-only, no check logic touched): each runner copy in
`test/` differs from its origin ONLY in `LOG`/`SUM`/`RUN` output paths (see
REPORT.md); `BIN` resolves through
`composition/build/host/modinst_composed -> build-host/modinst_composed_cfgentry`
(symlink). Origins: bridge75 = `worker-zip-to-mfb/test/run_bridge_tests.sh`;
fr2-87 = `worker-d1-chain-relink/test/run_bridge_tests_fr2.sh`;
fault-186 = `worker-d1-chain-relink/test/run_fault_tests_merged.sh`;
config-entry = `worker-e2e-config-entry/host-test/host_cmd_entry_patched_test.py`.

## 1. Results

| # | suite (design sec 8.6) | runner (my copy) | PASS | FAIL | exit | summary log | full log |
|---|---|---|---|---|---|---|---|
| 1 | Bridge TU suite, zip-to-mfb lineage (75 checks: 3 real-mod flows download->prepare(auto-bridge)->install->ack->uninstall, negative-corpus refusal, --no-auto-bundle opt-out, transcript sanity) | `test/run_bridge_tests_75.sh` | **75** | **0** | 0 | `test/bridge75-e2e-summary.log` | `test/bridge75-e2e.log` |
| 2 | F-R2 archive suite (87 checks: 75 + matched begin/end on refusals, terminal stage-3-install status events, byte-exact refusal line, zero unmatched begins) | `test/run_bridge_tests_fr2_87.sh` | **87** | **0** | 0 | `test/fr2-87-e2e-summary.log` | `test/fr2-87-e2e.log` |
| 3 | Fault matrix, merged-chain revision (186 checks: crash at every uninstall journal/transaction boundary + recovery invariants I1-I4, interleavings, power-loss sims, quarantine-retained pin, per-case unmatched-begin sanity) | `test/run_fault_tests_merged_186.sh` | **186** | **0** | 0 | `test/fault-186-e2e-summary.log` | `test/fault-186-e2e.log` |
| 4 | Config-entry host suite (compiled-in entry: 5 bare-dispatch, 15 dispatch replay via `--e2e-cmd` == argv twins w/ transcript sha `31a4764a…`, recover probe, stale re-dispatch, argv-wins, 14-case refusal matrix — 13x exit 6 + absent 2, valueless flag) | `host-test/host_cmd_entry_patched_test.py` | **OVERALL: PASS** (46 OK check lines, 0 bad) | **0** | 0 | `host-test/HOST-TEST-RECEIPT.md` | `host-test/host-test.log` |

TOTAL: **348 checks passed, 0 failed** across the four suites; no fixing-by-edit
was needed or performed (program under test never edited; harness diffs are
output-path renames only).

## 2. Cross-checks against the pinned 51-tuple runs

The 51-tuple D1 chain's receipts (`worker-d1-chain-relink/test/*-summary.log`)
list the same checks for the pre-config-entry core. PASS-line comparison
(after stripping the verdict word):

- bridge75: IDENTICAL check-lines vs `bridge75-summary.log`
- fr2-87: IDENTICAL vs `fr2-87-summary.log`
- fault-186: IDENTICAL vs `fault-merged-186-summary.log`
- config-entry: IDENTICAL check-lines vs
  `worker-e2e-config-entry/host-test/HOST-TEST-RECEIPT.md` (the original
  32-check-class receipt; 46 OK lines in both)

i.e. the config-entry revision changed NOTHING observable by these suites —
exactly the delta-zero requirement of design sec 6/8.6 (the new entry code is
reachable only through the bare/`--e2e-cmd` slots; argv-dispatch paths and
transcripts are byte-identical, with transcript sha `31a4764a…` re-proven in
suite 4).

Expected non-zero notes (not failures): the neg/off fault-suite transcripts
show `1 failed ends` each — the REFUSAL end lines the suites deliberately
assert (`PASS sanity … unmatched=1 expected=1` where pinned); zb-neg/zb-off
refusals (exit 2) are the designed negative cases.

## 3. Honest scope

- All four suites are HOST-side evidence on `4659c24c…` (host flavor of the
  build-r1 sources). The CONSOLE payload (`a21771f8…`, 681888 B) and RUN-CONFIG
  B behavior remain [TARGET] until the controller's staged dispatch (console
  E2E staging is still PENDING; user unavailable — not attempted, zero console
  contact).
- The 16-byte `.note.gnu.build-id` per-link nondeterminism applies to console
  relinks only; the host link is byte-deterministic (proven by cmp).
- Scratch state written by the suites lives under `test/run*` and
  `host-test/scratch/` in this worker dir; frozen trees untouched.
