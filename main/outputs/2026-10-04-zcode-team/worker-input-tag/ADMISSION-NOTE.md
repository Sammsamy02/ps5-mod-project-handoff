# ADMISSION-NOTE — worker-input-tag staged NEXT-candidate unit
worker-input-tag · 2026-10-05 · PS5 UE4SS runtime port
Status: PROPOSAL-ONLY STAGING. No shared lineage file was edited (all changes
under `outputs/2026-10-04-zcode-team/worker-input-tag/`). No composition run,
no packaging, no console dispatch, no game or in-game claims. The only
acceptance path remains a controller composition run with refreshed pins.

## 0. What this unit is
The bounded instrument fill for acceptance gap **G-AP1** (the one genuine
instrument gap in `worker-acceptance/ACCEPTANCE-CHECKLIST.md` §G-AP1 and
`worker-menu-input-map/RUNG-TABLE.md`): today NO tag fires when a pad event
reaches the handler, so a silent first OPTIONS press cannot discriminate
input-dead vs drain-dead from logs alone. Two additive, behavior-free
diagnostic tags close it:

1. `pad_first_state_read=<buttons-raw>` — once per boot, in
   `PS5PadSource::poll`, on the first err-free AND connected read AFTER
   availability (pad_available proves only the OPEN path; this proves the
   READ path).
2. `pad_key_edge=<keycode>` — per ARRIVING key edge in
   `Handler::process_event`, RATE-BOUNDED: at most 60 tags per boot, then
   exactly ONE `pad_key_edge_silent=<60>` marker and total silence (the
   diagnostic sink is append-only; per-press tagging must not flood it).
3. The six mod keybind codes are pairwise distinct (verified:
   F7=0x76 the OPTIONS bind, RETURN=0x0D, LEFT/UP/RIGHT/DOWN_ARROW
   =0x25/0x26/0x27/0x28; `static_assert` in the host test), so
   `pad_key_edge=0x76` identifies the first OPTIONS press unambiguously.

The resulting log shapes (discriminators, all asserted by the host test):
- input-dead (open never succeeds): `pad_available` ABSENT,
  `pad_first_state_read` ABSENT, `pad_key_edge` ABSENT.
- read-dead-after-open: `pad_available` present, `pad_first_state_read`
  ABSENT (host case (a) polarity 1).
- edges arriving: `pad_first_state_read` + `pad_key_edge` present; the
  pre-existing downstream lines (Lua first-press / ExecuteInGameThread throw)
  then discriminate drain-dead as before — this unit claims nothing below the
  dispatch boundary.

## 1. Staged file list (all under this worker dir)
- `staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp`
  — d2f14de1… (baseline = ADMITTED pad_coexist 24692ec7…)
- `staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.hpp`
  — 2d63c094… — **byte-identical CARRY of the staged-provider header** (same
  bytes as the admitted coexist staging header), included ONLY so the staged
  TU's quoted include resolves inside a self-contained staging dir and the
  TU sees the appended-members definition (the documented coexist ODR
  requirement). NOT a change; no pin moves for it.
- `staging/input-handler-input-tag/Handler.cpp`
  — 16fcfe49… (baseline = ADMITTED pad_coexist f7539c60…)
- `diffs/PS5PadSource.cpp.24692ec7-to-d2f14de1.diff` (2 hunks, +18 lines)
- `diffs/Handler.cpp.f7539c60-to-16fcfe49.diff` (2 hunks, +52 lines)
- `host-test/host_input_tag.cpp` + `host-test/host-test-run.stdout` (+ empty
  stderr; ASan+UBSan clean), `host-test/compile/{command.json,stdout.txt,
  stderr.txt,exit.txt}`, `host-test/host_input_tag` binary
- `provenance/SHA256SUMS.txt` (baselines + staged + diffs; all verified with
  `shasum -a 256 -c`), `stage_units.py` (deterministic restaging script:
  exact-string insertion on the verified baselines, emits the diffs, and
  verifies the roundtrip)
- `build_and_run.sh` (the lineage compile recipe, staged TUs swapped in)

Both diffs were verified to apply cleanly to fresh baseline copies and to
reproduce the staged bytes EXACTLY (`patch -p1` roundtrip + sha256 match,
2026-10-05, by `stage_units.py`).

