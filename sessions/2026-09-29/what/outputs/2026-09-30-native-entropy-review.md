# Native raw entropy checkpoint review — 2026-09-30

**Verdict: ready for the narrowly scoped native API observation checkpoint. No remaining product blocker found in the final reviewed source and artifacts.** This is not completion of the PS5-only Nexus manager, entropy-quality validation, DRBG integration, or trusted HTTPS. `installation_supported` remains false.

The review covered working-tree changes based on HEAD `6ad311cdbf61a2a56d149da93ba2fb3f52cef0b5`: `native-host/entropy.c/.h`, startup integration, HTTP metadata and display, build provenance, focused fixtures, optimized native output, and the separate bounded Mbed TLS feasibility report and ignored artifacts. The checkout review was read-only except for this report. No PS5 ELF was executed, uploaded, or dispatched; no console, account, Nexus, or game action was performed. The actual browser fixture used only an owned desktop host on an ephemeral loopback port.

## Product findings

- `entropy.c:31` performs one native `sysctl` attempt for a fixed 32-byte caller-owned buffer. It clears errno after the first clock read and captures return code, errno, and the complete `size_t` result before cleanup or the second clock call. Classification gives API return failure precedence, then conservative nonzero errno, zero progress, short, oversized, and exact result. There is no retry, fallback, byte encoding, hash, pointer output, or TLS dependency.
- The complete 32-byte capacity is wiped with volatile writes on every returned path, independently of the untrusted returned length. Native disassembly of both profile objects and final ELFs retains one API call site and the fixed-capacity wipe before every return. The startup caller owns distinct valid result and buffer objects; cached HTTP state contains metadata only.
- `/report` preserves full-width length digits, including `SIZE_MAX`, and desktop responses expose unavailable status, call count zero, and null API/timing scalars. Repeated report serialization does not repeat the observation. The added JSON fits the bounded response buffers and preserves the existing request limits and shutdown gate.
- Checked nanosecond timestamps reject invalid or backward values and floor the duration after subtraction. The README correctly discloses that the synchronous entropy call precedes the host lifetime timer, has no hard deadline, and that a missing report cannot identify the startup operation that failed or blocked.

**Resolved review finding:** the initial page parsed JSON into JavaScript numbers and reserialized it, displaying wire value `18446744073709551615` as `18446744073709552000`. The final page displays exact response text through `textContent`; its CSP hash was updated. An independent actual Chrome run now checks the exact digits, CSP, keyboard shutdown, and actual desktop exit and finishes with exit 0.

There are no outstanding critical, important, or minor product issues identified for this checkpoint.

## Independent verification

- Final-source entropy fixtures: **8 tests pass** normally and **8 tests pass** under ASan/UBSan. They compile the actual C observer and HTTP serializer, inject only syscall/clock boundaries, and check classification, no retry, saved errno/length, canaries, complete wipes, timing, `SIZE_MAX`, desktop null fields, field allowlisting, and cached reports.
- Actual bundled Node/Playwright/Chrome browser fixture: **exit 0**. The system Node invocation initially lacked Playwright on its module path; using the existing bundled runtime resolved that environment issue without installing dependencies or changing product code.
- All three final build identities were independently recomputed from all **15 source inputs** and profile metadata. Artifact size/hash and native ELF layout match the manifests. Both native profiles retain `BIND_NOW` and **121 undefined imports**; comparison with the previous reviewed core artifacts shows exactly one addition, `sysctl`, and no removal. The new eager import still requires separate native runtime evidence.

| Profile | Build ID | Artifact bytes | SHA-256 |
| --- | --- | ---: | --- |
| Development LAN | `799f24791574def26a3a696f9dbb18d5c36ccaeb9bb2115a7453092042d52d96` | 1,490,712 | `7673fa5c80a997549954cbd1808146fe1ff7a035234a7eb3e1011938c127dd65` |
| Loopback | `4f1a38ccba4523fd94ba31af2766e3a7f87b49a9b059c138efef9634126fb222` | 1,490,712 | `5c52461fa08f2585b9ad752e6b2d135a33e6bdd700c4655914041ebca1d0eb05` |
| Desktop fixture | `fe8e7ce202917a729ed9988563656f5ddd0b27fd2fe01511eaebb0f7bfaf7cee` | 272,208 | `2a56ec200cfc256056da4adc56b64d40e0b341af061dcf8943ddac8454d6c24c` |

## Separate feasibility audit

The two pinned source archive hashes match the report. Each target/host copy matches all **948 Mbed TLS archive files**, with only the stated default-source configuration switch changed, and all **1,091 framework archive files**. The three target archive sizes/hashes and research ELF size/hash match the report.

