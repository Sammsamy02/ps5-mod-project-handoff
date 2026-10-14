# WIRING-SPEC — what a correct composition-5 `module_start` must call

Worker: worker-composition5-gate · 2026-10-04 · offline review, read-only.
Every symbol below was read back from the actual composed artifacts, not from
headers alone. Composed-object evidence:
`outputs/2026-10-04-zcode-team/worker-composition-4-fix/prx-candidate-composition4-1/build/combined-supplier.o`
(packaging-time `ld.lld -r` merge of bootstrap units + the composed runtime
object `…/ue4ss-native-supplier-composition-4/run-3/coherent-tick-file-witness-runtime.o`,
sha256 `45dd9901db7d626e63b8ac813bd8bf09f00938b1c5ecfa5f726ff7c9ff075af5`)
and `run-3/exports.stdout`.

## 0. The one-paragraph summary

The composition-4 PRX loads and does nothing because its packaging-time entry
`lifecycle/module_entry.c` calls only `probe_lifecycle_start` (probe tree). A
correct composition-5 entry must, from `module_start`, (1) activate the mod
inventory with `mfa_activate`, then (2) hand the activation to the composed
startup chain — either via the one-call helper
`modbridge::native::StartActivatedRuntimeWithAck(context, activation,
expected_title, mod_root)` (preferred; it internally does
`PrepareActivatedUE4SSRuntime` → `NativeModSource` → `RuntimeOwner::start()`),
or by calling `PrepareActivatedUE4SSRuntime` and `start()` directly. All of
these are already **defined** (T) inside the composed object, so a wired entry
that calls only them adds zero new imports.

## 1. Current (unwired) entry — the defect

`outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`
(byte-identical to the pinned lineage
`.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/lifecycle/module_entry.c`,
compiled at packaging time from `BOOTSTRAP_UNITS` entry
`"lifecycle/module_entry.c"` in
`outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py:128-139`):

- `module_entry.c:47-78` — `module_start` does entry-guard → diagnostic channel
  → `probe_lifecycle_start(&lifecycle, &ops, …)` (line 69-71). **No startup-chain
  call exists.**
- `module_entry.c:80-93` — `module_stop` → `probe_lifecycle_stop` (line 86).

## 2. Actual linkable symbols in the composed object

Verified by `nm combined-supplier.o` and `run-3/exports.stdout` (type T =
defined, globally visible). These are the exact mangled names the wiring links
against (Itanium ABI, clang/libc++; `std::__1` = libc++):

| step | demangled | exact linkable symbol | defined in |
|---|---|---|---|
| A | `int mfa_activate(mfa_config const*, mfa_activation**, mfa_error*)` | `mfa_activate` (C linkage, unmangled) | composed object (`exports.stdout`: `mfa_activate T 0 14b1`; defined by `source/activation/modforge_activation.c`) |
| B | `modbridge::native::StartActivatedRuntimeWithAck(RC::Platform::NativeProgramContext, std::unique_ptr<mfa_activation, modbridge::native::ActivationDeleter>, std::basic_string_view<char, std::char_traits<char>>, char const*) -> std::unique_ptr<RuntimeOwner>` | `_ZN9modbridge6native28StartActivatedRuntimeWithAckEN2RC8Platform20NativeProgramContextENSt3__110unique_ptrI14mfa_activationNS0_17ActivationDeleterEEENS4_17basic_string_viewIcNS4_11char_traitsIcEEEEPKc` | extra unit `module-runtime-ack-supplier.o` (run-3 nm-verified) |
| C | `modbridge::native::PrepareActivatedUE4SSRuntime(RC::Platform::NativeProgramContext, std::unique_ptr<mfa_activation, modbridge::native::ActivationDeleter>, std::basic_string_view<char, std::char_traits<char>>, modbridge::native::RuntimeAckConfig const&) -> std::unique_ptr<RuntimeOwner>` | `_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeEN2RC8Platform20NativeProgramContextENSt3__110unique_ptrI14mfa_activationNS0_17ActivationDeleterEEENS4_17basic_string_viewIcNS4_11char_traitsIcEEEERKNS0_16RuntimeAckConfigE` | composed runtime (`exports.stdout`) |
| D | `modbridge::native::MakeActivatedUE4SSModSource(std::unique_ptr<mfa_activation, modbridge::native::ActivationDeleter>, std::basic_string_view<char, std::char_traits<char>>) -> std::shared_ptr<RC::Platform::NativeModSource const>` | `_ZN9modbridge6native27MakeActivatedUE4SSModSourceENSt3__110unique_ptrI14mfa_activationNS0_17ActivationDeleterEEENS1_17basic_string_viewIcNS1_11char_traitsIcEEEE` | composed runtime (called internally by C) |
| E | `RC::Platform::NativeModSource::NativeModSource(std::vector<RC::Platform::NativeModFile>)` {C1, C2} | `_ZN2RC8Platform15NativeModSourceC1ENSt3__16vectorINS0_13NativeModFileENS2_9allocatorIS4_EEEE` (and `C2`) | composed runtime (called internally by D) |
| F | `modbridge::native::RuntimeOwner::start()` | `_ZN9modbridge6native12RuntimeOwner5startEv` | composed runtime (`exports.stdout`; called internally by B) |
| G | `modbridge::native::RuntimeOwner::stop()` | `_ZN9modbridge6native12RuntimeOwner4stopEv` | composed runtime (module_stop side; also wrapped by `StopRuntimeWithAck`) |
| H | `modbridge::native::StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>&) -> bool` | `_ZN9modbridge6native18StopRuntimeWithAckERNSt3__110unique_ptrINS0_12RuntimeOwnerENS1_14default_deleteIS3_EEEE` | extra unit `module-runtime-ack-supplier.o` |
| I | `modbridge::native::MakeRuntimeAckConfig(char const*) -> RuntimeAckConfig` | `_ZN9modbridge6native20MakeRuntimeAckConfigEPKc` | extra unit (only needed for the direct-C variant) |
| J | `RC::Platform::QueryHarness::Harness::InstallDefaultEngineBindings()` | `_ZN2RC8Platform12QueryHarness7Harness28InstallDefaultEngineBindingsEv` | `native-query-harness.o` (called internally by B) |