## 2. Diff summary — PS5PadSource.cpp (2 hunks, +18, 0 deletions)
- Hunk 1 (+6): one file-local flag `bool pad_first_state_read_emitted` in the
  existing anonymous namespace. File-local ON PURPOSE: no header change, no
  ABI change; written only from `poll()` on the sole processing thread (same
  serialized-thread discipline the coexist retry counters are documented
  under).
- Hunk 2 (+12): in `poll()`, immediately after
  `m_status.phase=PadPhase::Active;` and BEFORE the duplicate-timestamp
  guard: once-per-boot `diagnostic_event("pad_first_state_read",
  data.buttons)`. Fires on the first err-free AND connected read (a
  disconnected or failing read emits nothing — deliberate: the tag's value
  must be a real state sample), even with no press ever made (the baseline
  sample). Pure emission; every other line byte-unchanged.

## 3. Diff summary — Handler.cpp (2 hunks, +52, 0 deletions)
- Hunk 1 (+13): `extern "C" void diagnostic_event(const char*, unsigned)`
  namespace-scope declaration (the identical pattern the admitted coexist
  `PS5PadSource.cpp` already uses) + anonymous-namespace
  `constexpr unsigned int pad_key_edge_tag_cap = 60`.
- Hunk 2 (+39): in `Handler::process_event`, immediately after
  `auto events = m_platform_handler->process_event(this);` and BEFORE the
  `m_allow_input` early return: bounded per-edge
  `diagnostic_event("pad_key_edge", event.key)`; when the 60th tag is
  emitted, exactly one `pad_key_edge_silent`(=60) marker fires and tagging
  stops for the boot. Dispatch below is byte-unchanged and runs even while
  silent. Placement before the allow gate is deliberate: the pad source
  emits no events at all while `allow=false` (upstream swallow, G-AP3
  silence preserved — asserted by host case (d)), so "arrival is arrival".
  Function-local statics = per-boot state (the composition has exactly one
  Handler), unsynchronized exactly like every other Handler mutation under
  the sole-processing-thread lifecycle contract.

## 4. Host-test results (96/96 PASS — host-test/host-test-run.stdout)
Drives the REAL staged units end-to-end with the lineage owned-PadBackend
stub and a capturing `diagnostic_event` stub (same C signature, defined
in-link like the composition); admitted coexist `NativePadInput.cpp` supplies
the unchanged configure path; compile is the coexist host recipe
(`/usr/bin/clang++ -std=c++23`, `-fsanitize=address,undefined`), exit 0,
only the pre-existing fmt `localtime` deprecation warning; run stderr empty.
- (a) read half, both polarities, before any edge exists: poll-200 retry
  opens (`pad_available`=7, `pad_open_class`=0 once, `pad_retry`=1);
  injected read ERROR after open → phase=Read, NO `pad_first_state_read`;
  reads heal → exactly ONE `pad_first_state_read`=<raw OPTIONS 0x8> (the
  baseline sample, before the duplicate guard), still zero edges; duplicate
  timestamp does not repeat the tag.
- (b) six distinct mod keybinds through the REAL PS5Pad.ini parser
  (OPTIONS=F7, CIRCLE=RETURN, UP/DOWN/LEFT/RIGHT=arrows): each press emits
  exactly one `pad_key_edge` with the right distinct code (0x76, 0x0D,
  0x25–0x28) and fires exactly its own callback; budget run to exactly 60
  tags then ONE `pad_key_edge_silent`=60; after the cap, presses STILL fire
  callbacks with ZERO new tags (the no-behavior-change core).
- (c) disconnect→reconnect: still exactly ONE `pad_first_state_read` per
  boot; silence holds; RETURN bind fires, no tag.
- (d) `allow_input=false`: no events, no tags, no fires (G-AP3 preserved);
  re-allow → delivery resumes, tags stay silent (per-boot budget).
- (e) input-dead control: open never succeeds in 1000 polls → exactly 2
  bounded retries (polls 200/600, admitted backoff), and `pad_available` /
  `pad_first_state_read` / `pad_key_edge` / `pad_key_edge_silent` counts
  NEVER move; reads never happen; borrowed-service discipline intact (no
  terminate, no close, including source drop).
