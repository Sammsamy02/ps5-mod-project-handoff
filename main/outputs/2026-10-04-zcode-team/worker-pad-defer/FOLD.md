# FOLD — how to fold worker-pad-defer-1 into the composition (proposal-only)

Nothing in this packet is folded. This is the exact instruction for the
controller/chief who owns the next fixed-edition build. All paths relative to
the worktree root. DO NOT edit any in-use launch10-build file; this fold is a
NEW fixed-script edition child of the l10 driver, exactly like the l10 edition
was a child of the comp6 driver.

## 0. Summary

Two staged sources, two units, zero caller changes:

| unit (composition name) | kind | staged replacement | sha256 | admission today |
|---|---|---|---|---|
| `native-pad-input` | runtime unit (retained object `coherent-full-1/native-pad-input.o`, base receipt `selected_runtime_units['native-pad-input']`, source `native-coherent-pad-runtime-1/build-1/source/NativePadInput.cpp`) | `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/native-pad-input-wpd1/NativePadInput.cpp` | `ac8325f7db3bb08365175fa91c7a1be523660218aa8c734b2cd8177f8923f005` | **NOT replaceable — needs NEW admission** (see §2) |
| `program` | retained runtime unit, ALREADY in `RETAINED_REPLACEABLE` (l10 driver `build_runtime_composition6_fixed_l10.py:52`), precedent: comp6's own staged `comp6-program/UE4SSProgram.cpp` | `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` | `a16c8972e22b9b3076e99a33896afad41085bf014acee15cd2db11d4c602df37` | replaceable via existing `--replace-runtime-unit program=<staged>` + `--replace-receipt` |

Caller (`ue4ss_backend.cpp`, bootstrap unit `ue4ss_backend`) is UNCHANGED —
the deferral happens at the origin, so no `--replace-bootstrap-unit` and no
new bootstrap admission is needed for this packet.

## 1. Why two units are REQUIRED for the launch-10 floor

The pad-config deferral alone does NOT stop the refusal on this wiring
(verified against the exact l10 sources, CLAIM.json C3):
1. `UE4SSProgram::validate_native_services` throws
   "Native supplied input source is unavailable or failed activation" when
   `Handler::init` fails (staged program source :740-743), and
   `Handler::init` returns false for an unavailable source
   (`Handler.cpp:59`).
2. With no source at all, the second gate (program :748-750) throws on every
   input-less boot because `General.InputSource` defaults to non-empty
   `"Default"` (`SettingsManager.hpp:42`) and is not parsed from any ini on
   this target.
The staged pair handles both: deferral supplies nothing (1 is skipped), and
the gate downgrade (2) keeps only the explicit `EnableDebugKeyBindings`
refusal fatal.

## 2. New admission needed for `native-pad-input` (checked, not present)

Answer to the brief's admission question: **`native-pad-input` is NOT already
replaceable.** In `build_runtime_composition6_fixed_l10.py`:
- `RETAINED_REPLACEABLE = {'program','luamod','cppmod','settings','asmhelper-asmhelper'}`
  (:52) — `native-pad-input` is not a member;
- `--replace-runtime-unit` eligibility (:142-148) additionally requires the
  unit to be in the freshly compiled `affected` set, which is ASSERTED to
  exactly `{unreal-unrealinitializer, native-ownedmappedscansource,
  native-psscanbinding, native-ownedmappedcandidateselection,
  native-loadercontext, native-programfactory}` (:118-125).

Required (SEC32-3 mechanism, precedent = how comp6 admitted `program`):
1. Fork the l10 driver into a NEW fixed edition (distinct ID, e.g.
   `build_runtime_composition6_fixed_l11_paddefer.py`), byte-identical
   except `RETAINED_REPLACEABLE` gains `'native-pad-input'` (the unit is a
   retained prebuilt object — the SEC32-3 "own pinned argv" path applies:
   recompile asserts only the source, `-o` and `-MF` tokens differ, and the
   fresh `.d` dependency walk must be covered by pins | rr).
2. Author a NEW `--replace-receipt` (hash-pinned attestation) covering:
   - the staged `NativePadInput.cpp` (sha256 above) and its destination
     identity (unit `native-pad-input`);
   - the staged `UE4SSProgram.cpp` (sha256 above) for the `program` unit;
   - the include_dir_map entry used by the comp6 receipt
     (`native-coherent-pad-runtime-1/build-1/source` → the comp3
     bootstrap-stage copy) ONLY if the driver edition keeps the redirect;
     otherwise the canonical `.superpowers` path pins apply unchanged
     (`NativePadInput.hpp` and `PS5PadConfig.*` are byte-unchanged and stay
     pinned at their attested hashes).
3. Invoke:
   `--replace-runtime-unit native-pad-input=<staged NativePadInput.cpp>`
   `--replace-runtime-unit program=<staged UE4SSProgram.cpp>`
   `--replace-receipt <new receipt>`
   Everything else (extra units, diagnostics substitution, bridge_cli,
   undefined-delta/SDK gates, rust/host receipts) is UNCHANGED from l10.
4. Post-link check (fold-time, not yet executed): `nm` on the new
   `combined-supplier.o` must show `diagnostic_event` still DEFINED (it is
   defined by the linked diagnostic.c, EDITION.json edit n=3) and the
   undefined-symbol delta vs the l10 baseline must be EMPTY (the two staged
   TUs add no new extern references: `diagnostic_event` is declared locally
   in both and resolved in-link).

## 3. Expected observable behavior after fold (what to verify on the next boot)

- Startup proceeds past the pad: sdiag jsonl gains, in the writer epoch of
  the new pid, exactly two new records right before runtime construction
  completes:
  `{"tag":"pad_input_deferred","value":6}` (PadPhase::Open) and
  `{"tag":"pad_input_deferred_error","value":2157051908}` (0x80920004),
  plus `{"tag":"pad_input_deferred_input_gate","value":1}` at program
  construction; `runtime_start_failed` must be ABSENT;
  `runtime_started`/ack success tags follow the l10 success shape.
- UE4SS.log gains the Lua mod start error
  "Exception during mod startup: Input registration unavailable: no active
  input source" (expected, non-fatal — ANALYSIS.md §5): the runtime is up,
  load-capable, input-less. No F7 menu until the pad coexist packet lands.
- Failure discipline unchanged: INI parse errors and the conflict case still
  refuse startup (they are config errors, and the old host test's other
  assertions remain covered by host-1-fix).

## 4. What this fold does NOT do

- No retry: the deferred pad does NOT come back during the same boot. The
  coexist path (scePadGetHandle probe and/or Handler-side pending source +
  provider poll-retry) is a separate packet (ANALYSIS.md §3-4).
- No bootstrap-unit replacement, no settings-unit replacement, no Handler
  change, no provider change, no INI or settings-content change.
- No claim of input restoration; launch-10 floor only.
