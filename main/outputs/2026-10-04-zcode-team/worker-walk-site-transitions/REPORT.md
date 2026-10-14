# REPORT.md — worker-walk-site-transitions, 2026-10-04

PROPOSALS ONLY. No console contact, no nested subagents, nothing executed on target, no
spike/driver/staging file modified, initializer staging untouched (sha re-verified
`c85c42f7aaab0630569d65db599fbf8cfcbd0b79b8e99c6e27ea709aee5a8b7b`, 2044 lines, byte-identical).
Workspace: `outputs/2026-10-04-zcode-team/worker-walk-site-transitions/` (this directory).
Deliverable order honored: `CLAIM.json` written first, this `REPORT.md` last, `RECEIPTS.txt`
pinning sha256 of every file.

---

## 1. Task 1 — the read point and the design

### 1.1 Where the unsafe read actually is

The merged staging initializer's three walk sites (`:1725` vtable display walk,
`:1923` ProcessInternal resolve, `:1961` ProcessLocalScriptFunction resolve) call
`ASM::resolve_function_address_from_potential_jmp`, which funnels — as do `resolve_jmp`,
`resolve_call`, `resolve_absolute_address` — into **`get_first_instruction_at_address`**,
whose `ZydisDecoderDecodeFull(..., 16 - offset, ...)` is an unconditional <=16-byte read at
the given address. Under `__PROSPERO__` the game text span `[0x400000,0x787c000)` is
EXEC-ONLY (VMMAP prot=0x4, EXEC-WRITABILITY-EVIDENCE §1) and igf6 died at exactly this read
class, twice, module-contained (EWV §2). A grep of the composed tree
(`native-coherent-runtime-1/source`) shows exactly four resolver consumers: this unit,
`UnrealVirtualBaseVC.hpp:115/:133` (the `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` macro —
the E-1/E-2/E-5 vtable walks), `DetourSubclasses.hpp:95`, and the initializer. **Guarding the
single innermost read covers every FLAG-2 walk with zero call-site edits.**

### 1.2 The unit and argv this diff targets (task 2's composition verification)

The composition does NOT recompile ASMHelper per pass. Verified chain:

- Compiled source: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/src/ASMHelper/ASMHelper.cpp`
  (byte-identical to the `ue4ss-unreal-native-build-1/port/first` copy; the `deps/first`
  copy differs — operands-array refactor — and is NOT the compiled one).
- Real unit argv (`coherent-full-1/asmhelper-asmhelper.command.json`, 148 args):
  `prospero-clang++ -std=c++23 -fms-extensions -O0 -fno-lto -fPIC ... -DRC_ASM_HELPER_BUILD_STATIC
  -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero -c <src> -o asmhelper-asmhelper.o`.
- The comp-4 run-3 `commands.json` compiles 15 units and ASMHelper is not among them; the
  link (`CMD 15`, ld.lld) consumes the PREBUILT `coherent-full-1/asmhelper-asmhelper.o`.

**Consequence for integrators:** a patched ASMHelper needs an explicit `asmhelper` unit
rebuild (its object is the link input) plus driver receipt attestation
(`regen_*.py --file`), or the link silently keeps the stale object — a silent half-state,
the exact class the no-exec gate forbids. No header changes: the whole diff is inside the
single .cpp, so the answer to "does a PROSPERO ifdef in ASMHelper alone suffice" is YES,
with the scope limits in §4.

### 1.3 The wrapper design

Verdict fold, compile-time (NoExecGate §2.1 precedent — no runtime trust, no silent re-arm):

- `-DRC_ASM_WALK_RX_GUARD=1` → guard ACTIVE: covering-page X->RX transition → read → restore.
- define absent / `=0` → guard REFUSES: loud `Output::send<LogLevel::Error>` line +
  `std::runtime_error` with ZERO syscalls; the transition code is preprocessor-dead.
  This is the no-exec gate's behavior at the helper layer — the named backstop for rows
  that compile the walk sites out (R5/R9), and the trap-catcher if a stale ini re-opens one.

Mechanics, all from the IGW probe's proven ladder (PROBE-DESIGN H1/H3, C-4, C-7):

1. **Covering-page math** (pure, tested): `[addr, addr+16)` over 16-KiB pages
   (`PAGE_SHIFT 14`) → 1 or 2 pages, one `mprotect(first, npages*0x4000, 0x5)` range call
   (fc1 `igfc1_page_range` discipline; overflow-safe; degenerate input = loud refusal).
2. **Transition**: single inline-syscall-74 `mprotect(range, PROT_READ|PROT_EXEC=0x5)` —
   the unmodified igcar 3-instruction shim (no import identity; `sceKernelMprotect` stays
   forbidden-symbol-gated; libc `mprotect()` weak-resolves to 0 in composed PRX,
   AUDIT-RESULTS §2.4). `rc != 0` → refusal reason `rx_transition_denied`, **NO read**.
3. **Read + restore**: RAII scope; destructor restores the LABELED span constant 0x4
   (kernel-reported prot of the whole span, EWV §1; fc1 `checkpoint_constant` discipline).
4. **Anomaly discipline without a throwing destructor**: failed restore → loud
   `LogLevel::Error` ANOMALY line + permanent latch in the guard context; every FUTURE
   walk read refuses with zero syscalls (`anomaly_latched_earlier_restore_failed`). C-7
   ladder-stop without destructor-throw UB.
5. **Nesting/depth**: in-flight range stack (depth cap 8, single game-thread init
   discipline documented in the file header); an inner scope whose range an outer scope
   already covers no-ops (no double-restore strip-window); stack-ordered restores.
6. **Constants** are build-stampable defines; defaults read back from pinned SDK v0.43
   `target/include` this session: `SYS_mprotect 74` (sys/syscall.h:82), `PROT_READ 0x01`
   (sys/mman.h:53), `PROT_EXEC 0x04` (sys/mman.h:55), `PAGE_SHIFT 14`
   (machine/param.h:99, PS5 branch). `static_assert` pins the fold to 0/1.

Diff shape: 290-line unified diff (`ASMHELPER-WALK-GUARD.diff`), all inside
`#if defined(__PROSPERO__)`, plus a 3-line guarded construction at the top of
`get_first_instruction_at_address`. The pure decision core is delimited by
`RC_ASM_WALK_GUARD_CORE BEGIN/END` markers so the host test extracts and runs the exact
shipped bytes. No header changes, no new files, no call-site changes.

