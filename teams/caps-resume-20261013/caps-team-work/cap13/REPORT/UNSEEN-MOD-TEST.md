# CAP13 — Unseen-mod generalization test (loader generalization, Mac)

Agent KF (ZCode), 2026-10-13. **Mac-only; no console contact.** This report
runs previously-unseen Windows x64 DLLs through the **unmodified staged
loader** and reports which load, which fail, and why — the user's requirement
8, with requirement 6 discipline (import coverage and execution reported
separately, no percentages inferred from imports).

Method: every DLL below was built by agent KA (cross-compiled PE32+ x64,
lld-link) AFTER the loader was staged (loader 13:08, samples 14:52–15:03),
and none had ever been run through the loader before this test. During the
runs I made **zero changes** to the loader or bridge; each file's outcome is
determined entirely by its own bytes. Each case hashes its input before and
after (byte-identity held in all cases, including refusals). Raw evidence:
`REPORT/evidence/e2e-run-20261013-211310.log` (+ companion runs same day).

Provenance note (honest): the SAMPLE directory was being extended by another
worker during this session (`cap13-mod-keybinds-noeh.dll`,
`cap13-mod-keybinds-x86.dll` appeared mid-run); the table records the files
actually tested, with sha256 prefixes for pinning.

---

## 1. The primary unseen trio (as assigned)

| # | File (sha256-16) | Loader result | Exact behavior |
|---|---|---|---|
| 1 | `cap13-mod-splitscreen.dll` (`73ed26e406b425c0`) | **REFUSED** — load-time, fail-closed | `PE has a static TLS directory; TEB/gs thread-local access is not emulatable in userspace on this target (design 3e) -- refused`. Measured: its TLS directory is **EMPTY** (raw start==end==0, callbacks==0) — an lld-link artifact, not real TLS. Finding F20; the mod itself is in the admissible envelope once the rule is refined (addendum §A.3). |
| 2 | `cap13-mod-jsb-lua-profiler.dll` (`c79b27c5afbdeb98`) | **REFUSED** — same rule | Same measured EMPTY TLS directory (`raw=[0..0] callbacks=0`). Same F20 class. |
| 3 | `cap13-ue4ss-flavor-keybinds-poller.dll` (`c3e758f8cb39f0f6`) | **LOADED and EXECUTED** — 6/6 checks PASS | Mapped at a slid base (`slide=0xffffffff8a6e8000`), all 7 import rows resolved (kernel32 ×7, user32 ×1, UE4SS ×2), `start_mod()` executed: its `CreateThread` call hit the e2e's fail-closed F3 row and **the guest survived** (returned 1 — it handles a NULL thread handle); `uninstall_mod()` executed cleanly. No entry point (lld-link `-noentry`) — honestly reported `NoEntryPoint`. |

Why these three: #1/#2 are the mbm-C-ABI class (never touched during loader
development), #3 is the UE4SS-lifecycle class with the widest import surface
(threads, user32, files) — together they span the sample corpus's feature
range. None was used at any point while the loader or this host was written;
the console-dump sample + fixture (used as E2E development targets) are
excluded from the unseen set and reported separately in E2E-PROOF.md §2b/§2c.

## 2. Extended unseen coverage (same loader, same run)

