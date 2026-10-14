# TESTS — capability #10 (Team G). Saved output under `tests/output/`.

Every test/gate prints `items_checked=` and fails on zero.

## GPT's 19 integration tests

- **Against GPT's own delivered stage** (`tests/output/gpt19-on-gpt-stage.txt`):
  **19/19 OK.** (One needed a large evidence object, `cap10-with-base-fixed.o`
  `1d479999…`, that GPT kept in its own worktree, not in the packet; supplied
  read-only from `caps-resume-20261013/.../evidence/`.)
- **Against the Team G tree** (`tests/output/gpt19-on-team-g-tree.txt`):
  **After the review fixes: 16/19 OK, 3 superseded by design, not regressions**
  (`tests/output/gpt19-on-team-g-tree.txt`). The third: `test_compile_paths_derive_from_pinned_active_root`
  passes a minimal dict without the pinned runtime assets; `compile_argv` now
  refuses that (fix 1 needs the pinned corlib/config sizes). The first two:
  1. `test_actual_make_new_units_clones_real_program_argv_with_pins` asserts
     exactly 2 cap10 NEW_UNITS; Team G adds a third (`cap10-offline-net`, the
     offline SceNet stub), so it sees 3. Correct new behaviour.
  2. `test_actual_finalizer_positive_exact_counts` builds a 207-row fixture and
     expects GPT's self-referential P7/P8 to pass it; Team G's **fixed** gate
     correctly rejects a 207-row package (it expects the reviewed 350). The gate
     doing its job. (The negative finalizer test — rejecting zero/missing/
     duplicate/constructor/static/twin — still passes.)

## Team G tests (the replacements + tasks 2–3)

- `tests/test_team_g_drivers.py` (`tests/output/team-g-drivers.txt`): **4/4 OK.**
  Three cap10 NEW_UNITS with correct pins + cap10 defines; the fixed finalizer
  gate PASSES a correct 350-identity fixture, REJECTS a dropped row, REJECTS a
  SceNet name. `items_checked=4`.
- `tests/test_team_g_gates.py` (`tests/output/team-g-gates.txt`): **8/8 OK**,
  `items_checked=152`. The admitted-import list passes; refuses an
  sdk-stub-only row, a pending-identity row, a count mismatch, a provider-file
  sha drift; no SceNet in the set; every row has captured evidence + two
  sources; the `<csignal>` include exemption is exact and scoped to one unit.
- `tests/host/test_csharp_planner.cpp` (`tests/output/host-csharp-planner.txt`):
  **PASS, items_checked=16 — on the pre-review planner. STALE:** not updated to
  the post-review signature (runtime-files argument) and not rerun. Team G could
  not complete the update in this session (stopped by a safety classifier); left
  open for the integrator's reviewer. Lazy start: probe requested ONLY for an enabled C#
  mod whose `csharp/` holds the exact probe; not for a disabled mod, a non-probe
  assembly, a relative mods dir, or a missing probe image. (host clang++)
- `tests/host/test_offline_net.cpp` (`tests/output/host-offline-net.txt`):
  **PASS, items_checked=10.** The 7 owned SceNet entry points return fail-closed
  values, the errno slot reads `ENETDOWN`, and each logs its first refusal once.

## Target compile (tools/target_compile.py — build's own commands, ≤2 parallel)

`build/tc1.log` and, after the review fixes, `build/tc2.log`: **4/4 pass** with the R15e `program` compile command +
`MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1` and the merged header overlay:
`program` (lazy-edited UE4SSProgram.cpp → `f8b2cb69…` → obj, 0 errors),
`cap10-mono-owner`, `cap10-invocation-boundary`, `cap10-offline-net` (0 errors
each). Memory guard held ≥4 GB throughout.

## Partial link (tools/link_probe.py) and gates

- `build/link1/`: R15e link command + cap10 units/artifacts → `ld.lld -r` OK;
  raw undefined 210 → **361** (+146 strong, +5 weak, −0); **0 `sceNet*`**.
- include-graph replica (`tests/output/include-graph.txt`): `items_checked=2184`,
  **0 unapproved** with the `<csignal>` exemption.
- fs_throw (`tests/output/fs-throw.txt`) + key comparison: **0 added**; the
  edited UE4SSProgram.cpp key-set is identical to live; new cap10 files 0 sites.
- no_detour (`tests/output/no-detour.txt`): PASS over candidate + include +
  cap10/native (17 files; 0 detour sites).
- field_gate (`tests/output/field-gate.txt`): PASS; the cap10 code and the
  lazy-start edit read no engine field (Team G's minimal tree: 38 proven, 0
  unproven; the integrator re-runs on the full live tree).

## Rebase onto live R16d + #11 follow-ups (saved as `tests/output/r16-*`)

- Target compile against R16d's own commands (≤2 parallel, memory guard on): 4/4 pass
  (`build/tc-r16.log`); `program` object byte-identical to before the rebase.
- Partial link vs R16d (`build/link-r16/`): 210 → 361, delta identical to the R15e
  measurement, 0 `sceNet*`, 401 inputs, `.ctors` unchanged, no TLS.
- include-graph: 2184 rows, 0 unapproved. fs_throw: 0 keys added, new files 0 sites.
  no_detour: 41 files, PASS. field_gate (patched candidate file): 32 fields, all proven, PASS.
- Team G gates 8/8 (152 items), drivers 4/4, host offline-net 10/10.
- GPT's 19: 16 pass, 3 superseded by design (same three as before).
- Still open: the host planner test (stale since review fix 1).
- Patch + binaries on fresh live copies == the rebased scratch tree.
