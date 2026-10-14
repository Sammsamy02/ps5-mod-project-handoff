# REPORT — worker-pad-defer (worker-pad-defer-1), 2026-10-05

Proposal-only packet. NOTHING folded; no launch10-build file edited; no
console, engine, provider or SDK execution. All deliverables under
`outputs/2026-10-04-zcode-team/worker-pad-defer/`.

## What was done

1. **Located the blocker end-to-end** (ANALYSIS.md §1):
   - throw: `ue4ss-native-pad-config-1/src/NativePadInput.cpp:19-24`
     (byte-identical to the composition fold copy);
   - phase-6 step: `ue4ss-native-pad-provider-1/port/PS5PadSource.cpp:43-44`
     — `scePadOpen` refusal recorded as `PadPhase::Open` (enumerator 6);
   - error: `-2137915388 == 0x80920004 = PS5_PAD_ERROR_ALREADY_OPENED`
     (arithmetic verified; mission's earlier 0x80A40004 reading wrong, per
     TIMING.md);
   - eager activation origin: `PS5PadNative.cpp:16-22` (`source->activate()`
     in the factory), called from the backend bootstrap ctor
     (`ue4ss_backend.cpp:26`, bootstrap unit `ue4ss_backend`).

2. **Ownership probe (duplicate-init hypothesis): DISPROVEN for our port**
   (ANALYSIS.md §2). Exactly one `scePadOpen` call site exists in the
   composed units (PS5PadSource.cpp:43); `Handler::init` cannot re-open an
   unavailable source and `activate()` is idempotent; run-3 failed on the
   FIRST attempt. The handle was therefore already held by another component
   of the host process (most plausibly the game's own pad stack — the game's
   internals are not observable in our evidence).

3. **Semantics verdict: the failure is CACHED** — `poll()` never re-attempts
   activation (`PS5PadSource.cpp:69-71`); only an explicit `activate()`
   retries (retry-safe primitive, host-proven). AND the original brief's
   fix ("log-not-throw + accept the source") does NOT stop the refusal on
   this wiring: the l10 program unit throws on a supplied-but-unavailable
   source (:740-743) AND on every input-less boot because
   `General.InputSource` defaults to non-empty `"Default"` and is never
   parsed from ini (:748-750 + SettingsManager.hpp:42). ANALYSIS.md §3.

4. **Coexist route: none verified** (ANALYSIS.md §4). No open-mode/flag
   parameter, no enumeration/peek API in the pinned surface;
   `scePadGetHandle` is declared (SDL_ps5joystick.h:94) but its semantics on
   this target are UNVERIFIED — flagged as the single bounded console probe
   that would settle coexistence; NOT used here.

5. **Authored the staged fix (2 units, 0 caller changes)** (DIFF.md):
   - `native-pad-input` → `staging/native-pad-input-wpd1/NativePadInput.cpp`
     (sha256 ac8325f7…): unavailable → two `diagnostic_event` records
     (`pad_input_deferred` phase, `pad_input_deferred_error` raw error) and
     NO input assignment; all refusals that are genuine config errors are
     unchanged; owned/borrowed cleanup discipline preserved (documented in
     source and host-asserted).
   - `program` (already replaceable) → `staging/program-wpd1/UE4SSProgram.cpp`
     (sha256 a16c8972…): PROSPERO input gate downgraded to a diagnostic
     (`pad_input_deferred_input_gate`) except for explicit
     `EnableDebugKeyBindings`; everything else byte-unchanged from the exact
     comp6 l10 program source.
   - Provider/Handler/bootstrap/settings deliberately untouched; the
     prepared (unstaged) design for the coexist packet is in ANALYSIS.md §3.

6. **Host-prove (PASS, lineage ASan+UBSan flags preserved)**
   (`host-prove/receipt.json`, `host-1-fix/`, `host-2-control/`):
   - **fix (19 checks, exit 0):** drives the REAL `ConfigureRetainedPS5PadInput`
     with the REAL `PS5PadSource` over an owned backend stub — case (a)
     unavailable-then-available: no throw, input not supplied, both deferral
     diag records with phase=6 / error=-2137915388, Handler::init refuses
     the unavailable source (the documented composition gate), and an
     explicit `activate()` on the recorded source connects once the handle
     "frees" (capability proven; NOT composition-wired — documented); case
     (b) permanently unavailable (the live normal case): startup function
     STILL returns normally, borrowed service never terminated, unopened
     handle never closed; plus preserved semantics (absent INI, parse
     refusal, available-assignment, conflict).
   - **control (3 checks, exit 0):** the ORIGINAL spike NativePadInput.cpp
     against the same stub throws the live refusal VERBATIM
     ("PS5Pad input unavailable: phase=6, error=-2137915388") — the harness
     detects the original bug.

7. **Compile gates (PASS, exact pinned composition argv)**
   (`compile-native-pad-input/`, `compile-program/`): both staged sources
   compiled with the base-receipt unit argv, exactly 3 token differences
   (source, -o, -MF — machine-verified), producing prospero ELF x86-64
   relocatable objects, exit 0 each.

8. **Fold instruction** (FOLD.md): `program` is already replaceable (existing
   SEC32-3 mechanism); `native-pad-input` is NOT — folding needs a NEW
   fixed-edition driver extending `RETAINED_REPLACEABLE` with
   `'native-pad-input'` plus a new rr attestation; no bootstrap-unit change;
   post-link check: undefined-symbol delta vs l10 must be EMPTY
   (`diagnostic_event` is defined in-link by diagnostic.c).

## Honest limits

- Run-4 (pid 370) fact is taken from the coordinator message, not
  independently re-parsed by this worker.
- The program-gate change is compile-gated only; its runtime behavior is not
  host-proven (needs the full runtime).
- Input-less mod-start behavior ("Exception during mod startup: Input
  registration unavailable…") is code-read, not console-proven.
- The pad stays dead for the whole boot after a deferral: no retry is wired
  (by design, see 3/5). The floor is "runtime starts, load-capable,
  input-less" — no input restoration is claimed.
- ASan dev-runs hung at exit on this host 3 times during development; final
  PASS runs use the lineage sanitizer flags with `detect_leaks=0` (receipt
  `flags` fields); watchdog added in build_and_prove.py. Three stale UE pids
  from development runs may linger until logout (unkillable, harmless, no
  locks on anything owned by others).
- The ~0.8 s composition figure is the early-refusal path only; full
  successful-startup timing remains unmeasured; nothing here extrapolates it.

## File inventory

- CLAIM.json · ANALYSIS.md · DIFF.md · FOLD.md · REPORT.md (this file)
- staging/native-pad-input-wpd1/NativePadInput.cpp · staging/program-wpd1/UE4SSProgram.cpp
- native-pad-input.wpd1.diff · program.wpd1.diff
- tests/host_pad_defer.cpp · tests/host_pad_defer_control.cpp · build_and_prove.py
- host-1-fix/ · host-2-control/ · host-prove/receipt.json (PASS receipt)
- compile-native-pad-input/ · compile-program/ (pinned-argv receipts + objects)
- build.log (final gate suite transcript)
