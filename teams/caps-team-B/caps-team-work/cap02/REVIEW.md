# REVIEW — cap02 (automatic first-launch layout profile), independent reviewer

Reviewer: Team B #2 single independent reviewer. Mac-only. No console contact.
Nothing in the deliverable was edited. Every test re-run happened in a scratch copy, so the
team's evidence files were not overwritten.

## Verdict: PASS-WITH-FIXES (for PATCH/01 as a log-only shadow)

PATCH/01 is safe. It only adds log output, and the armed layout stays byte-identical to R11.
It does NOT prove capability #2, though. Fix the items below before the integrator
builds. Do not count this launch as "capability #2 done".

## What I re-ran (Mac-only)

| Check | Result |
|---|---|
| `tests/run.sh` (scratch copy) | build_packs: 105 rows (81 member / 24 vtable), 94 two-source / 11 legacy, 489 UE4.26 candidate rows. C++: **20 checks PASS**. Image identity self-test PASS. No-game-identity gate PASS (1 file). Regenerated files are byte-identical to the team's. |
| `tests/integration_harness.cpp` against the real R11 lineage declaration (sha `ab8750c5…`, same file as `l26-runtime/profile/units/`) for x86_64 | `parsed=1 verdict=MATCH member_rows=81 vtable_rows=24`, exit 0. Reproduced. |
| PATCH/01 against the real candidate `src/UE4SSProgram.cpp` (sha `121f2383…` = the R11 snapshot 4513a2c6 copy) | Applies cleanly with `patch -p1 --fuzz=0 -d src`. **Fails** with the command APLY.md gives (see F1). |
| PATCH/02 dry-run | **Malformed** (`malformed patch at line 37`). Its hunk-1 context also does not match the file (see F6). |
| `no_detour_gate.py --root <scratch patched tree>` | PASS, 3 sources scanned, 0 sites. |
| `fs_throw_gate.py --root <scratch include with both new headers>` | PASS, 2 files, 0 std::filesystem calls. The patch adds no filesystem calls to UE4SSProgram.cpp. (The 21 old sites there show up only because the scratch path misses the baseline.) |
| `field_gate.py` real candidate vs patched scratch copy | Same before and after: 49 distinct fields read, 48 proven, 0 unproven, 1 accepted with a reason (FUObjectItem::RefCount), PASS, rc=0. The counts are unchanged, as APLY.md claims. |
| Header under `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion` | 1 sign-conversion warning (line 85). Harmless unless the PS5 build uses -Werror. |

## Findings (concrete scenarios)

**F1 FIX-SOON — APLY.md apply command fails.** The diff header is `a/UE4SSProgram.cpp`, with no
`src/` prefix. `cd <candidate>; patch -p1 --fuzz=0 < 01-...diff` stops at "File to patch:"
and ignores both hunks. I reproduced this. Fix: make the header `a/src/UE4SSProgram.cpp`, or
document `patch -p1 --fuzz=0 -d src`.

**F2 FIX-SOON — APLY.md leaves out the build pin step, so the build fails at G2.**
`build_variants.py` reads `typescan-r4/CHANGES.json` and checks that every candidate file
matches its `candidate_sha256`. UE4SSProgram.cpp is pinned there at `121f2383…`. Once
patched, it fails `G2-part-change-pins` ("part CHANGES pin drift") unless someone adds a row
sync (like `r11_row_sync`). The two new headers also need entries in `added_headers` /
`added_header_hashes`. Their include dir (`candidate/include`) is already on the path through
BuildStamp.hpp. Without this, the one allowed build fails or needs rework. Add both steps to
APLY.md.

