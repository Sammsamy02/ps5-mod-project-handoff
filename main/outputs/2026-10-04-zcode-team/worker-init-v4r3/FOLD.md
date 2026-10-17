# FOLD.md — l11 editions re-point instruction (worker-init-v4r3, 2026-10-05)

The v4+armed initializer staging is DELIVERED. Loop-side edits to admit it into the
launch-11 combined candidate. All paths relative to the workspace root
(`/WORKSPACE/.codex/worktrees/translation-core/what`). Verify every hash
before replacing any owned file; never duplicate-send; proposal-only until the
controller/loop executes these re-points.

## New staging (replaces the plan-A0 armed staging `8b0c7e99` as the initializer unit)

```
outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/
  UnrealInitializer.cpp        22f062863fe2d599ccbccabec678ccd8f2fbe5f74c677ed9f6377ae116417fb0   (composition entrypoint)
  UnrealInitializer-v4r3.cpp   22f062863fe2d599ccbccabec678ccd8f2fbe5f74c677ed9f6377ae116417fb0   (mission-named deliverable; byte-identical)
  SeedValidation.hpp           f09ec9b319985cf86a5899894a30fc8d20a905072008ac8d4a35efbcaec3a5de
  SeedTable.h                  d183e0c03e4ec7568e2e1416787599f8c9f363f5a940cfc47a2a31ff04ca3cda
```

Provenance: `4a2b9400` (launch-10-final v4, final-units) + DIFF-2 rev B `68d0c006`
(patch -p1, clean) + in-file arming flip (line 179, `0 -> 1`). Receipts: compile
gate (control reproduces the v4 object `a437861a` EXACTLY; merged object
`fe5d26d9`, deterministic ×2), object census (v4 strings PRESENT; R3 markers
PRESENT; the three `R3Marshal*` bridges DEFINED; 0 `R3Marshal*` UND), host suite
69/69. See this dir's CLAIM.json / MERGE.md / DIFF.md / receipts/.

The staging compiles `SeedValidation.hpp` + `SeedTable.h` from its OWN dir
(quoted-include sibling resolution — same mechanism the l10r3 precedent relied on),
so re-pointing the unit path re-points the header world automatically. The headers
MUST be the A-world pins above: the merged initializer requires the GMalloc labels
and the v4 `rc_seed` API; the old staging's `b8d851e9`/`59effe6e` would NOT compile.

## Edit 1 — driver `build_runtime_composition6_l11.py` (L11-E2)

File: `outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py`

Replace the L11-E2 `--replace-runtime-unit` initializer path:

```
- '--replace-runtime-unit', 'unreal-unrealinitializer=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-luamod-rebase/staging/initializer-l10-r3/UnrealInitializer.cpp',
+ '--replace-runtime-unit', 'unreal-unrealinitializer=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/UnrealInitializer.cpp',
```

and update the L11-E2 comment block: sha pin `8b0c7e99…` → `22f06286…`; staging
dir `worker-luamod-rebase/staging/initializer-l10-r3` →
`worker-init-v4r3/staging`; header pins `SeedTable.h 59effe6e` / `SeedValidation.hpp
b8d851e9` → `d183e0c0` / `f09ec9b3`; DELETE/replace the "RECORDED CAVEAT
(PRE-launch10-final-c … LACKS the GMalloc V1/V2 validators, the v4 pool predicate
and the read_bytes_at entry-0 anchor)" block with:

```
# RESOLVED (worker-init-v4r3, 2026-10-05): this staging IS the v4+armed fold —
# the shipped launch-10-final v4 lineage (4a2b9400) + DIFF-2 rev B + the in-file
# arming flip. The PRE-FINAL-C caveat is superseded; admissions.initializer_fold
# = ADMITTED-V4ARMED-FOLD.
```

The pairing-law paragraph stays valid verbatim (the bridges are still defined under
`defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE`).

## Edit 2 — EDITIONS.json

File: `outputs/2026-10-04-zcode-controller/launch11-combined/EDITIONS.json`

1. `admissions.initializer_fold.status` → `"ADMITTED-V4ARMED-FOLD"` (the runner's
   admission gate accepts exactly this token), and add the files map:

