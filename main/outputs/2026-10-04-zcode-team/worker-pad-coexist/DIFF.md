# DIFF — staged sources (worker-pad-coexist), proposal-only

All staged files live under `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/`.
No lineage file was edited; the wpd1 lineage and composition baselines were only read.
Full unified diffs are stored under `diffs/`:

- `diffs/native-pad-input.wpc.diff` — vs composition base
  `native-coherent-pad-runtime-1/build-1/source/NativePadInput.cpp`
  (sha256 `ec7a10ce7f2c9868555468f89fbbf1b62216124cff606652174c861c632c2d2f`;
  the same base wpd1 staged against).
- `diffs/PS5PadSource.hpp.wpc.diff`, `diffs/PS5PadSource.cpp.wpc.diff` — vs
  `ue4ss-native-pad-provider-1/port/PS5PadSource.{hpp,cpp}`.
- `diffs/Handler.wpc.diff` — vs composition base
  `native-coherent-runtime-1/source/port/first/src/Input/Handler.cpp`
  (sha256 `3babc32515a643a8a15ba2caeadeea0c29b91d3d22318c9cb119d91ed5363acf`;
  byte-identical to the `ue4ss-unreal-native-build-1` copy the host recipe uses).

Staged sha256:

| staged file | sha256 |
|---|---|
| `staging/native-pad-input-coexist/NativePadInput.cpp` | `03c9f6d049609c81fae6adb0f28b9f0bb14d716c8fa92e5a60c6b7f80d471d6a` |
| `staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.hpp` | `2d63c094bf8d3718c53e14087bbf304dcabd8d8ebcc4674513d8f7bce27bda33` |
| `staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp` | `24692ec70e9137f156286e3c2ffa30fc8713a94b1bc1ce78f1de5a5cb26dd986` |
| `staging/ue4ss-native-pad-provider-coexist/port/PS5PadNative.cpp` | `501d77afbe85585630a2e31a4ff958c46d37f1236a4d0896fb9b390db4515447` (**byte-identical** to base — relocation, see §3) |
| `staging/ue4ss-native-pad-provider-coexist/port/PadServiceLease.hpp` | `5caebd6305668ebe32441b94cea89e50bae90ede979763e468eeba93189e0b85` (**byte-identical** relocation) |
| `staging/ue4ss-native-pad-provider-coexist/primary-sources/SDL/src/joystick/ps5/SDL_ps5joystick.h` | `16436774be2082a3fa4fd6ca4e9792ce45bbfcac64de676d5a56a7b0e6f1a706` (**byte-identical** relocation) |
| `staging/input-handler-coexist/Handler.cpp` | `f7539c6094944ca294e4e017685e2e44a39f370070b5583186cd3673c8655e13` |

## 1. Unit `native-pad-input` — `staging/native-pad-input-coexist/NativePadInput.cpp`

Evolution of the wpd1 staging (worker-pad-defer `staging/native-pad-input-wpd1/`,
sha256 `ac8325f7db3bb08365175fa91c7a1be523660218aa8c734b2cd8177f8923f005`):

- UNCHANGED from wpd1: absent-INI no-op; conflict-throw on a supplied source; full
  strict INI parse before the factory; null-factory throw; the deferral diagnostics
  `pad_input_deferred` (phase) + `pad_input_deferred_error` (raw error) on the shared
  supplier-diag channel (`diagnostic_event`, defined in-link by diagnostic.c —
  EDITION.json edit n=3; declaration only, no new import).
- THE DELTA (wpd1 dropped the source; this staging ATTACHES it): `!source->is_available()`
  no longer skips the assignment. `input = std::move(source)` runs for the deferred
  source too. Rationale: with the drop, later recovery was unreachable (Handler::init
  refused unavailable sources, process_event skipped them, RegisterKeyBind threw, and
  no retry existed). The deferred-ATTACHED source pairs with the coexist Handler
  (deferred attach) and the provider's poll()-side retry: startup stays input-tolerant
  (outcome A preserved) AND the source can come alive mid-session (outcome B route).
- Lifecycle discipline untouched: a never-acquired handle is never closed;
  `sceUserServiceTerminate` only when THIS object's `sceUserServiceInitialize`
  returned 0 (borrowed 0x80960003 is never terminated) — host-asserted again.

## 2. Units `native-pad-PS5PadSource` (+ `PS5PadNative` relocation) — provider staging

- `PS5PadSource.hpp`: THREE appended private members (`m_polls_since_retry`,
  `m_retry_interval{200}`, `m_retry_attempts`) + one private `maybe_retry()`
  declaration + the append-only ABI note. Every pre-existing member keeps its exact
  offset (append-after-`m_timestamp`), so any TU still compiled against the BASE
  header (e.g. `pad-config`/`native-pad-input` via the -I recipe order) stays
  layout-consistent: the only inline member reading state is `status()` → `m_status`,
  whose offset is unchanged. No includes changed.
