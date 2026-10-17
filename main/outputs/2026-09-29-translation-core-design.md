# Universal PS5 mod translation — shared core design

Date: September 29, 2026. Written specification for review. The human's instruction to keep going after the architecture review authorizes continuing that direction into this document. This written specification has not yet received human review; it is not an approved implementation plan.

## 1. Intent and scope

The final product is a console application that connects to Nexus through official authorization, discovers/selects installed games, and automatically translates and installs PC mods for native console games. Required coverage includes assets, pak files, scripts, managed assemblies, native plugins, UE4SS, ME2/me3-style loaders, Bethesda extenders and other runtime families. The user rejects individual-mod fixes and requires optimization. Stellar Blade is the first hardware validation title; neither that title nor a single mod defines the product.

The project is too large for one implementation plan. This specification covers its shared package/dependency representation, capability/evidence model, backend contracts, preparation/cache orchestration and reversible deployment foundation. Runtime implementations, automatic game mapping, asset conversion and Nexus/UI integration require their own specifications and plans. All remain required by the overall goal. Completing this workstream establishes a foundation, not universal translation or game compatibility.

A desktop harness supports development and deterministic tests. The intended shipped user workflow stays on the console. No companion-computer dependency is assumed for users.

## 2. Observable outcomes

The core must ingest a package plus target description and produce:

1. A content/dependency inventory that separates discovered facts from unresolved requirements.
2. A plan naming the backends and validated target capabilities needed for each component.
3. Prepared outputs and evidence only when selected backends actually perform their operations.
4. A compatibility report distinguishing analysis, transformation, deployment and behavioral validation.
5. A deployment/removal journal that can restore project-owned test files without overwriting later user edits.

No component may claim that a package works because its archive was extracted, its DLL was loaded, or its output resembles a target file. The core must represent an unresolved requirement honestly and resume preparation after a capable backend or sufficient input becomes available.

## 3. First workstream deliverables

- A portable C++ core and a desktop command-line harness. C++ allows investigation against the community PS5 SDK while keeping the core free of desktop/console UI dependencies. Availability of required library/compiler features on PS5 remains to be verified before a console-build claim.
- Versioned, machine-readable package, target, capability, mapping-evidence, plan and journal records.
- Archive ingestion and format detection with explicit parser coverage. Use libarchive as the proposed portable archive dependency; enabled formats are recorded from the built library and verified with fixtures. Unsupported archives remain unresolved inputs rather than being treated as empty packages.
- PE/ELF structural inventory and managed-versus-native detection. Parsing is nonexecuting; the harness never loads inspected plugins or runs package installers.
- Backend registration, deterministic selection, dependency planning, cache invalidation and structured failure reporting.
- Scratch-directory preparation/deployment/removal with interruption recovery.
- A console-capability report format and probe design. Console probe implementation and execution are gated by the selected loader's verified interfaces and the human-reviewed plan. No game injection is assumed available.

The first workstream does not substitute stub backends for UE4SS, SKSE, Windows translation or asset conversion. A backend that only reports requirements must identify itself as an analyzer and cannot emit a successful transformation result.

## 4. Core records and invariants

| Record | Required information | Invariant |
| --- | --- | --- |
| Package | Content hash, provider/version metadata if supplied, entries, dependency declarations, installer choices and provenance. | Provider mod IDs are metadata; they do not select special translation patches. |
| Entry | Original path, normalized logical path, content hash/size, structural format facts, dependencies and evidence references. | Detection uses content where possible; extensions alone are provisional hints. |
| Target | Console platform, title/edition/region, update identity, executable and asset-schema fingerprints, engine/runtime facts. | Unknown values remain explicit; a title name is not an exact build fingerprint. |
| Requirement | Named capability, version/ABI/schema constraints, consumer component, discovery method and unresolved inputs. | Dependencies are typed, not merely filenames to copy. |
| Capability | Name/version, execution context, supported operations/constraints, status and evidence reference. | Standalone-host capability cannot imply in-game capability. |
| Mapping | Source/target fingerprints, function/type/member/hook correspondence, derivation, preconditions and validation evidence. | Candidate similarity cannot silently become a validated semantic mapping. |
| Plan | Ordered graph, selected backend versions, inputs/outputs, requirements, conflicts and unresolved nodes. | Every transformed node has an accountable backend and complete cache inputs. |
| Evidence | Method, result, environment/build identities, tested operation/scope, tool/backend version and observed limitations. | Off-console tests cannot establish console gameplay behavior. |
| Journal | Transaction identity, target paths, original/output hashes, backup locations, completed operations and recovery state. | Removal checks current content before replacing it. |