```json
"initializer_fold": {
  "status": "ADMITTED-V4ARMED-FOLD",
  "packet": "worker-init-v4r3",
  "files": {
    "outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/UnrealInitializer.cpp": "22f062863fe2d599ccbccabec678ccd8f2fbe5f74c677ed9f6377ae116417fb0",
    "outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/UnrealInitializer-v4r3.cpp": "22f062863fe2d599ccbccabec678ccd8f2fbe5f74c677ed9f6377ae116417fb0",
    "outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/SeedValidation.hpp": "f09ec9b319985cf86a5899894a30fc8d20a905072008ac8d4a35efbcaec3a5de",
    "outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/SeedTable.h": "d183e0c03e4ec7568e2e1416787599f8c9f363f5a940cfc47a2a31ff04ca3cda"
  }
}
```

2. `compose.initializer_armed` →

```json
"initializer_armed": {
  "dir": "outputs/2026-10-04-zcode-team/worker-init-v4r3/staging",
  "files": {
    "UnrealInitializer.cpp": "22f062863fe2d599ccbccabec678ccd8f2fbe5f74c677ed9f6377ae116417fb0",
    "SeedValidation.hpp": "f09ec9b319985cf86a5899894a30fc8d20a905072008ac8d4a35efbcaec3a5de",
    "SeedTable.h": "d183e0c03e4ec7568e2e1416787599f8c9f363f5a940cfc47a2a31ff04ca3cda"
  },
  "note": "V4+ARMED fold (worker-init-v4r3): launch-10-final v4 4a2b9400 + DIFF-2 rev B 68d0c006 + arming flip. PRE-FINAL-C caveat SUPERSEDED. Unit comparator fe5d26d9 (single-unit 190-entry argv ONLY)."
}
```

(Keep the `dir` value a plain relative path exactly as the existing entry is
shaped — `launch11_one_pass.py` does `EDITION['compose']['initializer_armed']['dir']`
and `Path(init)/'UnrealInitializer.cpp'` in regen_receipts, which is why the
staging carries the basename copy `UnrealInitializer.cpp`.)

3. `units[]`: re-point the three initializer staging entries
   (`worker-luamod-rebase/staging/initializer-l10-r3/{UnrealInitializer.cpp,
   SeedValidation.hpp, SeedTable.h}`) to the `worker-init-v4r3/staging` paths +
   shas above; re-word their `what` notes to "V4+ARMED fold (worker-init-v4r3);
   PRE-FINAL-C caveat superseded".
4. `comparators`: add
   `"initializer_unit_object_single_unit_argv": "fe5d26d9aa1d2abfab6db0eac84ff28e3a5da9afdf1afaa5a449b716409c133c"`.
   SCOPE (honest): valid ONLY under the standard 190-entry single-unit argv
   (`worker-fname-validator-v4/receipts/compile-gate.argv.json`,
   `LLVM_CONFIG=outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`);
   composition-time initializer objects (4 prepended `-I`) are a different argv
   class and are measured fresh (l10r3 precedent). It is NOT wired into
   `luamod_comparator_gate`-style enforcement; the composition-time initializer
   contract is the existing plan-D `consumer_und_gate`, which this object
   pre-satisfies 3/3 (bridges DEFINED, 0 `R3Marshal*` UND).
5. `stage_header_pins_carried_from_l10.SeedValidation.hpp` (`f09ec9b3`): UNCHANGED —
   same sha in the new staging; no edit.

## Edit 3 — runner `launch11_one_pass.py` (optional, comments only)

regen_receipts needs NO code edit: it derives the three covered files from
`EDITION['compose']['initializer_armed']['dir']` + hardcoded filenames, which the
new staging satisfies (basename copy present). Optionally refresh the reason
strings that still cite `b8d851e9` / `59effe6e` / "PRE-FINAL-C" — measured hashes
are computed live at regen time, so the pins self-update.

## Gate expectations for the combined build (for the loop's awareness, no edits)

- A3 admission gate passes with `initializer_fold=ADMITTED-V4ARMED-FOLD`.
- `consumer_und_gate`: `_ZN2RC6Unreal20R3MarshalPumpPendingEv` and
  `_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv` are DEFINED by this
  initializer's armed fold (measured on `fe5d26d9`); combined with the
  unreal-ufunction unit for `UFunction::GetFuncPtr`, 3/3 resolve; no `R3Marshal*`
  UND expected from the initializer.
- The initializer unit object in the run receipt will NOT equal `fe5d26d9`
  (composition argv class); do not widen any comparator to demand it.
- Everything else (luamod M-0 `813b249d`, pad, asmhelper fold-0, bridge CLI,
  stage-header pins) is untouched by this fold.