- `PS5PadSource.cpp`: adds the `extern "C" diagnostic_event` declaration (same
  in-link symbol argument as wpd1) and the retry machinery:
  - `poll()`: the `!is_available()` early-return now calls `maybe_retry()` before
    returning empty — THE EVENT-DRIVEN RETRY, on the sole processing thread
    (PlatformInputSource.hpp:11-14 lifecycle; Handler.hpp:60-64 SAFETY: no overlap
    with any other source call, no cross-thread Handler::init re-injection anywhere).
  - `maybe_retry()`: at most ONE `activate()` per interval; interval = 200 polls
    (~1 s at the documented 5 ms UpdateThread cadence), doubling per failed attempt,
    capped at 3200 polls (~16 s worst case); the cumulative attempt counter, the
    open class and the raw failure go to the diag channel:
    `pad_open_class` (=0, STANDARD scePadOpen — emitted once, first attempt),
    `pad_retry` (=attempt number), `pad_retry_failed` (=raw error),
    `pad_available` (=handle, on success; interval resets to fast-retry).
    Each attempt is ≤4 bounded SDK calls (user_initialize short-circuits after the
    first success; typically pad_initialize+user_list+open). The ONLY open class
    attempted is the standard one (SDK-ANALYSIS.md §5 verdict; no undecidable
    second-handle bet in production).
  - Everything else byte-preserved: activate/deactivate ownership+lease discipline,
    snapshot guard, edge/dispatch semantics, phase handling.
- `PS5PadNative.cpp` + `PadServiceLease.hpp` + the SDL header: byte-identical
  RELOCATIONS into the staging mirror so the provider's own two TUs compile against
  ONE header (the new one) via quoted-include self-directory resolution — this keeps
  `native-pad-PS5PadSource.o` and `native-pad-PS5PadNative.o` ODR-consistent and the
  `make_shared` allocation size in PS5PadNative in step with the appended members.
  Content unchanged; the pinned composition argv gate re-proves the byte-identical
  copy compiles on the real target compiler.

## 3. Unit `input-handler` — `staging/input-handler-coexist/Handler.cpp`

EXACTLY five deltas vs the composition base (diff-verified; no other line differs;
`Handler.hpp` is byte-unchanged — no new members needed):

1. `init(shared_ptr)`: DEFERRED ATTACH — an unavailable injected source no longer
   refuses init; it is attached (`m_platform_handler` + `m_injected_source`,
   injected mode) WITHOUT activation, and never displaces an existing attachment.
2. `process_event()`: the availability gate moved into the source's own path
   (`m_platform_handler == nullptr` is the only early return). A deferred-attached
   source must be polled so its retry can run; PS5PadSource::poll returns no events
   while unavailable.
3+4. `register_keydown_event` (both overloads): the refusal now requires "no injected
   source AT ALL" (`!m_injected_source`) instead of "no AVAILABLE source". This is the
   load-rung enabler: main.lua's six binds register while the pad is deferred and fire
   when the retry opens it. The no-source refusal text and everything else unchanged.
5. `set_input_source` (injected branch): identity check decoupled from availability
   (name must be "Default" or the provider's name; `activate_input_source` still never
   displaces and never activates a different source). This keeps the wpd1 program
   gate's `set_input_source("Default")` succeeding on deferred boots.

Deliberately NOT staged: `UE4SSProgram.cpp` (the wpd1 program staging is REQUIRED
as-is — see FOLD.md; this packet adds no program delta), `PlatformInit.cpp`
(factory-mode `init()` untouched), `LuaMod.cpp` (no change), `Handler.hpp`
(no change), `PS5PadConfig.*` / `NativePadInput.hpp` / `ue4ss_backend.cpp`
(byte-unchanged, same as wpd1).

## 4. Host fixture note (ODR lesson, recorded)

`tests/host_pad_coexist.cpp` must include the STAGED provider header directly: the
host-1 recipe -I order resolves `"PS5PadSource.hpp"` to the BASE header, and a TU that
ALLOCATES the object (the test's factory control, via `make_shared`) must see the same
member set as the TU that CONSTRUCTS it. The mismatch was caught by the lineage
sanitizers (heap-buffer-overflow: the ctor writing the appended members past a
base-size allocation) — the fix and rationale are documented in the test header.

## 5. Compile gates (exact pinned composition argv)

`build_and_prove.py` compiles all four staged sources with the EXACT unit argv from
the launch-10 base receipt (`ue4ss-native-notify-cancel-1/build-1/receipt.json`,
`selected_runtime_units`), only the source, `-o` and `-MF` tokens redirected (asserted
≤3 token changes). Receipts: `compile-native-pad-input/`, `compile-native-pad-PS5PadSource/`,
`compile-native-pad-PS5PadNative/`, `compile-input-handler/` (command.json with full
argv, exit code, stdout/stderr). All four PASS (exit 0). Host proof: `host-1-coexist/`
+ `host-prove/receipt.json` (54 checks, lineage ASan+UBSan flags, exit 0).