Effective configuration and retained preprocessing disable default registrations while leaving the upstream platform poll implementation compiled. The target archive contains an upstream `entropy_poll.o` with an undefined `sysctl`; the linked probe map does not extract it. The separate no-platform object is empty. The owned adapter has the sole explicit source read, and the research ELF has the stated FreeBSD x86-64 ET_DYN format, SDK library dependencies, and `BIND_NOW`. These are link observations, not target resolution or execution evidence.

The initial research fixture failed to assert zero counters immediately after initialization and allowed either one or two default seed polls. Those nonblocking evidence gaps were corrected. A fresh independent warnings-as-errors compile/run of the final fixture exits 0 and asserts zero counters before source registration, one read on failed seeding, and exactly one read on successful default seeding.

The callback requests **64 bytes per callback**, with a 128-byte supplied capacity and a 64-byte strong-source threshold. That is not a lifetime budget. A separate temporary deterministic fixture independently confirms that an explicit entropy request followed by default DRBG seeding consumes **two reads**. The research callback accepts `rc == 0` with nonzero errno and restores the caller's previous errno; the product raw observer deliberately uses a more conservative errno-zero checkpoint policy. The report correctly distinguishes these semantics. Neither the deterministic fixture nor the source's strong designation establishes entropy quality.

## Behaviors considered and set aside

- Native import resolution, successful 32-byte or 64-byte native responses, and target entry/exit: outside this read-only desktop review; the newly added eager import has no runtime evidence from earlier core runs.
- Entropy quality, DRBG suitability, certificate/hostname verification, and trusted Nexus HTTPS: unsupported by API-result and deterministic fake-source evidence; no TLS implementation is included in this checkpoint.
- A shared lifetime read budget across raw draws, DRBG seed/reseed, and other callers: future integration work; the research bound is per callback/request.
- A syscall hard deadline or wiping after a nonreturning call: unavailable in the synchronous design and explicitly disclosed; returned-path cleanup was verified.
- Upstream Mbed TLS internal digest-error cleanup paths: not injected or repaired here; the report explicitly notes the local-buffer cleanup bypass, and Mbed TLS is not linked into the product host.
- Existing LAN-client trust and unauthenticated arbitrary-client shutdown behavior: unchanged baseline contract, already documented; this review checked that the entropy addition preserves the existing gate.
- Account access, downloads, archive handling, DDS writing/conversion, transactions, installation, and game acceptance: outside the raw-observation checkpoint; the full manager remains unfinished and noninstallable.

**Ready to merge this scoped checkpoint: yes.** The corrected product matches the one-attempt, metadata-only, fixed-wipe contract with fresh fixture, browser, provenance, import, and optimized-code evidence. Any later native validation must use these exact artifact identities and record its result separately.

## Post-run record closure

This addition inspects the controller's saved records after its native validation; it is **record inspection, not an independent device observation**. The earlier no-execution statements describe this reviewer's work and the prelaunch review. This closure performed no tests, builds, device/network operations, product/SDK changes, index operations, or commits.

All **27 entries** in `outputs/native-entropy-artifacts/checksums.json` match, with exact package-file coverage excluding the checksum file itself. Both copied ELF digests, profile/build identities, and all 15 current source inputs still match the reviewed manifests. The six copied license notices and reference record are byte-identical to the historical core package, and the three recorded LLVM notice digests match; external license provenance was not re-fetched. The historical core checksum file matches HEAD and all **22 historical entries** remain unchanged and valid.

Public receipt sections match their saved controller records; native report/version/core copies match the saved files. They consistently identify development build `799f24791574def26a3a696f9dbb18d5c36ccaeb9bb2115a7453092042d52d96`, ELF `7673fa5c80a997549954cbd1808146fe1ff7a035234a7eb3e1011938c127dd65`, and PID 300. The recorded raw API result is one request for 32 bytes, return 0, errno 0, exact length 32, floored elapsed 0 ms, and `hard_deadline_enforced:false`; seven core checks pass and repeated reports match. The stored ELF read-back matches all 1,490,712 bytes and the reviewed digest. The unique payload name is absent from saved preflight metadata, and the receipt records one manager transport dispatch followed by two refused readiness reads and then the matching native report.

Shutdown records HTTP 200, endpoint closure, and PID 300 absent from the saved manager snapshot; FTP PID 267 remains present and its welcome record remains available. `waitpid_exit_status_observed` is explicitly false. Build-time manifests remain separate with `console_executed:false`, while the later console receipt carries the recorded run. Canonical context, checkpoint, roadmap, and README preserve native-versus-desktop and historical-review distinctions; the two noted documentation nits were corrected.

No inconsistent 64-byte proof, entropy-quality, DRBG/TLS, native exit-status, installation, or game-capability claim was found in this closure. The 64-byte feasibility remains unexecuted; subsequent DRBG and worker/resolver material retains proposal or declaration/link scope. **No remaining evidence/package blocker found; the scoped checkpoint is ready for commit.**
