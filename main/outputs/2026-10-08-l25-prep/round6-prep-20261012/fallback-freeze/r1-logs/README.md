# fallback-freeze (2026-10-12, Mac-only)

Track: make a fallback launch instant if B4 misbehaves. **Read FALLBACKS.md
first:** it has the order to step back, the full shas, the frozen paths, the
verify results, what each build lacks compared with B4, and the exact dry-run
commands.

Frozen copies (new, read-only, layout as the B4 frozen dir):

- `outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/`
- `outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/`
- `outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/`

No console contact, no builds, no git changes. The existing B4 and 1e49188b
frozen dirs and `harvest/markers-declared.json` were only read.

## Files in this dir

| File | What it is |
|---|---|
| `FALLBACKS.md` | The main result: per-build table, checks, what each build lacks vs B4, dry-run commands, watch-outs |
| `freeze_fallbacks.py` | Script that made the frozen copies. Modes: `--preflight` (read-only cross-checks), `--freeze` (copy + verify + chmod 444/555; refuses to overwrite), `--check` (read-only re-check). Prints its check count; exits 1 on any failure or zero checks |
| `preflight-20261012.log` | `--preflight` output: 96/96 PASS (sha in 6 places, magic, sizes, receipts, symbols, snapshots, build ids) |
| `freeze-20261012.log` | `--freeze` output: 183/183 PASS (preflight + copies + post-check) |
| `check-final-20261012.log` | Final `--check` run (read-only) after everything else |
| `shasum-check-frozen.log` | Independent `shasum -a 256 -c` of each frozen PRX and snapshot tar, plus `ls -la` showing the 444/555 modes |
| `protected-frozen-fingerprint-before.txt` / `-after.txt` | Mode, size, mtime and sha256 of every file in the B4 and 1e49188b frozen dirs, before and after (identical) |
| `reconstruct_manifests.py` | Rebuilds the marker list each fallback was verified with, from 4 independent sources. Reads `add_build_markers.py` with `ast` and never runs it. Writes only into this dir. Prints its check count; exits 1 on failure or zero checks |
| `reconstruct-manifests-20261012.log` | Its output: 36/36 PASS |
| `markers-B3-iterators-23.json` | B3's marker list (23), byte-identical to the copy in B3's own source snapshot |
| `markers-B2b-error-wrapper-memlog-21.json` | B2b's marker list (21) = pre-B2 19 + the 2 B2 markers |
| `markers-B1-lookup-nocache-19.json` | B1's marker list (19), byte-identical to `harvest/markers-declared.json.pre-B2markers-20261012` and to B1's snapshot copy |
| `MANIFESTS-PROVENANCE.json` | sha256, marker ids and sources for the three marker lists |
| `harvest-markers-sha-before.txt` | sha256 of the harvest marker files before the run (unchanged after) |
| `run_verify_and_dryrun.py` | Runs `verify_build.sh` (own markers), the 24-marker negative control and the sandboxed `prep_modded_launch.py --dry-run` for each fallback. Prints its check count; exits 1 on failure or zero checks |
| `run-verify-dryrun-20261012.log` | Its output: 51/51 PASS |
| `VERIFY-DRYRUN-SUMMARY.json` | Machine-readable summary of those runs (exit codes, verdicts, missing markers, commands) |
| `verify-B3-iterators-23markers.log` | `verify_build.sh` on frozen B3: PASS, 23/23, receipt MATCH |
| `verify-B2b-error-wrapper-memlog-21markers.log` | `verify_build.sh` on frozen B2b: PASS, 21/21, receipt MATCH |
| `verify-B1-lookup-nocache-19markers.log` | `verify_build.sh` on frozen B1: PASS, 19/19, receipt MATCH |
| `negative-control-24markers-*.log` (3) | The same PRXs checked against today's 24-marker list. They are **expected** to FAIL, and the missing markers are exactly the later builds' markers. This is evidence of what each build lacks, **not** a verdict on the build |
| `dryrun-B3-iterators.log`, `dryrun-B2b-error-wrapper-memlog.log`, `dryrun-B1-lookup-nocache.log` | `prep_modded_launch.py --dry-run` output for each frozen PRX, run under a no-network, no-write sandbox. **These contain the console's LAN address (the script prints it): do not paste them into reports for others** |
