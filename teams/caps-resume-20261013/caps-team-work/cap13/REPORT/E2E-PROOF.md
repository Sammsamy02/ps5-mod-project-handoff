# CAP13 — Mac end-to-end proof: the PE loader executes real Windows DLLs

Agent KF (ZCode), 2026-10-13. **Mac-only; no console contact.** This is the
first execution of the staged cap13 PE loader (`native-loader/`, JH) on real
PE bytes — the M0 milestone of the design's plan (§8), run to completion.

Companion: `REPORT/GENERICITY-AUDIT.md` (audit + findings),
`REPORT/UNSEEN-MOD-TEST.md` (generalization), `DESIGN/native-dll-loader-addendum.md`
(recommended changes), raw logs under `REPORT/evidence/e2e-run-*.log`, host
source under `e2e/`.

---

## 0. Verdict box

1. **The PE loading mechanism works.** On this Mac (Apple Silicon, running the
   host as an x86_64 process under Rosetta 2), the staged loader mapped real
   Windows x64 PE DLLs at slid bases, applied relocations, resolved every
   import against a bridged host shim, and the CPU then **executed the
   DLL's actual machine code**: `DllMain` entry points, `start_mod` /
   `stop_mod` / `uninstall_mod` exports, and guest→host import calls — all
   returning verifiable values. Final suite: **98 checks PASS, 0 FAIL**
   across 18 cases, plus one documented WEDGE case (§6).
2. **The staged sources ran unmodified.** `pe_loader.cpp`, `pe_types.h`,
   `win64_bridge.S/.h`, `dllmain_invoker.cpp` are consumed as-is. What the
   Mac could not exercise is listed in §6 and is Rosetta-specific
   (addendum §A.4), not a design gap for PS5.
3. **"Imports resolved" and "actually executed" are now separate, measured
   claims** (user requirement 6): every crossing that fired is logged as an
   event with its arguments (e.g. the guest's exact `.rdata` strings arriving
   at a shim body); import *coverage* without execution is reported
   separately in UNSEEN-MOD-TEST.md and the shim-coverage audit.
4. **Original files preserved** (requirement 7): every case sha256-hashes its
   input before and after; 18/18 byte-identity checks passed — relocations
   and IAT patches happen only in the loader's private in-memory copy.
5. Not proven here, and not claimed: anything on PS5 (J1 exec-memory, J2
   fault containment, J3 CRT bootstrap remain open), C++ object-model
   compatibility, EH, the >4-argument g2h staging classes, and the anon-page
   trampoline path — each with the precise reason it is out of the Mac's
   reach (§6).

---

## 1. Test rig

| Element | Value |
|---|---|
| Host | Apple Silicon Mac (arm64, macOS 15.7.8), host process built `-arch x86_64` and run under Rosetta 2 — so the CPU executes the x86-64 bytes inside the PE |
| Loader | `../native-loader/src/pe_loader.cpp` + `pe_types.h`, staged, unmodified |
| Bridge | `../native-loader/src/win64_bridge.S` + `.h`, staged, unmodified; both directions (h2g `call0..6`, g2h shared thunks `cap13_g2h_thunk4s0`) |
| Entry invocation | `../native-loader/src/dllmain_invoker.cpp`, staged, unmodified |
| Host shim | `e2e/e2e_host.cpp`: named (dll, symbol)→(SysV body, arity) rows + per-row host-text thunks that enter the staged g2h thunks (why: addendum §A.4.3) |
| Load options | `page_size = 0x1000` (Rosetta virtualizes 4 KiB pages; PS5 keeps 0x4000 — addendum §A.4.1), `allow_rwx_partition = true` (Mac stands in for a positive J1 probe; PS5 default is refuse) |
| Integrity | sha256 of each input file before/after every case; 18/18 identical |

## 2. Cases and results (full log: `REPORT/evidence/e2e-run-20261013-211310.log`)

### 2a. Bridge sanity (`bridge`) — 2/2 PASS
- `cap13_win64_call1` (SysV→Win64) → Win64-shaped row thunk → staged
  `cap13_g2h_thunk4s0` → SysV body, with the argument value verified on the
  far side: the register remap (RCX↔RDI/RSI/RDX/R8/R9) is proven, not assumed.

