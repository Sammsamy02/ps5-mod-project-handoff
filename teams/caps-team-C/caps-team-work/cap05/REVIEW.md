# cap05 (crash capture) - independent review

Reviewer: the ONE independent reviewer for Team C #5. Mac-only: no console contact. I did not edit the
deliverable; this file is the only one I wrote here. My re-runs used copies in my scratchpad.

## Verdict: PASS-WITH-FIXES (nothing blocks integration or the launch)

The runtime change is small, opt-in and fails closed. It applies cleanly to the real candidate tree and
passes the gates, which I re-ran myself. The tests are real: they use real artifacts and no test is
skipped. The fixes below are about the tests breaking later and about build-policy cost. None of them
is a safety issue.

## What I re-ran myself (all Mac-only)

| Check | Result |
|---|---|
| `tests/run_tests.sh` (on a copy of the folder) | overall PASS, exit 0: Python 21/21, C++ 10/10 (-O0 and -O2), Lua 12/12, double-apply refused 1/1, wrapper 2/2 (launch-3 copy: 6950 klog lines, 1 crash block, 18 exact frames; no klog gives exit 3) |
| `PATCH/FILES.sha256` | 14/14 OK |
| candidate `src/LuaMod.cpp` sha256 | `96e8ff4d...` = the diff's stated base |
| `patch -p1` of `LuaMod.cpp.cap05.diff` on a copy | applies cleanly; result sha `87c5081f...` = `PATCH/work/LuaMod.cpp.cap05` |
| `apply_cap05.py` on the real candidate (output to scratch) | PASS anchors=2, added_lines=66, same sha `87c5081f...` |
| `no_detour_gate.py --root <patched copy> --patch <diff>` | PASS, 48 sources + 1 patch scanned, 0 detour sites |
| `syntax_check_tree.py --unit luamod=src/LuaMod.cpp` on the patched copy (PS5 flags) | checked 1 unit, exit 0, 0 errors |
| fs-throw | baseline keys are `relpath|function|code` (not line numbers), so the 66-line shift cannot create a NEW site. The added lines contain no `std::filesystem`, file I/O, `#include`, `sigaction`, `mprotect` or hook call (grep over the 67 `+` lines: only the comment matched). |
| `run_labelled_build.sh.cap05.diff` | the base sha `7439ad89...` matches the live script; applies on a copy; `bash -n` OK; identical to `work/run_labelled_build.sh.cap05` |
| x86-64 codegen (`clang++ -O2 -target x86_64`, the PS5 CPU family) | the fault function is kept (not inlined), loads `fault_va`, then stores; `trap` emits `ud2`; `dispatch` calls it |

## Hard rules

- Generic: the runtime names no game, mod or engine. `zzcrashtest` is our own diagnostic folder, and the
  runtime never names it. OK.
- No detours. The change is a new Lua global and nothing else. OK.
- Fail closed: the refusal path works with a wrong or missing token, a bad kind, or arguments that are not
  strings (checked against `Lua::is_string`/`get_string`, `LuaMadeSimple.cpp:1375-1386`). The second
  `is_string()` reads index 1 again after the first `get_string` removed it. That is correct. With
  `(nil, TOKEN)` the call is refused, not crashed. It never raises a Lua error. OK.
- Two sources / SDK: no shipped engine value and no new import. The tool's address rule is checked on
  two modules (our PRX and the eboot), and the build-id is bound to the fingerprint. No SDK material. OK.
- No console contact, and the mods are untouched. OK.
- Every gate or tool prints a count and fails when the count is zero: crashcap exits 3 on 0 klog lines;
  final_symbols refuses 0 sections, 0 bytes or 0 symbols; run_tests fails on 0 checks. OK.

## Findings

