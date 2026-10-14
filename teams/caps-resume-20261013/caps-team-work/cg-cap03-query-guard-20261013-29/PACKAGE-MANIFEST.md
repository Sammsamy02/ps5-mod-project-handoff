# cap03 PACKAGE-MANIFEST — corrected #3 diagnostic payload

Packaged: 2026-10-13 20:26 UTC (agent KG, Mac-only packaging pass; no console
contact). Source build + host test: KD, BUILD-RECEIPT.md (2026-10-13).
All SHA256 values below were independently recomputed at packaging time and
match BUILD-RECEIPT.md sections 2-3 exactly.

## 1. Package contents (authoritative artifacts)

| Artifact | Path | Size (bytes) | SHA256 |
|---|---|---|---|
| Payload ELF | `cap03/build/cap03-diagnostic.elf` | 521,192 | `35500cb7b901c283a8b06eae956e8c00cd0092279569ccc886dad3adaee550ae` |
| CodePatch object | `cap03/build/ps5-obj/CodePatch.o` | 24,592 | `7b90aa54aeb7f09b945a5b2f6540a348ea5336f4cbba93ab3094632f2bd70a6a` |
| PatchDiagnostic object | `cap03/build/ps5-obj/PatchDiagnostic.o` | 11,472 | `27a699ffcc56ea3cd7afabe5dc49524c661ba5c91943c9ba94aacbf0dcf49137` |
| Test card (copied alongside the binary) | `cap03/build/SINGLE-TEST-CARD.md` | 5,870 | `09e0f7e252fbd9ee7548d5e5c7f3bae8f616737d798cd9e48422733a46fede92` |

Path correction vs the directive: both objects live under
`cap03/build/ps5-obj/` (`ps5-obj`, one directory); there is no
`build/ps5obj/`. `build/SINGLE-TEST-CARD.md` is a byte-identical copy of
`cap03/SINGLE-TEST-CARD.md` (same sha256), placed alongside the ELF.

Payload ELF format (verified at packaging): ELF 64-bit LSB PIE executable,
x86-64, FreeBSD, dynamically linked, not stripped — same format class as the
console-proven ploader.elf (BUILD-RECEIPT.md section 3).

## 2. Toolchain and compile flags (exact)

Driver (BUILD-RECEIPT.md section 1):
`what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
(target `x86_64-sie-ps5`; driver adds `-isysroot <sdk> -isystem
<sdk>/target/include -L <sdk>/target/lib -fno-stack-protector -fno-plt
-femulated-tls -lc -lkernel_web -lSceLibcInternal -lSceNet`).

Object compile (BUILD-RECEIPT.md section 2, verbatim):

```
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -D_XOPEN_SOURCE=700 -Wall -Isrc -c src/CodePatch.cpp      -o build/ps5-obj/CodePatch.o
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -D_XOPEN_SOURCE=700 -Wall -Isrc -c src/PatchDiagnostic.cpp -o build/ps5-obj/PatchDiagnostic.o
```

Link (BUILD-RECEIPT.md section 3, verbatim):

```
prospero-clang++ -std=c++23 -Os -fPIC -ffunction-sections -fdata-sections \
    -o build/cap03-diagnostic.elf build/ps5-obj/CodePatch.o build/ps5-obj/PatchDiagnostic.o
