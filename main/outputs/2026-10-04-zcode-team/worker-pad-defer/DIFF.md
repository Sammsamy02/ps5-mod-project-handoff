# DIFF — staged sources (worker-pad-defer-1), proposal-only

All staged files live under `outputs/2026-10-04-zcode-team/worker-pad-defer/staging/`.
No in-use launch10-build file was edited; the lineage baselines were only read.
Full unified diffs are stored alongside this file:
`native-pad-input.wpd1.diff`, `program.wpd1.diff`.

## 1. Unit `native-pad-input` — `staging/native-pad-input-wpd1/NativePadInput.cpp`

- Replaces (composition lineage): `native-coherent-pad-runtime-1/build-1/source/NativePadInput.cpp`
  (the base-receipt `selected_runtime_units['native-pad-input'].source`,
  sha256 `ec7a10ce7f2c9868555468f89fbbf1b62216124cff606652174c861c632c2d2f`;
  byte-identical to the spike copy `ue4ss-native-pad-config-1/src/NativePadInput.cpp`).
- Staged sha256: `ac8325f7db3bb08365175fa91c7a1be523660218aa8c734b2cd8177f8923f005`.
- Includes: unchanged (`NativePadInput.hpp`, `PS5PadConfig.hpp`,
  `Platform/NativeModSource.hpp`, `<stdexcept>`, `<string>`, `<utility>`).
  ONE additive declaration: `extern "C" void diagnostic_event(const char*, unsigned int);`
  (namespace-scope legal form; the symbol is DEFINED in-link by the
  composition's diagnostic.c — EDITION.json edit n=3 — so this is not a new
  import and appears as no new undefined symbol at the combined-supplier
  boundary). `NativePadInput.hpp` is byte-UNCHANGED.
- Behavior deltas (and what is deliberately unchanged):
  - UNCHANGED: absent-INI no-op; conflict-throw when a source is already
    supplied; full strict INI parse (parse errors throw before the factory);
    throw when the factory returns null; assignment when the source IS
    available.
  - CHANGED (the fix): `!source->is_available()` no longer throws. It emits
    `diagnostic_event("pad_input_deferred", phase)` and
    `diagnostic_event("pad_input_deferred_error", raw scePad error)` on the
    existing supplier-diag channel and returns WITHOUT assigning
    `context.input_source` (the runtime starts input-less; the composition
    discards the source, whose destructor keeps the owned/borrowed cleanup
    discipline — see ANALYSIS.md §6).

## 2. Unit `program` (retained, already replaceable) — `staging/program-wpd1/UE4SSProgram.cpp`

- Base: the EXACT l10 program source
  `outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`
  (sha256 `75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574`),
  copied and edited; nothing in that directory was touched.
- Staged sha256: `a16c8972e22b9b3076e99a33896afad41085bf014acee15cd2db11d4c602df37`.
- Deltas (two hunks, `__PROSPERO__`-scoped):
  1. Namespace-scope `extern "C" void diagnostic_event(const char*, unsigned int);`
     declaration above `UE4SSProgram::validate_native_services` (same in-link
     symbol rationale as above).
  2. The PROSPERO HAS_INPUT gate
     `if (!m_input_handler.is_input_available() && (EnableDebugKeyBindings || !InputSource.empty())) throw ...`
     becomes: throw ONLY when `EnableDebugKeyBindings` (an explicit binding
     request; default false, unset in the shipped ini); otherwise emit
     `diagnostic_event("pad_input_deferred_input_gate", 1u)` and continue.
     Rationale: `General.InputSource` defaults to non-empty `"Default"` and is
     not parsed from any ini on this target, so the old clause is a constant
     that re-aborted every input-less boot right after the pad-config
     deferral. All other service gates (crash dump, consoles, GUI, hot
     reload) and the non-PROSPERO branch are byte-unchanged.

## 3. Deliberately NOT staged

- `PS5PadSource.cpp` / `PS5PadNative.cpp` (units `native-pad-PS5PadSource`,
  `native-pad-PS5PadNative`): no source change. A poll-side lazy-retry would
  be unreachable dead code on the current wiring (Handler never accepts an
  unavailable source, and `process_event` skips unavailable sources). The
  prepared design for the follow-up coexist packet: in `poll()`, when
  `!is_available()` and `m_valid`, attempt `activate()` at most once per
  call with a bounded exponential backoff counter (members added to
  PS5PadSource.hpp; no new imports), keeping the existing owned-close and
  borrowed-service discipline; pair it with Handler/program-side
  pending-source acceptance so the retried source can actually re-enter.
- `NativePadInput.hpp`: byte-unchanged (comment remains accurate).
- The caller (`ue4ss_backend.cpp`, bootstrap unit): byte-unchanged. Deferring
  at the origin means no bootstrap-side change and no new bootstrap admission.

## 4. Compile gates (exact pinned composition argv)

Both staged sources were compiled with the EXACT unit argv from the launch-10
base receipt (`ue4ss-native-notify-cancel-1/build-1/receipt.json`,
`selected_runtime_units`), with only the source, `-o` and `-MF` tokens
redirected — the same assertion the composition driver applies to retained
replacements. Receipts: `compile-native-pad-input/`, `compile-program/`
(command.json with full argv, exit code, stdout/stderr).
