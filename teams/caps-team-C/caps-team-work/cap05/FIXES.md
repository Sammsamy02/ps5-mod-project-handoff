# cap05 fixes after review (Mac-only; no console contact; no PRX built)

Review verdict was PASS-WITH-FIXES (no BLOCK findings). Every finding is handled below.

| # | Finding | Status | What changed | Evidence |
|---|---|---|---|---|
| 1 | FIX-SOON: tests hard-wire R11 = package-A and break after the next build | **fixed** | `tests/test_cap05.py` now finds builds by PRX sha. R11: the packaged PRX is the frozen copy `outputs/2026-10-08-l26-frozen/R11-watch-world-uenum-bfd95db6/` (sha checked = `bfd95db6...`). The object + bare PRX + sidecar come from whichever of package-A or `archive-r6/*/2026-10-08-l26-package-A/pass-1` holds a PRX with that sha (the frozen folder has no object/bare). B6b is found the same way. No match = `BuildNotFound`, loud, suite fails. 3 new tests (`LocateBuild`). | `tests/out/python-suite.txt`: `located R11 bfd95db6: packaged=.../frozen/R11-...`, Ran 24 tests OK. Planted wrong sha -> `BuildNotFound`, overall FAIL (`tests/out/planted-failures.txt`). |
| 2 | FIX-SOON: step 8b can waste a build label | **fixed** | Step 8b is now **non-fatal**. On a final_symbols error, or if the output file already exists, it prints `8b/9 final symbols: WARN: ...`, writes `FINAL_SYMBOLS=WARN: ...` to `<build evidence>/FINAL-SYMBOLS-STATUS` and goes on to step 9; it never calls `fail`. Success writes `FINAL_SYMBOLS=PASS`. New diff re-made from the live script (base sha `7439ad89...` unchanged); applies on a copy and gives the same file as `PATCH/work/run_labelled_build.sh.cap05`; `bash -n` OK. APPLY.md step 4 updated. New runner step 6/6 runs the real 8b block in the same shell setup (`set -u -o pipefail`, same `say`/`fail`). | `tests/out/TEST-RUN.txt` step 6: bad inputs -> WARN, reached step 9, no RESULT file; existing output -> WARN, file untouched; real R11 inputs -> `final_symbols: PASS sections_verified=77993 ... packaged=bfd95db6`; `checked 3 cases`. Planted "8b fatal again" -> `8b bad inputs: FAIL`, overall FAIL. |
| 3 | NOTE: host C++ test runs on arm64 (trap = SIGTRAP), PS5 is x86-64 (ud2 = SIGILL) | **fixed (cheap)** | The runner prints the host arch and this fact. The test already accepted both signals; the launch uses only `segv`. | `TEST-RUN.txt`: `host arch: arm64 ('trap' = SIGTRAP 5 on arm64; on the PS5 (x86-64) it is ud2 = SIGILL 4 ...)` |
| 4 | NOTE: `inline volatile uintptr_t fault_va` uses the GOT | **no change needed** | Same pattern already in LuaMod.cpp (reviewer confirmed); no new relocation kind or import. | REVIEW.md finding 4 |
| 5 | NOTE: token is not a security boundary | **fixed (wording)** | NOTES.md section 4 no longer says a mod "cannot know" the token: it says the token stops accidents only. Whether the function stays in later builds stays the user's call (APPLY step 10). | NOTES.md |
| 6 | NOTE: launch card wording | **fixed** | LAUNCH-CARD.md rewritten short and plain: says the deliberate-crash launch does NOT count as a Townfall clean launch; says missing UE4SS.log lines alone are not a FAIL (crashcap's two `EXPECT PASS` lines decide); adds the `FINAL-SYMBOLS-STATUS = PASS` check. NOTES.md marks the agent-CL F3 log-survival claim as not re-verified. | LAUNCH-CARD.md, NOTES.md section 1 |
| 7 | NOTE: do not bundle with the B7 launch | **fixed (wording)** | APPLY.md step 2 now says: do not bundle cap05 into B7 or any other capability's launch. | APPLY.md step 2 |
| - | Reviewer's notes on the Leon checkerboard / bright scene | **out of scope here** | Not a crash; belongs to #7/#8/#12 (as NOTES section 6 and the review say). | - |

## Not changed

- The runtime diff (`PATCH/LuaMod.cpp.cap05.diff`, `PATCH/work/LuaMod.cpp.cap05`, sha `87c5081f...`) is
  unchanged, so the reviewer's gate re-runs still hold.
- `PATCH/FILES.sha256` refreshed for the two changed build-script files; `shasum -c`: 14/14 OK.

## Limit to know

R11's object file and bare PRX exist only in package-A right now (the frozen folder keeps just the packaged
PRX). After the next build moves package-A to `archive-r6/<label>/`, the tests find them there by sha. If
those files are ever lost, the tests stop with `BuildNotFound` instead of testing the wrong build.

## Proof the runner fails when it should (`tests/out/planted-failures.txt`)

Each plant ran on a separate scratch copy of this folder; the copies were deleted afterwards.

| Plant | Result |
|---|---|
| one extra failing Python test | `python: ran 25 tests rc=1`, overall FAIL, exit 1 |
| Python suite replaced by zero tests | `python: ran 0 tests`, overall FAIL, exit 1 |
| R11 sha changed to an unknown value | `BuildNotFound`, `ran 0 tests`, overall FAIL, exit 1 |
| step 8b made fatal again | `8b bad inputs: FAIL`, `checked 2 cases`, overall FAIL, exit 1 |

## Final full host run (`tests/out/TEST-RUN.txt`, exit 0)

Python 24/24, C++ 10/10 (-O0 and -O2), Lua 12/12, double-apply refused 1/1, wrapper 2/2, step 8b 3/3:
`== summary: 6 steps run; overall PASS ==`.
