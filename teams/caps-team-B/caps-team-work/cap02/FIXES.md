# cap02 FIXES — review findings -> what was done (Mac-only, 2026-10-12)

All work is inside `caps-team-B/caps-team-work/cap02/`. No console contact, no
PRX build, the live candidate tree was only READ (copied to a temp folder for
the apply/gate tests). Every claim below is Mac-only.

| # | Level | Status | What changed |
|---|---|---|---|
| F1 | FIX-SOON | fixed | `PATCH/01` header is now `a/src/UE4SSProgram.cpp`. Applied with `patch -p1 --fuzz=0 -d <ts>/candidate` by `PATCH/apply_cap02.py` (dry run by default). |
| F2 | FIX-SOON | fixed | `apply_cap02.py` also copies the two headers into `candidate/include/`, syncs the CHANGES.json `files` row pin (`121f2383…` -> `642b1988…`), adds both headers to `added_headers` + `added_header_hashes`, writes a `cap02_row_sync` record, backs up CHANGES.json, and re-checks with the G2 rule. All inputs pinned (`PATCH/MANIFEST.json` + before/after sha). A second run refuses. |
| F3 | FIX-SOON | fixed | `tests/run.sh` rewritten (bash): every step's exit code is captured (no pipe hides it); the C++ step also needs a `PASS: <n> checks` line with n > 0; a missing image FAILS unless `CAP02_ALLOW_IMAGE_SKIP=1`; the exit code is the number of failed steps; "ALL PASSED" prints only when every step passed. Proven with two planted failures (below). |
| F4 | NOTE | fixed | Wording made honest in the log line, header comments, NOTES and card: the shadow MATCH is true by construction and is a drift check, not proof of #2. The card now rides along with the next capability launch (#7), no launch of its own; the real #2 test (compiled layout off, AUTO path on) is listed as later work. |
| F5 | FIX-SOON (blocks PATCH/02) | fixed in code for the arming path; PATCH/02 stays blocked | Packs now carry `arming_cleared` and `single_source_rows`. `generate()` (the arming path) refuses any pack with single-source or config-dependent rows, for every image. The UE5.6 pack has 11 single-source rows + the IRIS-dependent ProcessEvent slot, so it is refused (`arming_cleared=false`). The shadow uses a separate compare-only `render()`. Clearing a pack (per-image verification, or an IRIS config axis) is deferred to the PATCH/02 work. |
| F6 | NOTE | fixed | `PATCH/02` regenerated as a valid diff (applies on top of PATCH/01 in a scratch copy, no fuzz), renamed `02-NOT-SHIPPABLE-firstlaunch-auto-followon.diff`, with a NOT SHIPPABLE banner; not in MANIFEST, not used by the apply script (host test step 9 checks all three). `APLY.md` -> `APPLY.md`; the dead reference to `UE4SSProgram.load_offsets.diff` is gone. PATCH/02 still does not compile (`detected_engine` does not exist) — by design until its pieces exist. |
| F7 | NOTE | fixed | `parse_header` uses non-throwing `std::from_chars` (bad or out-of-range numbers return parsed=0); the shadow block in PATCH/01 is wrapped in `try { } catch (...)` that logs `verdict=DIVERGE` and still stages the compiled layout. New host test T10. |
| F8 | NOTE | fixed | `build_packs.py` reads the R11 declaration/profile from `l26-runtime/profile/units/` (same shas `ab8750c5…` / `09a14712…`), not the `l26-package-A/lineage/` folder that moves at the next build. The integration harness still needs `NativeLayoutDeclarations.hpp` from `l26-package-A/lineage/`; if that folder moves, step 6 fails loudly. |
| F9 | NOTE | deferred | Build-id width (20-byte note vs 16-byte R11 form) only matters for PATCH/02. Listed as PATCH/02 precondition 4 in `PATCH/APPLY.md`. No effect on PATCH/01. |
| warn | (table) | fixed | The `-Wsign-conversion` warning at header line 85 is gone; the C++ test now builds with `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror`. |

## Test evidence (full suite re-run at the end)

