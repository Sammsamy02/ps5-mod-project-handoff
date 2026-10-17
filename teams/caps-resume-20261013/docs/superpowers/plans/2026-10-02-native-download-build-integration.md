# Native Download Build Integration Plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans. The user has already authorized autonomous local implementation and focused experimental checks.

**Goal:** Build the existing native host with the callable archive downloader and give its real entry point ownership of the matching TLS runtime.

**Architecture:** Keep the legacy checkpoint build as the default. An explicit download dependency profile selects one matching curl/MbedTLS configuration, excludes the incompatible legacy DRBG object, and starts/closes the serialized download runtime in `host.c`. This step does not enable account, job, install, or runtime-mod capabilities.

**Tech Stack:** C11/C++20, Python build wrapper, pinned SDK v0.43/LLVM 21.1.8, curl 8.18 with the existing init-failure backport, MbedTLS 3.6.7.

**Spec:** `docs/superpowers/specs/generic-title-adapter-ui-contract.md`; callable transport contract in `outputs/ps5-mod-bridge/native-host/download/curl_download.h`.

## Global Constraints

- Local builds and owned host fixtures only; no console contact or promotion in this step.
- Preserve paused title-catalog work, user files, and retained dependency inputs.
- Never link two incompatible unnamespaced MbedTLS profiles or old experiment owner objects.
- Reuse the curl 8.18 backport; do not retry the stopped curl 8.22 upgrade or other stopped actions.
- Account/jobs/download/install capabilities remain unavailable until their actual dispatcher is wired and verified.

## Review Focus

- Wrong dependency configuration must fail before compilation rather than silently mix profiles.
- The real PS5 entropy adapter must accept only exactly 32 returned bytes with zero return and errno.
- Entropy failure must prevent the host listener and remain terminal for its owner.
- Every returned host exit after startup must close the runtime exactly once.
- Native header and symbol evidence must distinguish link success from console execution.

### Task 1: Checked PS5 entropy source

**Files:** `native-host/download/ps5_entropy.c/.h`, focused private-boundary tests and verifier.

**Interface:** `int mb_curl_ps5_entropy_read32_v1(void *, uint8_t[32])`; null context, one checked `sysctl`, full output wipe on refusal, no retries or local budget.

- [x] Run a failing test before adding the adapter.
- [x] Implement and test exact success, return/errno/length refusals, partial writes, output canaries, owner quota, and terminal failure.

### Task 2: Actual build composition

**Files:** `native-host/download/dependency_build.py`, `native-host/build.py`, `native-host/tests/test_build.py`.

**Interface:** Explicit `--download-profile` and `--curl-source` select a retained matching dependency composition. The helper returns compile inputs, ordered link archives, identity and attribution paths. Legacy builds retain their existing CLI and checkpoint behavior.

- [x] Add a failing CLI/composition test: an incomplete transport selection is rejected before output writes.
- [x] Prepare a fresh copied curl archive with the existing two-TU patch; retain source/profile/archive identities.
- [x] Compile downloader, owner, checked source and sink in the actual app build; exclude `drbg.c` for this profile.
- [x] Link a local PS5 ELF and inspect actual include, import and symbol closure.

### Task 3: Entry-point ownership

**Files:** `native-host/host.c`, `native-host/http.c/.h`, focused startup verifier.

**Interface:** `MB_HOST_DOWNLOAD` selects runtime start before listening, runtime close on returned exits, truthful legacy checkpoint `not_selected`, and scalar transport metadata in `/report`.

- [x] Add a failing actual-host startup/shutdown test.
- [x] Implement the entry-point lifecycle with the checked PS5 callback or real host `getentropy` fixture callback.
- [x] Verify normal startup/shutdown and entropy refusal before listener; keep all live service capability flags false.

### Task 4: Focused review and record

- [x] Review the integrated diff for composition and lifetime faults; fix actionable findings.
- [x] Run the focused build/startup checks after the final edit.
- [x] Commit only this work and record compile/link evidence without claiming native execution or universal mod support.

## Observed result

Final actual CLI builds are retained under `.superpowers/native-dependency-spike/native-download-host-integration-1/{host-3,native-3}`. The PS5 ELF is 1,619,112 bytes, SHA-256 `6e07c24269abcdd8c3fb55e8caba6e50b14aec46c4929720cac6719510754b32`; all four transport APIs are present, no legacy checkpoint symbol is linked, and no host header appears in the native app include closure. `final-startup/summary.json` records nine passing checks with real host runtime startup and four private lifecycle controls. The final build suite passed 11 tests; the preserved legacy host suite passed 16 tests and checked PS5 adapter controls passed 16. The first host link failed on GNU `-Map`; its diagnostic is retained, and the final host build uses Apple `-map`. Focused review found and resolved a verifier process-cleanup hole and a retained-wrapper path-binding gap. Native execution, live account/jobs, engine bindings and installation remain unfinished.