Format facts include PE machine type/imports/relocations/TLS/exception metadata, managed-runtime directory presence, ELF class/machine/segments/relocations, and archive member structure where supported. Detecting one category does not mean all its semantics were recovered. Dynamic Lua imports, computed script calls, dynamically loaded native libraries and unknown installer logic must be represented as unresolved dependencies.

Paths have two representations: original spelling for evidence and a normalized logical path for planning. Preserve case requirements of the target backend. Reject traversal, absolute destination escape, alias collisions and entries whose expansion exceeds declared resource limits. Do not silently flatten folders or merge distinct case-sensitive resources. Preserve symlink metadata for analysis; deployment requires an explicit validated link policy.

## 5. Component boundaries

### Ingestion and inventory

The archive reader streams bytes into content-addressed preparation storage. It enforces entry-count, total-expanded-size and per-entry limits supplied by the job. Limits are configurable and appear in the report; a rejected package reports which limit it exceeded. Hash each input during ingestion. Interpret manifests and installer choices as data; never execute installer scripts during analysis.

Format parsers return structural facts and requirements with provenance. Parser errors identify the entry and format problem. A mixed asset/Lua/DLL package stays one graph with several component types, rather than being assigned a single compatibility label from its first file.

### Requirement graph and planner

Edges distinguish required runtime, native import, resource reference, ordering and installation conflict. Collapse permitted mutual resource/runtime-reference cycles into groups when the backend defines how to handle them; ordinary cyclic references must not be confused with impossible initialization order. Strict load/initialization-order cycles remain unresolved unless a backend supplies validated joint-initialization semantics. Report incompatible version constraints with the involved nodes, rather than breaking constraints arbitrarily. Optional components are selected only through recorded installer/user choices.

The planner matches requirements against target capabilities and backend declarations. It produces both runnable steps and unresolved nodes. It must not deploy a partial package as though all selected required components succeeded. Re-running with new evidence or backends updates affected nodes and invalidates dependent outputs.

### Backend contract

Each backend declares a stable ID/version, input formats, output types, platform/engine/runtime constraints, required capabilities, and whether it analyzes, transforms or supplies runtime services. It exposes operations for inventory, applicability assessment, preparation and validation through a versioned contract.

Use a C-compatible boundary with opaque handles and explicit ownership for future native host extensions; do not expose STL objects or borrowed game pointers across arbitrary compiler/runtime boundaries. Portable adapters may be linked directly in the first harness. Binary plugin loading is not required to establish the contract.

Applicability results contain supported requirements and unresolved requirements separately. Transformation outputs contain hashes, reference/dependency information and validation evidence. Backend errors include an operation, component and missing/failed requirement. Backend selection uses format/capability constraints, not a mod-name lookup table.

### Game mapping evidence

The core stores correspondence graphs but does not invent mappings. Producers may use reflection, metadata, source/target binary matching or runtime observations. Every edge records its source/target build identities and whether it is a candidate, structurally checked or behaviorally validated. A manually supplied experimental mapping is marked manual and excluded from claims of automatic discovery.

Native plugins require more than matching exported function names: direct fields, vtable slots, call-site instructions, allocation/lifetime and thread/lifecycle rules may be requirements. Their absence blocks the relevant transformation/runtime claim. Pointer proxies and instruction rewriting remain possible backend strategies; the core must not prescribe raw pointer passthrough as universally correct.

### Preparation and cache

The cache key includes component bytes, canonically serialized selected options, dependency outputs, target executable/schema identities, backend ID/version/configuration, core/parser version, record-schema version and mapping/evidence versions. Tokens and other account secrets are excluded. Inventories may be produced with incomplete target identity, but target-dependent preparation/cache reuse requires every fingerprint declared by the backend. Evidence revocation, downgrade or scope change invalidates dependent results. A cache hit never upgrades validation status or keeps revoked evidence usable.

Preparation writes to an isolated transaction directory. Outputs are published into the cache only after structural validation and hash completion. Interrupted preparation leaves no ready entry. Changing a mapping, target build or dependency invalidates affected descendants even if the original archive is unchanged.

### Deployment and removal

Deployment first stages complete prepared outputs, checks destination conflicts, records originals, then performs journaled replacements. Same-filesystem atomic rename and synchronization are used only where the platform has validated their behavior. The portable harness exercises a filesystem adapter; it does not establish durability or permission behavior on console mounts.

