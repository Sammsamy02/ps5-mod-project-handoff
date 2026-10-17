# CLAIM — worker-crt-startup-design (2026-10-04)

Role: design + host-prototype the CRT startup block (backlog rank 4).

## Claimed scope

- All writes confined to `outputs/2026-10-04-zcode-team/worker-crt-startup-design/`.
- Read-only inspection of the workspace root for context; no writes elsewhere.
- No network access, no console (game-console/PS5) targeting, no nested agents.
- Host-prototype compiled and run locally with `cc` (macOS/clang) only; no engine,
  no Unity, no build.py changes (build.py is not present in this workspace).

## Input status (honest)

- `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`: NOT FOUND in this
  workspace. Operative rules taken from the role brief itself (workspace root,
  no network/console/nested agents, write only in my directory, CLAIM+REPORT,
  <=35 tool calls).
- `worker-dll-mod-retention/baseline-dll-census.json` (exact observed retained-DLL
  symbol set): NOT FOUND anywhere in the repo (filename and content search).
  Consequence: the "exact observed set" is NOT fabricated. DESIGN.md carries a
  PENDING admission table filled only with the two symbols the task prompt itself
  names (`_initterm`, `_seh_filter_dll`, attributed to the prompt) plus explicit
  fill-in instructions for when the census lands. See CENSUS-GAP.md.

## Deliverables claimed

1. DESIGN.md — documented MSVC DLL startup semantics mapped to the retained-runtime
   model, admission boundaries (attach vs lazy first-call), composition map,
   scope boundary, 10-point wiring sketch.
2. crt_startup.h / crt_startup.c / test_crt_startup.c — host prototype of
   `_initterm`, `_initterm_e`, `_seh_filter_dll` stub contract, atexit table,
   and composed attach/detach orchestrators, with tests from synthetic tables
   including terminator sentinels.
3. build_and_test.sh — reproducible compile+run transcript for REPORT.md.
4. CENSUS-GAP.md — the missing-input record.
5. REPORT.md — closure report.
