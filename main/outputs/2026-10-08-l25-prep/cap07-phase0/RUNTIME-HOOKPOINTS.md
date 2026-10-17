# RUNTIME-HOOKPOINTS: where Phase 0 (pak observe-only) plugs into our runtime

Mac-only research, read-only. No console contact, no builds. Date 2026-10-12.
Timing rows marked **console-proven** come from the R11 launch-14 harvest (build `bfd95db6`, the current
rollback build). They describe the current build only. They are not a Phase 0 verdict.

Path short names:
- `RT` = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`
- `LIN` = `outputs/2026-10-08-l26-package-A/lineage/` (the staged lineage that package A / R11 compiled; `runtime_start_bridge.cpp` sha a35c7f91 == `l26-runtime/copyin-probe/candidate/lifecycle/`)
- `DIAG` = `outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c` (the `diagnostic.c` that is really linked: `package-A/BUILD-INPUTS.json` `diagnostic_source`, swapped in at `package-A/package_combined.py:77`. `LIN/diagnostic.c` is NOT the linked copy.)
- `OWN` = `outputs/2026-10-07-l19-r3-full-build/units/runtime_owner.cpp`
- `BACK` = `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp`
- `EV` = `outputs/2026-10-08-l25-prep/evidence/launch-l26-R11-launch14-20261013/`

Line-number note: R11 compiled `l26-build-variants/staging-copyin-probe/UnrealInitializer.cpp`. That file is
`RT/src/UnrealInitializer.cpp` plus three blocks: +88 lines at 889, +1 at 1104, +12 at 1556. The numbers below
are for the `RT` file.

---

## 1. Startup timeline and the earliest safe thread

### Call chain (file:line)
1. `module_start` (`LIN/lifecycle/runtime_module_entry.c:337`). It takes the entry guard, then opens the
   diagnostic channel (`:346`). Logs `module_start_entered` (`:349`).
2. `probe_lifecycle_start` (`:359`, which runs `LIN/lifecycle/lifecycle.c:59-104`):
   - `o->validate()` (`lifecycle.c:77`) runs `probe_runtime_validate_and_preload` (`LIN/probe/probe.c:23-35`).
     That calls `probe_imports_validate` (`LIN/import_check.c:6-17`): 187 strong imports, any null fails.
     It then loads the startup input file.
   - All `.init_array` constructors, each wrapped in a C++ catch (`lifecycle.c:85-93`;
     `constructor_boundary.cpp:3-10`).
   - `o->worker_start()` (`lifecycle.c:96`) = `probe_worker_start` (`probe.c:85-94`): **`pthread_create(&worker, NULL, ...)`**.
     That worker only `nanosleep`s, then runs one buffered Lua self-check (`probe.c:57-83`).
3. `start_composed_runtime` (`runtime_module_entry.c:264-334`):
   - `mfa_activate` (`:284`).
   - `modbridge_runtime_start_with_activation` (`LIN/lifecycle/runtime_start_bridge.cpp:1763`). It does the
     lmf VM capture and base scan (`:1809-1890`) and `bridge_capture_loaded_image` (`: