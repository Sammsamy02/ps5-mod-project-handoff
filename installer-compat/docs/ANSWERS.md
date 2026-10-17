# ANSWERS.md — the three open review questions + two open questions for GPT

Mac-only, host-only, no console contact. Written 2026-10-07 against the
current tree. All counts cited here are defined in `docs/COUNTS.md`.
Q1-Q3 answer the reviewer's questions as of the first 2026-10-07 pass;
in-file UPDATE notes and Q4/Q5 (open questions for GPT) reflect the later
Key/ModifierKey catalog v4 work the same day, and Q5 the lowercase
`scripts/` vs `Scripts/` install refusal. A final round-5 note: the grade
policy was reversed after v4 — a blank `ps5_runtime_status` outranks
everything, so the live TownfallItemSpawner grade is `unknown` (the keyboard
difference rides inside as a KNOWN PS5 DIFFERENCE).

---

## Q1. The 2 system-level BLOCKERs (findings #22 / #23) and who owns them

Source: tester report `tests/REVIEW.md` (integration caveats I-1 / I-2),
numbered as findings **#22** and **#23** in `REVIEW-PACKAGE.md` Section 8.
Both were re-verified against the current code before this answer: still no
adapter in shipped modules, still no `check_install` call in the install path.

### Finding #22 — no installer↔uninstaller record adapter (tester's I-1)

**What exactly is missing.** The three installer-side modules model the same
installed mod with three incompatible record contracts:

- `installer/installer.py` writes `manifests/<Mod>.json` with **absolute**
  `files[].install_path`s and a config model keyed by loadlist row blocks
  (`config_entries.loadlist_rows`, `ploader_prx_name`/`ploader_prx_mode`);
- `installer/uninstall.py` consumes `install_state.json` plus a manifest whose
  `files[].path` entries are **relative to a game root**, and
  `config_entries` as **line lists per config file** (`{"loadlist.txt":
  [...], "ploader.ini": [...]}`);
- `installer/conflicts.py` indexes a third path scheme again.

Nothing shipped converts between them — the round trip works only through the
tester's ~38-line `adapt_install_to_uninstall()` bridge inside
`tests/src-tester/integration_tests.py:46-84` (recomputes relative paths via
`os.path.relpath`, synthesizes the uninstaller manifest + `install_state.json`
from the installer manifest). For a user, install → uninstall is not a working
pipeline today.

**Who owns the fix: zcode (the installer side).** All three modules live in
this project (`installer/`); neither GPT's runtime nor the mod manager is
involved. It is pure host-side Python contract work.

**What the fix looks like.** Ship an official adapter (or converge the
contracts): either (a) a small `installer/adapter.py` — essentially the
tester's bridge promoted to product code, converting the installer manifest
into uninstaller state (relative `files[].path` + per-file line lists) at
install time — or (b) the cleaner unification: have the installer record
paths relative to the title root (which also fixes accepted-risk finding #5,
"absolute install_paths break on relocation") and emit the uninstaller's
config-entry line lists directly, making both modules read one record.

### Finding #23 — conflict gate never wired into the install path (tester's I-2)

**What exactly is missing.** `installer/conflicts.py` implements
`check_install()` / `check_batch()` with path, keybind, hook, version and
dependency conflict detection (blocker vs warning) — but nothing calls it from
the install path. `installer/installer.py` contains zero `check_install`
invocations (verified: grep hits only the separate case-conflict machinery).
The installer will happily write files that collide with another installed
mod's files; the tester's L2 integration scenario works only because the test
itself calls `conflicts.check_install` explicitly. Note the narrower
`find_case_conflicts` (case-sensitivity) IS wired in at installer.py:1130 —
the gap is the full path/keybind/hook/dependency gate.

**Who owns the fix: zcode (the installer side).** Same reasoning as #22 —
both endpoints (`installer.py`, `conflicts.py`) are this project's modules.
(The mod manager UI may later choose how to surface blockers vs warnings, but
the gate itself is installer-side.)

**What the fix looks like.** In `Installer.install()`, before the transaction
starts, build the `InstalledRegistry` from existing manifests/state and call
`conflicts.check_install(candidate, registry)`; treat PATH_CONFLICT-class
blockers as a hard refusal (exit before any write, like `CaseConflictError`)
and record warnings in the manifest so verify/uninstall can re-surface them.
One design decision needed first: #22's adapter, because `check_install`'s
registry expects yet another record shape — fixing #22 first makes #23 a
~1-call wiring plus tests.

**Not owned by GPT or the mod manager:** the remaining runtime-status work
(finding #20's fill-in, `ps5_runtime_status` blank in 106/106 globals and
476/476 type rows) is GPT's; the UI/flow around installs is the mod
manager's. #22/#23 are entirely zcode's.

---

## Q2. The 6 APIs TownfallItemSpawner uses that are not in the catalog

Source: `compat/townfall-item-spawner-grade.json`
(`checker_scan_full.unknown`) and `compat/townfall-item-spawner-grade.md`
("Not in the catalog at all"). Verified directly against the then-current
merged catalog (`compat/catalog-real-merged.json`, the v2 98-globals build
of the round-2 review): neither `Key` nor any `Key.*` member existed in it,
and neither did the real `catalog/globals.json` (98 entries, none named
`Key`). (Catalog v4 has since added the `Key`/`ModifierKey` TYPE tables —
see the last UPDATE note in this file; the 6 constants now resolve to the
PC-only-input bucket instead of "not in the catalog".)

The 6 "APIs" are **constant references, not function calls** — each is read
exactly once and passed as an argument to `RegisterKeyBind(...)` in
`compat/townfall-item-spawner-mod/Scripts/main.lua`:

| Reference | Line in main.lua | Use |
|---|---|---|
| `Key.F7` | 694 | menu toggle keybind |
| `Key.UP_ARROW` | 698 | menu navigation |
| `Key.DOWN_ARROW` | 703 | menu navigation |
| `Key.RETURN` | 708 | menu confirm |
| `Key.LEFT_ARROW` | 713 | menu navigation |
| `Key.RIGHT_ARROW` | 722 | menu navigation |