### 2b. Fixture: entry point + lifecycle exports — 11/11 PASS
Input: `FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll` (hand-crafted, labeled;
x64 PE32+, real `AddressOfEntryPoint`, imports
`UE4SS.dll!is_ue4ss_initialized` + `KERNEL32.dll!DisableThreadLibraryCalls`).
- `pe_load` OK; mapped at a **slid base** (e.g. slide `0xffffffff89567000`),
  exercising the load≠ImageBase condition.
- `find_export` found `start_mod`/`stop_mod`.
- **`DllMain(PROCESS_ATTACH)` executed** through the staged `dllmain_invoker`
  (`cap13_win64_call3`) and returned TRUE; both imports were called exactly
  once each (guest `call [rip+rel32]` through its IAT into the bridge).
- `start_mod()` executed and returned 0 (its contract), calling the UE4SS
  import a second time; `stop_mod()` executed.
- File byte-identical.

### 2c. KA-built real sample: `cap13-ue4ss-flavor-console-dump.dll` — 10/10 PASS
The strongest single case — a real cross-compiled PE32+ DLL (lld-link,
`-noentry`), never executed before:
- `pe_load` OK at a slid base; **no entry point** — the loader/invoker report
  `NoEntryPoint` honestly instead of crashing.
- Route-selection check: exports `start_mod`/`uninstall_mod`, and no
  `mbm_mod_entry_v1` → this is a UE4SS-lifecycle PE, not an SDK PE.
- **`start_mod()` executed the DLL's real code**: it called
  `is_ue4ss_initialized()` (host shim returned 1), then
  `execute_lua_in_mod("ConsoleEnablerMod", "ExecuteInConsole('') DumpAllActors() ")`
  — **the guest's exact `.rdata` string literals arrived through the bridge
  into the host body** — and returned `(void*)1` in RAX as its source
  dictates. `uninstall_mod(0)` executed.
- File byte-identical.

