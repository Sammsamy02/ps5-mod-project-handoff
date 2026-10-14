# REVIEW-VERDICT — runtime_module_entry.c + runtime_start_bridge (.cpp/.h)

Worker: worker-entry-review · 2026-10-04 · independent, read-only.
Reviewed bytes: `runtime_module_entry.c` sha256 `1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80` (371 lines, verified), `runtime_start_bridge.cpp` `1889bc7d…`, `runtime_start_bridge.h` `ecada77e…`.
All paths below are relative to the workspace root `/WORKSPACE/.codex/worktrees/translation-core/what`.

## VERDICT: GO-WITH-FIXES

The C entry is launch-quality: probe path preserved, wiring order exactly per WIRING-SPEC, fail-open discipline correct, import surface fully inside the frozen 191-row contract, buffers bounded. One P1 is an **integration/gate blocker** (composition-5 gate W2 will fail with the bridge as a separate unit even though the wiring itself is correct), and three P2s should be fixed before first real launch (ack-session truncation window, a bridge ownership-contract hole, and the bridge's missing compile evidence).

---

## Checklist verdicts

### 1. Behavioral equivalence of the preserved probe path — PASS, with one documented diagnostic-stream drift (P3-1)

Diffed line-by-line against `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`:

- entry_guard: identical atomic `__atomic_test_and_set(ACQUIRE)` / `leave_entry` release-clear; `-20` on re-entry for BOTH exports (entry:297-298, 338-339 = original:51-52, 84-85).
- `-20/-21/-22` semantics identical incl. `PROBE_ACTIVE → 0`, `state != PROBE_NEW → -21`, `!diagnostic_ready() || diagnostic_open_channel() < 0 → -22` (entry:299-304 = original:53-58).
- Constructor invocation identical: `constructor_index++`, `constructor_begin` / `probe_call_constructor_catching` / `constructor_return` (entry:99-106 = original:19-26). `PROBE_CTOR_EXCEPTION (-2)` surfaces via `probe_lifecycle_start` unchanged (`lifecycle.c:85-93`).
- Probe ops table identical: all 8 members, same order, same `&__dso_handle` (entry:115-119 = original:35-39); `finalize_dso` identical.
- Probe-result terminal handling preserved: on `result != 0` with `PROBE_FINISHED`, `probe_runtime_release_input()` + `diagnostic_close_channel()` (entry:319-328 = original:73-76).
- module_stop ordering: NEW owner stop strictly before `probe_lifecycle_stop` (entry:340-349) — required by `runtime_start_bridge.h:70-76` and WIRING-SPEC §4; the probe half is byte-identical to original:86-92, including the double-call-safe `result == 0 && PROBE_FINISHED` release/close (same idempotent pattern the original already had).
- Frame probe: `__builtin_frame_address(0) + sizeof(void*)` and the `mov %%rsp` asm are identical on x86-64 (entry:308-312 = original:62-64); the non-x86 fallback only affects host builds.

Drift (P3-1): original emitted `module_start_return` with the probe result in ALL post-probe paths. New entry emits `probe_lifecycle_result` (entry:318) and, on probe failure, `runtime_start_skipped` (entry:322) — `module_start_return` is now emitted only on the success path (entry:330). Return codes, state transitions and side effects are unchanged; only the diagnostic event stream differs. Any receipt or playbook that greps `module_start_return` on a failed probe (e.g. the pinned probe receipts) must be re-baselined. Intentional and documented in WIRING-SPEC §5.3, but it is a drift from "byte-for-byte probe behavior" and is flagged for honesty.

### 2. Wiring order vs WIRING-SPEC — PASS (exact), modulo the P1-1 gate interaction

- `mfa_activate` exactly once: `runtime_started` atomic test-and-set, entry:232; config zero-initialized then only `root`/`title_id` set (entry:246-247) — NULL `preload_roles`/`cancel`/`io`, zero limits/deadline = defaults, exactly as WIRING-SPEC §3 "mfa_activate details" prescribes (`modforge_activation.h:87-95`).
- Bridge order verified in code, not comments: capture (VMMAP-first, `supplier->capture()` bridge:156, default-arg `capture(const char* = nullptr)` at `live_module_facts.hpp:220` links to the composed `_ZN3lmf31RetainedLiveModuleFactsSupplier7captureEPKc`) → conversion with explicit opt-in `options.allow_vm_scan_base_as_module_image_base = true` (bridge:165; default `false` at `lmf_to_native_loader_facts.hpp:132`) → `MakeNativeProgramContext` (bridge:182) → `StartActivatedRuntimeWithAck` (bridge:193).
- `StartActivatedRuntimeWithAck` internals verified in the composed unit source (`worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp:68-95`): `MakeRuntimeAckConfig(mod_root)` (status_root = EXACT mod root, pid=getpid, CLOCK_MONOTONIC ms) → `InstallDefaultEngineBindings()` → 4-arg `PrepareActivatedUE4SSRuntime` → `owner->start()`. This is spec §3 Variant 1 steps 1-4 exactly.
- module_stop → `modbridge_runtime_stop()` gated on `runtime_active` (entry:340-342) → `StopRuntimeWithAck(slot)` (bridge:112) = `owner->stop()` + reset (`runtime_ack_supplier.cpp:56-65`), from the loader thread per `runtime_owner.hpp:91-94`. Spec §4 satisfied.
- Constants cross-checked against staged-layout evidence: title `PPSA33286` and module path `/data/plugins/modbridge_supplier_composition.prx` = `worker-composition-4-fix/.../package-profile.json`; mod root `/data/modbridge/roots/PPSA33286` = RUNTIME-STAGING-PLAN.md ("activation root /data/modbridge/roots/PPSA33286/"); `/data/plugins/UE4SS.log` = RUNTIME-STAGING-PLAN.md:96-97,150 ("the runtime writes UE4SS.log NEXT TO THE PRX").

P1-1 (see findings): WIRING-SPEC §6 W2 reads its call relocations from the per-unit object that defines `module_start`. The shipped design routes `StartActivatedRuntimeWithAck` through a separate bridge TU, so the entry object contains `mfa_activate` relocations but NOT helper relocations → `via_helper` is false → W2 fails a correct wiring. Fix before the gate run.

### 3. Fail-open correctness — PASS for the shipped wiring (two P2 caveats)

- Every wiring failure path returns 0 from `module_start`: `start_composed_runtime` is void; both failure branches (`mfa_result != MFA_OK` entry:251-268; `start_result != MRS_OK` entry:289-290) return, and `module_start` ends `return leave_entry(0)` (entry:331). Probe-lifecycle failures intentionally keep their nonzero preserved returns — per spec, correct.
- Three-channel evidence: `runtime_failure` (entry:221-226) = `diagnostic_event` (proven channel) + `write_ue4ss_log_line` (guarded by `diagnostic_ready()`, bounded, best-effort) + `write_status_failed` (guarded `mfa_status_open != 0` return). The mfa path additionally emits `mfa_activate_os_error` (entry:267). Host tests s2/s3/s4 assert all three channels (`host_test/run_host_tests.sh` scenarios 2-4).
- Ownership: the entry never calls `mfa_release` (verified: no occurrence). The bridge adopts the activation into `ActivatedMods mods{activation}` (bridge:147; `unique_ptr<mfa_activation, ActivationDeleter>` per `activated_mod_source.hpp:12-16`), which is destroyed on every exit path including exceptions — host test s3 asserts `activation_released` exactly once and no `bridge_stop` on a dead owner.
- P2-1: on `MRS_ERR_START` (stage-3 exception) and on the stop-exception path, the entry's `write_status_failed` re-opens `<root>/runtime-status.txt`, and `mfa_status_open` is `O_TRUNC` (`modforge_activation.c:609`) — it REPLACES the runtime's own ack session (which may already contain `activated`/`failed` lines written by the prepare-path catch in `ue4ss_backend.cpp`). This contradicts the entry's own comment (entry:196-197, "no RuntimeAckSink exists") and the append-only one-sink discipline. Details + diff under findings.
- P2-2: bridge early returns (bad args / already started, bridge:130-139) occur BEFORE adoption — a non-NULL activation would leak there, contradicting `runtime_start_bridge.h:53-57` ("OWNERSHIP IS ALWAYS TRANSFERRED on non-NULL input"). Unreachable from this entry (never NULL; `runtime_started` single-shot; slot cannot pre-exist), but a trap for future callers.

### 4. Import surface — PASS

- Frozen contract: `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json` measured with `len(contract.imports) == 191` exactly. `getpid`, `clock_gettime`, `sceKernelOpen`, `sceKernelWrite`, `sceKernelClose` are ALL present (checked by name). `__cxa_finalize` is present too; `__init_array_*`/`__fini_array_*`/`__eh_frame_*`/`__dso_handle` are packaging boundary symbols (`package_prx.py` `frozen_boundary`).
- No implicit libc call: the entry's only constructs with memset/memcpy potential are the two byte zero-init loops (see item 7). Verified three ways: (a) target flags are `-O1 -ffreestanding -fno-builtin` (`package_prx.py:140-142`) — clang marks the function `nobuiltin`, which disables LLVM loop-idiom memset synthesis; (b) at `-O1` LoopIdiomRecognize is not the deciding factor anyway, and (c) empirical: `nm -u host_test/out/entry.o` (host cc -O1, WITHOUT -fno-builtin) shows NO memset/memcpy/printf/snprintf/strlen undefined — the undefined set is exactly the intended probe_*/diagnostic_*/mfa_*/modbridge_*/sceKernel*/getpid/clock_gettime plus boundary symbols. No printf-family formatting exists anywhere in the entry (all logging is hand-rolled appenders). `<sys/fcntl.h>`, `<time.h>`, `<unistd.h>` contribute declarations only; the `O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW` constants (entry:181) match what the proven `diagnostic.c`/`modforge_activation.c` already use.
- `mfa_activate`/`mfa_preloaded_bytes`/`mfa_result_name`/`mfa_status_*` are C-linkage symbols DEFINED in the composed object (`nm combined-supplier.o`: `mfa_activate T`), so they add zero imports — exactly the WIRING-SPEC §2 premise.
- Residual check happens fail-closed at packaging anyway (`package_prx.py` §3.3 unresolved-set pre-check), which would catch any surprise before Packager.

### 5. Threading/lifetime — PASS

- `start_composed_runtime` runs on the loader thread inside `module_start`, after `probe_lifecycle_start` returned. The only pre-existing thread is the probe worker (present in the console-proven composition-4 build as well); the UE4SS init thread is spawned last, inside `owner->start()` (`runtime_ack_supplier.cpp:93`), after the log/status writes. `write_ue4ss_log_line` therefore runs strictly before any runtime thread — matching its own comment (entry:162-164).
- `runtime_started`: `__atomic_test_and_set(ACQUIRE)` (entry:232) — wiring at most once per process; host test s5 asserts exactly one `bridge_start` across a double `module_start`.
- `runtime_active`: plain `unsigned char`, but only ever read/written while the caller holds `entry_guard` (module_start/module_stop are mutually exclusive through it, entry:297/338), so no data race. Double `module_stop`: first stop clears `runtime_active` (entry:344) before `probe_lifecycle_stop`; a sequential second stop skips the owner branch and hits `PROBE_FINISHED → 0` early return in `lifecycle.c:108`.
- Stop semantics verified in `runtime_owner.cpp:43-78`: loader-thread `stop()` always joins (`thread_.join()`, line 59) before returning true; the false return is exclusively the init-thread self-join guard, unreachable from `module_stop`. `~RuntimeOwner` calls `stop()` and would `std::terminate` only on a self-join — not reachable here.
- Wiring-failure then later module_stop: `runtime_active` stays 0, bridge slot is NULL on every MRS_ERR path (assignment happens only after a successful return of the start call, bridge:193), and `modbridge_runtime_stop` returns 0 for an empty slot (bridge:103-105). No dangling owner.

### 6. Buffer/format safety — PASS

- `log_append` / `log_append_sanitized` (entry:129-144): bounded by the caller-passed capacity, sanitize maps control/non-ASCII to `'?'`, NULL-tolerant. Callers always pass `sizeof(buf)-1` and add the terminator/newline with explicit room checks.
- `log_append_i32` (entry:146-159): `INT32_MIN` handled via int64 negation (`magnitude` = 2147483648, 10 digits fits `decimal[10]`); loop bounded by `n < sizeof(decimal)`; value 0 emits `'0'` via do-while. Correct.
- `write_ue4ss_log_line` (entry:166-192): 256-byte line, appends capped at 255, `'\n'` added only `if (used < 255)`; the buffer is written by length (`used`), never NUL-required. Partial-write loop bounds-correct (`written <= 0 || written > remaining → break`).
- `detail[160]` (mfa path, entry:252-265): appends capped at 159; terminator index `used < sizeof ? used : sizeof-1` — in bounds on every branch, always NUL-terminated.
- `detail[192]` (bridge path, entry:279-290): bridge `copy_detail` (bridge:56-72) guarantees NUL within capacity and handles NULL/0; entry passes `sizeof(detail)` and only reads `detail[0]`/passes the pointer — safe.
- `note[128]` (entry:207-216): both termination branches (`used < 128` / else `note[127]`) in bounds.
- `mfa_error.message[160]`/`path[256]` reads (entry:257, 261): defended by the full-struct zero-init (entry:244-245) even if `mfa_activate` failed without populating `err`; reads are `message[0]`/`path[0]` sentinels plus bounded sanitized copies. `mfa_result_name` result NULL-checked (entry:254-256).
- `mfa_status status = {-1, 0}` (entry:200) matches `struct { int fd; uint32_t seq; }` (`modforge_activation.h:149-152`) — fd -1 = closed, seq 0.

### 7. Zero-init loops for config/error — PASS (no padding/alignment hazard)

- Byte loops through `unsigned char*` over `sizeof(struct)` (entry:242-245): char-type access is strict-aliasing-clean, byte stores have no alignment requirement on x86-64, and zeroing padding bytes is harmless (padding is never read; `mfa_activate` reads only declared fields).
- Not a memset emission risk in practice: target compile is `-O1 -ffreestanding -fno-builtin` (item 4) and the host-compiled object proves the loops stay loops. Even in the worst hypothetical, `memset` is itself one of the 191 contracted imports, so the packaging pre-check would not fail — only the entry's "no formatting imports" comment would be slightly stale.
- No VLA, no `malloc`, all stack objects fixed-size. `mfa_config` (64 bytes on x86-64) and `mfa_error` (~432 bytes) are well within any sane frame budget at `-O1`.

### 8. Packaging checks — PASS on the code; integration requirements flagged explicitly

- `force_align_arg_pointer`: `MODBRIDGE_ENTRY_ATTRS` expands to `visibility("default"), force_align_arg_pointer` under `__x86_64__/_M_X64` (entry:75-76) — prospero-clang is x86-64, so BOTH `module_start` (entry:293) and `module_stop` (entry:334) carry the frozen attribute pair, satisfying the `entry_alignment_evidence` byte-level check (WIRING-SPEC §5.1, `package_prx.py:980-988`).
- Both exports defined with default visibility; `argc/args` ignored (entry:296, 337) per spec §5.4.
- No C++ in the `.c` entry: pure C99; both consumed headers are C-safe (`runtime_start_bridge.h:28-30,78-80` and `modforge_activation.h:40-42,163-165` use `extern "C"` guards; `diagnostic.h`/`probe_api.h` are plain C). The hidden `modbridge_ctor_phase`/`modbridge_ctor_value` markers are preserved verbatim (entry:359-371; WIRING-SPEC §4 notes they may even be dropped).
- INTEGRATION REQUIREMENT (explicit, per the review mandate):
  1. **Bridge = SECOND packaging unit.** `BOOTSTRAP_UNITS` (`package_prx.py:128-139`) must gain `"runtime_start_bridge.cpp"` (place it after `lifecycle/module_entry.c`). The compile loop already selects `prospero-clang++` for `.cpp` (package_prx.py:658-663 — precedent: `lifecycle/constructor_boundary.cpp`), but the unit list, includes and the fail-closed pre-check must be re-run; nothing compiles the bridge today.
  2. **Header staging.** The compile loop's only `-I` dirs are `lin/generated, lin/probe, lin/lifecycle, lin, lua_include`. The bridge needs `runtime_start_bridge.h` PLUS the seven composed headers (`activated_mod_source.hpp`, `live_module_facts.hpp`, `lmf_to_native_loader_facts.hpp`, `NativeLoaderContext.hpp`, `runtime_ack_supplier.hpp`, `runtime_owner.hpp`, `ue4ss_backend.hpp`) and their transitive includes (`NativeLoaderContext.hpp` pulls `<Platform/NativeProgramContext.hpp>` and `<OwnedMappedScanSource.hpp>`). Stage them into one of the existing `-I` dirs, using the EXACT copies the composition compiled from (`ue4ss-native-tick-file-owner-1/include` + `worker-query-s2-arm/integration-patch/new-units`, per run-3 `commands.json`) — multiple divergent copies of these headers exist across worker dirs; hash-verify. Consider dropping the unneeded `#include "ue4ss_backend.hpp"` from the bridge (P3-4) to shrink that burden.
  3. **Gate W2 must be amended** (P1-1, diff below) or the composition-5 gate will report the (correct) wiring as unwired.
  4. Re-run the §3.3 unresolved-set pre-check: the bridge should add ZERO new strong undefineds (it calls only composed-defined symbols plus libc++/exception-runtime symbols the composed object already references), but this is asserted, not measured, until that pre-check runs.

---

## Findings (ranked)

### P1-1 — composition-5 gate W2 will FAIL this correct wiring (integration blocker, gate-side fix)

`gate_check_composition5.py` `find_entry_objects()` collects only per-unit objects that DEFINE `module_start`, and `check_w2()` requires those objects' `.rela.text` to reference `mfa_activate` AND (`StartActivatedRuntimeWithAck` OR `{PrepareActivatedUE4SSRuntime + RuntimeOwner::startEv}`) (gate script lines 381-438). The delivered design splits the chain across TUs: the entry object references `mfa_activate` (entry:250) and `modbridge_runtime_start_with_activation` only; the `StartActivatedRuntimeWithAck` relocation lives in the bridge object. Result: `via_helper=false`, `via_direct=false` → W2 "module_start-references-chain" = FAIL on a spec-correct composition (spec §0/§3 Variant 1 assumed the helper call textually inside module_start).

Required fix (gate side — recommended; the wiring itself is correct per WIRING-SPEC §0):

```diff
--- a/outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py
+++ b/outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py
@@ def find_entry_objects(build_dir: Path) -> list[Path]:
     entries = []
     for p in sorted(bld.glob("*.o")):
         if p.name == "combined-supplier.o":
             continue
         syms = symbols_of(p)
         ms = syms.get("module_start")
-        if ms and ms["defined"]:
+        wiring = syms.get("modbridge_runtime_start_with_activation")
+        if (ms and ms["defined"]) or (wiring and wiring["defined"]):
             entries.append(p)
     return entries
```

(Any object that defines `module_start` OR defines the C bridge entry point `modbridge_runtime_start_with_activation` participates in the W2 relocation scan. Alternative — not recommended: fold the chain into the entry TU, which C cannot call.)

### P2-1 — `write_status_failed` can truncate the runtime's own ack session (entry-side fix recommended before launch)

`mfa_status_open` opens with `O_TRUNC` (`.superpowers/.../modforge_activation.c:609`) — "a new session replaces the file" (`runtime_owner.hpp:34`). The entry claims it only opens a status session "where the bridge never ran (or already stopped) and no RuntimeAckSink exists" (entry:196-197). That claim does not hold on two reachable paths:

- `MRS_ERR_START` (stage-3 exception): `PrepareActivatedUE4SSRuntime` opens the sink BEFORE constructing the backend (`ue4ss_backend.cpp` prepare path: sink open, then `make_unique<UE4SSBackend>`; the catch writes point-4 `failed` and rethrows). The entry then writes a fresh session over the runtime's own `failed` line — losing the earlier session lines and double-opening the same sink file (whose fd also leaks: `RuntimeAckSink` has no destructor close; `close()` is only called on the successful stop path, `runtime_owner.cpp:71-75`).
- stop-exception path (`modbridge_runtime_stop` → catch → `MRS_ERR_STOP_EXN`): `slot.reset()` destroys the owner; if `stop()` threw before `ack_->close()`, the sink fd is still open and the entry truncates the file again.

Trigger window is narrow (backend-construction throw / stop throw), consequence is evidence loss + a second fd, not a crash — hence P2, not P1. Proposed fix:

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c
@@ static void runtime_failure(const char *stage, int code, const char *detail)
 {
     diagnostic_event(stage, (uint32_t)code);
     write_ue4ss_log_line(stage, code, detail);
-    write_status_failed(stage, code, detail);
+    /* Only paths where the bridge can NOT have opened a RuntimeAckSink:
+       conversion/context refusals happen strictly before the sink opens. */
+    if (code != MRS_ERR_START)
+        write_status_failed(stage, code, detail);
 }
```

(The stop path is covered by the same guard: stop codes are 0/1/MRS_ERR_STOP_EXN; a stop exception is post-sink by definition. If stop-failure status evidence is still wanted, emit it through `mfa_status_note` on the RETAINED sink instead of a fresh open — but the diagnostic + UE4SS.log channels already carry it.)

### P2-2 — bridge leaks the activation on its own early returns (contract text vs code)

`runtime_start_bridge.h:53-57` promises "OWNERSHIP IS ALWAYS TRANSFERRED on non-NULL input … releases it on EVERY exit path". `modbridge_runtime_start_with_activation` returns `MRS_ERR_INTERNAL` for null-string args (bridge:130-134) and "already started" (bridge:135-139) BEFORE `ActivatedMods mods{activation};` (bridge:147) — a non-NULL activation leaks there. Unreachable from the shipped entry (it never passes NULL and calls at most once), but the header contract is the interface, and the next caller will trust it. Fix:

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.cpp
@@ extern "C" int modbridge_runtime_start_with_activation(...)
 {
     auto &slot = owner_slot();
-    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
-    {
-        copy_detail(detail, detail_capacity, "bad arguments");
-        return MRS_ERR_INTERNAL;
-    }
-    if (slot)
-    {
-        copy_detail(detail, detail_capacity, "runtime already started");
-        return MRS_ERR_INTERNAL;
-    }
-
-    // Adopt the activation unconditionally: ...
+    // Adopt the activation unconditionally and FIRST: the ActivatedMods
+    // deleter (ActivationDeleter -> mfa_release) then runs on EVERY exit
+    // path below, including the refusal checks, matching the header
+    // ownership contract ("ALWAYS TRANSFERRED on non-NULL input").
     modbridge::native::ActivatedMods mods{activation};
+    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
+    {
+        copy_detail(detail, detail_capacity, "bad arguments");
+        return MRS_ERR_INTERNAL;
+    }
+    if (slot)
+    {
+        copy_detail(detail, detail_capacity, "runtime already started");
+        return MRS_ERR_INTERNAL;
+    }
```

### P2-3 — the bridge TU has zero compile evidence; header provenance must be pinned at integration

`host_test/` compiles the entry against `fakes/bridge_host.c` — the real `runtime_start_bridge.cpp` is never compiled anywhere in the worker's evidence. My review verified every signature by reading the exact headers the composed units used (LiveFirmwareProfile fields `live_module_facts.hpp:187-201`; `capture(const char* = nullptr)` :220; `LmfConversionOptions.allow_vm_scan_base_as_module_image_base` `lmf_to_native_loader_facts.hpp:132`; `LmfConversionResult::facts/error_message/operator bool` :135-145; `StartActivatedRuntimeWithAck`/`StopRuntimeWithAck`/`MakeRuntimeAckConfig` signatures match `runtime_ack_supplier.hpp` exactly) and every mangled symbol against `nm combined-supplier.o` (all `T`, exact Itanium names, including two `TryMake…FromSupplier` overloads where the bridge's `shared_ptr<const supplier>` call selects the correct one). That is static evidence only: divergent header copies exist across worker dirs, and an integration compile against the WRONG copy fails late. Required action: stage the exact header copies (content-hash against run-3 `commands.json` inputs) and make the first packaging compile of the bridge a gate artifact before first launch.

