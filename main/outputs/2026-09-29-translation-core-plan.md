# Translation Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Build a tested portable foundation for automatic package analysis, shared backend planning, cache identity and reversible scratch deployment without claiming working console translation.

**Architecture:** Nonexecuting inventory produces a versioned JSON requirement graph. A deterministic planner matches it against capabilities/backend declarations; preparation identity and journaled filesystem operations remain separate from actual runtime/asset backends. No production rule selects a patch by individual mod identity.

**Tech Stack:** C++20, Clang, Make, Python 3 test fixtures, libarchive system library with vendored public headers, nlohmann/json 3.11.3, PicoSHA2 v1.0.1 (commit 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29). Record upstream URLs, versions, licenses and SHA-256 of vendored files. No network access during ordinary builds.

**Spec:** [Shared core design](2026-09-29-translation-core-design.md).

**Execution authorization:** The user requested autonomous progress while asleep. Routine written-review and execution-choice pauses are handled internally for this reversible local work; do not claim the human reviewed nonexistent artifacts. Root implements foundation/integration while isolated subagents implement independent modules and another reviewer checks the combined result. No console contact, app registration or credential handling is authorized by this plan.

## Global Constraints

- Final product remains universal automatic translation across the Nexus catalog; Stellar Blade is the first hardware test only.
- Provider mod IDs are metadata; they do not select special translation patches.
- Inventorying a DLL does not automatically recover its source or every behavior.
- Standalone-host capability cannot imply in-game capability.
- A manually supplied experimental mapping is marked manual and excluded from claims of automatic discovery.
- Unknown target values remain explicit. Target-dependent preparation/cache reuse requires the fingerprints declared by its backend.
- Evidence revocation, downgrade or scope change invalidates dependent results.
- Live game directories are not deployment destinations in the first desktop tests.
- Automatic transpilation/code generation is permitted; manually maintained replacement scripts do not satisfy automatic translation.
- A backend that only reports requirements identifies itself as an analyzer and cannot emit a successful transformation result.

## Review Focus

1. A misleading extension or truncated executable must not become a confidently classified working plugin — Task 2 structural parsing tests.
2. An archive that changes after hashing must not associate new bytes with old provenance — Task 2 coherent-read/change detection test.
3. Two backends with the same version, or revoked evidence, must not reuse one prepared result — Task 3 identity/invalidation tests.
4. Cyclic resource references are distinct from impossible initialization order — Task 3 typed-edge cycle tests.
5. An external destination edit or parent symlink during installation must preserve user data and containment — Task 4 precondition/recovery tests.

## File map

Product files live in `outputs/ps5-mod-bridge/`; build/fixture scratch is ignored. `include/modbridge/core.hpp` holds stable module declarations and `Json` alias. `src/hash.cpp` owns content hashing/canonical identity helpers. `src/inspection.cpp` owns executable/format structure. `src/inventory.cpp` owns bounded ingestion. `src/planner.cpp` owns dependencies/backend applicability; `src/cache.cpp` owns identity/evidence checks. `src/journal.cpp` owns scratch transactions. `src/main.cpp` is the command-line adapter. Tests mirror consequential behaviors, not implementation internals. Root owns shared declarations/build tooling/CLI; subagents own disjoint module and test files.

### Task 1: Foundation and reproducible desktop build

**Files:** Create `Makefile`, `include/modbridge/core.hpp`, `src/hash.cpp`, `tests/test.hpp`, `tests/main.cpp`, `tests/hash_tests.cpp`, `third_party/manifest.json`, license/header files, and `tools/bootstrap_deps.py`.

**Interfaces:**

```cpp
using Json = nlohmann::json;
struct Limits {
  std::uint64_t max_entries = 10000;
  std::uint64_t max_total_bytes = 1073741824;
  std::uint64_t max_entry_bytes = 268435456;
  std::uint64_t max_inspection_bytes = 8388608;
};
std::string sha256_bytes(std::span<const std::uint8_t> bytes);
std::string sha256_file(const std::filesystem::path& path);
std::string canonical_json(const Json& value);
```

