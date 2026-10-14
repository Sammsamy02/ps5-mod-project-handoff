# REVIEW — one independent reviewer on Team G's #10 shipping changes

Verdict: **PASS-WITH-FIXES** (1 Medium, 5 Low, 5 Info). Mac-only; the reviewer
edited nothing, ran no build, did not touch the console.

Reviewed: the patch (`c527cdf0` at review time), the lazy-start edit in
`UE4SSProgram.cpp`, `Cap10CSharpMods.hpp`, `Cap10OfflineNet.cpp`, the fixed-gate
helpers in `cap10_integration.py`, the driver edits, and `ADMITTED-IMPORTS.json`.

Confirmed PASS (reviewer's own checks):
1. Lazy start: with no C# mod, `init()` makes only the pre-#10 call; no admission
   scope, no `run_embedded_probe_once`. Linking Mono adds no constructors
   (`.ctors` 237 = R15e), no strong-definition overlap with R15e (0 of 5,710 vs
   64,926), no import switched to owned code. Flag release/acquire correct;
   default false; generic (only constant: folder name `csharp`).
2. Planner: error_code-only filesystem calls (fs_throw PASS, 3 sites), bounded
   loops, no path traversal, byte compare only (Mono loads the embedded copy).
   19 extra sanitizer (ASan/UBSan) checks with the real 3,584-byte image passed.
3. Offline SceNet stubs: exactly 7; ABI matches the only caller (`netdb.o`
   disassembly: arg counts 3/3/7/6, results checked with `js`, errno only read);
   no network, no exec memory.
4. Fixed gates not self-referential; include exemption exact and one-unit scoped.
5. All 143 admitted rows independently re-verified against the two captured
   providers (NID, symbol index, encoded name, kind, library/module versions);
   no SceNet; 207+143=350, raw 210+151=361, 240/2.

Findings (all handled — see FIXES.md):
1. Medium — missing Mono runtime file would abort the game at init.
2. Low — "hidden visibility" claim false on the PS5 target (no functional effect).
3. Low — `admitted()` trusted the evidence label.
4. Low — start of the pin chain not enforced.
5. Low — catch-block log calls can throw.
6. Low — tests: stand-in probe image; GPT tests left failing.
7–11. Info — fs-throw scratch-path false "NEW" sites; raw delta assumes #7 adds 0;
   libkernel vs libScePosix library choice; corlib resident when #10 is built in;
   launch-card enable notes.