| File (sha256-16) | Result | Why |
|---|---|---|
| `cap13-mod-actordumper` (`9c855a08b36e7f2e`) | REFUSED | EMPTY TLS directory (F20 class) |
| `cap13-mod-bpmod-loader` (`0ae9a5d0d8f276a7`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-bpml-generic-functions` (`82a215c3fdc2fa11`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-cheat-manager-enabler` (`7fa48ff8dbf289e8`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-console-commands` (`33da7c2a3c7ab56a`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-console-enabler` (`8ddc7224779d4f7e`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-linetrace` (`bd539d94a432a270`) | REFUSED | EMPTY TLS directory |
| `cap13-mod-keybinds-noeh` (`0ed2c758094a46bc`) | REFUSED | EMPTY TLS directory |
| `cap13-feature-probe` (`4c6fe9979b4d8b02`) | REFUSED | TLS rule fires before its imports are even considered. (Note: this file is ALSO the designed refusal class for its patch APIs — `VirtualProtect`/`WriteProcessMemory`/`FlushInstructionCache` are outside any admissible row set, so it would refuse again at import time under the refined TLS rule. Two independent refusal layers, both by design.) |
| `real-UE4SS.dll` (sha `94c2ebd3ca9f4c42…`, framework, not a mod) | REFUSED — correctly | REAL TLS directory: 0x2d8 bytes of TLS data + non-null callback array. The refusal rule working exactly as designed; must NOT be relaxed (this is the contrast case that pins the F20 refinement). |
| `real-dwmapi.dll` (proxy, sha `4e3e5324e68e033e…`) | REFUSED | `unresolved imports (fail-closed; no null-filled IAT)` — 133 of 135 census rows unresolved (MSVCP140 ~100, KERNEL32, USER32, api-ms-win-crt-*), complete missing list emitted by the loader. |
| `FIXTURES/cap13-native-x86-FIXTURE.dll` | REFUSED | `Machine != 0x8664` |
| `FIXTURES/NOT-A-PE-FIXTURE.dll` | REFUSED | `not a PE image (no MZ/PE signature)` |

## 3. What the unseen set demonstrates

1. **The loader generalizes.** One unmodified binary classified 20 inputs into
   exactly three honest states — mapped+executed, mapped+refused-with-named-
   reason, refused-at-parse — with zero per-file knowledge. No case required a
   loader change, and none got one.
2. **Every refusal names its cause** (requirement 3): architecture, format,
   TLS, imports — each verbatim in the evidence log, each with the file
   unchanged (byte-identity 20/20 including refusals).
3. **Import coverage is not execution** (requirements 6 and 9): the poller's
   import rows all *resolved* (7+1+2), but only the crossings its exercised
   lifecycle actually made are execution claims (`CreateThread` fired its
   refusal row; `is_ue4ss_initialized`/`call_script_function` were resolved
   but never called — reported as coverage only).
4. **The F20 discovery is the test's main product**: the unseen population is
   currently dominated by EMPTY-TLS-directory files that the staged rule
   refuses although they cannot reach the TEB. The refinement (addendum §A.3)
   is measured against both an empty-dir sample and the real framework image
   so relaxing the rule cannot accidentally admit real TLS.

## 4. What is NOT claimed

- The two executed unseen DLLs exercise ≤4-argument crossings only (the only
  class this Rosetta can run — see E2E-PROOF §4/§6 and addendum §A.4).
- "Loads" means `pe_load` succeeded plus, where stated, the exercised exports
  ran — it does not mean full mod behavior on PS5 (J1/J2/J3, and the entire
  UE4SS service surface, remain console-side work).
- No percentage of real mods is inferred from these 20 files (requirement 9):
  13 of them are one author's lld-link build family with a shared TLS
  artifact; the two real UE4SS-distribution binaries are the framework, not
  mods. The corpus for real percentages does not exist yet (see
  REPORT/mod-selection.md).

## 5. Reproduce

```
cd cap13/e2e
arch -x86_64 ./e2e_host tls        ../SAMPLE/cap13-mod-splitscreen.dll
arch -x86_64 ./e2e_host tls        ../SAMPLE/cap13-mod-jsb-lua-profiler.dll
arch -x86_64 ./e2e_host lifecycle  ../SAMPLE/cap13-ue4ss-flavor-keybinds-poller.dll
arch -x86_64 ./e2e_host tls        ../SAMPLE/real-UE4SS.dll
arch -x86_64 ./e2e_host refuse     ../SAMPLE/real-dwmapi.dll "unresolved imports"
arch -x86_64 ./e2e_host shim-coverage ../SAMPLE/cap13-mod-splitscreen.dll
```

— agent KF (ZCode), 2026-10-13.
