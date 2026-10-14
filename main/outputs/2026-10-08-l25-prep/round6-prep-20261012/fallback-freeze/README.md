# fallback-freeze (2026-10-12, Mac-only, revision 2)

Track: make a fallback launch instant if B4 misbehaves. **Read FALLBACKS.md
first.** It has:
- the order to step back (B4 -> B3 is the plan; the rest is a proposal)
- the full shas and frozen paths, with the verify results
- what each build lacks compared with B4
- how the path shim behaves on the fallbacks: it switches itself off, as on
  1e49188b (corrected in revision 2)
- the log lines to expect on a fallback launch
- the steps required before a live upload
- the exact dry-run commands

Frozen copies (new, read-only, same layout as the B4 frozen dir):

- `outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/`
- `outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/`
- `outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/`

There was no console contact, no PRX build and no git change. The existing
B4 and 1e49188b frozen dirs and `harvest/markers-declared.json` were only
read. One small Mac host-test compile was run (a single file, after checking
free memory).

## Files in this dir

| File | What it is |
|---|---|
| `FALLBACKS.md` | The main result (revision 2) |
| `freeze_fallbacks.py` | Script that made the frozen copies. `--preflight` runs read-only cross-checks. `--freeze` copies, verifies and sets 444/555; it refuses to overwrite and refuses the B4/1e49188b dirs. `--check` is a read-only re-check. Prints its check count; exits 1 on any failure or zero checks |
| `preflight-20261012.log`, `preflight-r2-20261012.log` | `--preflight` output: 96/96 PASS (first run and revision 2 re-run) |
| `freeze-20261012.log` | `--freeze` output: 183/183 PASS |
| `check-final-20261012.log`, `check-final-r2-20261012.log` | `--check` output: 66/66 PASS (revision 1 and revision 2) |
| `shasum-check-frozen.log` | Independent `shasum -a 256 -c` of each frozen PRX and snapshot tar, plus `ls -la` showing the 444/555 modes |
| `protected-frozen-fingerprint-before.txt`, `-after.txt`, `-r2.txt` | Mode, size, mtime and sha256 of every file in the B4 and 1e49188b frozen dirs; identical all three times |
| `reconstruct_manifests.py` | Rebuilds the marker list each fallback was verified with. It reads `add_build_markers.py` with `ast` and never runs it, and writes only into this dir. Revision 2 corrects the independence wording: S1 and S2 share inputs, and only S3 and S4 are independent |
| `reconstruct-manifests-20261012.log`, `reconstruct-manifests-r2-20261012.log` | Its output: 36/36 PASS both times; the manifests are byte-identical |
| `markers-B3-iterators-23.json` | B3's marker list (23), byte-identical to the copy in B3's own source snapshot |
| `markers-B2b-error-wrapper-memlog-21.json` | B2b's marker list (21): the pre-B2 19 plus the 2 B2 markers. The 2 B2 markers are backed independently only by B2b's evidence verify log |
| `markers-B1-lookup-nocache-19.json` | B1's marker list (19), byte-identical to the pre-B2 backup and to B1's snapshot copy |
| `MANIFESTS-PROVENANCE.json` | sha256, marker ids, and the independent sources for each list |
| `harvest-markers-sha-before.txt`, `harvest-inputs-sha-r2-before.txt`, `harvest-listing-r2-before.txt` | sha256 of the harvest inputs and a listing of `harvest/` before the runs (unchanged afterwards) |
| `check_shim_state.py` | **New in revision 2.** A read-only static check of each frozen build's own snapshot, read in memory and never extracted. It shows the path shim gets an empty root and switches itself off on 1e49188b, B1, B2b and B3, but not on B4. It also shows which self-test-visible features each build has. Prints its check count; exits 1 on failure or zero checks |
| `check-shim-state-20261012.log` | Its output: 73/73 PASS |
| `shim_root_lexical_host_test.cpp`, `shim-root-lexical-host-test.log` | **New in revision 2.** A Mac host test of the shim-root text steps with the real `std::filesystem`: `"ue4ss/Mods"` gives an empty root, and an absolute path gives the root. 3/3 PASS |
| `run_verify_and_dryrun.py` | Runs, for each fallback: `verify_build.sh` with its own markers, the 24-marker negative control, and `prep_modded_launch.py --dry-run` inside a no-network, no-write sandbox and under the audit hook. Revision 2 adds positive controls. Prints its check count; exits 1 on failure or zero checks |
| `audit_dryrun_wrapper.py` | **New in revision 2.** Runs a script under a Python audit hook and reports every write, network, process or file-change *attempt* |
| `audit_probe_controls.py`, `sandbox-audit-positive-control.log` | **New in revision 2.** A deliberate write and a loopback connect, to prove that both the hook and the sandbox really catch them |
| `run-verify-dryrun-r2-20261012.log` | Revision 2 run output: 63/63 PASS |
| `run-verify-dryrun-20261012.log` | Revision 1 run output (51/51), kept for the record |
| `VERIFY-DRYRUN-SUMMARY.json` | Machine-readable summary of the revision 2 runs |
| `verify-<label>-<N>markers.log` (3) | `verify_build.sh` on each frozen PRX: PASS N/N, receipt MATCH |
| `negative-control-24markers-<label>.log` (3) | The same PRXs checked against today's 24-marker list. They are **expected** to FAIL and miss exactly the later builds' markers. This is evidence of what each build lacks, **not** a verdict on the build |
| `dryrun-<label>.log` (3) | `prep_modded_launch.py --dry-run` output plus the audit summary for each frozen PRX. **These contain the console's LAN address (the script prints it), so do not paste them into reports for others** |
| `r1-logs/` | Unchanged copies of the revision 1 logs, FALLBACKS.md, README.md and provenance file |