Critical signature note: the composed `PrepareActivatedUE4SSRuntime` is the
**4-arg ack-enabled** version — there is no `activation_root` parameter in the
composed object (the 5-arg form in
`outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.hpp:25-28`
is a different, newer source revision than the composed unit at
`outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp:84-88`).
The wiring must call the 4-arg form (or go through helper B); a call to the
5-arg form will not link and must NOT be attempted against this object.

## 3. Required call order and arguments

### Variant 1 (preferred) — one-call helper

Source of truth: `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp:68-95` (unit delivered in the composition as `module-runtime-ack-supplier.o`, run-3 `commands.json`), header contract `runtime_ack_supplier.hpp:46-63`.

```cpp
// inside module_start, after the diagnostic channel is open and before any
// failure return; entry_guard (module_entry.c:51) already prevents re-entry:
mfa_config cfg{};                       // modforge_activation.h:95-104
cfg.root     = "<mod root, e.g. /data/modbridge/PPSA33286>";   // exact string, also passed as mod_root below
cfg.title_id = "<expected title>";      // must equal the load list's title
// preload_roles/limits/deadline/cancel/io: zero/NULL defaults are valid
mfa_activation* raw = nullptr;
mfa_error err{};
if (mfa_activate(&cfg, &raw, &err) != 0) { /* fail-open diag event, return policy code */ }
auto owner = StartActivatedRuntimeWithAck(std::move(context),   // ActivatedMods adopts raw (ActivationDeleter)
                                          ActivatedMods{raw},
                                          /*expected_title*/ cfg.title_id,
                                          /*mod_root*/ cfg.root);
// owner kept in file-static storage for module_stop; see section 4
```

What the helper does, in order (line numbers from `runtime_ack_supplier.cpp`):

1. `MakeRuntimeAckConfig(mod_root)` (line 75; def line 44-54) — the ONLY
   RuntimeAckConfig birth; `status_root` MUST be the exact `mfa_config::root`
   string (header comment lines 33-36), `pid=getpid()`, `now_ms`=monotonic ms.
2. `RC::Platform::QueryHarness::Global().InstallDefaultEngineBindings()` (line 82)
   — exactly once, BEFORE `RuntimeOwner::start()` spawns the init thread.
3. `PrepareActivatedUE4SSRuntime(std::move(context), std::move(activation),
   expected_title, runtime_ack)` (line 87-90) — the 4-arg composed form.
4. `owner->start()` (line 93) — spawns the serialized init thread;
   `UE4SSBackend::initialize` → `program_->init()` (composed
   `ue4ss_backend.cpp:65`) blocks that thread until real engine initialization.

Inside step 3 (`worker-composition-3/bootstrap-stage/.../ue4ss_backend.cpp:84-123`):
reject if `context.mod_source` already set (line 89-90) →
`ValidateNativeProgramContext` → `context.mod_source = MakeActivatedUE4SSModSource(std::move(activation), expected_title)` (line 93-94; this is the **NativeModSource** step: `activated_mod_source.cpp` consumes the activation, matches `expected_title`, and returns the retained source) → open the
`RuntimeAckSink` on `<status_root>/runtime-status.txt` (line 100-102) → construct
`UE4SSBackend` with buffered mods (`start_native_mods`, line 106 and 51-57) →
`ack->activated()` (line 107) → `RuntimeOwner` (line 108).

