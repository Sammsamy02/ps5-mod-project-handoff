# l23 Runtime and Layout Generator Implementation Plan

> **For agentic workers:** use the user's explicit two-track parallel execution instruction. Work in the existing managed worktree with isolated overlays. No console contact, uploads, commits of Epic source, or live fallback hook implementation.

**Goal:** prepare the minimal l23 PRX and independently generate source-derived UE 5.6.1 layout data with a strict 93-fact calibration.
**Architecture:** retain l22 source/object lineage; copy only changed units and headers into l23 overlays, recompile the complete dependent closure, then package twice. Keep Track B outputs separate and unable to silently promote profile values.
**Tech Stack:** C++20, existing PS5 target compiler/SDK, authenticated saved image, pinned official Epic headers, Python local receipts and host controls.
**Spec:** user's l23 Track A/Track B directive in this chat; l22 harvest PID196 at outputs/2026-10-08-l22-package-r2/console-test-20261008T155200Z-634c29/harvest-20261008T155939Z-0a44ac/CONSOLE-REPORT.md.

## Global constraints

- Every new shipped offset needs independent official-header/target-compiler and image evidence; calibrate against FArrayProperty::Inner=0x78. Preserve image identity binding and all 93 prior facts.
- No mod edits or UTF8/ANSI pusher registrations. No new console action before packet review. Only the future PRX may be uploaded under a separate approval.
- Catch missing-layout-key failures specifically; do not swallow allocation errors, faults, or unrelated exceptions. Optional nested string discovery may skip; missing core types stop before Lua.
- New pointer facts require readable slot/target and a validated known property FField class before use. Unknown or invalid targets must refuse; no guessed heap reads.
- Game-thread no-op uses the actual queue/dispatcher and existing lifetime; no fake drain or initialization-thread execution. Timeout is monotonic 30 seconds, diagnostic-only, without blocking startup or retaining freed owners.
- VMT fallback is a design report only. Epic source stays local and is never committed or sent to GitHub. Packet excludes Epic source unless the user explicitly authorizes its inclusion; cite local evidence hashes instead.
- Track B has source-verified absent vs compiler/UHT-blocked statuses. A compile error is not evidence that a key does not exist. Any 93-fact mismatch prevents accepting the generated table; no adjustments to force calibration.

## Review focus

- Missing map key/value or set element must not abort unrelated type discovery; errors collected once, core gate explicit.
- Invalid/unreadable slot, null/foreign target class, cycles, partial capture and wrong identity refuse without dereference.
- Queue job drains once on genuine registered game thread; deadline race and shutdown do not use a dead owner.
- Official macro branches/ABI and image evidence must actually agree; source-only facts are not promoted.
- Full generated inventory must preserve unresolved distinctions and reproducible compiler provenance.

## Parallel ownership and tasks

- [ ] Official/compiler worker: outputs/2026-10-08-l23-layout-generator/. Prioritize actual official container declarations compiled with target settings, then table generator for 711 keys. Deliver Track A source measurements early. Test calibration and deliberately mismatched controls; preserve raw target output and local source hashes.
- [ ] Image/profile worker: outputs/2026-10-08-l23-runtime/profile/. Measure Map KeyProp/ValueProp and Set ElementProp in authenticated image; verify Array Inner. Join official measurements when ready; promote only agreement. Produce copied profile/declarations/preflight and digest, with Inner startup-required. Do not edit shared initializer.
- [ ] Type-scan worker: outputs/2026-10-08-l23-runtime/typescan/. Copy selected TypeChecker and required generated getter/header overlays. Add optional nested probes, per-field missing-key collection, core-type gate and pointer validate-on-use. Tests exercise multiple independent missing keys, missing optional classes, missing core classes, unreadable/foreign pointers and preserved non-layout exceptions.
- [ ] Parent: outputs/2026-10-08-l23-runtime/thread/. Add actual queue no-op, thread/time/drain/timeout markers and cleanup tests. Produce VMT fallback report with offset-to-top/typeinfo and original Tick forwarding design; do not build hook. Integrate required initializer/owner changes after worker interfaces settle.
- [ ] Parent: independent source review, target affected-unit rebuild twice, same full packaging/import/static checks, byte comparison, current progress and review packet. No upload before user's OK.
- [ ] Track B finish: generated table plus new-values list, complete 93-fact calibration (or explicit mismatch/block evidence), verified absent/UHT/compiler-blocked inventory. Track B does not hold a valid Track A candidate.

User has supplied the scope and execution method explicitly; continue implementation without another design-approval pause. Packet review remains the deployment boundary.

## User revision after packet r2 (authoritative)

Build l23 now with the accepted local code and the same93 measured facts. New Map/Set offsets remain absent/conditional; no compiler measurements are required to ship absent facts. Rebuild complete affected header closure twice, package twice, exact imports/static gates and review packet before upload. No console contact in this stage.

Track B now uses public generic Unix/Linux/Clang Itanium headers or exact official declarations with explicit audited platform stubs. Do not seek Sony/Epic PS5 platform-extension material. Mark transitive layout-bearing platform/stub dependencies PLATFORM-DEPENDENT; accept their observations only with matching calibration/image evidence. Preserve93expectedvalues. Container offset source/image join is future l24, never an l23 change. Epic source stays local/uncommitted.
