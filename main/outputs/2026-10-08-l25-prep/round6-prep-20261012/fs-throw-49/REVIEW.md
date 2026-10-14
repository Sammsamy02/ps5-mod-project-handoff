# fs-throw-49: adversarial review, pass 2 (Mac-only, 2026-10-12)

**Verdict: not refuted.** The main result holds: 49 = 33 never run on PS5 + 16 already caught, 0 unhandled. No fix build is needed for safety. P1 is correct and optional. All 5 problems from pass 1 are fixed. I found 4 new minor problems. None of them changes the verdict.

Everything here is Mac-only. I made no console contact and read no console logs or harvests. I ran no PRX build and no state-changing git. This file is the only thing I wrote. (The previous review is replaced; its 5 points are in CLASSIFICATION.md "Changes after the adversarial review".)

## What I re-ran (Mac-only)

- **Gate, current compiled set vs the 1e49188b baseline:** 3,561 files, 68 calls, 19 safe, 49 violations. 49 known, 49 present, 0 resolved, 0 NEW. PASS, exit 0. Same as the worker.
- **Compiled list is current:** the l26-A pass1 build is from Oct 11 23:12. No candidate file is newer than it (LuaMod.cpp 22:51, UE4SSProgram.cpp 23:02).
- **check_classification.py:** PASS, exit 0. It reports 49/49/49 coverage, 266 citations, 97 anchors and 4,037 files for the no-caller check, with 4 and 5 known mentions. Same as the worker.
- **P1 hashes:** the patched copy is `04e89a43…5326`. The control copy and the live candidate are both `7ae47172…9df8`. The patch file matches `diff control patched`, apart from the header paths.
- **PS5-target `-fsyntax-only` on the P1 copy:** exit 0. This used the worker's exact argv plus `LLVM_CONFIG`. One compile.
- **New, independent check of R0 at object level:** I ran `llvm-nm` on all 396 objects in the l26-A link command. `PrepareDeferredUE4SSRuntime` and `CreateNativeUE4SSProgram` appear only as definitions (T). No object references either one (no U). This backs check 5 with a second source that does not depend on the source text.

## Claims I checked against the source (they hold)

- **Every PC disk-scan branch checks the mod source first:**
  - setup_paths: U:872
  - setup_mod_directory_path: U:930
  - setup_mods: U:1964
  - LuaMod constructor: L:1128 (m_native_source comes from program.native_mod_source(), L:1124)
  - custom_module_searcher: L:1435
- **make_compatible_path:** add_mods_directory and remove_mods_directory throw before reaching it when a mod source is set (U:2941, 2955).
- **Startup chain:** ValidateNativeProgramContext runs in the UE4SSBackend constructor (ue4ss_backend.cpp:24), then again in the UE4SSProgram constructor inside its `try` (U:222/225). On failure:
  - the backend catch reports it and rethrows (ue4ss_backend.cpp:93-113);
  - the bridge catches it and returns MRS_ERR_START (lineage runtime_start_bridge.cpp:1992-2015).
- **Log-file path (row 15):** if the first UE4SS.log line fails, FileDevice::start_device leaves `m_is_device_ready` false, so every later send would retry and throw again. I checked that this cannot get past the constructor catch:
  - The first send is inside the constructor `try` (U:348/350).
  - `shutdown_native_program` (noexcept) only logs through VmtTickPumpRetire, which has its own catch(...).
  - `m_mods` is still empty, because setup_mods comes later (U:437/439).
  - Output devices are closed at the end of shutdown.
- **Exception types:** nothing in the compiled set catches `filesystem_error` or `system_error` by name.
- **No-caller rows:**
  - DumpStatsToFile ×2: no callers.
  - queue_start_lua_mod_by_path: its only caller is itself.
  - get_mods_txt_entries: no callers.
  - persist_cache: only reachable through the File serialization API, which nothing outside File/ calls.
- **Catch lines:** U:1689/1894/1899, U:664/750, the LuaMadeSimple TRY (LMH:843-857, LMS:1726) and the RC::TRY used at L:3537 (ExceptionHandling.hpp:75) all catch std::exception.
- **Compiler flags:** `-frtti -fexceptions` at prospero-clang++:83.
- **Settings:** the ini line `EnableDebugKeyBindings=false` exists at the cited path, line 13. The code default `false` is at SettingsManager.hpp:38.

## Problems (all minor)

1. **README contradicts itself about `gate/tmp/`.** README.md:27 says `tmp/` is "(empty)". README.md:11 and the worker's report say it holds the 4 `*.before-review-fix` copies, and it does (`ls gate/tmp`: 4 files).
2. **Gate blind spot not reported: `directory_iterator(..., ec)` in a range-for still throws.** The gate counts these as safe error_code calls, but the loop's `operator++` uses the throwing increment (libc++ `__filesystem/directory_iterator.h:86`). Two sites:
   - L:3466-3467, in IterateGameDirectories, the path self-test call 134 runs on every launch;
   - L:3650-3651, in the `__files` branch.

   Both are inside the Lua TRY, so a mod gets a Lua error, not a crash. The verdict stands. But "19 safe" overstates things, and the "Extra findings / gate scope" section should name this class. Calls to `directory_entry` members without `ec` (for example U:888 `item.is_directory()`) are the same blind spot, though only in unreachable code.
3. **Evidence label.** CLASSIFICATION.md:3 says "All of this is Mac-only". But the second source for EnableDebugKeyBindings (CLASSIFICATION.md:204-210) is a console FTP readback from the earlier morning session, 11:40Z. It is not a log, and it only supports a "most likely" side fact about rows 7-9, which are HANDLED either way. It should still be labelled "console readback, earlier session, not a verdict source", to follow CLAUDE.md §5.
4. **D3 understates the catch-block raise.** At L:3805-3808, `lua.throw_error` inside `catch` longjmps out of the handler. With Lua built as C, that means `__cxa_end_catch` never runs. The exception stays on the thread's caught-exceptions stack as well as leaking. A later bare `throw;` or `std::current_exception()` on that thread could see the stale object. It is still not a crash on the normal path, and D3 already routes it to its own build. The wording "leak" is just incomplete. This is a Mac-only reading of how the C++ runtime handles exceptions; I did not test it.

## Rule check

- No console contact. No console logs were read (the ini readback is not a log; see problem 3).
- No edits to the candidate tree. The live PosixFile.cpp still matches the control sha.
- No game- or mod-specific code in P1.
- Every checker the worker wrote exits non-zero when it checks zero items: check_classification, syntax_check, run_p1_host_test, fs49_context and rerun_gates.
- No Unreal source was copied into outputs. The track only holds a copy of the project's own PosixFile.cpp.
- No evidence in /tmp. The earlier temp-dir writes from the gate self-test are disclosed in the README.
- The hard-coded Townfall title id at lineage runtime_start_bridge.cpp:538 is an existing diagnostic, not the worker's code. The worker reported it.