### Variant 2 — direct chain (acceptable, more code in the entry)

`MakeRuntimeAckConfig` (I) → `InstallDefaultEngineBindings` (J) →
`PrepareActivatedUE4SSRuntime` (C, 4-arg) → `RuntimeOwner::start()` (F).
Semantically identical; B exists precisely so the entry does not re-implement
the ordering (ack birth before prepare, bindings before start), so Variant 1 is
the reviewed path.

### `mfa_activate` details (step A)

Declared `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/activation/modforge_activation.h:113`
(vendored copy `outputs/2026-10-04-zcode-team/worker-7-installer/vendored/modforge_activation.h:113`):
`int mfa_activate(const mfa_config *cfg, mfa_activation **out, mfa_error *err)`.
On success `*out` owns everything and must reach `PrepareActivatedUE4SSRuntime`
(adopted by `ActivatedMods`, deleter `ActivationDeleter` → `mfa_release`);
never `mfa_release` a raw pointer that was handed off. `mfa_config` fields at
lines 95-104: `root`, `title_id`, `preload_roles`, `limits`, `deadline_ms`,
`cancel`, `io` (NULL io = POSIX). On failure `*out` is NULL and `err` carries
code/line/path/message — the entry must emit a diagnostic event with the code
and return its fail-open policy value.

## 4. module_stop side and owner lifetime

- The `std::unique_ptr<RuntimeOwner>` returned by the chain must outlive
  `module_start` (file-scope static or function-local static), because
  `RuntimeOwner::stop()` may only be called from a loader control thread, never
  the init thread (`runtime_owner.hpp:91-94`).
- `module_stop` must call `StopRuntimeWithAck(owner)` (H,
  `runtime_ack_supplier.cpp:56-65`: `owner->stop()` then reset) or `owner->stop()`
  directly (G). It must NOT call `RuntimeOwner::stop()` from the init thread.
- `module_entry.c:96-108` helpers `modbridge_ctor_phase`/`modbridge_ctor_value`
  are unreferenced by the composed object (verified against
  `run-3/undefined.stdout` — no `modbridge_ctor*`); the new entry may drop them
  without creating imports (note, not a requirement).

## 5. Hard constraints on the new entry (from the packaging contract)

1. Keep `__attribute__((visibility("default"), force_align_arg_pointer))` on
   BOTH `module_start` and `module_stop` — the packaging byte-level check
   `entry_alignment_evidence` (`package_prx.py:980-988`) requires the
   realignment prologue on both exports in the FINAL PRX
   (`worker-composition-5-rebuild/REBUILD-PROCEDURE.md` section 0, bullet 3).
2. Every undefined symbol the new entry introduces must already be in the
   frozen 191-import contract plus the 5 extended imports
   (`sceKernelDlsym`, `sceKernelGetModuleInfo`, `sceKernelLoadStartModule`,
   `sceKernelStopUnloadModule`, `qsort`; `package_prx.py:152-158`). The chain
   symbols in section 2 are defined internally, so calling them adds nothing;
   but any NEW libc call in the entry (e.g. `strlen`, `memcpy` variants not in
   the contract) fails the packaging pre-check. Stick to the compiled chain.
3. Keep the entry-guard + fail-open diagnostic discipline
   (`module_entry.c:41-58`): atomic test-and-set guard, `-20` on re-entry,
   diagnostic events around the chain (`module_start_entered`,
   per-step wiring markers, `module_start_return`) so a failed start is
   observable on the diagnostic channel without a debugger.
4. `argc/args` are ignored (`module_entry.c:50`); the mod root and title are
   compile-time/probe-supplied facts, not derivable from loader arguments.

## 6. What "wired" means for the gate (machine-checkable form)

In the final combined object, `module_start` must contain call relocations
against: `mfa_activate` AND (B `StartActivatedRuntimeWithAck` OR
{C `PrepareActivatedUE4SSRuntime` AND F `RuntimeOwner5startEv`}). In a
relocatable object these appear as `R_X86_64_PLT32`/`R_X86_64_BRANCH*`
relocations in the entry translation unit's `.text` — that is exactly what
`gate_check_composition5.py` asserts (check W2), with disassembly evidence
emitted to the receipt. Presence of the definitions alone (W1) is NOT proof of
wiring; composition-4 passes W1 and fails W2 by construction.
