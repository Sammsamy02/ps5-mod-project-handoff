# Native downloaded INI preparation implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development. User-authorized local experiments use focused checks; the already approved autonomous execution does not wait for another design confirmation.

**Goal:** Connect an unchanged downloaded Engine.ini archive to the existing automatic Unreal PS5 config converter inside the native application's private job workflow.

**Architecture:** Preserve the original download and reuse archive intake, basename selection and journaled materialization. The shared config v2 job derives a unique project/platform target, mount and seed from the captured PAK. Its result uses the existing private output owner and stays unpublished.

**Tech stack:** C11/C++20, existing libarchive profile, curl 8.18 with pinned Mbed TLS 3.6.7, separate same-pin SHA-1 provider, existing captured v11 None PAK.

**Spec:** `outputs/ps5-mod-bridge/EXPERIMENTS.md` and the user's automatic PS5-only mod workflow. This plan advances one source class within that larger active goal.

## Global constraints

- No console contact, account access, upload, execution or game/system writes during the user's offline period.
- No stopped placement, memory-read or dependency-upgrade actions are retried.
- No AI or mod-specific script at installation time; project paths derive from verified captured entries.
- Retain the original archive. Refuse ambiguous selection, extra regular members and source sections outside the explicit SystemSettings policy.
- Preserve old config structs, entrypoints, job create/profile structs and analysis-only behavior.
- Preparation, output hash and native compilation do not establish runtime, CVar or installation support.

## Review focus

- A mixed archive must report extra regular members and acquire no output.
- A late stop during allocator cleanup must release every allocation and publish no completion.
- An ambiguous project/platform target must not be guessed from the Nexus domain.
- A section outside the selected scalar subset must not be silently dropped.
- Failed output/member ownership must remain private and recoverable; no substituted inode is unlinked.

### Task 1: Add config job operation gates

**Files:** `native-host/format/config_file_job.c/.h`, new `config_file_job_gate_tests/` and `config_file_job_gate_verify.py`.

**Interface:** Add `pm_config_file_job_gated_v1(params_v1*, result_v1*, mb_pak_prepare_gate_v1, void*)` and matching v2. Append STOPPED=14; keep layouts and ungated wrappers. Gate callbacks and bounded phase/row work, forward the same latch to the writer, clean up unconditionally and gate final completion after release.

- [x] Freeze old implementation and run a meaningful ignored-gate RED.
- [x] Implement the additive entrypoints and ownership contract.
- [x] Verify real automatic-v2 conversion, pre-I/O stop, partial-write stop and late-cleanup expiry with borrowed FDs retained.

### Task 2: Compose the resident archive/config route

**Files:** `native-host/format/private_pak_job.hpp/.cpp`, new `private_ini_job.cpp`, `jobs.cpp/.h`, `build.py`.

**Interface:** Add `PrivatePakJob::prepare_config(source)` and `kind=download_prepare_ini` with the existing five source/idempotency fields. Reuse the configured captured base and private output owner. Scan and select a unique Engine.ini; count/refuse other regular members before materialization. Pin the selected journal member, require all source headers to be SystemSettings, then call gated v2 with platform PS5 and no supplied target/mount/seed.

The same increment adds generic `kind=download_prepare`: use retained filename counts as a routing hint for the strict PAK or INI handler, followed by that core's independent original-archive validation. Unknown/multiple candidates remain additional_action_required with no output; no automatic component-only policy.

- [x] Run actual prior app with the new intent to prove the missing route.
- [x] Connect scan → strict scope → materialize → member identity/hash → shared v2 → private finalizer.
- [x] Preserve old PAK routes, cleanup journals and public secret/path boundaries.
- [x] Include config sources under their verified compile contexts; retain one SHA-1 provider and one transport SHA-256/zeroize owner.

### Task 3: Verify the actual download-to-preparation workflow

**Files:** New `native-host/tests/nexus_config_jobs_verify.py`, focused checkpoint and private-preparation API notes.

- [x] Build configured host and run owned HTTPS login/resolution/download jobs using the unchanged 582-byte Engine RAR and mixed performance ZIP.
- [x] Independently decode the completed PAK and compare all 4,699 paths/payloads with the retained config oracle; preserve the original source bytes and verify strict mixed-archive refusal, wrong-game unresolved, idempotency, cleanup and secret boundaries.
- [x] Verify the same unchanged RAR through generic automatic dispatch, and an unknown DLL archive acquiring no output.
- [x] Build the corresponding native artifact and inspect its dynamic layout/header/symbol closure; do not execute it.
- [x] Save verified scope and outstanding limitations, preserve the paused baseline and commit only this experiment's files.

The full goal remains active after this bounded result: UE4SS engine/game-thread connection, native DLL semantics, prerequisites, on-console discovery/publication and the complete Nexus UI are separate unfinished work.