```

Both objects compiled zero errors / zero warnings; link clean first try.
NEEDED set: `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`
(same as ploader.elf); SCE imports `sceKernelQueryMemoryProtection`,
`sceKernelVirtualQuery` verified exported by the pinned `libkernel_web.so`.

## 3. Source revision (what is the corrected version)

Sources packaged (hashes recomputed at packaging time):

| Source | Size | SHA256 |
|---|---|---|
| `cap03/src/CodePatch.cpp` (786 lines) | 31,427 B | `4690de63eadc94b994f1cf361a374e3680040a354fd085802076f4faf40b4f07` |
| `cap03/src/PatchDiagnostic.cpp` (282 lines) | 12,244 B | `a4dd0f5d53a598e1d10d22b27996adb6c76131e6edcab5da8316b1e70244819c` |
| `cap03/src/CodePatch.hpp` | 11,406 B | `6f43badea0846dd578ef36f29e8b0a17279b1fafee835539435cea3dbef3cf4d` |

The corrected version of `CodePatch.cpp` is identified by two
`HOST-TEST FIX (cap03 KD, 2026-10-13)` markers (BUILD-RECEIPT.md section 5):

1. **Restore-protection preservation** — `preflight_query_pages()` no longer
   writes into `span->orig_prot`. Comment + new signature:
   `src/CodePatch.cpp:266-285` (marker at :271), caller sites: patch path
   passes `span.orig_prot` at :449, unpatch path passes scratch `cur_prot[]`
   at :558-560. Without this fix every unpatch would have "restored" the open
   RWX protections while claiming proven restoration.
2. **Verdict-7c retry viability** — comment at `src/CodePatch.cpp:611-617`,
   fix `restore = PatchResult::kOk;` at :618 at the top of each retry
   attempt. Without it a proven-successful retry could never clear
   `kRestoreVerifyFailed`.

Compile fix in `src/PatchDiagnostic.cpp:36-44`: `MAP_ANON` fallback
(`#define MAP_ANON 0x1000` + `static_assert` pinning the value, needed under
`-D_XOPEN_SOURCE=700`).

Reproducibility check for the integrator: an object/payload whose sha256
differs from section 1 was NOT built from these sources with these flags —
do not mix.

## 4. Evidence state packaged with the payload

- Host test: 98/98 assertions PASS (`cap03/build/host/run-output.txt`;
  BUILD-RECEIPT.md section 4), including static proof that `kekcall_check`
  encodes CHECK sub-op `0xFFFFFFFF00000027` + `syscall` in BOTH the host and
  PS5 objects (`build/host/kekcall_check.{host,ps5}.dis.txt`).
- NOT verified (console rungs outstanding, TEST-PLAN.md / BUILD-RECEIPT.md
  section 7): kstuff engagement (U1), real SELF-text mprotect skip (U2),
  PS5 query-API fidelity (U3), cross-core visibility (U4), restoration over
  time (U5), pause-window behavior (U6), and loading/running on the console
  at all. No console-readiness claim is made.

## 5. Prior silent console attempt — UNRESOLVED, different compile

The earlier console attempt (`cap03-diag-v0.elf`, 129,344 B,
sha256 `8c462064068284462b84a57757e4d1d41a5155680251d4f788ee799960fac0c3`,
compiled `prospero-clang -Wall -Werror -nostdlib`, CONSOLE-SESSION-20261013.md
section "#3") loaded via pldmgr with "OK" but produced NO output.

**Its cause is UNRESOLVED.** It was a different compile — `-nostdlib` with no
crt initialization, 129,344 bytes, different sha256 — and is NOT the payload
packaged here. The packaged `cap03-diagnostic.elf` (521,192 B, SDK-linked,
crt1.o `_start -> main`) shares none of the v0 link characteristics, so the
v0 silence neither indicts nor validates this package. Candidate causes for
v0 remain as recorded in CONSOLE-SESSION-20261013.md lines 29-33 (broken
_start->main chain without crt init, log path mismatch, early exit).
Root-causing v0 is NOT a prerequisite for testing the packaged payload, but
any v0 retry would be a new ID, new evidence, per the no-blind-retry rules.

## 6. Handling rules for the integrator (unchanged, restated)

- PROPOSAL ONLY: nothing here authorizes a console action. The single-test
  procedure and Phase B gating are in `cap03/build/SINGLE-TEST-CARD.md`.
- Any future executable upload requires decrypt-OFF readback verify (magic +
  full sha256) before launch, one launch per boot, clean-exit discipline
  (PS5 rules 2/4/5; make_fself.py if any modification is ever made).
- Foreign artifacts at the cap03 ROOT (`CodePatch.o` 39,144 B,
  `PatchDiagnostic.o` 11,392 B, `cap03-diag-v0.elf`) are NOT part of this
  package (BUILD-RECEIPT.md section 6 foreign-artifact note) and were left
  untouched.