**F3 FIX-SOON — `tests/run.sh` reports PASS when tests fail.** Every step pipes into `tee`
under plain `sh` (no pipefail), so `set -e` only sees tee's exit code. I injected one failing
CHECK in a scratch copy. The output showed `FAIL: INJECTED…`, then `ALL CAP02 HOST TESTS
PASSED`, and the script exited 0. A missing Townfall image is also logged as "SKIP" and still
ends in "ALL PASSED". The team's own PASS lines are real (I saw them in my run). The
end-of-run verdict is not. Fix: check each step's exit code (`cmd > tmp; rc=$?; cat tmp >>
$OUT`), and make SKIP fail unless it is explicitly allowed.

**F4 NOTE — the shadow MATCH is true by construction.** `build_packs.py` builds the pack by
parsing the R11 compiled declaration itself. `shadow_check` takes EngineRevision/BuildId from
that same declaration's header, then compares the output back to the same declaration. A
MATCH only shows that parsing and re-rendering round-trip, and that the PS5 compiler builds
the code. It does NOT show that a generic source produces Townfall's layout on its own. The
LAUNCH-CARD / NOTES wording ("generic path reproduces the console-proven layout") overstates
this. On the console the MATCH is essentially certain, since the data and the pure function
are the same ones the Mac harness already ran. So a dedicated #2 launch adds almost nothing.
Recommendation: fold PATCH/01 into the next capability launch that is happening anyway
(e.g. #7 paks), where it costs no extra launch and acts as a drift detector for future
layout edits. Save the real #2 launch for an ARMING test on Townfall: compiled declaration
off, AUTO path plus the profile-side generator on, and the same [FIELDS]/[BuildProfile]
PASS lines expected. That test would actually prove #2.

**F5 FIX-SOON (blocks PATCH/02, not PATCH/01) — the "engine-level" pack is calibrated on
Townfall only, and its cell has no config axes.** For a different UE5.6 PS5 game, every row
would come from one other game's image. The 11 legacy rows have one source even for
Townfall. One of them, `UObject::ProcessEvent` 0x268, is config-dependent by the team's own
evidence (0x260 when UE_WITH_IRIS=0). `select_pack` keys only on
(format, ptr, major, minor, target, non_shipping, stats). Scenario: a second UE5.6 game built
with Iris off selects this pack. ProcessEvent then resolves to the wrong vtable slot, and the
first Lua call crashes or calls the wrong virtual. Nothing in PATCH/02 checks anything against
the new image (the profile side is unspecified). The same applies to the three single-source
UEngine/AGameModeBase slots. Before PATCH/02 can ship, the AUTO path needs one of two things:
(a) per-image verification of each row (the existing BuildProfile/LayoutPreflight checks
re-run on the new image), or (b) config axes (at least IRIS) in the cell, with single-source
rows refused for any image other than the calibration one. Otherwise it breaks
"missing values fail closed / wrong values crash".

**F6 NOTE — PATCH/02 is not a valid diff.** It fails to parse. Hunk 1 uses a blank-line
context where the real file has `#endif`, and its counts are off. It references the
non-existent `m_native_context.detected_engine` (the team says so). APLY.md also points to a
non-existent `PATCH/UE4SSProgram.load_offsets.diff`, and the file is named `APLY.md`, not the
requested `APPLY.md`. This is follow-on only, so it is not a launch risk.

**F7 NOTE — the shadow can throw.** `parse_header` uses `std::stoul`. The shadow runs before
`StageNativeLayoutTexts`, so an exception (bad header, bad_alloc) would abort native
activation, which R11 does not do. With the fixed compiled constant this cannot happen in
practice (same bytes as the Mac MATCH). Wrapping the shadow block in `try { … } catch (...) {
log DIVERGE }` would make "log-only" literally true.

**F8 NOTE — inputs are pinned to a folder that moves.** `build_packs.py` reads
`l26-package-A/lineage/`. The ledger says this folder is replaced at the next build (R11
moves to archive-r6/). The sha pin makes it fail loudly ("drifted"), which is correct. It
could point at `l26-runtime/profile/units/` (same sha) or the frozen R11 copy instead.

**F9 NOTE — build_id width (team's open question 1) is a real gap for PATCH/02.** On the
AUTO path the reader returns 40 hex characters (20-byte note). The only existing profile uses
32. Decide before PATCH/02. It does not affect PATCH/01.

## Hard rules

- Generic: no game names in the shipped headers (gate re-run). Pack id and data are
  engine/target level. The data comes from Townfall's image (allowed: produced from the
  game's own files), but see F5 on reusing it for other games.
- No detours: confirmed (gate PASS; the code only reads strings and builds text).
- Fail closed: era/cell refusal is real (UE4.26, stats/non-shipping/target/minor/ptr deltas
  are tested). F5 is the gap within the same era.
- Two sources: 89 rows are image pin + Epic 5.6.1 compile, 5 are Epic compile + image field
  evidence, 11 are single-source R11 legacy. The team labels these honestly. They are
  acceptable for Townfall (console-proven in launch 14) and not acceptable for a new game (F5).
- No SDK, no console contact, mods untouched, writes only in the team folder: I found no
  violation. The UE4.26 data is correctly kept as a refused candidate.

## Launch card

It is safe and specific (exact PASS line, DIVERGE treated as a finding, rollback R11
`bfd95db6`). It needs: the F1/F2 apply steps, a build-ledger entry, and an honest statement
that this launch does not prove capability #2 (F4). Preferably merge it into another
capability's launch.

## About the user's report (Leon textures checkered gray/white; outdoors too bright, monster clearly visible)

This is not caused by cap02. cap02 only adds log lines, and the launch-14 report already
notes "bright outdoor light and wrong textures show up in every launch". Mac-only reading:
a gray/white checker is the engine's stand-in when a texture or material can't be used. The
likely cause is that PC-cooked textures/shaders inside the mod pak don't work on PS5. The
missing darkness could have the same cause: a pak replacing lighting / post-process / fog
assets with PC-only data, so the game falls back to defaults. This belongs to the pak
capabilities (#7/#8/#12, Leon). A useful first diagnostic, when the user approves that
launch, is one launch without the Leon pak. If the darkness comes back, the pak is the cause.