Honest bound: this is host-level evidence of the predicate/bounding logic
with stubbed hardware and a stubbed sink — console sink semantics (the real
supplier-diag jsonl) are NOT exercised by this unit.

## 5. diagnostic_event needs NO new import — confirmation
`diagnostic_event` is DEFINED in-link by the composition's `diagnostic.c`
bootstrap unit (already pinned in `launch11-combined/EDITIONS.json`
`units` row f5e96c41…; signature/call sites unchanged per
worker-diagnostic-sink). The staged TUs add only namespace-scope
`extern "C"` DECLARATIONS of that same in-link symbol — the exact pattern
the already-admitted coexist `PS5PadSource.cpp` TU uses today. Internal
references to an in-link definition do not touch the PRX import boundary:
EXPECTED added undefined imports = ∅; the fixed edition's undefined-delta
gate would refuse anything else. The host test proves resolution shape: the
capturing stub satisfies both staged TUs at link time with no other
binding.

## 6. Admission shape — files changed and pin cascade IF ADOPTED
### Files that change in the port tree (2 TUs, both currently pinned)
- `port/PS5PadSource.cpp` (pad provider TU; pinned
  `admissions.pad_coexist.files` 24692ec7… → d2f14de1…)
- `Handler.cpp` (input handler TU; pinned
  `admissions.pad_coexist.files` f7539c60… → 16fcfe49…)
- NO header changes: `PS5PadSource.hpp` byte-identical (2d63c094…),
  `Handler.hpp` untouched (no new members — all new state is file-local or
  function-local), so every TU that includes the base headers stays
  ABI-consistent and no include-side pin moves.
- Unchanged and not restaged: `PS5PadNative.cpp` (501d77af…),
  `NativePadInput.cpp` (03c9f6d0…), `diagnostic.c` (f5e96c41…).

### Pin cascade (per worker-luamod-lineage COMPARATOR-REGEN-PROCEDURE;
### controller-owned, all-or-nothing)
1. `launch11-combined/EDITIONS.json` `admissions.pad_coexist.files` rows
   (verify step re-hashes on drift): the two rows above move; add/repurpose
   the admission record (e.g. a `pad_input_tag` entry in the same shape,
   with measurement provenance — sha fields must EQUAL measurement, never
   prediction).
2. The launch11 driver/fixed-edition `LAUNCH11 PAD PLACEHOLDER` blocks
   (L11-E4 compose_argv pad admission argv + runner regen_receipts pad
   `--file` admissions, per EDITIONS `placeholders`) take their concrete
   unit paths/shas from this staging dir when the controller resolves the
   pad fold.
3. Comparators: no PS5PadSource/Handler object comparator is pinned (only
   the luamod + asmhelper folds are). `input-handler.o` and
   `native-pad-PS5PadSource.o` (and hence the composed object) change →
   both passes re-measure them fresh and the determinism twin must agree;
   `comparators.composed_object` policy unchanged. Luamod is untouched by
   this unit: NO luamod comparator regen is triggered.
4. Undefined-import surface: EXPECTED added = ∅ (§5). No new files enter
   the composed TUs → no freshen-deps change; regen_receipts picks the two
   edited TUs up via re-measurement.
5. Pre-flight order (procedure §5): verify/pin agreement first, then the
   full pass. First on-target value of this unit is read off the supplier-
   diag channel during the next authorized press protocol; the acceptance
   checklist's rung-3.5 rows should then consult `pad_first_state_read` /
   `pad_key_edge` before declaring delivery UNRESOLVED.

## 7. Honesty ledger
- Proposal-only staging; NO console, engine, provider or SDK execution; no
  fresh game/console evidence; NO quality score, no AAA/parity/completion
  claims.
- Host evidence covers the tag predicates, bounding, distinctness, and
  no-behavior-change at host level with stubbed hardware/sink only.
- Known semantic notes carried in the diffs: once-per-boot = per process
  (per PRX load); `pad_key_edge` budget is per boot across all sources
  (on PROSPERO the only attached source is the pad — audit table §2 of
  RETRY-PATH-AUDIT.md); read errors/disconnects emit nothing until a usable
  state is read (by design, so the tag value is always a real sample).
- Bounded task: staging + host test + admission note; stopped after this
  report.