They are members of the UE4SS **`Key` constant table** (input-key constants).
The checker counts them as API usage by design ("type/constant references are
counted as API usage", `compat/checker.py:872`), so they land in the grade
reason as "6 API(s) not in the catalog at all" (each `call_count: 1`,
centrality not applicable).

**Catalog gap or not?** Split answer:

- **Per-constant entries: NOT needed.** These are data (enum-like constants),
  not callable APIs; the catalog deliberately "counts functions only" — the
  coverage map's orphan-row verdict for the conformance suite's own `Key.*`
  spot checks (`docs/COVERAGE-MAP.md`, orphan table) is "intentional; catalog
  counts functions only". Six function-shaped entries for constants would be
  the wrong fix.
- **The `Key` TABLE itself: a real (minor) catalog gap.** The runtime does
  expose a `Key` global constant table, and the catalog has no entry for the
  table (same gap class as `EGameThreadMethod`, `NAME_None`, the `Loop`
  table, and the loaded-modules table, all flagged in the coverage map's
  orphan/orphan-follow-up sections). Two viable closes: add ONE `Key`
  constant-table entry to the catalog (with `ps5_runtime_status` filled from
  GPT's console evidence, like any other entry), or teach the checker to
  resolve constant-table members without function entries.
- **Why it matters for this grade:** it is cosmetic today (the grade is
  `unknown` on the 9 blank-status APIs anyway), but per the checker's ladder
  (`compat/checker.py:850-851`) once statuses are filled, leftover
  uncataloged APIs alone still force grade `unknown` — so the `Key` table
  must be resolved (cataloged or checker-aware) before TownfallItemSpawner
  can get a real compatible/unsupported verdict. The shipped grade artifact
  says the same: "these too block a real grade once statuses are filled".

**Related open question added 2026-10-07 (raised by the Key-table agent; also
contract open question #8 in `docs/CONTRACT-PROPOSAL.md`):** cataloging the
`Key` table is necessary but not sufficient. The compat checker flags `Key.*`
constant usage as a PC-only input dependency, and the runtime currently maps
keys only via PS5Pad.ini chords (e.g., L3+R3 = F7). Until the runtime
documents a mapping strategy (config-file chords, built-in defaults, or both)
— including what happens when a mod registers a keybind the config doesn't
map (today it registers but never fires) — keyboard-dependent mods cannot
grade higher than `partly-compatible` even with a complete catalog.

**UPDATE 2026-10-07 (later the same day, Key/ModifierKey catalog v4):** the
`Key` table gap described above is now CLOSED as recommended — `Key` (165
constants) and `ModifierKey` (3 active constants) are TYPE entries in
`catalog/globals.json` v4 with `pc_only_input: true`, and the checker has a
dedicated PC-ONLY INPUT category, so the six constants no longer appear as
"not in the catalog". The TownfallItemSpawner grade moved from `unknown` to
`partly-compatible - PC-only input dependency` (see
`compat/townfall-item-spawner-grade.md`) — that was the round-4 policy;
round 5 reversed it (blank status outranks), so the live grade is `unknown`
again with the keyboard difference as a KNOWN PS5 DIFFERENCE (same file,
round-5 re-grade). The open mapping-strategy question
is now formalized as Q4 below, for GPT.

---

## Q3. The test that now checks the Townfall grade automatically

**None existed** (verified: `grep -rn townfall` over `compat/` and `tests/`
hit only the artifacts and `installer/test_installer.py`, which uses the name
"Townfall Item Spawner" as an install fixture but never grades it). Per the
task, one was **created**: class `TestTownfallItemSpawnerGrade` in
`/WORKSPACE/Projects/ps5-installer-compat/compat/test_checker.py`, four tests:

- **`TestTownfallItemSpawnerGrade::test_townfall_grade_unknown_reason_runtime_status_not_filled`**
  — THE required test: loads the real staged mod
  `compat/townfall-item-spawner-mod/Scripts/main.lua` plus the real merged
  catalog `compat/catalog-real-merged.json`, runs `checker.scan()`, asserts
  `grade == "unknown"` and `"runtime status not filled"` is contained in
  `grade_reason`.
- `test_townfall_inputs_exist` — both inputs present.
- `test_townfall_blank_status_core_apis` — the blank-status bucket includes
  the mod's core APIs (RegisterKeyBind, ExecuteInGameThread,
  StaticFindObject, StaticConstructObject, FindFirstOf, LoadAsset).
- `test_townfall_six_uncataloged_key_constants` — the unknown bucket is
  exactly the six `Key.*` constants from Q2.

Current live result (2026-10-07, round-5 re-grade): grade `unknown`, reason
"unknown - runtime status not filled (9 cataloged API(s) this mod calls have
an empty ps5_runtime_status); KNOWN PS5 DIFFERENCE: PC-only input dependency"
(the 9th blank API is `print`, surfaced by the cataloged-stdlib-override fix;
the 6 `Key.*` constants now bucket as PC-only input, not "not in the
catalog"). Full suite: `python3 -m unittest test_checker` in `compat/` —
**63/63 OK** (55 prior + 8 round-5 policy tests).
The test intentionally goes red if someone fills
`ps5_runtime_status` or rebuilds the merged catalog without re-grading the
artifact — that is its job.

**UPDATE 2026-10-07 (later the same day, Key/ModifierKey catalog v4):** the
grade output changed, so the pinning tests were updated with it:
`test_townfall_grade_unknown_reason_runtime_status_not_filled` is superseded
by **`test_townfall_grade_partly_compatible_pc_only_input`** (grade
`partly-compatible`, reason mentions PC-only input dependency / controller
mapping / PS5 has no keyboard) and `test_townfall_six_uncataloged_key_constants`
by **`test_townfall_six_key_constants_are_pc_only_input`** (the six `Key.*`
constants land in the `pc_only_input` bucket, all central; `unknown` is
empty). Eight new `TestPcOnlyInput` unit tests cover the category itself
(distinct from missing/status-blank/unknown, dedicated reason, suggestion
text, real-catalog entry shape). Suite at that point: **55/55 OK**; live
grade then: `partly-compatible - PC-only input dependency` with the 9
blank-status APIs as an unverifiable caveat.

**UPDATE 2026-10-07 (round 5, grade policy):** blank `ps5_runtime_status`
now outranks everything, so `test_townfall_grade_partly_compatible_pc_only_input`
is superseded by **`test_townfall_grade_unknown_with_keyboard_note`** (grade
`unknown`, reason "runtime status not filled (9 cataloged API(s) ...)" plus
the KNOWN PS5 DIFFERENCE keyboard note; the six `Key.*` constants stay in
the `pc_only_input` bucket). Suite now: **63/63 OK** (55 prior + 8 round-5
policy tests). Current live grade: `unknown`.

---

## Q4. OPEN QUESTION FOR GPT — PC keyboard keybind → PS5 controller mapping

> How does the runtime map PC keyboard keybinds to PS5 controller inputs?
> The PS5Pad.ini chord mechanism (L3+R3 = F7) is the current approach — is it
> generalizable to arbitrary Key.* constants, or does the runtime need a
> translation layer?

Context (all Mac-only / host-tested, from the catalog and checker work in
`catalog/globals.json` v4 and `compat/checker.py`):

- The UE4SS `Key` global is a 165-constant table of **Windows virtual-key
  codes** (`Input::Key` enum: `RETURN=0x0D`, `LEFT_ARROW=0x25`, `F7=0x76`;
  port `src/Mod/LuaMod.cpp:446-614`, upstream `:438-606`), and `ModifierKey`
  exposes `SHIFT/CONTROL/ALT`. The constants are readable integers in any Lua
  state — but the PS5 has no keyboard, so nothing can ever press them.
- A mod like TownfallItemSpawner wires its whole UI through
  `RegisterKeyBind(Key.F7, ...)` etc. (6 constants). The compat checker now
  reports these as a **PC-only input dependency** (under the round-5 policy
  they ride inside the `unknown` grade as a KNOWN PS5 DIFFERENCE; under the
  earlier round-4 policy they graded `partly-compatible - PC-only input
  dependency`), with the suggested
  runtime/config-side workaround: controller chord mapping via PS5Pad.ini
  (e.g. L3+R3 = F7).
- What is NOT yet decided (GPT's call, runtime-side): is a config-file chord
  table (PS5Pad.ini) the general answer for arbitrary `Key.*` constants —
  including unmapped keys (today a keybind registers but never fires) and
  ModifierKey combinations — or should the runtime grow a translation layer
  that maps keyboard keys/modifiers onto controller state (and possibly
  on-screen keyboard for text-ish keys)? This also decides what
  `ps5_runtime_status` the `Key`/`ModifierKey` entries and
  `RegisterKeyBind`/`IsKeyBindRegistered` should carry once console-verified.

---

## Q5. OPEN QUESTION FOR GPT — lowercase mod paths (`scripts/` vs `Scripts/`)

> A mod shipping lowercase `scripts/main.lua` works on Windows
> (case-insensitive filesystem) but is refused by our installer
> (case-conflict detection). Should the runtime look up mod files
> case-insensitively on PS5, or should the installer keep refusing?
> The current behavior: the installer detects the case mismatch between
> `Scripts/` (expected) and `scripts/` (shipped) and refuses with a
> runtime/config note — it never renames mod files.

Context (all Mac-only / host-tested, from the installer code in
`installer/installer.py`):

- Two refusal paths produce that behavior:
  - `find_mod_root()` requires the entrypoint with **exact case**
    (`Scripts/main.lua`); a lowercase-only archive is refused as "not a
    UE4SS Lua mod" with the hint "found 'scripts/main.lua' — UE4SS requires
    exact 'Scripts/main.lua' case".
  - an archive carrying both spellings (or clashing with another installed
    mod's casing) trips `CaseConflictError`; `_runtime_case_notes()` labels
    it a runtime/config issue and the stated policy is that the installer
    **never renames mod files** to paper over it.
- Why the check exists in code rather than on disk: the host Mac filesystem
  is case-insensitive (it would silently merge `Scripts/` and `scripts/`
  into one directory), while the PS5 target filesystem is case-sensitive —
  so the comparison happens on the archive listing before anything is
  installed.
- What is NOT yet decided (GPT's call, runtime-side): either the runtime
  resolves mod file paths case-insensitively on PS5 (lowercase-shipping
  Windows mods would then work as-is and the installer could accept them),
  or the installer keeps refusing and the mod package itself must be fixed
  (re-cased) upstream. Also open: whether an installer-side *renaming*
  option would ever be acceptable — today it is explicitly rejected.
- Cross-references: contract open question #9 (`docs/CONTRACT-PROPOSAL.md`
  Section 8) and `REVIEW-PACKAGE.md` Section 10 item 10.