- [ ] Write failing `hash_known_vectors` (empty and `abc` SHA-256), `stream_hash_large_file` (agrees with Python hashlib), and `canonical_object_key_order` (same object different key insertion order; array order differs).
- [ ] Run `make test-hash`; confirm missing implementation failure, then implement helpers with streaming PicoSHA2 and deterministic JSON serialization.
- [ ] Vendor dependency bytes/licenses from pinned upstream refs, record hashes, and make bootstrap verify checked-in files. Fetch only explicit pinned URLs; normal builds require no fetch.
- [ ] Run `make test-hash`; require all named tests pass. Build flags: `-std=c++20 -Wall -Wextra -Wpedantic`; keep third-party warnings outside project warning policy.
- [ ] Commit foundation and contracts with assistant author identity.

### Task 2: Bounded mixed-package inventory

**Files:** Create `src/inspection.cpp`, `src/inventory.cpp`, `tests/inventory_tests.cpp`, `tests/make_fixtures.py`.

**Consumes:** Task 1 hash helpers, `Limits`, `Json`.

**Produces:**

```cpp
Json inspect_binary(std::span<const std::uint8_t> bytes, const std::string& path);
Json analyze_package(const std::filesystem::path& input, const Limits& limits);
```

Inventory fields: `schema_version:1`, `parser_version:"0.1.0"`, `package_sha256`, `entries:[{path,sha256,size,kind,facts,requirements}]`, `requirements`, `dependencies`, `issues`, `status:"analyzed"`. Kinds include `native_pe`, `managed_pe`, `elf`, `unreal_pak`, `unreal_asset`, `lua`, `data`, `unknown`; uncertain extension-only classifications expose that uncertainty. Dependencies use `{from,to,type}` with `type` of `reference`, `runtime`, `order` or `conflict`. Optional project-authored `modbridge.json` declares metadata/requirements/dependencies; other installer dialects remain unresolved, not executed.

- [ ] Generate owned fixtures: ZIP/tar mixed assets/Lua/native PE/managed PE/ELF, PE with ordinal/named imports, PE TLS/unwind metadata, ELF relocation headers, malformed/truncated variants. Include misleading `.dll`, duplicate/case-colliding members, absolute/traversal paths, symlinks and resource-limit overflow. Use no game assets or downloaded mods.
- [ ] Write tests `mixed_inventory`, `pe_structure_and_clr`, `elf_bounds`, `extension_is_not_proof`, `limits_and_paths`, `symlink_preserved_not_followed`, `input_provenance_coherent`, and `manifest_dependencies`. Assert format facts and unresolved requirements; assert no `prepared`, `deployed` or gameplay success claim.
- [ ] Run `make test-inventory`; confirm the intended missing-function failure before implementation.
- [ ] Implement libarchive streaming ingestion and nonexecuting bounded parsers. Detect truncation/overflow before reading structures. Stream entire content hashes, but cap in-memory inspection to the declared limit and report incomplete inspection. Preserve paths/references; reject unsafe/colliding logical destinations. Hash/open coherently and detect relevant input mutation rather than hashing one file version and reading another.
- [ ] Run `make test-inventory`; require all checks pass and document actual runtime libarchive version/format support. Commit module/tests.

### Task 3: Typed planning and cache/evidence identity

**Files:** Create `src/planner.cpp`, `src/cache.cpp`, `tests/planner_tests.cpp`.

**Consumes:** Task 1 helpers and Task 2 inventory record; tests may construct owned inventory records directly.

**Produces:**

```cpp
Json build_plan(const Json& inventory, const Json& target, const Json& backends);
std::string cache_key(const Json& context);
bool cache_evidence_usable(const Json& cached, const Json& current);
```

Backend declarations contain `id`, `version`, `mode` (`analyzer`, `transformer`, `runtime`), accepted `kinds`, required `capabilities`, and required `target_fingerprints`. Target capabilities contain ID/version/status/context/evidence. Capability/version constraint grammar for the first build: exact version and `>=` numeric dotted versions; other constraint syntax is explicitly unresolved. JSON records are validated, with errors rather than unchecked type coercion.

- [ ] Write `same_content_different_mod_id`, `mixed_missing_runtime`, `unknown_target`, `analyzer_cannot_transform`, `ambiguous_backends`, `reference_cycle_vs_order_cycle`, `version_constraints`, `cache_every_input`, and `revoked_evidence`. Check two backend IDs both version1 differ; every target/dependency/parser/schema/mapping/options change invalidates; withdrawn/downgraded evidence refuses reuse.
- [ ] Run `make test-planner`; confirm intended missing implementation failure.
- [ ] Implement typed graph processing, deterministic eligible/unresolved backend results, conflict reporting and explicit stages. Do not infer an actual transformation from a declaration. Cache context requires `backend_id`, `backend_version`, `core_parser_version`, `schema_version`, `source_hash`, `options`, `dependency_hashes`, `target_fingerprints`, `mapping_versions`, `evidence`; incomplete required identities fail eligibility.
- [ ] Run `make test-planner`; require all checks pass. Commit module/tests.

