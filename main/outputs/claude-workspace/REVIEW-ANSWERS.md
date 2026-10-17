# Answers to REVIEW.md

Source reviewed: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/claude-workspace/REVIEW.md`
(6 questions, confirmed present). A second, more detailed document in the same
directory, `FRAMEWORK-REVIEW.md`, poses the same 6 questions with richer
per-component descriptions and explicit source-location pointers; I used it
(read-only) to identify which actual directories in the repository each
generic component name ("Data Validator", "Pattern Matcher", etc.) refers to,
then verified claims against those directories' own files.

## Component → source mapping used below (verified, not assumed)

| REVIEW.md name | Actual worker directory (relative to `outputs/2026-10-04-zcode-team/` unless noted) | Claimed tests | What I actually found |
|---|---|---|---|
| Data Validator | `worker-kernel-saferead/` (`safe_read_client.h`, `safe_read_kernel.c`) | 100/100 | **No matching test artifact exists.** `safe_read_client.h` line 4 and the header of `safe_read_kernel.c` both read "STATUS: research prototype... never compiled, never run on console." There is no test file, Makefile, or run log anywhere under `worker-kernel-saferead/`. |
| Pattern Matcher | `worker-code-scan/` + `worker-shadow-integrate/` | 35/35 + 38/38 | Confirmed. `worker-code-scan/host_test/run-output.txt` ends "35 checks, 0 failures". `worker-shadow-integrate/host_test/run-output.txt` ends "38 checks, 0 failures". |
| Event Redirector | `worker-r3-ladder/` + `worker-gate5-loadmap/` | 40/40 + 10/10 | Confirmed. `worker-r3-ladder/REPORT.md:125` and `CLAIM.json:32` both state "40/40 PASS". `worker-gate5-loadmap/host-test/run-r3.stdout` ends "PASS: 10 rows, 0 checks failed". |
| Boot Loader | `worker-early-load/` | "Verified offline" | Confirmed. `patch-verification.txt` shows two offline patch runs against the 194,241,424-byte decrypted eboot, both "== exit 0 ==", changing 10 and 31 bytes respectively (DT_NEEDED-style dynamic-section edit) — matches "10-31 byte patch" in `FRAMEWORK-REVIEW.md` Component 4. |
| Remote Viewer | `worker-kernel-debugger/` (`gdbstub.c`, `ps5_backend.c`) | 72/72 | Confirmed. `HOST-TEST-LOG.txt:90` "72 passed, 0 failed"; line 178 "PASS: 72/72 conformance checks; lldb attach + register read + ...". |
| Input Sharer | `outputs/2026-10-06-claude-cloud/staging/pad-borrow/` | 17/17 | **Claimed but not independently verifiable from a saved log.** `outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md:118` asserts "Host test ... 17/17", but the `pad-borrow/host-test/` directory in this workspace contains only source files (`test_pad_borrow.cpp`, `run.sh`) — no captured run-output log, unlike every other component above. A separate, earlier attempt at the same problem, `worker-pad-coexist/`, has a saved log but reports "54 checks" (`REPORT.md:67`), not 17 — a different test suite, not a mismatch in the same one. |
| File Overlay | `worker-file-overlay/` | "Design complete" | Confirmed as a design-only deliverable. `REPORT.md` opens "Scope honored: research + design only; no console contact; no game-state, mount, or acceptance claims." Its own recommended approach is explicitly unproven in-game: `engine_mount_or_asset_origin_proved: false` is quoted as the still-open verdict. |
| Format Translator | `worker-asset-convert/` | "Design complete" | Confirmed design-only. `ASSET-CONVERT.md` opens "offline research + design only. No code changed, no console contact, no game/runtime execution, no installation, no completion or acceptance claims." |

Two load-bearing, independently-verified facts underlie several answers below:

- **Timing.** With the exception of `worker-r3-ladder`/`worker-gate5-loadmap`
  (2026-10-05 ~19:54-19:58) and `worker-virtual-memory` (2026-10-04 17:39),
  every other component directory above was created within one ~31-minute
  window on 2026-10-06 (13:03 through 13:34, by directory mtime). This is a
  same-day, same-session batch of host-only prototypes, not independently
  matured/console-validated subsystems.
- **No in-game confirmation anywhere in this repo.** `README-RETURN.md` §1
  states the console has not run any build since "launch-9" (2026-10-05),
  which itself reached the menu with "NO mods loaded"; §4 states "Item
  Spawner activation has still never been observed in-game." The 2026-10-06
  `MAC-SESSION-BRIEF.md` (written the same day as the component batch above)
  independently confirms no build in the current lineage can show a menu at
  all because of three unresolved blockers (B1/B2/B3), and that none of its
  own proposed fixes were compiled with the PS5 SDK or run on console.

With that mapping established, here are the six answers.

---

## Question 1: Is the component separation clean? Any you'd merge or split?

**Answer:** The *naming* separation in REVIEW.md is reasonably clean and
maps to genuinely distinct technical concerns, but the *actual* repository
structure is not organized this way at all — it is organized by dated
"worker" task assignments (`worker-code-scan`, `worker-shadow-integrate`,
`worker-r3-ladder`, `worker-gate5-loadmap`, ...), several of which implement
the *same* REVIEW.md component as sequential patches on top of each other
(Pattern Matcher = 2 workers; Event Redirector = 2 workers). I would:

- **Merge** `worker-code-scan` and `worker-shadow-integrate` into one
  "Pattern Matcher" unit before calling it a component — they are base
  implementation + its runtime-integration wrapper, not two peer concerns
  (`worker-shadow-integrate/ADMISSION.md` lines 1-6 explicitly describes
  itself as integrating "the host-verified shadow-copy code scanner
  (worker-code-scan, 35/35)"). Likewise `worker-r3-ladder` +
  `worker-gate5-loadmap` are both fixes to the same Event Redirector /
  marshal-arming predicate chain (`worker-r3-ladder/REPORT.md` §1 traces the
  same `UnrealInitializer.cpp`/`LuaMod.cpp` call chain that
  `worker-gate5-loadmap/receipt.json` "mission" field also targets) and
  should be one unit with one test suite, not two.
- **Split** "Data Validator" as currently named: `FRAMEWORK-REVIEW.md`
  Component 1 describes a single check-then-read memory validator, but the
  repository actually contains two unrelated things that could both answer
  to that name: the never-compiled kernel-level `safe_read_*` service
  (`worker-kernel-saferead/`) and the host-tested FName-pool bounds/shape
  checker (`worker-fname-validator-v4/`, 69/69 passing per
  `test_pool_bounds_v4.receipt.txt`, root of the `what/` tree). These solve
  different problems (arbitrary-address kernel read vs. in-process pool
  bounds checking) and conflating them under one "Data Validator" line is
  itself a separation failure worth fixing before any production claim.
- **Separate out** the dependency on privileged/kernel-level console access.
  "Data Validator" (as `worker-kernel-saferead` would implement it) and
  "Remote Viewer" (`worker-kernel-debugger/ps5_backend.c`, which reads/writes
  process memory and registers through a kernel I/O shim,
  `worker-kernel-debugger/kio_shim.h`) both sit on top of the same kind of
  privileged access path. That shared, high-risk dependency is not called
  out as its own architectural layer anywhere in the review docs or the
  component table — it should be, since it is a single point of failure (and
  of legal/ToS exposure) for two "components."

**Confidence:** High on the mapping and duplication findings (directly
observed in file contents/paths cited above). Medium on the merge/split
*recommendation* itself, which is a judgment call.

---

## Question 2: Are the test coverage levels adequate for production?

**Answer:** No. None of the numbers in the table are evidence of
production-readiness, for three independently-verified reasons:

1. **All of it is host-side only.** Every "N/N" count I could verify
   (35/35, 38/38, 40/40, 10/10, 72/72, 69/0 for the pool-bounds checker) comes
   from a Mac-compiled, Mac-executed test binary against mocked or
   recorded/offline inputs (`worker-code-scan/host_test/Makefile` — `c++
   -std=c++17`; `worker-kernel-debugger/REPORT.md` — "72/72 checks pass"
   against a *mock* backend `mock_backend.c`, with a *separate*, narrower
   real-LLDB session described alongside it). Zero of these numbers are PS5
   console test results. `worker-code-scan/REPORT.md` §5 states this
   explicitly: "Host verification only... In-game seed arming (Phase F)
   remains pending." `worker-shadow-integrate/ADMISSION.md` §6 repeats it:
   "Host verification only... NOT console-executed." `worker-file-overlay`
   and `worker-asset-convert` have *no* tests at all ("Design complete" is
   accurate — there is no test harness in either directory).
2. **One claimed number (100/100 for the Data Validator) does not
   correspond to any artifact in the repository.** See the mapping table
   above — the only candidate directory (`worker-kernel-saferead`) is marked
   "never compiled."
3. **One claimed number (17/17 for the Input Sharer) is asserted in prose
   but not backed by a saved run log** in this workspace, unlike every other
   component, which does have a captured `run-output.txt` or
   `HOST-TEST-LOG.txt`.

On top of the test-evidence gap, the project's own status document
(`README-RETURN.md`) independently confirms the system has never run
successfully end-to-end on the actual target: no build in the current
lineage has ever loaded a mod in-game (§1: launch-9 "NO mods loaded"), and a
same-day review (`outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md`)
documents three separate blockers that currently prevent the menu from ever
appearing at all. Host-side unit tests, however numerous and clean (several
runs are explicitly ASan/UBSan-clean, which is a genuine quality signal for
the *code as written*), cannot stand in for that — especially for components
whose entire purpose is interacting correctly with console-specific memory
protection, ELF loading, and kernel-level primitives that cannot be modeled
faithfully on a Mac host.

**Confidence:** High. This is based on direct inspection of the test
artifacts (or their absence) plus the project's own status documents, not
inference.

---

## Question 3: Any design decisions you'd change and why?

Evaluating the 7 "Key Design Decisions" from REVIEW.md against what I found:

1. **Check-then-read (vs. read-and-recover).** Sound choice in the abstract
   (SEH-style recovery is indeed unavailable in the stated environment), but
   the implementation of record (`worker-kernel-saferead/safe_read_client.h`)
   is an uncompiled prototype depending on a privileged kernel-side service
   that is not part of this codebase and is not proven to exist in a
   deployable form. I would not change the *decision*, but I would not credit
   it as "built" — it is a design sketch for a capability the project does
   not currently have in hand.
2. **Shadow copy for pattern matching.** Well justified and the
   best-evidenced decision in the set: `worker-code-scan/REPORT.md` §1.1
   explains the exec-only-text constraint motivating it and backs the design
   with 4-for-4 real signature recovery against a real pinned binary. I would
   keep this decision as-is.
3. **Pointer swaps instead of code rewriting (Event Redirector).** Reasonable
   for the stated constraint (write-protected code pages), and the two
   component worker reports (`worker-r3-ladder`, `worker-gate5-loadmap`) show
   real traced bugs being found and host-fixed in the existing hooking
   predicate chain — evidence of actual engineering, not just a paper design.
   One thing I'd flag rather than change: this approach "covers the majority
   of PC hook use cases" per the review text, which is itself an admission
   that some PC-mod hook patterns are out of scope; that gap should be stated
   to mod authors up front, not left implicit.
4. **Library dependency for early loading.** The implementation
   (`worker-early-load/patch-apply.py` + `patch-verification.txt`) is a
   narrowly-scoped, low-byte-count ELF dynamic-section edit with a
   before/after diff and round-trip verification — this is the most
   conservative and best-verified of the eight components structurally
   (smallest blast radius, clearest verification). I would not change it, and
   it is one I'd point to as a model for how the others should be evidenced.
5. **GDB remote protocol for the viewer.** Reasonable interoperability
   choice, but it is built on the same unverified privileged-access
   dependency as item 1 above, and its 72/72 pass rate is against a mock
   backend plus a separate real-LLDB session whose scope (what it actually
   proved) I could not fully determine from the files present. I would ask
   for the real-backend test matrix to be as explicit and as large as the
   mock-backend one before trusting this number.
6. **Container projection for file overlay.** This is the one place the
   project's own worker report disagrees with calling the design decided:
   `worker-file-overlay/REPORT.md` ranks "Container projection" as
   "RECOMMENDED, build now" among *multiple* ranked candidate approaches
   (plural, §2) — i.e., it is a recommendation, not a decision already taken
   and verified, and the report itself flags the open, unproven item
   `engine_mount_or_asset_origin_proved: false`. I would not present this as
   a settled design decision in a status review until that verdict closes.
7. **Data permutation for format translation.** The design document
   (`worker-asset-convert/ASSET-CONVERT.md`) makes a specific, checkable
   technical claim (PC and PS5 BCn texture payload bytes are identical; only
   container header and block-order/swizzle differ) and reasons from public
   GPU tiling facts, which is a legitimate and attractively low-risk approach
   if the claim holds. I would not change the decision, but — as with #6 —
   this is explicitly "design complete," not implemented or verified against
   a real converted asset loaded in-game, and should not be scored as done.

**Overall change I'd make across the board:** institute one evidence bar
before anything in this set is called a "decision" rather than a "proposal":
real target build + at least one real console boot exercising the code path,
not just a host-side mock. Right now decisions 1, 5, 6, and 7 are
proposals/sketches wearing the language of settled decisions; decisions 2, 3,
and 4 have real (if host-only) evidence behind them.

**Confidence:** High on what each report itself claims and discloses; medium
on my overall prioritization/recommendation, which is a judgment call based
on that evidence.

---

## Question 4: How should a plugin manager app integrate with these components?

The actual mod-manager app lives at `outputs/ps5-mod-bridge/` (confirmed by
`PROJECT-BRIEF.md` — "App: `outputs/ps5-mod-bridge/` (the web UI, installer,
API)"), with a Python host/installer layer (`outputs/ps5-mod-bridge/companion/`:
`host.py`, `modbridge_host.py`, `modbridge_inventory.py`,
`modbridge_nexus/`, `modbridge_reports.py`), a native C++ side
(`outputs/ps5-mod-bridge/src/`: `cache.cpp`, `dds.cpp`, `hash.cpp`,
`inspection.cpp`, `inventory.cpp`, `journal.cpp`, `main.cpp`, `nexus.cpp`,
`planner.cpp`), and a small web UI (`outputs/ps5-mod-bridge/web/`:
`app.js`, `index.html`, `style.css`). `FRAMEWORK-REVIEW.md` lists 4 intended
integration points, which I can now anchor to real files:

1. **Mod installation → File Overlay (Component 7).** The app's installer
   pipeline (`PROJECT-BRIEF.md`: "mod installer pipeline (download → prepare
   → install, 42/42 tests)") should treat File Overlay strictly as "drop a
   correctly-named container into a known directory," per
   `worker-file-overlay/REPORT.md` §2's recommended container-projection
   approach — i.e., the app's job is to *name and place* files correctly,
   not to implement any loading logic itself. Given that component's own
   open verdict (`engine_mount_or_asset_origin_proved: false`), the app
   should not assume install success implies the mod actually loaded; it
   needs a separate, explicit "did this actually mount" signal before
   showing the user a success state.
2. **Compatibility checking → Pattern Matcher (Component 2).** `src/inspection.cpp`
   and `src/inventory.cpp` plus the git history's "Content index builder:
   per-game asset/class/function index from game data" commit (`f0917417`)
   suggest the app already builds a per-game index of what exists; this
   should consume the Pattern Matcher's *shadow-scan cross-check* results
   (`worker-shadow-integrate`), not the raw scanner, so the app only ever
   sees seed-authoritative, cross-checked addresses/labels, never silently
   shadow-overridden ones (`ADMISSION.md` §4 point 2 is explicit that
   disagreement must be a loud refusal the app can surface to the user, never
   a silent substitution).
3. **Version detection → Data Validator (Component 1).** Per the mapping
   above, there is currently no working implementation to integrate against
   — this integration point is blocked until Component 1 actually exists in
   compiled, console-tested form.
4. **Asset verification → Format Translator (Component 8).** `src/dds.cpp`
   in the app already looks like the natural home for the BCn
   header/swizzle-rewrite logic `worker-asset-convert/ASSET-CONVERT.md`
   designs; the integration point is real and the design is low-risk, but
   per Q3 it is unimplemented/unverified, so the app should gate "this asset
   is convertible" on an actual round-trip test, not just the presence of
   the design doc.

**General integration principle I'd add, not currently stated anywhere in
the review docs:** every component above that can refuse (seed/shadow
mismatch, container admission failure, pool-bounds violation, IO probe
failure) does so with a structured, loud, named refusal in its own tests
(e.g. `worker-shadow-integrate/host_test/run-output.txt` refusal lines name
both disagreeing values). The app layer should have one single, consistent
channel for surfacing these refusals to the user as "incompatible / why,"
rather than each integration point inventing its own error presentation —
this is exactly the "Compatible / Partially compatible / Incompatible with
reasons" UI described as still in progress in `PROJECT-BRIEF.md` "What you
were working on" item 4, and the component reports already give that UI
real, specific reason-strings to surface if it's wired up that way.

**Confidence:** Medium-high on the file/report mapping; medium on the
specific integration recommendations, since I did not trace the app's
current code to confirm how much of this wiring already exists versus is
still aspirational (the app's own next-steps list in `PROJECT-BRIEF.md`
indicates this wiring is explicitly unfinished work).

---

## Question 5: What's missing for a complete plugin ecosystem?

Based on what exists vs. what REVIEW.md's 8-component list and the app's
"Next steps" actually cover:

1. **A real, console-verified Data Validator.** This is the most
   foundational gap: every other component that reads game memory/data
   implicitly assumes safe reads are available, but the one component
   meant to provide that guarantee is an uncompiled prototype (see Q1/Q2).
2. **Closure on the three concrete blockers documented the same day as this
   component batch.** `outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md`
   lists three independent, already-diagnosed blockers (a walkguard refusing
   an exec-only-text read in `GameEngine::LoadMap`; the game holding the only
   pad handle so input never arrives; and the hook-arming logic only
   targeting native functions when the two actually-invoked carriers are
   Blueprint overrides) — none of which are solved by any of the 8
   REVIEW.md components. A "complete ecosystem" still cannot show a menu or
   a successful spawn without these being fixed and *console*-verified, not
   just host-tested (the brief itself: "nothing here was built or run on the
   console").
3. **A version matcher and name mapper**, called out as still "Next steps"
   in `PROJECT-BRIEF.md` item 1 — without these, a mod manager cannot tell
   whether a given PC mod's referenced functions/assets exist in a
   particular PS5 build, which is the entire premise of "compatibility
   checking" in Q4 point 2.
4. **A generic "mod engine loader" abstraction.** `PROJECT-BRIEF.md` item 4
   explicitly flags this as unexplored: everything described in both review
   documents is Unreal-Engine-specific (UFunction/vtable swaps, UE4SS seed
   tables, `/Script/Engine.Actor:ReceiveTick` carriers); nothing here
   generalizes to a non-Unreal engine, so "ecosystem" currently means
   "Unreal Engine 5.6 titles only," and that scope limitation isn't stated
   anywhere in the review docs themselves.
5. **An uninstall/rollback story.** None of the 8 components or the app's
   described pipeline (download → prepare → install) mention a corresponding
   clean uninstall/rollback path; `README-RETURN.md` §5's "No re-dispatch on
   ambiguity... never retry or replay an interrupted/ambiguous step" implies
   the team has already hit failure states mid-staging that needed manual
   recovery — a real ecosystem needs this handled automatically, not by a
   human following a runbook.
6. **NexusMods-side provenance/compatibility metadata**, i.e. a way to know
   *before* download whether a given Nexus mod page's file targets a
   PC-only API surface this framework can't bridge — right now compatibility
   is discovered only after inspecting a downloaded mod's contents.
7. **A single coherent test/evidence policy** across components — right now
   some components have ASan/UBSan-clean host suites, some have design docs
   with zero tests, and at least one has a test count claimed in prose with
   no corresponding log. A real ecosystem needs one bar all components are
   held to before being advertised as done.

**Confidence:** High for items 1-4 (directly sourced from the project's own
documents). Medium for items 5-7 (inferred from absence of evidence plus
general software-ecosystem practice, since I did not find any file that
explicitly discusses uninstall/rollback or a unified test policy).

---

## Question 6: Which component should be deployed first for earliest value?

**Answer: Boot Loader (`worker-early-load`), not any of the others —** and
specifically, deploy it *alone*, as a verification milestone, before
anything else in the table.

Reasoning, directly from the evidence gathered above:

- It is the only component with a complete, narrowly-scoped,
  round-trip-verified artifact: a byte-identical before/after diff (10 or 31
  bytes depending on patch variant) against the real 194,241,424-byte
  decrypted eboot, with an explicit postcheck of the ELF dynamic section
  (`patch-verification.txt`: "DT_NEEDED count 48 -> 58; DT_NULL intact...").
  Its blast radius is the smallest of all 8 components (one ELF table edit),
  which makes it the cheapest to validate on real hardware and the safest
  first real-world test of "does anything in this stack survive contact
  with the actual console."
- It is also the component everything else structurally depends on:
  REVIEW.md's own "Key Design Decisions" §4 frames early loading as what
  lets the framework initialize "before the game engine starts" — Pattern
  Matcher, Event Redirector, File Overlay, and the Remote Viewer are all
  meaningless without the framework being loaded into the process at all.
  Deploying it first is not just lowest-risk, it is a hard prerequisite for
  being able to observe *any* of the other components doing anything
  on-console.
- By contrast, the "obvious" first-value candidates fail this test: Pattern
  Matcher and Event Redirector have the best host-side numbers (35/35+38/38,
  40/40+10/10) but are pointless to deploy before the loader that gets the
  framework running at all; Remote Viewer and Input Sharer both sit on
  unverified or unconfirmed foundations (Q1/Q2); File Overlay and Format
  Translator are explicitly "design complete," i.e. not yet implemented
  code to deploy.
- This recommendation is also consistent with the project's own current
  priority as documented same-day: `MAC-SESSION-BRIEF.md` §4 describes the
  very next candidate build the team is assembling, and item B1 in that
  brief (the walkguard/GATE-1 fix) is specifically about getting *past* the
  early-init sequence that the Boot Loader establishes — i.e., the team's
  own real-world next step is already organized around proving the boot
  path works before anything downstream of it can be judged.

**Caveat:** "Deployed first" here means "staged and console-tested first as
an isolated change," consistent with how `worker-early-load` itself was
scoped (an isolated patch with its own verification, not bundled with other
component changes). It does not mean it delivers end-user-visible value on
its own — a successful early-load boot with nothing else wired up is purely
a verification milestone, not a feature a player would notice. If the
question is "earliest *player-visible* value" rather than "earliest
*de-risking* value," the honest answer is: none of the 8 components can
currently deliver player-visible value alone, because (per Q5 item 2) the
menu/spawn path is blocked by three separate, still-open issues documented
the same day this review's components were produced, none of which these 8
components fix.

**Confidence:** High on the dependency argument and the verification
evidence cited. Medium on treating this as a definitive single
recommendation, since "which to deploy first" ultimately depends on risk
tolerance the project documents don't state explicitly.