1. **FIX-SOON - the tests will break after the next labelled build.** `tests/test_cap05.py:22` hard-wires
   `R11 = outputs/2026-10-08-l26-package-A/pass-1`. Step 2 of `run_labelled_build.sh` moves package-A to
   `archive-r6/<prev-label>/` at the start of every build. Townfall step 1 (B6p, then B7) comes before
   cap05, so by integration time package-A will hold a different PRX. The test
   `test_r11_exact_layout_and_binding` would then fail on `R11_SHA`, and the refusal tests would run
   against the wrong build. Fix: find R11 by sha (package-A, or the archive folder whose PRX sha is
   `bfd95db6...`), and fail loudly if neither is found.
2. **FIX-SOON (build-policy cost) - step 8b can waste a build label.** If the final layout ever stops
   matching the writer rule that final_symbols.py re-implements (for example a packaging change from B7 or
   `L26_LUA_AS_CPP`), step 8b fails AFTER the PRX is built and verify_build has passed. Then `RESULT=FAIL`
   is written, labels are single-use, and `RESUME_AT_PACKAGE` accepts only `STEP=packaging`. A good PRX
   costs a rebuild. Suggestion: for the cap05 build, use the hand-run command from APPLY step 4 (same
   check, no label at risk), or make 8b write a FAIL note without failing the build.
3. **NOTE - host C++ test platform.** The C++ test runs on arm64 (the Mac), so `trap` shows SIGTRAP 5 there.
   On the PS5 (x86-64) it is `ud2`, which gives SIGILL. The launch only uses `segv`, so this does not
   matter. I checked the x86-64 code separately (table above).
4. **NOTE - `inline volatile uintptr_t fault_va`.** Under -fPIC it is a weak/COMDAT symbol read through the
   GOT. LuaMod.cpp already has the same pattern (`refused_log` inline atomics, `LuaMod.cpp:393-396`; perf
   counters at `:5051-5056`), so this adds no new kind of relocation and no new import. A simple
   `constexpr` plus a volatile read would also work; no change needed.
5. **NOTE - the token is not a security boundary.** `CRASH-THE-GAME-NOW` is plain text in the PRX. A hostile
   downloaded mod could use it, but it could crash the game anyway. The token only stops accidents, as
   claimed. Whether the function stays in later builds is the user's call (APPLY step 10).
6. **NOTE - launch card wording.**
   - Say plainly that this deliberate-crash launch does NOT count as one of Townfall's "2 clean launches".
   - The UE4SS.log PASS lines depend on log writes surviving the crash. NOTES cites agent-CL F3 for this
     (one `write()` per message); I did not re-verify it. If those lines are missing, crashcap's two
     `EXPECT PASS` lines still decide the verdict. The card should say so, so that missing log lines
     alone are not read as FAIL.
   - Otherwise the card is safe and specific: offline, klog tap running first, stay at the title, no save
     loaded, ask before reading logs, rollback R11 `bfd95db6`.
7. **NOTE - order.** APPLY step 2 correctly targets "the next clean base after Townfall step 1". Do not
   bundle this with the B7 launch: CLAUDE.md requires ONE launch per capability, and a deliberate crash
   would hide B7's results.

## User's relayed report (grey/white checkerboard on Leon; outdoor scene fully lit, monster clearly visible)

This is not a crash, so cap05 cannot capture it. I agree with NOTES section 6: it belongs to #7/#8/#12.
My own reading, Mac-only and unverified:
- The grey/white checker is how UE shows a texture that has no usable data for this platform. The PC pak's
  textures are PC-cooked, and the PS5 build cannot use them, so the engine shows its default instead.
- The over-bright scene looks like a material, post-process, fog or lighting asset (or a shader) from a pak
  failing on PS5 and falling back. That removes the game's darkness.

Evidence that would settle it (integrator, after the user confirms the game is closed):
- that launch's UE4SS.log around the door / level load;
- the list of paks mounted in that session;
- one launch with only the Leon pak removed, to show whether the lighting problem comes from the same pak.

## Integration checklist (unchanged from APPLY.md; all pass on my re-run)

fs_throw_gate (0 NEW), no_detour_gate --patch (PASS), syntax_check_tree luamod (0 errors), build once,
verify_build --manifest, final symbols PASS, then the console steps, each with the user's OK.