### Task 4: Reversible scratch journal

**Files:** Create `src/journal.cpp`, `tests/journal_tests.cpp`.

**Consumes:** Task 1 hashes/JSON. Scope is POSIX desktop project-owned scratch storage; future console platform adapter must be validated separately.

**Produces:**

```cpp
Json deploy_scratch(const std::filesystem::path& root,
                    const std::filesystem::path& prepared,
                    const Json& manifest, int fail_after = -1);
Json recover_scratch(const std::filesystem::path& root);
Json uninstall_scratch(const std::filesystem::path& root,
                       const std::string& transaction_id);
```

Root must contain `.modbridge-scratch` created explicitly by the harness. Manifest: `files:[{source,destination,sha256}]`; paths are relative and hashes required. Journal/backup state lives under `.modbridge/transactions/`. Conflicting jobs use a lock. Refuse missing marker, unsafe paths, source/parent symlinks and hash mismatches. Failpoint is testing-only and recorded as an interrupted operation, not success.

- [ ] Write `restore_original`, `new_file_removal`, `edit_before_removal_preserved`, `destination_precondition`, `symlink_parent_escape`, `unmarked_root_refused`, `conflicting_job_lock`, and `interrupt_each_operation`. Verify recovery never clobbers unexpected current bytes and retains backups/staged outputs for unresolved conflicts.
- [ ] Run `make test-journal`; confirm intended missing implementation failure.
- [ ] Implement durable journal state transitions, staged hashes, original capture/rechecks, containment and removal checks. Use POSIX descriptor-relative/no-follow operations where needed; explicitly document any concurrency/durability limits the implementation cannot guarantee. Never accept a live-console game directory as a test destination.
- [ ] Run `make test-journal`; require all checks pass. Commit module/tests.

### Task 5: CLI, integration and reviewable deliverable

**Files:** Create `src/main.cpp`, `tests/cli_tests.py`, `README.md`, `examples/target-unknown.json`, `examples/backends-unavailable.json`, `include/modbridge/backend.h` and capability-report schema/example.

**Consumes:** All produced interfaces above.

- [ ] Write CLI tests for `analyze INPUT --output REPORT`, `plan INVENTORY --target TARGET --backends BACKENDS`, `cache-key CONTEXT`, `scratch-init ROOT`, `scratch-deploy ROOT PREPARED MANIFEST`, `scratch-recover ROOT`, `scratch-remove ROOT TRANSACTION`. Assert missing/invalid options exit nonzero and preserve structured errors; analyze/plan unknown-target fixture never reports working PS5 mods.
- [ ] Run `make test-cli`; confirm missing executable/interface failures.
- [ ] Implement command adapter and a versioned C-compatible future-backend header with opaque handles/explicit ownership. Add a console-capability record example whose statuses are all unknown; it is not a payload or fabricated test result.
- [ ] Run `make test` and `make sanitize-test` (ASan/UBSan for owned parser/graph/journal tests). Generate a mixed-package report and reversible scratch demonstration using the built binary. Record actual commands/results; do not extrapolate desktop tests to hardware.
- [ ] Independent final code/spec review; fix material issues using fresh targeted regression tests. Repeat only affected checks and the final integrated suite after changes.
- [ ] Commit verified source, merge the isolated local branch into the task checkout, preserve source under outputs, and save a concise progress report linking actual artifacts. No remote publication or PR is requested.

## Self-review and remaining full-goal work

The plan covers the shared-core specification's desktop deliverables, data/evidence invariants, cache and scratch recovery. Backend actual preparation/translation, production Nexus authorization/downloads, console UI/build/probe and automatic runtime/game mapping remain separate required workstreams; no test double counts as implementing them. Third-party header versions and hashes are recorded at bootstrap, while system library version is reported at runtime. Future PS5 builds need their own toolchain/library proof.

The complete universal goal stays active after these tasks. The first hardware inputs still required are loader/version and exact Stellar Blade build. The next backend work must be driven by automatic mapping and whole format/runtime classes, not a private Stellar Blade-mod patch.
