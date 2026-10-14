# cap03 BUILD-RECEIPT — 2026-10-13 (KD)

Scope: Mac-only compile + host test of the Capability #3 code-patch
implementation (`src/CodePatch.cpp/.hpp`, `src/PatchDiagnostic.cpp`).
NO console contact. NO kernel contact. All work inside `cap03/`.

Status pushed: **"source complete" -> "compiled for the PS5 target with the
real Prospero toolchain, linked as a loadable payload, and host-tested."**

---

## 1. Toolchain (exact provenance)

| Item | Value |
|---|---|
| Driver | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++` |
| Underlying clang | SDK `bin/clang`, target `x86_64-sie-ps5` (driver adds `-isysroot <sdk> -isystem <sdk>/target/include -L <sdk>/target/lib -fno-stack-protector -fno-plt -femulated-tls -lc -lkernel_web -lSceLibcInternal -lSceNet`; recipe from the l26 build receipts, e.g. `2026-10-08-l26-build-variants/l26-A/pass1/objects/*.command.json`) |
| LLVM tools for inspection | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin` (`llvm-objdump`, `llvm-readelf`); SDK `bin/prospero-nm` with `LLVM_CONFIG` pointed at the same tree |
| Target macros verified | `__SCE__=1`, `__PROSPERO__=1`; `machine/param.h` -> `PAGE_SIZE = 1<<14 = 16384` on the PS5 target (vs 4096 in the host build; the code is page-size-generic) |

## 2. STEP 1 — PS5-target compile (object files)

Exact flags (per file):

```
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -D_XOPEN_SOURCE=700 -Wall -Isrc -c src/CodePatch.cpp      -o build/ps5-obj/CodePatch.o
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -D_XOPEN_SOURCE=700 -Wall -Isrc -c src/PatchDiagnostic.cpp -o build/ps5-obj/PatchDiagnostic.o
```

Result: **both compile clean, zero errors, zero warnings** (`file`:
ELF 64-bit LSB relocatable, x86-64).

| Artifact | Size (bytes) | sha256 |
|---|---|---|
| `build/ps5-obj/CodePatch.o` | 24,592 | `7b90aa54aeb7f09b945a5b2f6540a348ea5336f4cbba93ab3094632f2bd70a6a` |
| `build/ps5-obj/PatchDiagnostic.o` | 11,472 | `27a699ffcc56ea3cd7afabe5dc49524c661ba5c91943c9ba94aacbf0dcf49137` |

### Source fix required for the target compile (1)

* **`MAP_ANON` undeclared** (`PatchDiagnostic.cpp:126`) — the SDK
  `sys/mman.h:85` defines it only under `__BSD_VISIBLE`; `-D_XOPEN_SOURCE=700`
  hides it. Fix: documented `#ifndef MAP_ANON / #define MAP_ANON 0x1000`
  fallback + `static_assert(MAP_ANON == 0x1000)` when the header IS visible
  (value pinned to `sys/mman.h:85`). Compiles now under both strict-POSIX
  and default visibility.

## 3. STEP 2 — Link as a loadable payload

`PatchDiagnostic.cpp` already provides the payload entry point: `main()`
(crt1.o `_start` -> `main`), which is the ps5-payload-sdk format used by
this project's `pldmgr /loadpayload` payloads (ploader.elf precedent).

```
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -o build/cap03-diagnostic.elf build/ps5-obj/CodePatch.o build/ps5-obj/PatchDiagnostic.o
```

Result: **links clean on the first try.**

| Check | Result |
|---|---|
| Format | ELF 64-bit LSB **PIE executable, x86-64, FreeBSD**, dynamically linked — byte-format class identical to the known-good console-proven `ploader.elf` (`outputs/2026-10-08-l26-build-variants`-era builds) |
| Entry | `_start` (crt1.o) + `main` + `run_cap03_diagnostic` + `cap3_diag_probe_fn` all present |
| NEEDED | `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx` (same set as ploader.elf; SDK driver default — the pldmgr payload precedent) |
| SCE imports | `sceKernelQueryMemoryProtection`, `sceKernelVirtualQuery` as UND — both ARE exported by the pinned `libkernel_web.so` (verified with `prospero-nm -D`) |
| `mprotect` | served by the SDK `libc.a` syscall stub (syscall 74 on amd64 — exactly the gate the kstuff uelf intercepts, `uelf/main.c:87-93`) |
| Stock-readelf `invalid PT_DYNAMIC` warning | also present for the known-good ploader.elf — generic-readelf vs PS5-ELF quirk, not a defect |

| Artifact | Size (bytes) | sha256 |
|---|---|---|
| `build/cap03-diagnostic.elf` | 521,192 | `35500cb7b901c283a8b06eae956e8c00cd0092279569ccc886dad3adaee550ae` |

**Not done (out of scope / not authorized):** no upload, no fake-signing, no
console load. Per the mission rules, any future executable upload requires a
decrypt-OFF readback verify (magic + sha256) and the one-launch discipline.

## 4. STEP 3 — Mac host test of the protocol

Files: `tests/host_test_protocol.cpp` (the test), `tests/run_host_test.sh`
(build + run + static check). Production TU compiled UNMODIFIED-in-intent
for the x86_64 Mac host with command-line renames routing the SCE/mman
boundary to mocks:

```
clang++ -arch x86_64 -std=c++23 -O0 -g -Wall -Isrc -c \
  -DPAGE_SIZE=4096 -DPAGE_MASK=4095 \
  -DsceKernelQueryMemoryProtection=cap3_host_qmp \
  -DsceKernelVirtualQuery=cap3_host_vq \
  -Dmprotect=cap3_host_mprotect \
  -o build/host/CodePatch.host.o src/CodePatch.cpp
```

Mock model: page table (page base -> prot); `cap3_host_mprotect` mutates it,
`cap3_host_qmp`/`cap3_host_vq` report it (with fault injection: one-shot
query failure, stale/frozen view, one-shot + indexed + persistent lies,
vq writability disagreement, mprotect rc failure). Log sink captures every
`[CAP3]` line; ordered assertions verify the DESIGN.md step sequence.

### Host-test limitation recorded honestly

`kekcall_check()` contains a raw x86 `syscall`. A first attempt to execute
it live on the host (fork-guarded) **wedged a child in uninterruptible
exit under Rosetta** — macOS's invalid-syscall gate does not return cleanly
and would prove nothing about the PS5 sysent anyway. The live liveness test
was removed; instead:

* the test never reaches the CHECK (kFlagSkipLiveness / flags=0; module-slot
  tests use only pre-liveness refusal paths);
* `tests/run_host_test.sh` statically disassembles `kekcall_check` in BOTH
  the host object AND the PS5 object and requires the exact CHECK sub-op
  immediate `0xFFFFFFFF00000027` + `syscall` instruction (both OK);
* the live NOT-ALIVE -> `kLivenessFailed` abort stays console-rung material.

### Result

```
[HOST] SUMMARY: checks=98 failures=0 -> PASS
[run] STATIC-OK [host] kekcall_check encodes CHECK sub-op 0xFFFFFFFF00000027 + syscall
[run] STATIC-OK [ps5]  kekcall_check encodes CHECK sub-op 0xFFFFFFFF00000027 + syscall
[run] HOST TEST: PASS
```

Full log: `build/host/run-output.txt` (100 PASS lines incl. run summary).
Coverage: arg validation; pre-flight query failure; mixed-protection
refusal; happy patch (log order BEGIN < original-hex < OPEN <
WRITE+READBACK < BARRIER < PASS; exactly one span-level mprotect); unpatch
(restoration proven, backup cleared); NOT_PATCHED_STATE guard;
ALREADY_APPLIED (no mprotect); cross-page 2-page span (per-page
RESTORE-PROT); OPEN mprotect rc failure (nothing modified, no write);
VERIFY-OPEN failure via stale qmp -> REVERT-ONLY (prot + bytes restored);
VERIFY-OPEN failure via vq disagreement (conservative); verdict-7c retry
(one-shot lie -> exactly 2 attempts -> PASS; persistent lie -> exactly 2
attempts -> kRestoreVerifyFailed + STATE DIRTY + backup stays valid);
terminal restore mprotect failure (no retry, honest dirty state: page stays
open); PatchScope RAII (apply, auto-revert, failed constructor no-op
destructor, manual revert, SCOPE-LEAK without blind retry); backup
validation; module-slot refusals.

## 5. Bugs found by the host test and FIXED in the source

The Apple-clang syntax check JJ ran could not catch these; both are
protocol-logic bugs that would have shipped to the console:

1. **`unpatch_code_ex` destroyed the recorded original protection.**
   `preflight_query_pages()` wrote the CURRENT protections into
   `span->orig_prot`; the unpatch path called it for observation, so the
   restore values silently became the open (RWX) protections — every
   unpatch would have left game text pages RWX, with a "restoration
   proven" line proving the wrong value. Fix: the helper now writes to a
   caller-supplied `queried_prot[]` array; the patch path passes
   `span.orig_prot` (restore values), the unpatch path passes scratch
   storage. (`src/CodePatch.cpp`, comment "HOST-TEST FIX ... 2026-10-13".)

2. **The verdict-7c retry could never succeed.** In the unpatch restore
   loop, a failed attempt left `restore == kRestoreVerifyFailed`; the
   `restore != kOk` terminal check then broke out of the retry even when
   the retry had PROVEN restoration (re-query == original). Fix: `restore`
   is reset to `kOk` at the top of each attempt; rc failures and query
   failures remain terminal as designed.

Plus the compile fix in section 2 (`MAP_ANON`). No other source changes;
provenance of the original JJ implementation otherwise preserved.

## 6. Deliverables

| File | What |
|---|---|
| `build/ps5-obj/CodePatch.o`, `build/ps5-obj/PatchDiagnostic.o` | PS5-target objects (THIS receipt, hashes above) |
| `build/cap03-diagnostic.elf` | linked pldmgr-format payload (not uploaded) |
| `tests/host_test_protocol.cpp` + `tests/run_host_test.sh` | host test + reproducible run script |
| `build/host/run-output.txt`, `build/host/kekcall_check.{host,ps5}.dis.txt` | test log + disassembly evidence |
| `src/PatchDiagnostic.cpp`, `src/CodePatch.cpp` | updated sources (fixes above) |
| `BUILD-RECEIPT.md` | this file |

**Foreign-artifact note (recorded, untouched):** two ELF relocatables
`./CodePatch.o` (39,144 B, sha256 `f9ec65a3...c96c0e6`) and
`./PatchDiagnostic.o` (11,392 B, sha256 `df59094f...4f004`) appeared at the
cap03 ROOT during this session (mtimes 15:10 local), NOT produced by any
command in this receipt (every command here writes under `build/`; sizes and
hashes differ from the objects above — different flags, likely a concurrent
worker's build). Per the workspace rule on divergent/unowned content they
were left exactly as found; the authoritative cap03-KD artifacts are only
the ones in `build/` with the hashes in sections 2-3.

## 7. What is and is not verified

**Verified (Mac, real toolchain):** the implementation compiles clean for
`x86_64-sie-ps5` with the pinned SDK; links into a payload ELF of the same
format class as payloads this project has already run via pldmgr; the
protocol logic (order, rc-checking, revert-on-failure, restoration proof,
retry budget, RAII) passes 98/98 assertions against a fault-injecting mock;
the kekcall CHECK machinery is correctly encoded in both objects.

**NOT verified (needs the console rungs, TEST-PLAN.md):** actual kstuff
engagement (U1), mprotect skip on real SELF text (U2), PS5 query-API
fidelity + VqInfo layout (U3 — layout is PS4-derived public knowledge),
cross-core visibility (U4), restoration stick-over-time (U5), pause-window
behavior (U6), and the payload loading/running on the console at all. No
>8-quality or console-readiness claim is made from this receipt.