Immediately before replacement, recheck the destination against the captured original hash and verify path containment through existing parent directories; parent symlinks must not permit a destination escape. If external edits are detected, stop the affected operation, retain its original backup and staged output, and record a conflict. On restart, recovery uses journal state and hashes to distinguish operations that completed, did not complete or require human resolution. Never infer completion from filename existence alone. Removal restores an original only when the current file matches the installed output; otherwise it records a conflict and preserves the current file. Jobs serialize conflicting destinations. Backups/staged outputs remain available until recovery or conflict resolution completes.

Live game directories are not deployment destinations in the first desktop tests. A console deployment backend must supply validated mount/path/write capabilities for the exact target. Game injection and executable module initialization remain separate runtime operations.

## 6. Console capability evidence

Represent capabilities as `unknown`, `unavailable` or `validated`, scoped to an environment and operation. A source recipe is documentary evidence, not a validated console capability.

The initial console report covers standalone payload execution, filesystem read/write in project-owned storage, directory listing, synchronization/rename behavior, DNS/TLS with certificate validation, controller/display access and memory/resource limits. Optional reports separately cover native module initialization/unload, executable memory, game-process access, calling-convention marshalling and game-thread/lifecycle integration.

The exact loader/version and Stellar Blade title/update identity are required before defining a runtime probe that depends on them. The inspected debugger's ELF_RPC path must not be assumed nonexecuting: its source changes the thread instruction pointer. No automatically guessed addresses or undocumented empty-success stubs count as a capability test.

## 7. Compatibility reporting and user-facing meaning

Report per-component stage and evidence, plus a package-level explanation:

- **Analyzed:** inventory exists; unresolved dynamic behavior may remain.
- **Prepared:** actual backend outputs passed stated structural checks.
- **Deployed:** journal and destination hashes confirm installation.
- **Behavior tested:** the intended effect was observed under the recorded game/build/runtime conditions.

Use plain language in the eventual console UI, such as “needs a console UE4SS runtime” or “asset conversion needs source data.” Keep technical evidence available in details. No marketing label such as “universal compatible” is produced from this workstream.

## 8. Verification requirements

Deterministic core tests cover mixed packages, misleading extensions, malformed/truncated PE/ELF metadata, managed/native distinction, nested dependencies, permitted reference cycles versus impossible initialization cycles, missing/ambiguous dynamic dependencies, archive resource limits, path/case collisions, version/order conflicts and unchanged content under different provider IDs.

Cache tests change each dependency, target fingerprint, mapping version and backend version independently and verify invalidation. Interrupted preparation must never become a ready cache entry.

Deployment tests interrupt each journaled operation, fail writes/synchronization, introduce competing transactions and edit installed files before removal. Verify preservation of later user edits and correct recovery classifications. These are consequential invariants, not tests that merely mirror implementation structure.

Architecture tests run unfamiliar package names/IDs through the same backends and verify there is no private mod-specific route. Automatic transpilation/code generation is permitted; manually maintained replacement scripts do not satisfy automatic translation.

Console/game acceptance is separate: a known exact Stellar Blade build, several independently authored packages withheld from backend development, mixed dependencies, observable intended effects, cleanup/restart behavior and measured overhead. Subsequent acceptance spans another title/build and another runtime family, including a native Bethesda plugin case. The full goal remains incomplete while required categories are unresolved.

## 9. Performance and failure behavior

Expensive analysis/conversion occurs during preparation and is cached. Download/archive handling streams with bounded memory. Gameplay runtime backends keep catalog/network work outside gameplay, batch calls on the correct game thread and avoid per-frame full-binary scans. Measure startup/translation time, resident memory and frame CPU cost against an unmodified-game baseline before setting verified budgets.

Errors preserve the original package and completed evidence. Classify input corruption, missing source data, missing backend/capability, mapping ambiguity, target mismatch, conflict, resource exhaustion and platform operation failure. Retry only operations whose journal/backend contract supports it. Do not silently fall back to file copying when translation fails.

## 10. Dependencies and remaining review

Design evidence is consolidated in [the research proposal](ps5-mod-manager-feasibility.md), which links primary sources. Required external inputs for hardware work remain the working loader/version, exact installed game identity and a later user-provided endpoint. Permissioned UE4SS Unreal source access and console runtime interfaces remain unresolved research dependencies; neither is invented here.

Review this written specification before writing the implementation plan. The eventual plan must preserve the distinctions above and name the desktop versus console checks explicitly. The universal product objective stays active throughout the staged work.