### P3-1 — diagnostic event drift on the probe-failure path (documented, intentional)

`module_start_return(result)` (original:72) is replaced by `probe_lifecycle_result(result)` + `runtime_start_skipped(result)` on failure (entry:318, 322); `module_start_return` now appears only on the success path (entry:330). No return-code or side-effect change. Re-baseline any receipt/parser that matches the old stream. Evidence: checklist item 1.

### P3-2 — `MRS_ERR_STOP_EXN == MRS_ERR_CONTEXT == 2`

`runtime_start_bridge.h:38,41` define both as 2. The entry only distinguishes zero/nonzero today, but a future log reader cannot tell a context-build failure from a stop exception in evidence lines. Cosmetic; suggest renumbering `MRS_ERR_STOP_EXN` to 5.

### P3-3 — `owner->start()` return value ignored (`runtime_ack_supplier.cpp:93`)

If thread spawn fails, `RuntimeOwner::start()` returns false and records `RuntimeState::Failed` + point-4 `failed` ack (`runtime_owner.cpp:31-40`); `StartActivatedRuntimeWithAck` still returns the non-null owner → bridge returns `MRS_OK` → entry sets `runtime_active=1` and logs `runtime_start_ok`. The session honestly shows `failed` in runtime-status.txt and module_stop later stops cleanly, so this is a graceful, observable degradation — but `runtime_start_ok` overstates it. Accepted design (ack file is the truth); note for the triage playbook: `runtime_start_ok` + status `failed` means "owner started, init thread never spawned".