`bash tests/run.sh` (full log: `tests/test-output.txt`) -> exit 0,
`ALL CAP02 HOST TESTS PASSED (9 of 9 steps)`:

| Step | Result (Mac-only) |
|---|---|
| 1 build_packs | 105 rows (81 member / 24 vtable), 94 two-source, 11 single-source, 489 UE4.26 candidate rows (not shipped), `arming_cleared=false`, digest `47114766…` (unchanged) |
| 2 manifest | 3 pinned patch files, 0 mismatched |
| 3 C++ tests | `PASS: 29 checks` (was 20; new T9 arming refusal, T10 no-throw on bad header), built with `-Werror` + sign/conversion warnings |
| 4 image identity | PASS; derived identity byte-equal to `evidence/townfall-identity-derived.json` |
| 5 no game identity | 3 shipped files checked, PASS |
| 6 SysV compile vs real R11 declaration (`ab8750c5…`) | `parsed=1 verdict=MATCH member_rows=81 vtable_rows=24` |
| 7 apply on scratch copy of live candidate | dry run + execute PASS (7 pre-checks, 3 post-checks); second apply refused; G2 rule: cap02 adds 0 drift, its row and 2 header hashes match |
| 8 gates on patched copy | no_detour PASS (0 sites); fs_throw 35 = 35 sites before/after (no new call); field_gate PASS, same verdict lines (49 read / 48 proven / 0 unproven / 1 accepted), scanned 41 -> 43 files (the 2 new headers) |
| 9 PATCH/02 | banner present, not in MANIFEST, not used by the apply script |

Planted failures (scratch copies, deleted afterwards; saved in
`evidence/planted-failure-proof/`):
- injected `CHECK(false, ...)` -> `FAIL: INJECTED planted failure`,
  `CAP02 HOST TESTS FAILED: 1 of 9 steps failed`, exit 1;
- saved image path pointed at a missing file -> step 4 FAIL, exit 1.

The live candidate tree was not changed (`src/UE4SSProgram.cpp` still
`121f2383…`, no new headers, no cap02 backup file).

## Found while testing (not caused by cap02)

The live candidate tree already has 2 pins that drift from CHANGES.json before
cap02 is applied: `candidate/src/LuaMod.cpp` (row `95591da5…`, file
`96e8ff4d…`) and `candidate/src/UObjectGlobals.cpp` (row `78e48b3a…`, file
`69526476…`), both modified 21:59 on 2026-10-12. This looks like integrator
work in flight that has not been row-synced yet. The build's G2 gate will stop
on these until their rows are synced. cap02 does not touch them; the host test
checks that cap02 adds no drift of its own.

## Integrator follow-up (2026-10-13)

- **Why it changed:** the R13 build refused cap02. The include-graph gate rejected `<charconv>`
  (from AutoProfileGenerator.hpp), so cap02 came out of R13. R13c carries cap #4 only.
- **Header fix (applied here):**
  - `from_chars` is replaced by a digits-only parse with an overflow check.
  - `static thread_local std::string` is now `static std::string`, so there is no `__cxa_thread_atexit` import.
  - The MANIFEST sha is updated.
  - Result: the C++ generator tests (step 3) PASS.
- **Re-stage before cap02 rides along with a later build:**
  - PATCH/01 must be regenerated against the new `UE4SSProgram.cpp` base: `fe9fe7bc…`, which is R11 + R12 + cap #4.
  - Step 6's harness path must point at the archived R11 lineage
    (`l26-build-variants/archive-r6/R11-watch-world-uenum/2026-10-08-l26-package-A/lineage/`).
  - The field_gate step must count files relative to the new tree.
  - Steps 6, 7 and 8 fail today for exactly these reasons.
- **Re-staged (integrator, 2026-10-13).**
  - PATCH/01 was regenerated on R13c `fe9fe7bc`, giving `ad3ae621`. apply_cap02 pins and the MANIFEST are updated,
    and the harness now points at the archived R11 lineage.
  - Host tests: 9/9 PASS.
  - The include-graph gate is only exercised by a real build, so the next runtime build (the #5 build) confirms
    `<charconv>` is gone from the graph.
