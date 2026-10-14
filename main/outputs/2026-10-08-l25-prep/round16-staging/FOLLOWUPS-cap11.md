# Follow-ups for the next runtime build (from the R15e review, 2026-10-13, PASS-WITH-FIXES)

1. NativeGuiEngineBackend.inc build_surfaces: a partial build (any step failing after m_target/m_widget/m_image are
   set, incl. the new keep-image collapse) leaves prepare() treating the GUI as built -> window "opens" invisibly and
   blocks move/look input. Fix: `bool m_built{}`; clear at the start of build_surfaces, set just before `return true`,
   prepare() tests `!m_built ||`.
2. GetViewportScale lookup: when the function/param is missing, frame_for/put emit an ERROR "[GUI] refused" line and
   use one of the 8 refusal log slots although the GUI continues. Fix: direct lookup, one WARNING only.
3. Note: the scale is read at surface build time only (the shipping minimap re-checks every 100 ticks). Consider a
   cheap re-check (e.g. on viewport size change).
4. From the R15 review: Team D's host tests compile their own copies (NativeModHostCore.hpp still has <exception>,
   old .inc copies): rerun cap09/cap11 tests with the live typescan-r4 include first.
5. From the #9 launch: give the C++ mod game-thread tick the FrameGate (it runs per pump call, ~170-1,960/s).

## STATUS 2026-10-13 ~23:15Z
Items 1-5 are implemented, reviewed (PASS-WITH-FIXES, 7/8 fixes folded in) and host-tested (14/14, 320 checks;
cppmod target compile: 0 new undefined symbols) in caps-team-D/caps-team-work/cap11-followups/ (PATCH/apply_followups.py,
APPLY.md with the new added_header_hashes). NOT in R16 (R16 = R15e + #7 step 2 only). Goes into the next build
(with #10). Re-test #11 at 4K after that build (sharp text, re-check after a map change).

## Build-pipeline follow-ups (2026-10-14)
6. GATE SCOPE GAP: run_labelled_build.sh's pre-compile fs_throw_gate (and no_detour_gate) scan the candidate tree
   but NOT outputs/2026-10-08-l26-runtime/typescan-r4/include, where new headers such as PakRequestProducer.hpp
   live. A standalone `fs_throw_gate.py --root typescan-r4/include` flagged a site the build gate missed. Fix: add
   the include root to both pre-compile gates. That makes the gates stricter; do not change their counts by hand.
7. Ledger note: R16c-cap07s2-fsfix was stopped during step 1 (to rename error_code variables). R16d's step 2 then
   archived the R16b build dirs under the label `R16c-cap07s2-fsfix-stopped`. R16b's PRX is frozen at
   2026-10-08-l26-frozen/R16b-cap07s2-trigger-9d42deba/.
8. Optional (R16b reviewer): log one `[PAKMOUNT] request src=marshal|vmt` line before the call.
9. PakRequestProducer.hpp l.8-10 comment (R16d review, PASS-WITH-FIXES, comment only): the #9 scan does not prove PS5
   fills d_type (is_directory falls back to stat). Label it Mac-only and mention the lstat fallback: on DT_UNKNOWN
   plus a failing lstat, the mount is refused with "entry type unavailable". There is no behaviour change, so it was
   not rebuilt for R16d.