### 2d. UNSEEN executable sample: `cap13-ue4ss-flavor-keybinds-poller.dll` — 6/6 PASS
See UNSEEN-MOD-TEST.md §3 for the full story; here: the loader mapped it, its
`start_mod()` executed (its `CreateThread` call hit the e2e refusal row — audit
F3's semantics demonstrated live; the guest survived and returned 1), and
`uninstall_mod` ran cleanly. Byte-identical.

### 2e. Refusal matrix — all PASS (fail-closed, precise reasons)
| Input | Refusal reason (verbatim from the loader) |
|---|---|
| `FIXTURES/cap13-native-x86-FIXTURE.dll` | `PE is not x86_64 (Machine != 0x8664); ARM64 PEs unsupported` |
| `FIXTURES/NOT-A-PE-FIXTURE.dll` | `not a PE image (no MZ/PE signature)` |
| `SAMPLE/real-UE4SS.dll` (real framework) | `PE has a static TLS directory; TEB/gs thread-local access is not emulatable in userspace on this target (design 3e) -- refused` — **correct: this image has real TLS data (0x2d8 bytes) and callbacks** |
| `SAMPLE/real-dwmapi.dll` (real proxy) | `unresolved imports (fail-closed; no null-filled IAT)` — 133 of 135 census rows unresolved (MSVCP140 ~100, KERNEL32, USER32, api-ms-win-crt-*), full list emitted |
| `SAMPLE/cap13-feature-probe.dll` | same TLS rule fires before imports (it carries a TLS directory; see the refinement finding, UNSEEN-MOD-TEST §4) |

### 2f. Staged-shim coverage audit (`shim-coverage`) — import coverage ≠ execution
What the **staged** `WinApiShim::register_builtin_exports()` registry (16 rows)
alone resolves against real import censuses:

| Census | Staged registry resolves |
|---|---|
| `cap13-ue4ss-flavor-console-dump` (4 rows) | **0/4** |
| `cap13-feature-probe` (13 rows) | 1/13 |
| `cap13-mod-splitscreen` (5 rows) | 0/5 |
| `real-dwmapi.dll` (135 rows) | **2/135** |

This quantifies the M1 work (census-generated rows) and is the concrete proof
that a mod's imports resolving is a *separate axis* from anything executing —
the user's requirement 6 and 9. The execution pipeline in this proof used a
completion row set (31 import rows with documented Win32/UE4SS-C-API
signatures); it is data in the E2E host, not a change to the staged shim.

## 3. What executed, precisely (execution events, not coverage claims)

Every `[event]` line in the log is a guest→host crossing that actually ran:
- `KERNEL32!DisableThreadLibraryCalls()` — via guest DllMain (fixture case)
- `UE4SS!is_ue4ss_initialized()` — via guest DllMain and via guest start_mod
- `UE4SS!execute_lua_in_mod("ConsoleEnablerMod", "ExecuteInConsole('') DumpAllActors() ")`
  — via guest start_mod, string pointers from guest `.rdata`
- `KERNEL32!GetModuleHandleW()`, `Sleep(0 ms)`, `CreateThread(REFUSED...)` —
  bridge sanity + poller case
- 98 checks total; every crossing argument-verified where the prototype
  allows (strings, return values, call counts).

## 4. Bridge classes validated vs not (precise)

| Crossing | Class | Mac result |
|---|---|---|
| host→guest, 0..4 args (`call0..call4`) | all-integer | **PASS** (DllMain, start_mod, stop_mod, uninstall_mod, mbm entries) |
| host→guest, 5..6 args (`call5/call6`) | all-integer | built+linked; exercised only via `bridge6` (below) |
| guest→host, ≤4 args (`thunk4s0`) | all-integer | **PASS** (every import crossing in every executed case) |
| guest→host, 5..8 args (`thunk4s1..s4`) | all-integer | **WEDGE under this Rosetta** (3/3 attempts, watchdog-isolated) — see §6 |
| mixed int/float signatures | — | refused by the staged envelope by design (audit F5); not attempted |

## 5. Debugging notes (in order encountered — kept as evidence)

1. **Trampoline page sealed too early** — stamping into an already-R-X page
   faulted; the host wedged in the Rosetta exception server. Fix: stamp while
   RW, seal (mprotect R-X) after stamping, before the first guest call.
2. **`mprotect` ENOMEM under Rosetta** — Rosetta gives translated processes
   4 KiB-aligned mappings; the loader's 16 KiB outward rounding crossed
   neighbor mappings. Fix: `LoadOptions::page_size = 0x1000` on Mac only.
3. **`movabs r11` from anon-page JIT code wedges Rosetta** — reproduced with a
   22-byte minimal trampoline: any r11 write from JIT'd (anonymous-page) code
   enters the `com.apple.rosetta.exceptionserver` wedge; the identical shape
   with `movabs rax` runs; r11 writes from host `__text` run fine. Fix: the
   Mac host uses per-row naked thunks in host `__text` that set r11 and jump
   into the staged shared thunks. The staged anon-page trampoline
   (`bridge_stamp_tramp`) is therefore Mac-untestable, PS5-targeted.
4. **s1..s4-class crossings wedge** (6-arg `call6` → `thunk4s2`): the full
   chain through the staging block wedges the Rosetta exception server even
   from host text (3/3 attempts). Every instruction the s0 class shares
   passes; the delta is exactly the stack-arg staging block. Recorded as
   WEDGE, not worked around — console validation will own these classes.
5. **Rosetta flakiness** — unrelated demand-translation wedges appear
   intermittently on this machine (state `UE`; `kill -9` cannot reap). The
   runner watchdogs every case (20 s) with retries and records permanent
   wedges explicitly.

## 6. Honest scope of this proof

- Proves the **loader + relocations + import resolution + staged bridge
  (s0 class) + entry/exports** on executable x86-64 bytes. Does NOT prove:
  PS5 memory/exec policy (J1), fault containment (J2), `/MD` CRT bootstrap
  (J3), EH/SEH execution (fail-closed only), C++ object-model crossing,
  mixed-signature or >4-arg crossings, the stamped anon-page trampoline.
- The e2e shim rows are a **census-completion data set for the sampled DLLs**,
  not new shim policy; two rows intentionally refuse (CreateThread, F3;
  CreateFileW, F4) to demonstrate fail-closed behavior on live guest paths.
- Percentages: none. Execution claims are the event list in §3; coverage
  claims are in §2f and UNSEEN-MOD-TEST.md.

## 7. Reproduce

```
cd cap13/e2e
./build_and_run.sh            # builds x86_64 host, runs all cases under watchdog
# evidence: ../REPORT/evidence/e2e-run-<ts>.log
# single cases:
arch -x86_64 ./e2e_host bridge
arch -x86_64 ./e2e_host fixture ../FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll
arch -x86_64 ./e2e_host console-dump ../SAMPLE/cap13-ue4ss-flavor-console-dump.dll
```

— agent KF (ZCode), 2026-10-13.