## 2. Task 3 — host test + compile gate (actual evidence)

### 2.1 Logic test (software mprotect model, fc1 IGFC1_HOST_TEST pattern)

`host-test/run_host_test.sh` extracts the marker-delimited core (120 lines, drift-abort on
missing/duplicate markers) and runs `walk_guard_core_host_test.cpp` against an 8-page
synthetic arena. **Result: passes=144 failures=0, rc=0** (host clang++, -Wall -Wextra
clean). Coverage:

| test | what it pins |
|---|---|
| T1/T2/T2b | ordering RX→read→restore; exact op sequences; 16-byte window straddling a page end = 2 pages; exact fit = 1 page; table round-trips to 0x4 |
| T3 | fold OFF → `guard_off_no_rx_verdict`, ZERO model syscalls, no latch poisoning, context still usable |
| T4 | refused transition → NO read, NO restore, denial is not an anomaly, rc (-13) carried in `detail` |
| T5 | failed restore → exit returns rc, latch set, scope unwound, every later enter refuses with zero syscalls |
| T6/T7 | nested same-page scope no-ops (one transition/restore pair); nested different-page scopes restore inner-then-outer, all pages at baseline after unwind |
| T8 | depth cap 8: 9th UNCOVERED scope refuses `guard_depth_exceeded`, only 8 transitions issued, clean unwind |
| T9 | zero-length read and `addr+len` overflow refuse `page_range_invalid`, zero syscalls |
| T10 | recursion-shaped sequential resolves keep the whole map at baseline after every scope (8 ops for 4 reads) |

Honest note: the first T8 draft hit a hang — the sampled stack showed `strcmp` on a
nullptr reason because the 9th scope's address was still COVERED by the in-flight 8th
scope (the core correctly no-opped; the test mis-compared). Fixed in the test (uncovered
9th address + nullptr-safe `is_reason`); the shipped core needed no change. Recorded
because the bug is itself evidence the covered-scope no-op works.

### 2.2 Compile gate (composition's REAL argv, both folds)

`host-test/compile_gate.sh` replays `coherent-full-1/asmhelper-asmhelper.command.json`
argv with only source/output redirected (env note: the SDK wrapper needs `LLVM_CONFIG`
pointing at the retained `LLVM-21.1.8-macOS-ARM64` the drivers themselves use). Results,
objects retained under `host-test/compile-gate/`:

- fold0 absent/refuse: **rc=0**, 0 bytes stderr (zero warnings), object sha256
  `81dd832bd59fab43a57c0feb0c9c15b756b2e62f74e667e320e666a2c66b4bb6`
- fold1 `-DRC_ASM_WALK_RX_GUARD=1`: **rc=0**, 0 bytes stderr, object sha256
  `6604ce2d423c5d0dee4de6a089bd8141126e0fb02e7538ae08a4076d9b5af8d5`

Mechanical checks on the objects (retained LLVM llvm-objdump/llvm-nm):

- fold0 disassembly contains **0** `syscall` instructions; fold1 contains exactly **1** —
  the refuse-fold's "zero mprotect text" property (R9 posture) is proven, not asserted.
- Defined-symbol surface: all five public `RC::ASM` exports (`T`) identical to the
  shipped `coherent-full-1` object; the guarded object only ADDS file-local (`t`/`b`/`d`)
  and weak (`W`) `WalkGuard` internals — no removals, no renames, no ODR/link risk.

