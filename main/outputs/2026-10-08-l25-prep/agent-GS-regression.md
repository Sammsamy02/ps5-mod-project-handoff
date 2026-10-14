# agent-GS-regression — green-sheet regression gate (2026-10-10)

Agent GS. Mac-only, battery-safe: local test runs, no PRX builds, no console
contact, ZERO runtime-tree edits, ZERO stub-twin edits.

## Deliverables

| file | what |
|---|---|
| `host-tests/run-regression.sh` | one-shot orchestrator: every host-side suite in dependency-safe order, per-suite `RESULT suite=<id> rc=<n> expected=<e> verdict=GREEN\|RED\|ABSENT detail="..."` machine-readable lines, per-suite logs under `host-tests/regression-logs/latest/`, regenerates the sheet, exits nonzero (loud RED banner) on any failure |
| `host-tests/REGRESSION-SHEET.md` | the standing pre-build gate: law text (run before every AC-power build), suite inventory (owner/entry/expected), twin-parity change ledger, fresh results table |
| `host-lua-harness/runs-baseline/` | FROZEN copy of the recorded FU verdict baseline (27 logs + `VERDICTS.txt`) taken BEFORE any rerun — `run-all.sh` overwrites `runs/`, so the baseline had to be preserved for byte-identity comparison; self-heals only if deleted |

## Results — ALL GREEN (run twice, identical: repeatability proven)

| suite | owner | rc | expected | verdict |
|---|---|---|---|---|
| env-preflight | GS | 0 | interpreters present | GREEN |
| prop-parity (FR revert contract) | FR | 0 | rc=0 + ALL PASS | GREEN |
| path-shim rule contract | GA | 0 | 38/38 | GREEN |
| host-lua-harness 27-verdict calibration | FU+GB+GA | 0 | 27/27 byte-identical + spawner LOADS-CLEAN both io modes | GREEN |
| ueselftest validate (3 scenarios) | GB | 0 | ALL SCENARIOS PASS (S1 167/167) | GREEN |
| keymap determinism | FV | 0 | regen x2 identical; bindings==baseline; --check 0 violations | GREEN |
| mod-manager end-to-end | GG | 0 | 49/49 | GREEN |
| phasea-tools | GF | 0 | 18/18 | GREEN |
| blade-prep sanity (scripted subset) | GJ | 0 | py_compile + table JSON | GREEN (binary control runs not scripted — receipts only) |
| pak-overlay | GK | 0 | suite landed? | ABSENT (no suite landed; overlay_builder.py py_compile OK; auto-discovered when it lands) |

## The FU-drift question (root's Rule-10 shim edits) — NO DRIFT

Concern from the brief: root's Rule-10 root-derivation fix in
`LuaPathShim.{hpp,cpp}` + `LuaMod.cpp` (runtime now derives the modbridge
root from the program's mods directory — `weakly_canonical(get_mods_directory())`
minus trailing `Mods` + one parent — and DISABLES loudly with
`reason=no-root` when nothing can be derived; the title-id default is gone;
tests override via `set_modbridge_root_override`).

Findings:

1. **27/27 verdicts byte-identical** to the frozen baseline across two full
   reruns; calibration law holds (TownfallItemSpawner console copy
   LOADS-CLEAN in BOTH io modes; 6/6 keybinds; 844 recorded calls). Whoever
   re-ran the matrix at 16:14 after the 16:11 stub state already had it
   green; this gate now proves it durably.
2. **How the host twin handles it**: `stub-ue4ss.lua` derives the resolver
   root from the subject chunk's own source (`ue4ss/` component = the
   mods-dir-derivation analog) and falls back to
   `path-shim.lua:DEFAULT_MODBRIDGE_ROOT` ("/data/modbridge/roots/PPSA33286",
   nonexistent on the Mac) only when no ue4ss-bearing source is on the stack
   — which is only the spawner console-copy subject (0 io uses anyway). That
   fallback emulates "the program's real root" for out-of-tree subjects:
   reads miss non-fatally, writes stay virtual + mod-confined — exactly the
   observable behavior the runtime's loud-disable (stock io) would produce.
   **No `set_modbridge_root_override` wiring is needed**: the seam's host
   analog is `PathShim.new{ root = ... }`, and the stub already passes a
   derived-or-fallback root per subject.
3. **Documented divergence, deliberately NOT "fixed"**: the host twin still
   carries the title-id fallback constant while the runtime removed it
   (Rule 10). Since it is verdict-neutral for every current subject, changing
   the stub would be churn without a test signal; it is recorded in the
   sheet's "Twin-parity notes — legitimate-change ledger" with the exact
   re-mirror rule if a future subject ever makes it verdict-observable.
   Classification: HARNESS DOCUMENTATION GAP (fixed by documenting), not
   harness drift, not a runtime regression.

## Real regressions found: NONE

- Runtime tree: untouched by me; nothing cross-broke. FR revert, FW markers,
  GA shim, root's Rule-10 fix — all suites green against them.
- Harness drift: none (27/27 identity).
- Two bugs found and fixed during gate development, both in MY new script
  (before the recorded final runs): `WHAT` derived one directory too high
  (broke the keymap pin-pad path and the bonus-stamina subject path). Fixed;
  both final runs green. No pre-existing suite code was modified.

## Notes / boundaries

- keymap `inputs-sha256` comment line embeds invocation paths, so full-file
  byte-parity with `GENERATED-BASELINE.ini` is invocation-dependent; the gate
  compares everything EXCEPT that line (bindings parity) plus regen-x2
  determinism with a canonical invocation + `--check --expect-size 2422`
  (0 violations / 4 documented WARNs).
- phasea-tools depends on `/tmp/l25-variant-A/klog-20261009-021938.log`
  (still present) — if /tmp is purged that row will RED spuriously; noted in
  the sheet inventory.
- blade-prep: no scripted self-test exists; the gate covers py_compile +
  table JSON only (labeled). GJ's binary positive/negative control runs stay
  as one-off receipts under `blade-prep/runs/`.
- pak-overlay (GK): `pak-overlay/mod-manager/` is empty and
  `overlay_builder.py` has no self-test mode → row is ABSENT (non-blocking,
  auto-upgrades to a real run when a test script lands at the probed paths).
- `host-lua-harness/runs/*.log` are working output (overwritten each run);
  the LAW lives in `runs-baseline/VERDICTS.txt`.

## Standing gate (the ask, item 4)

Documented in `host-tests/REGRESSION-SHEET.md` header: run
`outputs/2026-10-08-l25-prep/host-tests/run-regression.sh` before EVERY
AC-power build; build only on exit 0; RED on host-lua-harness after a
runtime shim edit means twin divergence — fix the twin, never the verdict.