### P3-4 — bridge includes `ue4ss_backend.hpp` unnecessarily

The bridge never calls `PrepareActivatedUE4SSRuntime` directly (it goes through the helper) and `runtime_ack_supplier.hpp` deliberately forward-declares `NativeProgramContext`; the `ue4ss_backend.hpp` include only enlarges the header-staging burden (that header "needs the UE4SS port include environment" per `runtime_ack_supplier.hpp:15-18`). Suggest dropping it after confirming nothing else in the TU needs it.

### P3-5 — (observation) stack-local `lmf::LiveModuleOsOps ops` is borrowed by the retained supplier

`RetainedLiveModuleFactsSupplier` borrows `ops` by const-ref (`live_module_facts.hpp:207-208`); the bridge's `ops` is a stack local (bridge:153) that dies when the outer try scope exits, while the conversion's owning alias can keep the supplier alive inside `NativeProgramContext`. Safe today: with the VmScan-identity route the capture retains no loader reference (so no post-start ops callback is possible), and all supplier use is confined to the try scope. If a future chain ever calls `validate_retention()`/`release()` from the runtime thread, this dangles. Record-only.

---

## Non-findings explicitly checked and cleared

- Entry never calls `mfa_release` (item 3); activation reaches the bridge raw and is adopted once (host s3: `activation_released` exactly once).
- No double `diagnostic_close_channel`/`release_input` regression: the stop path's repeated release/close on FINISHED is identical to the original and idempotent by construction (`lifecycle.c:33-38,108`).
- Status/log path constants match the staged layout (package-profile.json + RUNTIME-STAGING-PLAN.md), so `mfa_activate` and the UE4SS.log line land where the launch playbook expects them.
- `MRS_ERR_STOP_EXN` from `modbridge_runtime_stop` cannot cause `~RuntimeOwner → std::terminate` from the loader thread (the false return of `stop()` is init-thread-only).
- Host test harness (5 scenarios) matches every claimed failure discipline; staged entry copy is sha-identical to the delivered file.