## 3. Task 4 — the verdict-dependency note (which matrix rows consume this)

The wrapper consumes IGW verdict dimension D2 (records `/data/modbridge-exec-writability.jsonl`,
contiguous seq law). The fold define maps to D2 as follows — **`RC_ASM_WALK_RX_GUARD=1`
is admissible exactly when seq5 `rx_result.mprotect_rc == 0`** (RX proven):

| D2 verdict | fold | behavior |
|---|---|---|
| `W_WORKS` (R1/R2) | `=1` allowed | walks pass behind per-page transitions |
| `W_FAILS_R_WORKS` (R3/R4) | `=1` allowed | RX alone is exactly the wrapper's transition; matrix R3 cell "per-page RX for walks" |
| `BOTH_FAIL` (R5) | ABSENT | loud helper-layer refusal, zero syscalls; walks are compiled out upstream by the no-exec gate (U-SWAP-5) |
| `FATAL` (R9) | ABSENT (mandatory) | permanently-closed syscall route cannot re-open through this unit; constexpr/preprocessor-dead |
| `ABSENT` | ABSENT (default) | fail-closed refusal; no optimistic syscall without evidence |

Terminology note vs the task text: the task's "W_FAILS" shorthand maps to D2's
`BOTH_FAIL`/`FATAL`/`ABSENT` family for the REFUSE fold; `W_FAILS_R_WORKS` keeps RX
working and is an ACTIVE fold (it is the row family that most needs this diff). Even when
folded ON, runtime fail-closure (rc!=0 → loud throw, no read) means a mis-folded verdict
degrades to the loud refusal, never a silent crash. Sequencing per LAUNCH10-MATRIX §4:
the D2 read (box 4) precedes choosing the fold; a missing IGW seq is FATAL → R9 → fold
stays ABSENT. Consumers of the delivered diff: rows R1/R2 (with U-SWAP-4's PLH layer),
R3/R4 (with U-SWAP-2's backend); R5/R9 consume only its refusal behavior. Every
consumption additionally requires the row's mapscan-delta staging (matrix §0 rule 4) and
the §1.2 unit-rebuild + `--file` receipt attestation.

## 4. Scope limits (named, not silent)

- **NOT covered — FLAG-3**: the initializer's direct 164-byte Zydis decode
  (staging `:1944-1972`) reads text WITHOUT this helper; its closure remains the v3 ini
  keys (PI/PLSF=0) / no-exec gate E-7. This diff does not touch it.
- **NOT covered — write tier**: the guard only ever ADDS READ (0x4→0x5) and restores;
  PLH `x64Detour`'s 100-byte disasm + RWX write phases, `MemProtector`/`MemAccessor`/
  `Misc` Prospero branches are U-SWAP-4's separate deliverable. FName-detour verifier
  reads (E-8) likewise untouched.
- **Restore source is the labeled 0x4 constant**, not a live mapscan: exact for the
  proven span state; the row-level mapscan delta (matrix §0 rule 4) remains the
  ridealong/verifier instrument. Residual risks documented: (a) an external transitioner
  mid-scope would be re-protected to 0x4 — excluded by the serialized single-thread init
  discipline; (b) no per-transition map verification inside this unit — a future revision
  can add a registered cover-provider (the extension point where a `__PROSPERO__`
  declaration in the deps header would go); deliberately NOT added now to keep the diff
  single-unit and the header frozen.
- **Threading**: guard state is plain file-scope (no TLS in PRX); safe under the
  composition's serialized init, unsafe if anyone ever walks from two threads — stated in
  the file header.
- The refusal/anomaly log vocabulary (`[WalkRxGuard] REFUSED` / `[WalkRxGuard] ANOMALY`)
  is new evidence vocabulary; the next matrix revision should add it to §4 box 2's D1
  match list.

## 5. Non-claims

No kernel-behavior claim of any kind (the IGW verdict does not exist yet); no in-game
execution; no staging/composition/receipt changes (compile-gate objects are throwaway
copies inside this worker directory); the host model proves ordering/refusal/page-math of
the shipped core bytes, not console behavior; no PLH/detour viability claim; no claim that
the wrapper makes W_WORKS sessions safe beyond the reads that flow through
`get_first_instruction_at_address`.

## 6. File index (sha256 in `RECEIPTS.txt`)

- `CLAIM.json` — claims/non-claims (written first)
- `ASMHELPER-WALK-GUARD.diff` — the unified diff vs the pristine port unit
- `ASMHelper.baseline.cpp` / `ASMHelper.guarded.cpp` — before/after full sources
- `host-test/run_host_test.sh` + `walk_guard_core_host_test.cpp` (+ generated
  `asm_walk_guard_core.inc`, binary) — logic test, 144/144 PASS
- `host-test/compile_gate.sh` + `compile-gate/` — real-argv compile gate, both folds,
  0 errors 0 warnings, syscall-count and symbol-surface checks recorded in §2.2
- `RECEIPTS.txt` — sha256 pins
